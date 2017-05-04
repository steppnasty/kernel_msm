/* linux/arch/arm/mach-msm/devices.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Modified 2014, Brian Stepp <steppnasty@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <mach/dma.h>
#include <mach/board.h>
#ifdef CONFIG_MSM_RMT_STORAGE_SERVER
#include "smd_private.h"
#endif
#include <asm/mach/flash.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <asm/mach/mmc.h>
#include <asm/setup.h>

#include "devices.h"
#include "clock.h"
#include "proc_comm.h"
#include <mach/msm_hsusb.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_hsusb_hw.h>
#include <linux/msm_rotator.h>
#include <linux/msm_ion.h>
#ifdef CONFIG_USB_FUNCTION
#include <linux/usb/mass_storage_function.h>
#endif
#ifdef CONFIG_PMIC8058
#include <linux/mfd/pmic8058.h>
#endif

#if defined(CONFIG_MSM_MDP40)
#define MDP_BASE	0xA3F00000
#else
#define MDP_BASE        0xAA200000
#endif

#ifndef CONFIG_ARCH_MSM8X60
static struct resource resources_uart1[] = {
	{
		.start	= INT_UART1,
		.end	= INT_UART1,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART1_PHYS,
		.end	= MSM_UART1_PHYS + MSM_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource resources_uart2[] = {
	{
		.start	= INT_UART2,
		.end	= INT_UART2,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART2_PHYS,
		.end	= MSM_UART2_PHYS + MSM_UART2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource resources_uart3[] = {
	{
		.start	= INT_UART3,
		.end	= INT_UART3,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= MSM_UART3_PHYS,
		.end	= MSM_UART3_PHYS + MSM_UART3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_uart1 = {
	.name	= "msm_serial",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(resources_uart1),
	.resource	= resources_uart1,
};

struct platform_device msm_device_uart2 = {
	.name	= "msm_serial",
	.id	= 1,
	.num_resources	= ARRAY_SIZE(resources_uart2),
	.resource	= resources_uart2,
};

struct platform_device msm_device_uart3 = {
	.name	= "msm_serial",
	.id	= 2,
	.num_resources	= ARRAY_SIZE(resources_uart3),
	.resource	= resources_uart3,
};

static struct resource msm_uart1_dm_resources[] = {
	{
		.start = MSM_UART1DM_PHYS,
		.end   = MSM_UART1DM_PHYS + PAGE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_UART1DM_IRQ,
		.end   = INT_UART1DM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_UART1DM_RX,
		.end   = INT_UART1DM_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = DMOV_HSUART1_TX_CHAN,
		.end   = DMOV_HSUART1_RX_CHAN,
		.name  = "uartdm_channels",
		.flags = IORESOURCE_DMA,
	},
	{
		.start = DMOV_HSUART1_TX_CRCI,
		.end   = DMOV_HSUART1_RX_CRCI,
		.name  = "uartdm_crci",
		.flags = IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm1_dma_mask = DMA_BIT_MASK(32);

struct platform_device msm_device_uart_dm1 = {
	.name = "msm_serial_hs",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm_uart1_dm_resources),
	.resource = msm_uart1_dm_resources,
	.dev		= {
		.dma_mask = &msm_uart_dm1_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};

static struct resource msm_uart2_dm_resources[] = {
	{
		.start = MSM_UART2DM_PHYS,
		.end   = MSM_UART2DM_PHYS + PAGE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = INT_UART2DM_IRQ,
		.end   = INT_UART2DM_IRQ,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = INT_UART2DM_RX,
		.end   = INT_UART2DM_RX,
		.flags = IORESOURCE_IRQ,
	},
	{
		.start = DMOV_HSUART2_TX_CHAN,
		.end   = DMOV_HSUART2_RX_CHAN,
		.name  = "uartdm_channels",
		.flags = IORESOURCE_DMA,
	},
	{
		.start = DMOV_HSUART2_TX_CRCI,
		.end   = DMOV_HSUART2_RX_CRCI,
		.name  = "uartdm_crci",
		.flags = IORESOURCE_DMA,
	},
};

static u64 msm_uart_dm2_dma_mask = DMA_BIT_MASK(32);

struct platform_device msm_device_uart_dm2 = {
	.name = "msm_serial_hs",
	.id = 1,
	.num_resources = ARRAY_SIZE(msm_uart2_dm_resources),
	.resource = msm_uart2_dm_resources,
	.dev		= {
		.dma_mask = &msm_uart_dm2_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
};
#endif

#ifdef CONFIG_ARCH_MSM7X30
static struct resource resources_i2c_2[] = {
	{
		.start	= MSM_I2C_2_PHYS,
		.end	= MSM_I2C_2_PHYS + MSM_I2C_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_PWB_I2C_2,
		.end	= INT_PWB_I2C_2,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_i2c_2 = {
	.name		= "msm_i2c",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resources_i2c_2),
	.resource	= resources_i2c_2,
};
#endif

static struct resource resources_i2c[] = {
	{
		.start	= MSM_I2C_PHYS,
		.end	= MSM_I2C_PHYS + MSM_I2C_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_PWB_I2C,
		.end	= INT_PWB_I2C,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_i2c = {
	.name		= "msm_i2c",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_i2c),
	.resource	= resources_i2c,
};

#ifdef CONFIG_ARCH_MSM7X30
static struct resource resources_qup[] = {
	{
		.name   = "qup_phys_addr",
		.start	= MSM_QUP_PHYS,
		.end	= MSM_QUP_PHYS + MSM_QUP_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "gsbi_qup_i2c_addr",
		.start	= MSM_GSBI_QUP_I2C_PHYS,
		.end	= MSM_GSBI_QUP_I2C_PHYS + 4 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "qup_in_intr",
		.start	= INT_PWB_QUP_IN,
		.end	= INT_PWB_QUP_IN,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "qup_out_intr",
		.start	= INT_PWB_QUP_OUT,
		.end	= INT_PWB_QUP_OUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "qup_err_intr",
		.start	= INT_PWB_QUP_ERR,
		.end	= INT_PWB_QUP_ERR,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device qup_device_i2c = {
	.name		= "qup_i2c",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_qup),
	.resource	= resources_qup,
};
#endif
#if defined(CONFIG_SPI_QSD_NEW) || defined(CONFIG_SPI_QSD_NEW_VIVO)
static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start	= 0xA8000000,
		.end	= 0xA8000000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "spidm_channels",
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "spidm_crci",
		.flags  = IORESOURCE_DMA,
	},
};

struct platform_device qsdnew_device_spi = {
	.name		= "spi_qsd_new",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qsd_spi_resources),
	.resource	= qsd_spi_resources,
};
#endif

#ifdef CONFIG_I2C_SSBI
#define MSM_SSBI6_PHYS	0xAD900000
static struct resource msm_ssbi6_resources[] = {
	{
		.name   = "ssbi_base",
		.start	= MSM_SSBI6_PHYS,
		.end	= MSM_SSBI6_PHYS + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi6 = {
	.name		= "i2c_ssbi",
	.id		= 6,
	.num_resources	= ARRAY_SIZE(msm_ssbi6_resources),
	.resource	= msm_ssbi6_resources,
};

#define MSM_SSBI7_PHYS  0xAC800000
static struct resource msm_ssbi7_resources[] = {
	{
		.name   = "ssbi_base",
		.start  = MSM_SSBI7_PHYS,
		.end    = MSM_SSBI7_PHYS + SZ_4K - 1,
		.flags  = IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi7 = {
	.name		= "i2c_ssbi",
	.id		= 7,
	.num_resources	= ARRAY_SIZE(msm_ssbi7_resources),
	.resource	= msm_ssbi7_resources,
};
#endif /* CONFIG_I2C_SSBI */

