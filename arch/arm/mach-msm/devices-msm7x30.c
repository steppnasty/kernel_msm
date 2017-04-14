/*
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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
#include <mach/irqs.h>
#include <mach/msm_iomap.h>

#include <mach/dal_axi.h>

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
		.clk = "grp_clk",
		.pclk = "grp_pclk",
	},
	.imem_clk_name = {
		.clk = "imem_clk",
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
		.clk = "grp_2d_clk",
		.pclk = "grp_2d_pclk",
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
