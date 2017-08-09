/* linux/arch/arm/mach-msm/board-glacier-panel.c
 *
 * Copyright (C) 2008 HTC Corporation.
 * Author: Jay Tu <jay_tu@htc.com>
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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>

#include <asm/io.h>
#include <asm/mach-types.h>
#include <mach/msm_fb.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <mach/vreg.h>

#include "board-glacier.h"
#include "devices.h"
#include "proc_comm.h"

#if 1
#define B(s...) printk(s)
#else
#define B(s...) do {} while(0)
#endif

extern int panel_type;

static struct clk *axi_clk;

struct vreg {
        const char *name;
        unsigned id;
};

struct vreg *vreg_ldo19, *vreg_ldo20;
struct vreg *vreg_ldo12;

static struct resource msm_fb_resources[] = {
	{
		.start = MSM_FB_BASE,
		.end = MSM_FB_BASE + MSM_FB_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
};

static int mddi_novatec_power(int on)
{
	unsigned pulldown = 1;

	if (panel_type == 0) {
		if (on) {
			if(axi_clk)
				clk_set_rate(axi_clk, 192000000);

			vreg_enable(vreg_ldo20);
			hr_msleep(5);
			vreg_disable(vreg_ldo20);
			hr_msleep(55);
			gpio_set_value(GLACIER_LCD_2V85_EN, 1);
			/* OJ_2V85*/
			vreg_enable(vreg_ldo12);
			hr_msleep(1);
			vreg_enable(vreg_ldo20);
			hr_msleep(2);
			vreg_enable(vreg_ldo19);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 1);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 0);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 1);
			hr_msleep(65);
		} else {
			hr_msleep(130);
			gpio_set_value(GLACIER_MDDI_RSTz, 0);
			hr_msleep(15);
			vreg_disable(vreg_ldo20);
			hr_msleep(15);
			vreg_disable(vreg_ldo19);
			/* OJ_2V85*/
			vreg_disable(vreg_ldo12);
			gpio_set_value(GLACIER_LCD_2V85_EN, 0);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo20->id);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo19->id);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo12->id);
		}
	} else {
		if (on) {
			if(axi_clk)
				clk_set_rate(axi_clk, 192000000);

			vreg_enable(vreg_ldo20);
			hr_msleep(5);
			vreg_disable(vreg_ldo20);
			hr_msleep(55);
			gpio_set_value(GLACIER_LCD_2V85_EN, 1);
			/* OJ_2V85*/
			vreg_enable(vreg_ldo12);
			hr_msleep(1);
			vreg_enable(vreg_ldo20);
			hr_msleep(2);
			vreg_enable(vreg_ldo19);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 1);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 0);
			hr_msleep(2);
			gpio_set_value(GLACIER_MDDI_RSTz, 1);
			hr_msleep(65);
		} else {
			hr_msleep(130);
			gpio_set_value(GLACIER_MDDI_RSTz, 0);
			hr_msleep(15);
			vreg_disable(vreg_ldo20);
			hr_msleep(15);
			vreg_disable(vreg_ldo19);
			/* OJ_2V85*/
			vreg_disable(vreg_ldo12);
			gpio_set_value(GLACIER_LCD_2V85_EN, 0);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo20->id);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo19->id);
			msm_proc_comm(PCOM_VREG_PULLDOWN, &pulldown, &vreg_ldo12->id);
		}
	}
	return 0;
}

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
	*clk_rate *= 2;
	return 0;
}

static int msm_fb_detect_panel(const char *name)
{
	return 0;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

struct platform_device msm_fb_device = {
	.name	= "msm_fb",
	.id	= 0,
	.num_resources	= ARRAY_SIZE(msm_fb_resources),
	.resource	= msm_fb_resources,
	.dev	= {
		.platform_data = &msm_fb_pdata,
	}
};

static struct mddi_platform_data mddi_pdata = {
	.mddi_power_save = mddi_novatec_power,
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

int mdp_core_clk_rate_table[] = {
	122880000,
	122880000,
	192000000,
	192000000,
};

static struct msm_panel_common_pdata mdp_pdata = {
	.hw_revision_addr = 0xac001270,
	.gpio = 30,
	.mdp_max_clk = 192000000,
	.mdp_core_clk_table = mdp_core_clk_rate_table,
	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
	.mdp_rev = MDP_REV_40,
	.mem_hid = MEMTYPE_EBI0,
	.ov0_wb_size = 0,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);
	msm_fb_register_device("pmdh", &mddi_pdata);
}

int __init glacier_init_panel(void)
{
	int rc;

	B(KERN_INFO "%s: enter. panel type %d\n", __func__, panel_type);

	/* turn on L12 for OJ. Note: must before L19 */
	vreg_ldo12 = vreg_get(NULL, "gp9");
	vreg_set_level(vreg_ldo12, 2850);

	/* lcd panel power */
	/* 2.85V -- LDO20 */
	vreg_ldo20 = vreg_get(NULL, "gp13");

	if (IS_ERR(vreg_ldo20)) {
		pr_err("%s: gp13 vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo20));
		return -1;
	}

	vreg_ldo19 = vreg_get(NULL, "wlan2");

	if (IS_ERR(vreg_ldo19)) {
		pr_err("%s: wlan2 vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_ldo19));
		return -1;
	}

	rc = vreg_set_level(vreg_ldo20, 3000);
	if (rc) {
		pr_err("%s: vreg LDO20 set level failed (%d)\n",
			__func__, rc);
		return rc;
	}

	rc = vreg_set_level(vreg_ldo19, 1800);
	if (rc) {
		pr_err("%s: vreg LDO19 set level failed (%d)\n",
		       __func__, rc);
		return rc;
	}

	rc = platform_device_register(&msm_fb_device);
	if (rc)
		return rc;

	msm_fb_add_devices();

	axi_clk = clk_get(NULL, "ebi1_clk");
	if (IS_ERR(axi_clk)) {
		pr_err("%s: failed to get axi clock\n", __func__);
		return PTR_ERR(axi_clk);
	}

	return 0;
}
