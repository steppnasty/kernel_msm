/*
 * AP4EVB board support
 *
 * Copyright (C) 2010  Magnus Damm
 * Copyright (C) 2008  Yoshihiro Shimoda
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mfd/sh_mobile_sdhi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/physmap.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/io.h>
#include <linux/smsc911x.h>
#include <linux/sh_intc.h>
#include <linux/sh_clk.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/input/sh_keysc.h>
#include <linux/usb/r8a66597.h>

#include <sound/sh_fsi.h>

#include <video/sh_mobile_lcdc.h>
#include <video/sh_mipi_dsi.h>

#include <mach/common.h>
#include <mach/irqs.h>
#include <mach/sh7372.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>

/*
 * Address	Interface		BusWidth	note
 * ------------------------------------------------------------------
 * 0x0000_0000	NOR Flash ROM (MCP)	16bit		SW7 : bit1 = ON
 * 0x0800_0000	user area		-
 * 0x1000_0000	NOR Flash ROM (MCP)	16bit		SW7 : bit1 = OFF
 * 0x1400_0000	Ether (LAN9220)		16bit
 * 0x1600_0000	user area		-		cannot use with NAND
 * 0x1800_0000	user area		-
 * 0x1A00_0000	-
 * 0x4000_0000	LPDDR2-SDRAM (POP)	32bit
 */

/*
 * NOR Flash ROM
 *
 *  SW1  |     SW2    | SW7  | NOR Flash ROM
 *  bit1 | bit1  bit2 | bit1 | Memory allocation
 * ------+------------+------+------------------
 *  OFF  | ON     OFF | ON   |    Area 0
 *  OFF  | ON     OFF | OFF  |    Area 4
 */

/*
 * NAND Flash ROM
 *
 *  SW1  |     SW2    | SW7  | NAND Flash ROM
 *  bit1 | bit1  bit2 | bit2 | Memory allocation
 * ------+------------+------+------------------
 *  OFF  | ON     OFF | ON   |    FCE 0
 *  OFF  | ON     OFF | OFF  |    FCE 1
 */

/*
 * SMSC 9220
 *
 *  SW1		SMSC 9220
 * -----------------------
 *  ON		access disable
 *  OFF		access enable
 */

/*
 * LCD / IRQ / KEYSC / IrDA
 *
 * IRQ = IRQ26 (TS), IRQ27 (VIO), IRQ28 (TouchScreen)
 * LCD = 2nd LCDC
 *
 * 		|		SW43			|
 * SW3		|	ON		|	OFF	|
 * -------------+-----------------------+---------------+
 * ON		| KEY / IrDA		| LCD		|
 * OFF		| KEY / IrDA / IRQ	| IRQ		|
 */

/*
 * USB
 *
 * J7 : 1-2  MAX3355E VBUS
 *      2-3  DC 5.0V
 *
 * S39: bit2: off
 */

/*
 * FSI/FSMI
 *
 * SW41	:  ON : SH-Mobile AP4 Audio Mode
 *	: OFF : Bluetooth Audio Mode
 */

/* MTD */
static struct mtd_partition nor_flash_partitions[] = {
	{
		.name		= "loader",
		.offset		= 0x00000000,
		.size		= 512 * 1024,
	},
	{
		.name		= "bootenv",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 512 * 1024,
	},
	{
		.name		= "kernel_ro",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 8 * 1024 * 1024,
		.mask_flags	= MTD_WRITEABLE,
	},
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 8 * 1024 * 1024,
	},
	{
		.name		= "data",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
	},
};

static struct physmap_flash_data nor_flash_data = {
	.width		= 2,
	.parts		= nor_flash_partitions,
	.nr_parts	= ARRAY_SIZE(nor_flash_partitions),
};

