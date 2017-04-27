/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/platform_device.h>
#include <mach/kgsl.h>
#include <linux/regulator/machine.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include <asm/clkdev.h>

#include "devices.h"
#include "clock-7x30.h"
#include "clock-local.h"
#include "footswitch.h"

#include <mach/dal_axi.h>

#ifdef CONFIG_MSMB_CAMERA
static struct resource msm_csic_resources[] = {
	{
		.name	= "csic",
		.start	= 0xA6100000,
		.end	= 0xA6100000 + 0x00000400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csic",
		.start	= INT_CSI,
		.end	= INT_CSI,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource msm_vpe_resources[] = {
	{
		.name	= "vpe",
		.start	= 0xAD200000,
		.end	= 0xAD200000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "vpe",
		.start	= INT_VPE,
		.end	= INT_VPE,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_csic0 = {
	.name		= "msm_csic",
	.id		= 0,
	.resource	= msm_csic_resources,
	.num_resources	= ARRAY_SIZE(msm_csic_resources),
};

struct platform_device msm_device_vpe = {
	.name		= "msm_vpe",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(msm_vpe_resources),
	.resource	= msm_vpe_resources,
};
#endif

struct resource msm_dmov_resource[] = {
	{
		.start = INT_ADM_AARM,
		.end = (resource_size_t)MSM_DMOV_BASE,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_dmov = {
	.name	= "msm_dmov",
	.id	= -1,
	.resource = msm_dmov_resource,
	.num_resources = ARRAY_SIZE(msm_dmov_resource),
};

static struct resource kgsl_3d0_resources[] = {
	{
		.name  = KGSL_3D0_REG_MEMORY,
		.start = 0xA3500000, /* 3D GRP address */
		.end = 0xA351ffff,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_3D0_IRQ,
		.start = INT_GRP_3D,
		.end = INT_GRP_3D,
		.flags = IORESOURCE_IRQ,
	},
};

static struct kgsl_device_platform_data kgsl_3d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 245760000,
			.bus_freq = 192000000,
		},
		{
			.gpu_freq = 192000000,
			.bus_freq = 153000000,
		},
		{
			.gpu_freq = 192000000,
			.bus_freq = 0,
		},
	},
	.init_level = 0,
	.num_levels = 3,
	.set_grp_async = set_grp3d_async,
	.idle_timeout = HZ/20,
	.nap_allowed = true,
	.clk = {
		.clk = "core_clk",
		.pclk = "iface_clk",
	},
	.imem_clk_name = {
		.clk = "mem_clk",
		.pclk = NULL,
	},
};

struct platform_device msm_kgsl_3d0 = {
	.name = "kgsl-3d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(kgsl_3d0_resources),
	.resource = kgsl_3d0_resources,
	.dev = {
		.platform_data = &kgsl_3d0_pdata,
	},
};

static struct resource kgsl_2d0_resources[] = {
	{
		.name = KGSL_2D0_REG_MEMORY,
		.start = 0xA3900000, /* Z180 base address */
		.end = 0xA3900FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = KGSL_2D0_IRQ,
		.start = INT_GRP_2D,
		.end = INT_GRP_2D,
		.flags = IORESOURCE_IRQ,
	},
};

static struct kgsl_device_platform_data kgsl_2d0_pdata = {
	.pwrlevel = {
		{
			.gpu_freq = 0,
			.bus_freq = 192000000,
		},
	},
	.init_level = 0,
	.num_levels = 1,
	/* HW workaround, run Z180 SYNC @ 192 MHZ */
	.set_grp_async = NULL,
	.idle_timeout = HZ/10,
	.nap_allowed = true,
	.clk = {
		.clk = "core_clk",
		.pclk = "iface_clk",
	},
};

struct platform_device msm_kgsl_2d0 = {
	.name = "kgsl-2d0",
	.id = 0,
	.num_resources = ARRAY_SIZE(kgsl_2d0_resources),
	.resource = kgsl_2d0_resources,
	.dev = {
		.platform_data = &kgsl_2d0_pdata,
	},
};

static int pll_src_enable(unsigned id)
{
	return local_src_enable(id);
}

static void pll_src_disable(unsigned id)
{
	local_src_disable(id);
}

#define CLK_PLL(clk_name, l_id, clk_dev, clk_flags) { \
	.con_id = clk_name, \
	.dev_id = clk_dev, \
	.clk = &(struct clk){ \
		.id = l_id, \
		.flags = clk_flags, \
		.dbg_name = clk_name, \
		.ops = &(struct clk_ops){ \
			.enable = pll_src_enable, \
			.disable = pll_src_disable, \
		}, \
	}, \
	}


struct clk_lookup msm_clocks_7x30[] = {
	CLK_PCOM("core_clk", ADM_CLK, "msm_dmov", 0),
	CLK_PCOM("adsp_clk", ADSP_CLK, NULL, 0),
	CLK_PCOM("cam_m_clk", CAM_M_CLK, NULL, 0),
	CLK_PCOM("camif_pad_pclk", CAMIF_PAD_P_CLK, NULL, OFF),
	CLK_PCOM("iface_clk", CAMIF_PAD_P_CLK, "qup_i2c.4", OFF),
	CLK_PCOM("codec_ssbi_clk", CODEC_SSBI_CLK, NULL, 0),
	CLK_PCOM("ebi1_clk", EBI1_CLK, NULL, USE_MIN),
	CLK_PCOM("ecodec_clk", ECODEC_CLK, NULL, 0),
	CLK_PCOM("emdh_clk", EMDH_CLK, NULL, OFF | MINMAX),
	CLK_PCOM("emdh_pclk", EMDH_P_CLK, NULL, OFF),
	CLK_PCOM("gp_clk", GP_CLK, NULL, 0),
	CLK_PCOM("core_clk", GRP_2D_CLK, "kgsl-2d0.0", 0),
	CLK_PCOM("iface_clk", GRP_2D_P_CLK, "kgsl-2d0.0", 0),
	CLK_PCOM("core_clk", GRP_3D_CLK, "kgsl-3d0.0", 0),
	CLK_PCOM("iface_clk", GRP_3D_P_CLK, "kgsl-3d0.0", 0),
	CLK_7X30S("grp_src_clk", GRP_3D_SRC_CLK, GRP_3D_CLK, NULL, 0),
	CLK_PCOM("hdmi_clk", HDMI_CLK, NULL, 0),
	CLK_PCOM("core_clk", I2C_CLK, "msm_i2c.0", 0),
	CLK_PCOM("core_clk", I2C_2_CLK, "msm_i2c.2", 0),
	CLK_PCOM("mem_clk", IMEM_CLK, "kgsl-3d0.0", OFF),
	CLK_PCOM("jpeg_clk", JPEG_CLK, NULL, OFF),
	CLK_PCOM("jpeg_pclk", JPEG_P_CLK, NULL, OFF),
	CLK_PCOM("lpa_codec_clk", LPA_CODEC_CLK, NULL, 0),
	CLK_PCOM("lpa_core_clk", LPA_CORE_CLK, NULL, 0),
	CLK_PCOM("lpa_pclk", LPA_P_CLK, NULL, 0),
	CLK_PCOM("mdc_clk", MDC_CLK, NULL, 0),
	CLK_PCOM("mddi_clk", PMDH_CLK, NULL, OFF | MINMAX),
	CLK_PCOM("mddi_pclk", PMDH_P_CLK, NULL, 0),
	CLK_PCOM("mdp_clk", MDP_CLK, NULL, OFF),
	CLK_PCOM("mdp_pclk", MDP_P_CLK, NULL, 0),
	/*Original is mdp_lcdc_pclk_clk and mdp_lcdc_pad_pclk_clk*/
	CLK_PCOM("mdp_lcdc_pclk_clk", MDP_LCDC_PCLK_CLK, NULL, OFF),
	CLK_PCOM("mdp_lcdc_pad_pclk_clk", MDP_LCDC_PAD_PCLK_CLK, NULL, OFF),
	CLK_PCOM("mdp_vsync_clk", MDP_VSYNC_CLK, NULL, 0),
	CLK_PCOM("mfc_clk", MFC_CLK, NULL, 0),
	CLK_PCOM("mfc_div2_clk", MFC_DIV2_CLK, NULL, 0),
	CLK_PCOM("mfc_pclk", MFC_P_CLK, NULL, 0),
	CLK_PCOM("mi2s_codec_rx_m_clk", MI2S_CODEC_RX_M_CLK, NULL, 0),
	CLK_PCOM("mi2s_codec_rx_s_clk", MI2S_CODEC_RX_S_CLK, NULL, 0),
	CLK_PCOM("mi2s_codec_tx_m_clk", MI2S_CODEC_TX_M_CLK, NULL, 0),
	CLK_PCOM("mi2s_codec_tx_s_clk", MI2S_CODEC_TX_S_CLK, NULL, 0),
        CLK_PCOM("mi2s_m_clk", MI2S_M_CLK, NULL, 0),
        CLK_PCOM("mi2s_s_clk", MI2S_S_CLK, NULL, 0),
	CLK_PCOM("pbus_clk", PBUS_CLK, NULL, USE_MIN),
	CLK_PCOM("pcm_clk", PCM_CLK, NULL, 0),
	CLK_PCOM("core_clk", QUP_I2C_CLK, "qup_i2c.4", 0),
	CLK_PCOM("rotator_clk", AXI_ROTATOR_CLK, NULL, 0),
	CLK_PCOM("rotator_imem_clk", ROTATOR_IMEM_CLK, NULL, OFF),
	CLK_PCOM("rotator_pclk", ROTATOR_P_CLK, NULL, OFF),
	CLK_PCOM("sdac_clk", SDAC_CLK, NULL, OFF),
	CLK_PCOM("core_clk", SDC1_CLK, "msm_sdcc.1", OFF),
	CLK_PCOM("iface_clk", SDC1_P_CLK, "msm_sdcc.1", OFF),
	CLK_PCOM("core_clk", SDC2_CLK, "msm_sdcc.2", OFF),
	CLK_PCOM("iface_clk", SDC2_P_CLK, "msm_sdcc.2", OFF),
	CLK_PCOM("core_clk", SDC3_CLK, "msm_sdcc.3", OFF),
	CLK_PCOM("iface_clk", SDC3_P_CLK, "msm_sdcc.3", OFF),
	CLK_PCOM("core_clk", SDC4_CLK, "msm_sdcc.4", OFF),
	CLK_PCOM("iface_clk", SDC4_P_CLK, "msm_sdcc.4", OFF),
	CLK_PCOM("core_clk", SPI_CLK, "spi_qsd_new.0", 0),
	CLK_PCOM("iface_clk", SPI_P_CLK, "spi_qsd_new.0", OFF),
	CLK_7X30S("tv_src_clk", TV_CLK, TV_ENC_CLK, NULL, 0),
	CLK_PCOM("tv_dac_clk", TV_DAC_CLK, NULL, 0),
	CLK_PCOM("tv_enc_clk", TV_ENC_CLK, NULL, 0),
	CLK_PCOM("core_clk", UART1_CLK, "msm_serial.0", OFF),
	CLK_PCOM("core_clk", UART2_CLK, "msm_serial.1", 0),
	CLK_PCOM("core_clk", UART3_CLK, "msm_serial.2", OFF),
	CLK_PCOM("core_clk", UART1DM_CLK, "msm_serial_hs.0", OFF),
	CLK_PCOM("core_clk", UART2DM_CLK, "msm_serial_hs.1", 0),
	CLK_PCOM("usb_hs_clk", USB_HS_CLK, NULL, OFF),
	CLK_PCOM("usb_hs_pclk", USB_HS_P_CLK, NULL, OFF),
	/* Now we can't close these usb clocks at the beginning change OFF to 0 temporarily */
	CLK_PCOM("usb_hs_core_clk", USB_HS_CORE_CLK, NULL, 0),
	CLK_PCOM("usb_hs2_clk", USB_HS2_CLK, NULL, 0),
	CLK_PCOM("usb_hs2_pclk", USB_HS2_P_CLK, NULL, 0),
	CLK_PCOM("usb_hs2_core_clk", USB_HS2_CORE_CLK, NULL, 0),
	CLK_PCOM("usb_hs3_clk", USB_HS3_CLK, NULL, 0),
	CLK_PCOM("usb_hs3_pclk", USB_HS3_P_CLK, NULL, 0),
	CLK_PCOM("usb_hs3_core_clk", USB_HS3_CORE_CLK, NULL, 0),
	CLK_PCOM("vdc_clk", VDC_CLK, NULL, OFF | MINMAX),
	CLK_PCOM("vfe_camif_clk", VFE_CAMIF_CLK, NULL, OFF),
	CLK_PCOM("vfe_clk", VFE_CLK, NULL, 0),
	CLK_PCOM("vfe_mdc_clk", VFE_MDC_CLK, NULL, OFF),
	CLK_PCOM("vfe_pclk", VFE_P_CLK, NULL, OFF),
	CLK_PCOM("vpe_clk", VPE_CLK, NULL, 0),

