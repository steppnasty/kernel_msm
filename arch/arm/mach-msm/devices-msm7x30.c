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
