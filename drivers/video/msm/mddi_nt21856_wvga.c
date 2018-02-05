/*
 * Copyright (C) 2014-2018 Brian Stepp <steppnasty@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <mach/panel_id.h>
#include "msm_fb.h"
#include "msm_fb_panel.h"
#include "mddihost.h"
#include "mddihosti.h"

#define PANEL_SHARP	1
#define PANEL_SONY	2

#define DEFAULT_BRIGHTNESS 100

#define PWM_USER_DEF	143
#define	PWM_USER_MIN	30
#define PWM_USER_DIM	20
#define PWM_USER_MAX	255

#define PWM_SHARP_DEF	103
#define PWM_SHARP_MIN	11
#define PWM_SHARP_MAX	218

#define PWM_SONY_DEF	120
#define PWM_SONY_MIN	13
#define PWM_SONY_MAX	255

static int bl_level_prevset = 1;
static int glacier_set_dim = 1;
mddi_host_type host_idx = MDDI_HOST_PRIM;

static int glacier_shrink_pwm(int brightness, int user_def,
		int user_min, int user_max, int panel_def,
		int panel_min, int panel_max)
{
	if (brightness < PWM_USER_DIM) {
		return 0;
	}

	if (brightness < user_min) {
		return panel_min;
	}

	if (brightness > user_def) {
		brightness = (panel_max - panel_def) *
			(brightness - user_def) /
			(user_max - user_def) +
			panel_def;
	} else {
		brightness = (panel_def - panel_min) *
			(brightness - user_min) /
			(user_def - user_min) +
			panel_min;
	}

	return brightness;
}

static void mddi_glacier_set_backlight(struct msm_fb_data_type *mfd)
{
	unsigned int shrink_br;

	if (panel_type == PANEL_SHARP)
		shrink_br = glacier_shrink_pwm(mfd->bl_level, PWM_USER_DEF,
				PWM_USER_MIN, PWM_USER_MAX, PWM_SHARP_DEF,
				PWM_SHARP_MIN, PWM_SHARP_MAX);
	else
		shrink_br = glacier_shrink_pwm(mfd->bl_level, PWM_USER_DEF,
				PWM_USER_MIN, PWM_USER_MAX, PWM_SONY_DEF,
				PWM_SONY_MIN, PWM_SONY_MAX);
	if (glacier_set_dim == 1) {
		mddi_queue_register_write(0x5300, 0x2C, FALSE, 0);
		/* we dont need to set dim again */
		glacier_set_dim = 0;
	}
	mddi_queue_register_write(0x5500, 0x00, TRUE, 0);
	mddi_queue_register_write(0x5100, shrink_br, TRUE, 0);
}

static void mddi_glacier_display_on(struct msm_fb_data_type *mfd)
{
}

static void mddi_glacier_bkl_switch(struct msm_fb_data_type *mfd, bool on)
{
	if (on) {
		if (mfd->bl_level == 0) {
			if (bl_level_prevset != 1)
				mfd->bl_level = bl_level_prevset;
			else {
				mfd->bl_level = DEFAULT_BRIGHTNESS;
				glacier_set_dim = 1;
			}
			
		}
		mddi_glacier_set_backlight(mfd);
	} else
		mfd->bl_level = 0;
}