	/* 7x30 v2 hardware only. */
	CLK_PCOM("csi_clk", CSI0_CLK, NULL, 0),
	CLK_PCOM("csi_pclk", CSI0_P_CLK, NULL, 0),
	CLK_PCOM("csi_vfe_clk", CSI0_VFE_CLK, NULL, 0),
	CLK_PLL("pll1_clk", PLL_1, "acpu", 0),
	CLK_PLL("pll2_clk", PLL_2, "acpu", 0),
	CLK_PLL("pll3_clk", PLL_3, "acpu", 0),
};

unsigned msm_num_clocks_7x30 = ARRAY_SIZE(msm_clocks_7x30);

#define FS(_id, _name) (&(struct platform_device){ \
	.name   = "footswitch-pcom", \
	.id     = (_id), \
	.dev    = { \
		.platform_data = &(struct regulator_init_data){ \
			.constraints = { \
				.valid_modes_mask = REGULATOR_MODE_NORMAL, \
				.valid_ops_mask   = REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies = 1, \
			.consumer_supplies = \
				&(struct regulator_consumer_supply) \
				REGULATOR_SUPPLY((_name), NULL), \
		} \
	}, \
})
struct platform_device *msm_footswitch_devices[] = {
	FS_PCOM(FS_GFX2D0, "fs_gfx2d0"),
	FS_PCOM(FS_GFX3D,  "fs_gfx3d"),
	FS_PCOM(FS_MDP,    "fs_mdp"),
	FS_PCOM(FS_MFC,    "fs_mfc"),
	FS_PCOM(FS_ROT,    "fs_rot"),
	FS_PCOM(FS_VFE,    "fs_vfe"),
	FS_PCOM(FS_VPE,    "fs_vpe"),
};
unsigned msm_num_footswitch_devices = ARRAY_SIZE(msm_footswitch_devices);