#if defined(CONFIG_ARCH_MSM7X30)
#define GPIO_I2C_CLK 70
#define GPIO_I2C_DAT 71
#elif defined(CONFIG_ARCH_QSD8X50)
#define GPIO_I2C_CLK 95
#define GPIO_I2C_DAT 96
#else
#define GPIO_I2C_CLK 60
#define GPIO_I2C_DAT 61
#endif

void msm_i2c_gpio_init(void)
{
	gpio_request(GPIO_I2C_CLK, "i2c_clk");
	gpio_request(GPIO_I2C_DAT, "i2c_data");
}

void msm_set_i2c_mux(bool gpio, int *gpio_clk, int *gpio_dat, int clk_str, int dat_str)
{
	unsigned id;
	if (gpio) {
		id = GPIO_CFG(GPIO_I2C_CLK, 0, GPIO_CFG_OUTPUT,
				   GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(GPIO_I2C_DAT, 0, GPIO_CFG_OUTPUT,
				   GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		*gpio_clk = GPIO_I2C_CLK;
		*gpio_dat = GPIO_I2C_DAT;
	} else {
		id = GPIO_CFG(GPIO_I2C_CLK, 1, GPIO_CFG_INPUT,
				   GPIO_CFG_NO_PULL, clk_str);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(GPIO_I2C_DAT , 1, GPIO_CFG_INPUT,
				   GPIO_CFG_NO_PULL, dat_str);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}
}

#define MSM_NAND_PHYS		0xA0A00000

struct flash_platform_data msm_nand_data = {
	.parts		= NULL,
	.nr_parts	= 0,
};

static struct resource resources_nand[] = {
	[0] = {
		.start	= 7,
		.end	= 7,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm_device_nand = {
	.name		= "msm_nand",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_nand),
	.resource	= resources_nand,
	.dev		= {
		.platform_data	= &msm_nand_data,
	},
};

struct platform_device msm_device_smd = {
	.name	= "msm_smd",
	.id	= -1,
};

static struct resource resources_sdc1[] = {
	{
		.start	= MSM_SDC1_PHYS,
		.end	= MSM_SDC1_PHYS + MSM_SDC1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC1_0,
		.end	= INT_SDC1_0,
		.flags	= IORESOURCE_IRQ,
		.name	= "cmd_irq",
	},
	{
		.start	= INT_SDC1_1,
		.end	= INT_SDC1_1,
		.flags	= IORESOURCE_IRQ,
		.name	= "pio_irq",
	},
	{
		.flags	= IORESOURCE_IRQ | IORESOURCE_DISABLED,
		.name	= "status_irq"
	},
	{
		.start	= 8,
		.end	= 8,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc2[] = {
	{
		.start	= MSM_SDC2_PHYS,
		.end	= MSM_SDC2_PHYS + MSM_SDC2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC2_0,
		.end	= INT_SDC2_0,
		.flags	= IORESOURCE_IRQ,
		.name	= "cmd_irq",
	},
		{
		.start	= INT_SDC2_1,
		.end	= INT_SDC2_1,
		.flags	= IORESOURCE_IRQ,
		.name	= "pio_irq",
	},
	{
		.flags	= IORESOURCE_IRQ | IORESOURCE_DISABLED,
		.name	= "status_irq"
	},
	{
		.start	= 8,
		.end	= 8,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc3[] = {
	{
		.start	= MSM_SDC3_PHYS,
		.end	= MSM_SDC3_PHYS + MSM_SDC3_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC3_0,
		.end	= INT_SDC3_0,
		.flags	= IORESOURCE_IRQ,
		.name	= "cmd_irq",
	},
		{
		.start	= INT_SDC3_1,
		.end	= INT_SDC3_1,
		.flags	= IORESOURCE_IRQ,
		.name	= "pio_irq",
	},
	{
		.flags	= IORESOURCE_IRQ | IORESOURCE_DISABLED,
		.name	= "status_irq"
	},
	{
		.start	= 8,
		.end	= 8,
		.flags	= IORESOURCE_DMA,
	},
};

static struct resource resources_sdc4[] = {
	{
		.start	= MSM_SDC4_PHYS,
		.end	= MSM_SDC4_PHYS + MSM_SDC4_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SDC4_0,
		.end	= INT_SDC4_0,
		.flags	= IORESOURCE_IRQ,
		.name	= "cmd_irq",
	},
		{
		.start	= INT_SDC4_1,
		.end	= INT_SDC4_1,
		.flags	= IORESOURCE_IRQ,
		.name	= "pio_irq",
	},
	{
		.flags	= IORESOURCE_IRQ | IORESOURCE_DISABLED,
		.name	= "status_irq"
	},
	{
		.start	= 8,
		.end	= 8,
		.flags	= IORESOURCE_DMA,
	},
};

struct platform_device msm_device_sdc1 = {
	.name		= "msm_sdcc",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(resources_sdc1),
	.resource	= resources_sdc1,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc2 = {
	.name		= "msm_sdcc",
	.id		= 2,
	.num_resources	= ARRAY_SIZE(resources_sdc2),
	.resource	= resources_sdc2,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc3 = {
	.name		= "msm_sdcc",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(resources_sdc3),
	.resource	= resources_sdc3,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

struct platform_device msm_device_sdc4 = {
	.name		= "msm_sdcc",
	.id		= 4,
	.num_resources	= ARRAY_SIZE(resources_sdc4),
	.resource	= resources_sdc4,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
	},
};

static struct platform_device *msm_sdcc_devices[] __initdata = {
	&msm_device_sdc1,
	&msm_device_sdc2,
	&msm_device_sdc3,
	&msm_device_sdc4,
};

int __init msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat,
			unsigned int stat_irq, unsigned long stat_irq_flags)
{
	struct platform_device	*pdev;
	struct resource *res;

	if (controller < 1 || controller > 4)
		return -EINVAL;

	pdev = msm_sdcc_devices[controller-1];
	pdev->dev.platform_data = plat;

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "status_irq");
	if (!res)
		return -EINVAL;
	else if (stat_irq) {
		res->start = res->end = stat_irq;
		res->flags &= ~IORESOURCE_DISABLED;
		res->flags |= stat_irq_flags;
	}

#ifdef CONFIG_MMC_SUPPORT_EXTERNEL_DRIVER
	if (plat->use_ext_sdiodrv)
		pdev->name = plat->ext_sdiodrv_name;
#endif

	return platform_device_register(pdev);
}

#ifdef CONFIG_MSM_RMT_STORAGE_SERVER
#define RAMFS_INFO_MAGICNUMBER		0x654D4D43
#define RAMFS_INFO_VERSION		0x00000001
#define RAMFS_MODEMSTORAGE_ID		0x4D454653

static void __init msm_register_device(struct platform_device *pdev, void *data)
{
	int ret;

	pdev->dev.platform_data = data;

	ret = platform_device_register(pdev);
	if (ret)
		dev_err(&pdev->dev,
			  "%s: platform_device_register() failed = %d\n",
			  __func__, ret);
}

static struct resource rmt_storage_resources[] = {
       {
		.flags  = IORESOURCE_MEM,
       },
};

static struct platform_device rmt_storage_device = {
       .name           = "rmt_storage",
       .id             = -1,
       .num_resources  = ARRAY_SIZE(rmt_storage_resources),
       .resource       = rmt_storage_resources,
};

int __init rmt_storage_add_ramfs(void)
{
	struct shared_ramfs_table *ramfs_table;
	struct shared_ramfs_entry *ramfs_entry;
	int index;

	ramfs_table = smem_alloc(SMEM_SEFS_INFO,
			sizeof(struct shared_ramfs_table));

	if (!ramfs_table) {
		printk(KERN_WARNING "%s: No RAMFS table in SMEM\n", __func__);
		return -ENOENT;
	}

	if ((ramfs_table->magic_id != (u32) RAMFS_INFO_MAGICNUMBER) ||
		(ramfs_table->version != (u32) RAMFS_INFO_VERSION)) {
		printk(KERN_WARNING "%s: Magic / Version mismatch:, "
		       "magic_id=%#x, format_version=%#x\n", __func__,
		       ramfs_table->magic_id, ramfs_table->version);
		return -ENOENT;
	}

	for (index = 0; index < ramfs_table->entries; index++) {
		ramfs_entry = &ramfs_table->ramfs_entry[index];

		/* Find a match for the Modem Storage RAMFS area */
		if (ramfs_entry->client_id == (u32) RAMFS_MODEMSTORAGE_ID) {
			printk(KERN_INFO "%s: RAMFS Info (from SMEM): "
				"Baseaddr = 0x%08x, Size = 0x%08x\n", __func__,
				ramfs_entry->base_addr, ramfs_entry->size);

			rmt_storage_resources[0].start = ramfs_entry->base_addr;
			rmt_storage_resources[0].end = ramfs_entry->base_addr +
							ramfs_entry->size - 1;
			msm_register_device(&rmt_storage_device, ramfs_entry);
			return 0;
		}
	}
	return -ENOENT;
}
#endif

static struct resource msm_mddi_resources[] = {
	{
		.start	= MSM_PMDH_PHYS,
		.end	= MSM_PMDH_PHYS + MSM_PMDH_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	}
};

static struct resource resources_mddi1[] = {
	{
		.start	= MSM_EMDH_PHYS,
		.end	= MSM_EMDH_PHYS + MSM_EMDH_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_MDDI_EXT,
		.end	= INT_MDDI_EXT,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_mddi_device = {
	.name	= "mddi",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_mddi_resources),
	.resource	= msm_mddi_resources,
};

struct platform_device msm_device_mddi1 = {
	.name = "msm_mddi",
	.id = 1,
	.num_resources = ARRAY_SIZE(resources_mddi1),
	.resource = resources_mddi1,
	.dev            = {
		.coherent_dma_mask      = 0xffffffff,
	}
};

static struct resource msm_mdp_resources[] = {
	{
		.name	= "mdp",
		.start	= MDP_BASE,
		.end	= MDP_BASE + MSM_MDP_SIZE - 1,
		.flags	= IORESOURCE_MEM
	},
	{
		.start	= INT_MDP,
		.end	= INT_MDP,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_mdp_device = {
	.name = "mdp",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm_mdp_resources),
	.resource = msm_mdp_resources,
};

#if defined(CONFIG_ARCH_MSM7X30)
static struct resource msm_vidc_720p_resources[] = {
	{
		.start	= 0xA3B00000,
		.end	= 0xA3B00000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_MFC720,
		.end	= INT_MFC720,
		.flags	= IORESOURCE_IRQ,
	},
};

struct msm_vidc_platform_data msm_vidc_platform_data = {
	.memtype = ION_CAMERA_HEAP_ID,
	.enable_ion = 1,
	.disable_dmx = 0,
	.cont_mode_dpb_count = 8
};

struct platform_device msm_device_vidc_720p = {
	.name = "msm_vidc",
	.id = 0,
	.num_resources = ARRAY_SIZE(msm_vidc_720p_resources),
	.resource = msm_vidc_720p_resources,
	.dev = {
		.platform_data = &msm_vidc_platform_data,
	},
};
#endif

static struct resource resources_tssc[] = {
#if defined(CONFIG_ARCH_MSM7225)
	{
		.start	= MSM_TSSC_PHYS,
		.end	= MSM_TSSC_PHYS + MSM_TSSC_SIZE - 1,
		.name	= "tssc",
		.flags	= IORESOURCE_MEM,
	},
#endif
	{
		.start	= INT_TCHSCRN1,
		.end	= INT_TCHSCRN1,
		.name	= "tssc1",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
	{
		.start	= INT_TCHSCRN2,
		.end	= INT_TCHSCRN2,
		.name	= "tssc2",
		.flags	= IORESOURCE_IRQ | IRQF_TRIGGER_RISING,
	},
};

struct platform_device msm_device_touchscreen = {
	.name = "msm_touchscreen",
	.id = 0,
	.num_resources = ARRAY_SIZE(resources_tssc),
	.resource = resources_tssc,
};

#if defined(CONFIG_ARCH_QSD8X50)
static struct resource resources_spi[] = {
	{
		.start	= MSM_SPI_PHYS,
		.end	= MSM_SPI_PHYS + MSM_SPI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.name	= "irq_in",
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.name	= "irq_out",
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.name	= "irq_err",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_spi = {
	.name		= "msm_spi",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_spi),
	.resource	= resources_spi,
};
#endif

#ifdef CONFIG_MSM_ROTATOR
static struct resource resources_msm_rotator[] = {
	{
		.start	= MSM_ROTATOR_PHYS,
		.end	= MSM_ROTATOR_PHYS + MSM_ROTATOR_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_ROTATOR,
		.end	= INT_ROTATOR,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct msm_rot_clocks rotator_clocks[] = {
	{
		.clk_name = "rotator_clk",
		.clk_type = ROTATOR_AXICLK_CLK,
		.clk_rate = 0,
	},
	{
		.clk_name = "rotator_pclk",
		.clk_type = ROTATOR_PCLK_CLK,
		.clk_rate = 0,
	},
	{
		.clk_name = "rotator_imem_clk",
		.clk_type = ROTATOR_IMEMCLK_CLK,
		.clk_rate = 0,
	},
};

static struct msm_rotator_platform_data rotator_pdata = {
	.number_of_clocks = ARRAY_SIZE(rotator_clocks),
	.hardware_version_number = 0x1000303,
	.rotator_clks = rotator_clocks,
	.regulator_name = "fs_rot",
};

struct platform_device msm_rotator_device = {
	.name		= "msm_rotator",
	.id		= 0,
	.num_resources  = ARRAY_SIZE(resources_msm_rotator),
	.resource       = resources_msm_rotator,
	.dev = {
		.platform_data = &rotator_pdata,
	},
};

#endif

#ifdef CONFIG_MSM_SSBI
static struct resource resources_ssbi_pmic[] = {
	{
		.start	= MSM_PMIC_SSBI_PHYS,
		.end	= MSM_PMIC_SSBI_PHYS + MSM_PMIC_SSBI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device msm_device_ssbi_pmic = {
	.name		= "msm_ssbi",
	.id		= -1,
	.resource	= resources_ssbi_pmic,
	.num_resources	= ARRAY_SIZE(resources_ssbi_pmic),
};
#endif