static void mddi_sharp_lcd_init(void)
{
	mddi_queue_register_write(0x1100, 0x00, TRUE, 0);
	mddi_host_reg_out(CMD, MDDI_CMD_POWERDOWN);
	mddi_wait(120);
	mddi_queue_register_write(0x3500, 0x00, TRUE, 0);
	mddi_queue_register_write(0x5100, 0x00, TRUE, 0);
	mddi_queue_register_write(0x89C3, 0x0080, TRUE, 0);
	mddi_queue_register_write(0x92C2, 0x0008, TRUE, 0);
	mddi_queue_register_write(0x0180, 0x0014, TRUE, 0);
	mddi_queue_register_write(0x0280, 0x0011, TRUE, 0);
	mddi_queue_register_write(0x0380, 0x0033, TRUE, 0);
	mddi_queue_register_write(0x0480, 0x0054, TRUE, 0);
	mddi_queue_register_write(0x0580, 0x0054, TRUE, 0);
	mddi_queue_register_write(0x0680, 0x0054, TRUE, 0);
	mddi_queue_register_write(0x0780, 0x0000, TRUE, 0);
	mddi_queue_register_write(0x0880, 0x0044, TRUE, 0);
	mddi_queue_register_write(0x0980, 0x0034, TRUE, 0);
	mddi_queue_register_write(0x0A80, 0x0010, TRUE, 0);
	mddi_queue_register_write(0x0B80, 0x0055, TRUE, 0);
	mddi_queue_register_write(0x0C80, 0x0055, TRUE, 0);
	mddi_queue_register_write(0x0D80, 0x0030, TRUE, 0);
	mddi_queue_register_write(0x0E80, 0x0044, TRUE, 0);
	mddi_queue_register_write(0x0F80, 0x0054, TRUE, 0);
	mddi_queue_register_write(0x1080, 0x0030, TRUE, 0);
	mddi_queue_register_write(0x1280, 0x0077, TRUE, 0);
	mddi_queue_register_write(0x1380, 0x0021, TRUE, 0);
	mddi_queue_register_write(0x1480, 0x000E, TRUE, 0);
	mddi_queue_register_write(0x1580, 0x0098, TRUE, 0);
	mddi_queue_register_write(0x1680, 0x00CC, TRUE, 0);
	mddi_queue_register_write(0x1780, 0x0000, TRUE, 0);
	mddi_queue_register_write(0x1880, 0x0000, TRUE, 0);
	mddi_queue_register_write(0x1980, 0x0000, TRUE, 0);
	mddi_queue_register_write(0x1C80, 0x0000, TRUE, 0);
	mddi_queue_register_write(0x1F80, 0x0005, TRUE, 0);
	mddi_queue_register_write(0x2480, 0x001A, TRUE, 0);
	mddi_queue_register_write(0x2580, 0x001F, TRUE, 0);
	mddi_queue_register_write(0x2680, 0x002D, TRUE, 0);
	mddi_queue_register_write(0x2780, 0x003E, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x2880, 0x000D, FALSE, 0);
	mddi_queue_register_write(0x2980, 0x0021, FALSE, 0);
	mddi_queue_register_write(0x2A80, 0x0058, FALSE, 0);
	mddi_queue_register_write(0x2B80, 0x002A, FALSE, 0);
	mddi_queue_register_write(0x2D80, 0x0020, FALSE, 0);
	mddi_queue_register_write(0x2F80, 0x0027, FALSE, 0);
	mddi_queue_register_write(0x3080, 0x0061, FALSE, 0);
	mddi_queue_register_write(0x3180, 0x0017, FALSE, 0);
	mddi_queue_register_write(0x3280, 0x0037, FALSE, 0);
	mddi_queue_register_write(0x3380, 0x0053, FALSE, 0);
	mddi_queue_register_write(0x3480, 0x005A, FALSE, 0);
	mddi_queue_register_write(0x3480, 0x005A, FALSE, 0);
	mddi_queue_register_write(0x3580, 0x008E, FALSE, 0);
	mddi_queue_register_write(0x3680, 0x00A7, FALSE, 0);
	mddi_queue_register_write(0x3780, 0x003E, FALSE, 0);
	mddi_queue_register_write(0x3880, 0x002B, FALSE, 0);
	mddi_queue_register_write(0x3980, 0x002E, FALSE, 0);
	mddi_queue_register_write(0x3A80, 0x0036, FALSE, 0);
	mddi_queue_register_write(0x3B80, 0x0041, FALSE, 0);
	mddi_queue_register_write(0x3D80, 0x001A, FALSE, 0);
	mddi_queue_register_write(0x3F80, 0x002D, FALSE, 0);
	mddi_queue_register_write(0x4080, 0x005D, FALSE, 0);
	mddi_queue_register_write(0x4180, 0x003D, FALSE, 0);
	mddi_queue_register_write(0x4280, 0x0020, FALSE, 0);
	mddi_queue_register_write(0x4380, 0x0027, FALSE, 0);
	mddi_queue_register_write(0x4480, 0x0076, FALSE, 0);
	mddi_queue_register_write(0x4580, 0x0017, FALSE, 0);
	mddi_queue_register_write(0x4680, 0x0039, FALSE, 0);
	mddi_queue_register_write(0x4780, 0x0055, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x4880, 0x0071, FALSE, 0);
	mddi_queue_register_write(0x4980, 0x00A6, FALSE, 0);
	mddi_queue_register_write(0x4A80, 0x00BF, FALSE, 0);
	mddi_queue_register_write(0x4B80, 0x0055, FALSE, 0);
	mddi_queue_register_write(0x4C80, 0x0055, FALSE, 0);
	mddi_queue_register_write(0x4D80, 0x0058, FALSE, 0);
	mddi_queue_register_write(0x4E80, 0x005F, FALSE, 0);
	mddi_queue_register_write(0x4F80, 0x0066, FALSE, 0);
	mddi_queue_register_write(0x5080, 0x0018, FALSE, 0);
	mddi_queue_register_write(0x5180, 0x0026, FALSE, 0);
	mddi_queue_register_write(0x5280, 0x0057, FALSE, 0);
	mddi_queue_register_write(0x5380, 0x003D, FALSE, 0);
	mddi_queue_register_write(0x5480, 0x001E, FALSE, 0);
	mddi_queue_register_write(0x5580, 0x0026, FALSE, 0);
	mddi_queue_register_write(0x5680, 0x006B, FALSE, 0);
	mddi_queue_register_write(0x5780, 0x0017, FALSE, 0);
	mddi_queue_register_write(0x5880, 0x003B, FALSE, 0);
	mddi_queue_register_write(0x5980, 0x004F, FALSE, 0);
	mddi_queue_register_write(0x5A80, 0x005A, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x5B80, 0x008E, FALSE, 0);
	mddi_queue_register_write(0x5C80, 0x00A7, FALSE, 0);
	mddi_queue_register_write(0x5D80, 0x003E, FALSE, 0);
	mddi_queue_register_write(0x5E80, 0x0066, FALSE, 0);
	mddi_queue_register_write(0x5F80, 0x0068, FALSE, 0);
	mddi_queue_register_write(0x6080, 0x006C, FALSE, 0);
	mddi_queue_register_write(0x6180, 0x006E, FALSE, 0);
	mddi_queue_register_write(0x6280, 0x0016, FALSE, 0);
	mddi_queue_register_write(0x6380, 0x002A, FALSE, 0);
	mddi_queue_register_write(0x6480, 0x0059, FALSE, 0);
	mddi_queue_register_write(0x6580, 0x004C, FALSE, 0);
	mddi_queue_register_write(0x6680, 0x001E, FALSE, 0);
	mddi_queue_register_write(0x6780, 0x0025, FALSE, 0);
	mddi_queue_register_write(0x6880, 0x007B, FALSE, 0);
	mddi_queue_register_write(0x6980, 0x0017, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x6A80, 0x003A, FALSE, 0);
	mddi_queue_register_write(0x6B80, 0x0053, FALSE, 0);
	mddi_queue_register_write(0x6C80, 0x0071, FALSE, 0);
	mddi_queue_register_write(0x6D80, 0x00A6, FALSE, 0);
	mddi_queue_register_write(0x6E80, 0x00BF, FALSE, 0);
	mddi_queue_register_write(0x6F80, 0x0055, FALSE, 0);
	mddi_queue_register_write(0x7080, 0x0063, FALSE, 0);
	mddi_queue_register_write(0x7180, 0x0066, FALSE, 0);
	mddi_queue_register_write(0x7280, 0x0070, FALSE, 0);
	mddi_queue_register_write(0x7380, 0x0076, FALSE, 0);
	mddi_queue_register_write(0x7480, 0x0018, FALSE, 0);
	mddi_queue_register_write(0x7580, 0x0027, FALSE, 0);
	mddi_queue_register_write(0x7680, 0x0058, FALSE, 0);
	mddi_queue_register_write(0x7780, 0x0047, FALSE, 0);
	mddi_queue_register_write(0x7880, 0x001E, FALSE, 0);
	mddi_queue_register_write(0x7980, 0x0025, FALSE, 0);
	mddi_queue_register_write(0x7A80, 0x0072, FALSE, 0);
	mddi_queue_register_write(0x7B80, 0x0018, FALSE, 0);
	mddi_queue_register_write(0x7C80, 0x003B, FALSE, 0);
	mddi_queue_register_write(0x7D80, 0x004C, FALSE, 0);
	mddi_queue_register_write(0x7E80, 0x005A, FALSE, 0);
	mddi_queue_register_write(0x7F80, 0x008E, FALSE, 0);
	mddi_queue_register_write(0x8080, 0x00A7, FALSE, 0);
	mddi_queue_register_write(0x8180, 0x003E, FALSE, 0);
	mddi_queue_register_write(0x8280, 0x0075, FALSE, 0);
	mddi_queue_register_write(0x8380, 0x0077, FALSE, 0);
	mddi_queue_register_write(0x8480, 0x007C, FALSE, 0);
	mddi_queue_register_write(0x8580, 0x007E, FALSE, 0);
	mddi_queue_register_write(0x8680, 0x0016, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x8780, 0x002C, FALSE, 0);
	mddi_queue_register_write(0x8880, 0x005C, FALSE, 0);
	mddi_queue_register_write(0x8980, 0x0055, FALSE, 0);
	mddi_queue_register_write(0x8A80, 0x001F, FALSE, 0);
	mddi_queue_register_write(0x8B80, 0x0024, FALSE, 0);
	mddi_queue_register_write(0x8C80, 0x0082, FALSE, 0);
	mddi_queue_register_write(0x8D80, 0x0015, FALSE, 0);
	mddi_queue_register_write(0x8E80, 0x0038, FALSE, 0);
	mddi_queue_register_write(0x8F80, 0x0050, FALSE, 0);
	mddi_queue_register_write(0x9080, 0x0071, FALSE, 0);
	mddi_queue_register_write(0x9180, 0x00A6, FALSE, 0);
	mddi_queue_register_write(0x9280, 0x00BF, FALSE, 0);
	mddi_queue_register_write(0x9380, 0x0055, FALSE, 0);
	mddi_queue_register_write(0x9480, 0x00B5, FALSE, 0);
	mddi_queue_register_write(0x9580, 0x0004, FALSE, 0);
	mddi_queue_register_write(0x9680, 0x0018, FALSE, 0);
	mddi_queue_register_write(0x9780, 0x00B0, FALSE, 0);
	mddi_queue_register_write(0x9880, 0x0020, FALSE, 0);
	mddi_queue_register_write(0x9980, 0x0028, FALSE, 0);
	mddi_queue_register_write(0x9A80, 0x0008, FALSE, 0);
	mddi_queue_register_write(0x9B80, 0x0004, FALSE, 0);
	mddi_queue_register_write(0x9C80, 0x0012, FALSE, 0);
	mddi_queue_register_write(0x9D80, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x9E80, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x9F80, 0x0012, FALSE, 0);
	mddi_queue_register_write(0xA080, 0x0000, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0xA280, 0x0000, FALSE, 0);
	mddi_queue_register_write(0xA380, 0x003C, FALSE, 0);
	mddi_queue_register_write(0xA480, 0x0001, FALSE, 0);
	mddi_queue_register_write(0xA580, 0x00C0, FALSE, 0);
	mddi_queue_register_write(0xA680, 0x0001, FALSE, 0);
	mddi_queue_register_write(0xA780, 0x0000, FALSE, 0);
	mddi_queue_register_write(0xA980, 0x0000, FALSE, 0);
	mddi_queue_register_write(0xAA80, 0x0000, FALSE, 0);
	mddi_queue_register_write(0xAB80, 0x0070, FALSE, 0);
	mddi_queue_register_write(0xE780, 0x0011, FALSE, 0);
	mddi_queue_register_write(0xE880, 0x0011, FALSE, 0);
	mddi_queue_register_write(0xED80, 0x000A, FALSE, 0);
	mddi_queue_register_write(0xEE80, 0x0080, FALSE, 0);
	mddi_queue_register_write(0xF780, 0x000D, FALSE, 0);
	mddi_queue_register_write(0x2900, 0x0000, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x4400, 0x0001, FALSE, 0);
	mddi_queue_register_write(0x4401, 0x002b, FALSE, 0);
	mddi_queue_register_write(0x0480, 0x0063, FALSE, 0);
	mddi_queue_register_write(0x22c0, 0x0006, FALSE, 0);
}

