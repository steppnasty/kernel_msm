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

#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <mach/kgsl.h>
#include <linux/regulator/machine.h>
#include <mach/irqs.h>
#include <mach/msm_iomap.h>
#include "footswitch.h"

#include <mach/msm_hsusb.h>
#include <mach/msm_rpcrouter.h>
#include <mach/msm_hsusb_hw.h>
#ifdef CONFIG_PMIC8058
#include <linux/mfd/pmic8058.h>
#endif
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

int usb_phy_error;

#define HSUSB_API_INIT_PHY_PROC	2
#define HSUSB_API_PROG		0x30000064
#define HSUSB_API_VERS		0x00010001

static void internal_phy_reset(void)
{
	struct msm_rpc_endpoint *usb_ep;
	int rc;
	struct hsusb_phy_start_req {
		struct rpc_request_hdr hdr;
	} req;

	printk(KERN_INFO "msm_hsusb_phy_reset\n");

	usb_ep = msm_rpc_connect(HSUSB_API_PROG, HSUSB_API_VERS, 0);
	if (IS_ERR(usb_ep)) {
		printk(KERN_ERR "%s: init rpc failed! error: %ld\n",
				__func__, PTR_ERR(usb_ep));
		return;
	}
	rc = msm_rpc_call(usb_ep, HSUSB_API_INIT_PHY_PROC,
			&req, sizeof(req), 5 * HZ);
	if (rc < 0)
		printk(KERN_ERR "%s: rpc call failed! (%d)\n", __func__, rc);

	msm_rpc_close(usb_ep);
}

/* adjust eye diagram, disable vbusvalid interrupts */
static int hsusb_phy_init_seq[] = { 0x1D, 0x0D, 0x1D, 0x10, -1 };

#ifdef CONFIG_USB_FUNCTION
static char *usb_functions[] = {
#if defined(CONFIG_USB_FUNCTION_MASS_STORAGE) || defined(CONFIG_USB_FUNCTION_UMS)
	"usb_mass_storage",
#endif
#if defined(CONFIG_USB_FUNCTION_ADB)
	"adb",
#endif
#if defined(CONFIG_USB_FUNCTION_DIAG)
	"diag",
#endif
#if defined(CONFIG_USB_FUNCTION_MODEM)
	"serial",
#endif
#if defined(CONFIG_USB_FUNCTION_PROJECTOR)
	"projector",
#endif
#if defined(CONFIG_USB_FUNCTION_MTP_TUNNEL)
	"mtp_tunnel",
#endif
#if defined(CONFIG_USB_FUNCTION_ETHER)
	"ether",
#endif
#if defined(CONFIG_USB_FUNCTION_MODEM)
	"modem",
	"nmea",
#endif
};

static struct msm_hsusb_product usb_products[] = {
	{
		.product_id	= 0x0ff9,
		.functions	= 0x00000001, /* usb_mass_storage */
	},
	{
		.product_id	= 0x0c02,
		.functions	= 0x00000003, /* usb_mass_storage + adb */
	},
	{
		.product_id	= 0x0c03,
		.functions	= 0x00000101, /* modem + mass_storage */
	},
	{
		.product_id	= 0x0c04,
		.functions	= 0x00000103, /* modem + adb + mass_storage */
	},
	{
		.product_id = 0x0c05,
		.functions	= 0x00000021, /* Projector + mass_storage */
	},
	{
		.product_id = 0x0c06,
		.functions	= 0x00000023, /* Projector + adb + mass_storage */
	},
	{
		.product_id	= 0x0c07,
		.functions	= 0x0000000B, /* diag + adb + mass_storage */
	},
	{
		.product_id = 0x0c08,
		.functions	= 0x00000009, /* diag + mass_storage */
	},
	{
		.product_id = 0x0c88,
		.functions	= 0x0000010B, /* adb + mass_storage + diag + modem */
	},
	{
	       .product_id = 0x0c89,
	       .functions      = 0x00000019, /* serial + diag + mass_storage */
	},
	{
	       .product_id = 0x0c8a,
	       .functions      = 0x0000001B, /* serial + diag + adb + mass_storage */
	},
	{
		.product_id = 0x0c93,
		.functions	= 0x00000080, /* mtp */
	},
	{
		.product_id = 0x0FFE,
		.functions	= 0x00000004, /* internet sharing */
	},
};
#endif