static struct resource nor_flash_resources[] = {
	[0]	= {
		.start	= 0x00000000,
		.end	= 0x08000000 - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct platform_device nor_flash_device = {
	.name		= "physmap-flash",
	.dev		= {
		.platform_data	= &nor_flash_data,
	},
	.num_resources	= ARRAY_SIZE(nor_flash_resources),
	.resource	= nor_flash_resources,
};

/* SMSC 9220 */
static struct resource smc911x_resources[] = {
	{
		.start	= 0x14000000,
		.end	= 0x16000000 - 1,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= evt2irq(0x02c0) /* IRQ6A */,
		.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL,
	},
};

static struct smsc911x_platform_config smsc911x_info = {
	.flags		= SMSC911X_USE_16BIT | SMSC911X_SAVE_MAC_ADDRESS,
	.irq_polarity   = SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type       = SMSC911X_IRQ_TYPE_PUSH_PULL,
};

static struct platform_device smc911x_device = {
	.name           = "smsc911x",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(smc911x_resources),
	.resource       = smc911x_resources,
	.dev            = {
		.platform_data = &smsc911x_info,
	},
};

/* KEYSC (Needs SW43 set to ON) */
static struct sh_keysc_info keysc_info = {
	.mode		= SH_KEYSC_MODE_1,
	.scan_timing	= 3,
	.delay		= 2500,
	.keycodes = {
		KEY_0, KEY_1, KEY_2, KEY_3, KEY_4,
		KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
		KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,
		KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
		KEY_K, KEY_L, KEY_M, KEY_N, KEY_O,
	},
};

static struct resource keysc_resources[] = {
	[0] = {
		.name	= "KEYSC",
		.start  = 0xe61b0000,
		.end    = 0xe61b0063,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = evt2irq(0x0be0), /* KEYSC_KEY */
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device keysc_device = {
	.name           = "sh_keysc",
	.id             = 0, /* "keysc0" clock */
	.num_resources  = ARRAY_SIZE(keysc_resources),
	.resource       = keysc_resources,
	.dev	= {
		.platform_data	= &keysc_info,
	},
};

/* SDHI0 */
static struct sh_mobile_sdhi_info sdhi0_info = {
	.dma_slave_tx = SHDMA_SLAVE_SDHI0_TX,
	.dma_slave_rx = SHDMA_SLAVE_SDHI0_RX,
};

static struct resource sdhi0_resources[] = {
	[0] = {
		.name	= "SDHI0",
		.start  = 0xe6850000,
		.end    = 0xe68501ff,
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.start  = evt2irq(0x0e00) /* SDHI0 */,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device sdhi0_device = {
	.name           = "sh_mobile_sdhi",
	.num_resources  = ARRAY_SIZE(sdhi0_resources),
	.resource       = sdhi0_resources,
	.id             = 0,
	.dev	= {
		.platform_data	= &sdhi0_info,
	},
};

/* USB1 */
void usb1_host_port_power(int port, int power)
{
	if (!power) /* only power-on supported for now */
		return;

	/* set VBOUT/PWEN and EXTLP1 in DVSTCTR */
	__raw_writew(__raw_readw(0xE68B0008) | 0x600, 0xE68B0008);
}

static struct r8a66597_platdata usb1_host_data = {
	.on_chip	= 1,
	.port_power	= usb1_host_port_power,
};

static struct resource usb1_host_resources[] = {
	[0] = {
		.name	= "USBHS",
		.start	= 0xE68B0000,
		.end	= 0xE68B00E6 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= evt2irq(0x1ce0) /* USB1_USB1I0 */,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device usb1_host_device = {
	.name	= "r8a66597_hcd",
	.id	= 1,
	.dev = {
		.dma_mask		= NULL,         /*  not use dma */
		.coherent_dma_mask	= 0xffffffff,
		.platform_data		= &usb1_host_data,
	},
	.num_resources	= ARRAY_SIZE(usb1_host_resources),
	.resource	= usb1_host_resources,
};

static struct sh_mobile_lcdc_info sh_mobile_lcdc_info = {
	.clock_source = LCDC_CLK_PERIPHERAL, /* One of interface clocks */
	.ch[0] = {
		.chan = LCDC_CHAN_MAINLCD,
		.bpp = 16,
		.interface_type = RGB24,
		.clock_divider = 1,
		.flags = LCDC_FLAGS_DWPOL,
		.lcd_cfg = {
			.name = "R63302(QHD)",
			.xres = 544,
			.yres = 961,
			.left_margin = 72,
			.right_margin = 600,
			.hsync_len = 16,
			.upper_margin = 8,
			.lower_margin = 8,
			.vsync_len = 2,
			.sync = FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT,
		},
		.lcd_size_cfg = {
			.width = 44,
			.height = 79,
		},
	}
};

static struct resource lcdc_resources[] = {
	[0] = {
		.name	= "LCDC",
		.start	= 0xfe940000, /* P4-only space */
		.end	= 0xfe943fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= intcs_evt2irq(0x580),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device lcdc_device = {
	.name		= "sh_mobile_lcdc_fb",
	.num_resources	= ARRAY_SIZE(lcdc_resources),
	.resource	= lcdc_resources,
	.dev	= {
		.platform_data	= &sh_mobile_lcdc_info,
		.coherent_dma_mask = ~0,
	},
};

static struct resource mipidsi0_resources[] = {
	[0] = {
		.start  = 0xffc60000,
		.end    = 0xffc68fff,
		.flags  = IORESOURCE_MEM,
	},
};

static struct sh_mipi_dsi_info mipidsi0_info = {
	.data_format	= MIPI_RGB888,
	.lcd_chan	= &sh_mobile_lcdc_info.ch[0],
};

static struct platform_device mipidsi0_device = {
	.name           = "sh-mipi-dsi",
	.num_resources  = ARRAY_SIZE(mipidsi0_resources),
	.resource       = mipidsi0_resources,
	.id             = 0,
	.dev	= {
		.platform_data	= &mipidsi0_info,
	},
};

/* FSI */
#define IRQ_FSI		evt2irq(0x1840)
#define FSIACKCR	0xE6150018
static void fsiackcr_init(struct clk *clk)
{
	u32 status = __raw_readl(clk->enable_reg);

	/* use external clock */
	status &= ~0x000000ff;
	status |= 0x00000080;
	__raw_writel(status, clk->enable_reg);
}

static struct clk_ops fsiackcr_clk_ops = {
	.init = fsiackcr_init,
};

static struct clk fsiackcr_clk = {
	.ops		= &fsiackcr_clk_ops,
	.enable_reg	= (void __iomem *)FSIACKCR,
	.rate		= 0, /* unknown */
};

struct sh_fsi_platform_info fsi_info = {
	.porta_flags = SH_FSI_BRS_INV |
		       SH_FSI_OUT_SLAVE_MODE |
		       SH_FSI_IN_SLAVE_MODE |
		       SH_FSI_OFMT(PCM) |
		       SH_FSI_IFMT(PCM),
};

static struct resource fsi_resources[] = {
	[0] = {
		.name	= "FSI",
		.start	= 0xFE3C0000,
		.end	= 0xFE3C0400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start  = IRQ_FSI,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device fsi_device = {
	.name		= "sh_fsi2",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(fsi_resources),
	.resource	= fsi_resources,
	.dev	= {
		.platform_data	= &fsi_info,
	},
};

static struct platform_device *ap4evb_devices[] __initdata = {
	&nor_flash_device,
	&smc911x_device,
	&keysc_device,
	&sdhi0_device,
	&usb1_host_device,
	&lcdc_device,
	&mipidsi0_device,
	&fsi_device,
};

/* TouchScreen (Needs SW3 set to OFF) */
#define IRQ28	evt2irq(0x3380) /* IRQ28A */
struct tsc2007_platform_data tsc2007_info = {
	.model			= 2007,
	.x_plate_ohms		= 180,
};

/* I2C */
static struct i2c_board_info i2c0_devices[] = {
	{
		I2C_BOARD_INFO("ak4643", 0x13),
	},
};

static struct i2c_board_info i2c1_devices[] = {
	{
		I2C_BOARD_INFO("r2025sd", 0x32),
	},
	{
		I2C_BOARD_INFO("tsc2007", 0x48),
		.type		= "tsc2007",
		.platform_data	= &tsc2007_info,
		.irq		= IRQ28,
	},
};

static struct map_desc ap4evb_io_desc[] __initdata = {
	/* create a 1:1 entity map for 0xe6xxxxxx
	 * used by CPGA, INTC and PFC.
	 */
	{
		.virtual	= 0xe6000000,
		.pfn		= __phys_to_pfn(0xe6000000),
		.length		= 256 << 20,
		.type		= MT_DEVICE_NONSHARED
	},
};

static void __init ap4evb_map_io(void)
{
	iotable_init(ap4evb_io_desc, ARRAY_SIZE(ap4evb_io_desc));

	/* setup early devices and console here as well */
	sh7372_add_early_devices();
	shmobile_setup_console();
}

/* This function will disappear when we switch to (runtime) PM */
static int __init ap4evb_init_display_clk(void)
{
	struct clk *lcdc_clk;
	struct clk *dsitx_clk;
	int ret;

	lcdc_clk = clk_get(&lcdc_device.dev, "sh_mobile_lcdc_fb.0");
	if (IS_ERR(lcdc_clk))
		return PTR_ERR(lcdc_clk);

	dsitx_clk = clk_get(&mipidsi0_device.dev, "sh-mipi-dsi.0");
	if (IS_ERR(dsitx_clk)) {
		ret = PTR_ERR(dsitx_clk);
		goto eclkdsitxget;
	}

	ret = clk_enable(lcdc_clk);
	if (ret < 0)
		goto eclklcdcon;

	ret = clk_enable(dsitx_clk);
	if (ret < 0)
		goto eclkdsitxon;

	return 0;

eclkdsitxon:
	clk_disable(lcdc_clk);
eclklcdcon:
	clk_put(dsitx_clk);
eclkdsitxget:
	clk_put(lcdc_clk);

	return ret;
}

device_initcall(ap4evb_init_display_clk);

/*
 * FIXME !!
 *
 * gpio_no_direction is quick_hack.
 *
 * current gpio frame work doesn't have
 * the method to control only pull up/down/free.
 * this function should be replaced by correct gpio function
 */
static void __init gpio_no_direction(u32 addr)
{
	__raw_writeb(0x00, addr);
}

#define GPIO_PORT9CR	0xE6051009
#define GPIO_PORT10CR	0xE605100A

static void __init ap4evb_init(void)
{
	struct clk *clk;

	sh7372_pinmux_init();

	/* enable SCIFA0 */
	gpio_request(GPIO_FN_SCIFA0_TXD, NULL);
	gpio_request(GPIO_FN_SCIFA0_RXD, NULL);

	/* enable SMSC911X */
	gpio_request(GPIO_FN_CS5A,	NULL);
	gpio_request(GPIO_FN_IRQ6_39,	NULL);

	/* enable LED 1 - 4 */
	gpio_request(GPIO_PORT185, NULL);
	gpio_request(GPIO_PORT186, NULL);
	gpio_request(GPIO_PORT187, NULL);
	gpio_request(GPIO_PORT188, NULL);
	gpio_direction_output(GPIO_PORT185, 1);
	gpio_direction_output(GPIO_PORT186, 1);
	gpio_direction_output(GPIO_PORT187, 1);
	gpio_direction_output(GPIO_PORT188, 1);
	gpio_export(GPIO_PORT185, 0);
	gpio_export(GPIO_PORT186, 0);
	gpio_export(GPIO_PORT187, 0);
	gpio_export(GPIO_PORT188, 0);

	/* enable Debug switch (S6) */
	gpio_request(GPIO_PORT32, NULL);
	gpio_request(GPIO_PORT33, NULL);
	gpio_request(GPIO_PORT34, NULL);
	gpio_request(GPIO_PORT35, NULL);
	gpio_direction_input(GPIO_PORT32);
	gpio_direction_input(GPIO_PORT33);
	gpio_direction_input(GPIO_PORT34);
	gpio_direction_input(GPIO_PORT35);
	gpio_export(GPIO_PORT32, 0);
	gpio_export(GPIO_PORT33, 0);
	gpio_export(GPIO_PORT34, 0);
	gpio_export(GPIO_PORT35, 0);

	/* enable KEYSC */
	gpio_request(GPIO_FN_KEYOUT0, NULL);
	gpio_request(GPIO_FN_KEYOUT1, NULL);
	gpio_request(GPIO_FN_KEYOUT2, NULL);
	gpio_request(GPIO_FN_KEYOUT3, NULL);
	gpio_request(GPIO_FN_KEYOUT4, NULL);
	gpio_request(GPIO_FN_KEYIN0_136, NULL);
	gpio_request(GPIO_FN_KEYIN1_135, NULL);
	gpio_request(GPIO_FN_KEYIN2_134, NULL);
	gpio_request(GPIO_FN_KEYIN3_133, NULL);
	gpio_request(GPIO_FN_KEYIN4,     NULL);

	/* SDHI0 */
	gpio_request(GPIO_FN_SDHICD0, NULL);
	gpio_request(GPIO_FN_SDHIWP0, NULL);
	gpio_request(GPIO_FN_SDHICMD0, NULL);
	gpio_request(GPIO_FN_SDHICLK0, NULL);
	gpio_request(GPIO_FN_SDHID0_3, NULL);
	gpio_request(GPIO_FN_SDHID0_2, NULL);
	gpio_request(GPIO_FN_SDHID0_1, NULL);
	gpio_request(GPIO_FN_SDHID0_0, NULL);

	/* enable TouchScreen */
	gpio_request(GPIO_FN_IRQ28_123, NULL);
	set_irq_type(IRQ28, IRQ_TYPE_LEVEL_LOW);

	/* USB enable */
	gpio_request(GPIO_FN_VBUS0_1,    NULL);
	gpio_request(GPIO_FN_IDIN_1_18,  NULL);
	gpio_request(GPIO_FN_PWEN_1_115, NULL);
	gpio_request(GPIO_FN_OVCN_1_114, NULL);
	gpio_request(GPIO_FN_EXTLP_1,    NULL);
	gpio_request(GPIO_FN_OVCN2_1,    NULL);

	/* setup USB phy */
	__raw_writew(0x8a0a, 0xE6058130);	/* USBCR2 */

	/* enable FSI2 */
	gpio_request(GPIO_FN_FSIAIBT,	NULL);
	gpio_request(GPIO_FN_FSIAILR,	NULL);
	gpio_request(GPIO_FN_FSIAISLD,	NULL);
	gpio_request(GPIO_FN_FSIAOSLD,	NULL);
	gpio_request(GPIO_PORT161,	NULL);
	gpio_direction_output(GPIO_PORT161, 0); /* slave */

	gpio_request(GPIO_PORT9, NULL);
	gpio_request(GPIO_PORT10, NULL);
	gpio_no_direction(GPIO_PORT9CR);  /* FSIAOBT needs no direction */
	gpio_no_direction(GPIO_PORT10CR); /* FSIAOLR needs no direction */

	/* set SPU2 clock to 119.6 MHz */
	clk = clk_get(NULL, "spu_clk");
	if (!IS_ERR_VALUE(clk)) {
		clk_set_rate(clk, clk_round_rate(clk, 119600000));
		clk_put(clk);
	}

	/* change parent of FSI A */
	clk = clk_get(NULL, "fsia_clk");
	if (!IS_ERR_VALUE(clk)) {
		clk_register(&fsiackcr_clk);
		clk_set_parent(clk, &fsiackcr_clk);
		clk_put(clk);
	}

	/*
	 * set irq priority, to avoid sound chopping
	 * when NFS rootfs is used
	 *  FSI(3) > SMSC911X(2)
	 */
	intc_set_priority(IRQ_FSI, 3);

	i2c_register_board_info(0, i2c0_devices,
				ARRAY_SIZE(i2c0_devices));

	i2c_register_board_info(1, i2c1_devices,
				ARRAY_SIZE(i2c1_devices));

	sh7372_add_standard_devices();

	platform_add_devices(ap4evb_devices, ARRAY_SIZE(ap4evb_devices));
}

static void __init ap4evb_timer_init(void)
{
	sh7372_clock_init();
	shmobile_timer.init();
}

static struct sys_timer ap4evb_timer = {
	.init		= ap4evb_timer_init,
};

MACHINE_START(AP4EVB, "ap4evb")
	.phys_io	= 0xe6000000,
	.io_pg_offst	= ((0xe6000000) >> 18) & 0xfffc,
	.map_io		= ap4evb_map_io,
	.init_irq	= sh7372_init_irq,
	.init_machine	= ap4evb_init,
	.timer		= &ap4evb_timer,
MACHINE_END