static void mddi_sony_lcd_init(void)
{
	mddi_queue_register_write(0x1100, 0x00, FALSE, 0);
	mddi_host_reg_out(CMD, MDDI_CMD_POWERDOWN);
	mddi_wait(120);
	mddi_queue_register_write(0x3600, 0xD0, FALSE, 0);
	mddi_queue_register_write(0x3500, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x5100, 0x00, FALSE, 0);
	mddi_queue_register_write(0x2480, 0x0069, FALSE, 0);
	mddi_queue_register_write(0x2580, 0x006C, FALSE, 0);
	mddi_queue_register_write(0x2680, 0x0074, FALSE, 0);
	mddi_queue_register_write(0x2780, 0x007C, FALSE, 0);
	mddi_queue_register_write(0x2880, 0x0016, FALSE, 0);
	mddi_queue_register_write(0x2980, 0x0029, FALSE, 0);
	mddi_queue_register_write(0x2A80, 0x005A, FALSE, 0);
	mddi_queue_register_write(0x2B80, 0x0072, FALSE, 0);
	mddi_queue_register_write(0x2D80, 0x001D, FALSE, 0);
	mddi_queue_register_write(0x2F80, 0x0024, FALSE, 0);
	mddi_queue_register_write(0x3080, 0x00B6, FALSE, 0);
	mddi_queue_register_write(0x3180, 0x001A, FALSE, 0);
	mddi_queue_register_write(0x3280, 0x0044, FALSE, 0);
	mddi_queue_register_write(0x3380, 0x005B, FALSE, 0);
	mddi_queue_register_write(0x3480, 0x00C9, FALSE, 0);
	mddi_queue_register_write(0x3580, 0x00F1, FALSE, 0);
	mddi_queue_register_write(0x3680, 0x00FD, FALSE, 0);
	mddi_queue_register_write(0x3780, 0x007F, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x3880, 0x0069, FALSE, 0);
	mddi_queue_register_write(0x3980, 0x006C, FALSE, 0);
	mddi_queue_register_write(0x3A80, 0x0074, FALSE, 0);
	mddi_queue_register_write(0x3B80, 0x007C, FALSE, 0);
	mddi_queue_register_write(0x3D80, 0x0016, FALSE, 0);
	mddi_queue_register_write(0x3F80, 0x0029, FALSE, 0);
	mddi_queue_register_write(0x4080, 0x005A, FALSE, 0);
	mddi_queue_register_write(0x4180, 0x0072, FALSE, 0);
	mddi_queue_register_write(0x4280, 0x001D, FALSE, 0);
	mddi_queue_register_write(0x4380, 0x0024, FALSE, 0);
	mddi_queue_register_write(0x4480, 0x00B6, FALSE, 0);
	mddi_queue_register_write(0x4580, 0x001A, FALSE, 0);
	mddi_queue_register_write(0x4680, 0x0044, FALSE, 0);
	mddi_queue_register_write(0x4780, 0x005B, FALSE, 0);
	mddi_queue_register_write(0x4880, 0x00C9, FALSE, 0);
	mddi_queue_register_write(0x4980, 0x00F1, FALSE, 0);
	mddi_queue_register_write(0x4A80, 0x00FD, FALSE, 0);
	mddi_queue_register_write(0x4B80, 0x007F, FALSE, 0);
	mddi_queue_register_write(0x4C80, 0x004F, FALSE, 0);
	mddi_queue_register_write(0x4D80, 0x0053, FALSE, 0);
	mddi_queue_register_write(0x4E80, 0x0060, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x4F80, 0x006A, FALSE, 0);
	mddi_queue_register_write(0x5080, 0x0016, FALSE, 0);
	mddi_queue_register_write(0x5180, 0x0029, FALSE, 0);
	mddi_queue_register_write(0x5280, 0x005B, FALSE, 0);
	mddi_queue_register_write(0x5380, 0x006C, FALSE, 0);
	mddi_queue_register_write(0x5480, 0x001E, FALSE, 0);
	mddi_queue_register_write(0x5580, 0x0025, FALSE, 0);
	mddi_queue_register_write(0x5680, 0x00B3, FALSE, 0);
	mddi_queue_register_write(0x5780, 0x001C, FALSE, 0);
	mddi_queue_register_write(0x5880, 0x004A, FALSE, 0);
	mddi_queue_register_write(0x5980, 0x0061, FALSE, 0);
	mddi_queue_register_write(0x5A80, 0x00B6, FALSE, 0);
	mddi_queue_register_write(0x5B80, 0x00C6, FALSE, 0);
	mddi_queue_register_write(0x5C80, 0x00F6, FALSE, 0);
	mddi_queue_register_write(0x5D80, 0x007F, FALSE, 0);
	mddi_queue_register_write(0x5E80, 0x004F, FALSE, 0);
	mddi_queue_register_write(0x5F80, 0x0053, FALSE, 0);
	mddi_queue_register_write(0x6080, 0x0060, FALSE, 0);
	mddi_queue_register_write(0x6180, 0x006A, FALSE, 0);
	mddi_queue_register_write(0x6280, 0x0016, FALSE, 0);
	mddi_queue_register_write(0x6380, 0x0029, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x6480, 0x005B, FALSE, 0);
	mddi_queue_register_write(0x6580, 0x006C, FALSE, 0);
	mddi_queue_register_write(0x6680, 0x001E, FALSE, 0);
	mddi_queue_register_write(0x6780, 0x0025, FALSE, 0);
	mddi_queue_register_write(0x6880, 0x00B3, FALSE, 0);
	mddi_queue_register_write(0x6980, 0x001C, FALSE, 0);
	mddi_queue_register_write(0x6A80, 0x004A, FALSE, 0);
	mddi_queue_register_write(0x6B80, 0x0061, FALSE, 0);
	mddi_queue_register_write(0x6C80, 0x00B6, FALSE, 0);
	mddi_queue_register_write(0x6D80, 0x00C6, FALSE, 0);
	mddi_queue_register_write(0x6E80, 0x00F6, FALSE, 0);
	mddi_queue_register_write(0x6F80, 0x007F, FALSE, 0);
	mddi_queue_register_write(0x7080, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x7180, 0x000A, FALSE, 0);
	mddi_queue_register_write(0x7280, 0x0027, FALSE, 0);
	mddi_queue_register_write(0x7380, 0x003C, FALSE, 0);
	mddi_queue_register_write(0x7480, 0x001D, FALSE, 0);
	mddi_queue_register_write(0x7580, 0x0030, FALSE, 0);
	mddi_queue_register_write(0x7680, 0x0060, FALSE, 0);
	mddi_queue_register_write(0x7780, 0x0063, FALSE, 0);
	mddi_queue_register_write(0x7880, 0x0020, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x7980, 0x0026, FALSE, 0);
	mddi_queue_register_write(0x7A80, 0x00B2, FALSE, 0);
	mddi_queue_register_write(0x7B80, 0x001C, FALSE, 0);
	mddi_queue_register_write(0x7C80, 0x0049, FALSE, 0);
	mddi_queue_register_write(0x7D80, 0x0060, FALSE, 0);
	mddi_queue_register_write(0x7E80, 0x00B9, FALSE, 0);
	mddi_queue_register_write(0x7F80, 0x00D1, FALSE, 0);
	mddi_queue_register_write(0x8080, 0x00FB, FALSE, 0);
	mddi_queue_register_write(0x8180, 0x007F, FALSE, 0);
	mddi_queue_register_write(0x8280, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x8380, 0x000A, FALSE, 0);
	mddi_queue_register_write(0x8480, 0x0027, FALSE, 0);
	mddi_queue_register_write(0x8580, 0x003C, FALSE, 0);
	mddi_queue_register_write(0x8680, 0x001D, FALSE, 0);
	mddi_queue_register_write(0x8780, 0x0030, FALSE, 0);
	mddi_queue_register_write(0x8880, 0x0060, FALSE, 0);
	mddi_queue_register_write(0x8980, 0x0063, FALSE, 0);
	mddi_queue_register_write(0x8A80, 0x0020, FALSE, 0);
	mddi_queue_register_write(0x8B80, 0x0026, FALSE, 0);
	mddi_queue_register_write(0x8C80, 0x00B2, FALSE, 0);
	mddi_queue_register_write(0x8D80, 0x001C, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x8E80, 0x0049, FALSE, 0);
	mddi_queue_register_write(0x8F80, 0x0060, FALSE, 0);
	mddi_queue_register_write(0x9080, 0x00B9, FALSE, 0);
	mddi_queue_register_write(0x9180, 0x00D1, FALSE, 0);
	mddi_queue_register_write(0x9280, 0x00FB, FALSE, 0);
	mddi_queue_register_write(0x9380, 0x007F, FALSE, 0);
	mddi_queue_register_write(0x2900, 0x0000, FALSE, 0);
	mddi_queue_register_write(0x22c0, 0x0006, FALSE, 0);
	mddi_queue_register_write(0x4400, 0x0002, FALSE, 0);
	mddi_queue_register_write(0x4401, 0x0058, FALSE, 0);
	mddi_queue_register_write(0x0480, 0x0063, FALSE, 0);
};