struct msm_hsusb_platform_data msm_hsusb_pdata = {
	.phy_reset = internal_phy_reset,
	.phy_init_seq = hsusb_phy_init_seq,
#ifdef CONFIG_USB_FUNCTION
	.vendor_id = 0x0bb4,
	.product_id = 0x0c02,
	.version = 0x0100,
	.product_name = "Android Phone",
	.manufacturer_name = "HTC",

	.functions = usb_functions,
	.num_functions = ARRAY_SIZE(usb_functions),
	.products = usb_products,
	.num_products = ARRAY_SIZE(usb_products),
#endif
};

#ifdef CONFIG_USB_FUNCTION
static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns = 1,
	.buf_size = 16384,
	.vendor = "HTC     ",
	.product = "Android Phone   ",
	.release = 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name = "usb_mass_storage",
	.id = -1,
	.dev = {
		.platform_data = &mass_storage_pdata,
		},
};
#endif

static struct resource resources_hsusb[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + MSM_HSUSB_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_HS,
		.end	= INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "vbus_on",
		.start	= PM8058_IRQ_CHGVAL,
		.end	= PM8058_IRQ_CHGVAL,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsusb = {
	.name		= "msm_hsusb",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_hsusb),
	.resource	= resources_hsusb,
	.dev		= {
		.coherent_dma_mask	= 0xffffffff,
		.platform_data = &msm_hsusb_pdata,
	},
};

static u64 dma_mask = 0xffffffffULL;
static struct resource resources_otg[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + MSM_HSUSB_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_OTG,
		.end	= INT_USB_OTG,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_otg = {
	.name		= "msm_otg",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_otg),
	.resource	= resources_otg,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

static struct resource resources_hsusb_host[] = {
	{
		.start	= MSM_HSUSB_PHYS,
		.end	= MSM_HSUSB_PHYS + MSM_HSUSB_SIZE,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_USB_HS,
		.end	= INT_USB_HS,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device msm_device_hsusb_host = {
	.name		= "msm_hsusb_host",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(resources_hsusb_host),
	.resource	= resources_hsusb_host,
	.dev		= {
		.dma_mask		= &dma_mask,
		.coherent_dma_mask	= 0xffffffffULL,
	},
};

void __init msm_add_usb_devices(void (*phy_reset) (void), void (*phy_shutdown) (void))
{
	/* setup */
	msm_hsusb_pdata.phy_reset = phy_reset;
	msm_hsusb_pdata.phy_shutdown = phy_shutdown;

	msm_device_hsusb.dev.platform_data = &msm_hsusb_pdata;
	platform_device_register(&msm_device_hsusb);
#ifdef CONFIG_USB_FUNCTION
	platform_device_register(&usb_mass_storage_device);
#endif
}

#ifdef CONFIG_USB_FUNCTION
void __init msm_set_ums_device_id(int id)
{
	usb_mass_storage_device.id = id;
}

void __init msm_add_usb_id_pin_function(void (*config_usb_id_gpios)(bool enable))
{
	/* setup */
	msm_hsusb_pdata.config_usb_id_gpios = config_usb_id_gpios;

}
void __init msm_enable_car_kit_detect(bool enable)
{
	msm_hsusb_pdata.enable_car_kit_detect = enable;
}

void __init msm_init_ums_lun(int lun_num)
{
	if (lun_num > 0)
		mass_storage_pdata.nluns = lun_num;
}

void __init msm_register_usb_phy_init_seq(int *int_seq)
{
	if (int_seq)
		msm_hsusb_pdata.phy_init_seq = int_seq;
}

void __init msm_register_uart_usb_switch(void (*usb_uart_switch) (int))
{
	if (usb_uart_switch)
		msm_hsusb_pdata.usb_uart_switch = usb_uart_switch;
}

void __init msm_add_usb_id_pin_gpio(int usb_id_pin_io)
{
	msm_hsusb_pdata.usb_id_pin_gpio = usb_id_pin_io;
}

void __init msm_hsusb_set_product(struct msm_hsusb_product *product,
			int num_products) {
	msm_hsusb_pdata.products = product;
	msm_hsusb_pdata.num_products = num_products;
}
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