static int mddi_glacier_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mddi_host_client_cnt_reset();

	if (panel_type == PANEL_SHARP) {
		mddi_sharp_lcd_init();
		mddi_queue_register_write(0x3600, 0x00, TRUE, 0);
	} else
		mddi_sony_lcd_init();

	glacier_set_dim = 1;

	mddi_queue_register_write(0x5300, 0x24, TRUE, 0);

	return 0;
}

static int mddi_glacier_lcd_off(struct platform_device *pdev)
{
	mddi_queue_register_write(0x5300, 0x24, TRUE, 0);
	mddi_wait(4);
	mddi_queue_register_write(0x2800, 0, TRUE, 0);
	mddi_wait(2);
	mddi_queue_register_write(0x1000, 0, TRUE, 0);
	mddi_wait(2);
	return 0;
}

static int __devinit mddi_glacier_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver mddi_client_driver = {
	.probe = mddi_glacier_probe,
	.driver = {
		.name = "mddi_nt21856_wvga"
	},
};

static struct msm_fb_panel_data glacier_panel_data = {
	.on		= mddi_glacier_lcd_on,
	.off		= mddi_glacier_lcd_off,
	.set_backlight	= mddi_glacier_set_backlight,
	.display_on	= mddi_glacier_display_on,
	.bklswitch	= mddi_glacier_bkl_switch,
};

static struct platform_device glacier_mddi_device = {
	.name	= "mddi_nt21856_wvga",
	.id	= 1,
	.dev	= {
		.platform_data = &glacier_panel_data,
	}
};

static int __devinit mddi_client_glacier_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = platform_driver_register(&mddi_client_driver);
	if (!ret) {
		pinfo = &glacier_panel_data.panel_info;
		pinfo->xres = 480;
		pinfo->yres = 800;
		pinfo->type = MDDI_PANEL;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		pinfo->pdest = DISPLAY_1;
		pinfo->bpp = 16;
		pinfo->lcd.vsync_enable = TRUE,
		pinfo->lcd.refx100 = 6800; // 6820
		pinfo->lcd.v_back_porch = 0;
		pinfo->lcd.v_front_porch = 0;
		pinfo->lcd.v_pulse_width = 0;
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = (1 * HZ);
		if(panel_type == PANEL_SHARP) {
			pinfo->bl_max = PWM_SHARP_MAX;
			pinfo->bl_min = PWM_SHARP_MIN;
		} else {
			pinfo->bl_max = PWM_SONY_MAX;
			pinfo->bl_min = PWM_SONY_MIN;
		}
		pinfo->mddi.is_type1 = FALSE;
		pinfo->clk_rate = 192000000;
		pinfo->clk_min =  192000000;
		pinfo->clk_max =  192000000;
		pinfo->fb_num = 2;
		pinfo->lcd.rev = 2;

		ret = platform_device_register(&glacier_mddi_device);
		if (ret)
			pr_err("%s: device registration failed\n", __func__);
	}
	return ret;
}

module_init(mddi_client_glacier_init);

