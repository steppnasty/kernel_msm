/*
 * Copyright (C) 2016, Brian Stepp <steppnasty@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "msm_sensor.h"
#include "msm_camera_io_util.h"

#define SENSOR_NAME "mt9v113"
DEFINE_MSM_MUTEX(mt9v113_mut);

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_sensor_ctrl_t mt9v113_s_ctrl;
static int mt9v113_probe_done = 0;

static struct msm_sensor_power_setting mt9v113_power_setting[] = {

	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
};

static struct msm_camera_i2c_reg_conf mt9v113_probe_settings_0[] = {
	{0x001A, 0x0210},
	{0x001E, 0x0777},
	{0x0016, 0x42DF},
	{0x0014, 0xB04B},
	{0x0014, 0xB049},
	{0x0010, 0x021C},
	{0x0012, 0x0000},
	{0x0014, 0x244B},
};

static struct msm_camera_i2c_reg_conf mt9v113_probe_settings_1[] = {
	{0x0014, 0xB04A},
	{0x098C, 0xA11D},
	{0x0990, 0x0002},
	{0x098C, 0xA244},
	{0x0990, 0x00BF},
	/* AE */
	{0x098C, 0xA24F},
	{0x0990, 0x0033},
	{0x098C, 0xA207},
	{0x0990, 0x0002},
	/* Char_settings */
	{0x098C, 0xAB1F},
	{0x0990, 0x00C9},
	{0x326C, 0x0900},
	{0x001E, 0x0400},
	/* Sharpness */
	{0x098C, 0xAB22},
	{0x0990, 0x0003},
	/* Flicker Setting */
	{0x098C, 0xA404},
	{0x0990, 0x0010},
	{0x098C, 0x222D},
	{0x0990, 0x008B},
	{0x098C, 0xA408},
	{0x0990, 0x0021},
	{0x098C, 0xA409},
	{0x0990, 0x0024},
	{0x098C, 0xA40A},
	{0x0990, 0x0028},
	{0x098C, 0xA40B},
	{0x0990, 0x002B},
	{0x098C, 0x2411},
	{0x0990, 0x008B},
	{0x098C, 0x2413},
	{0x0990, 0x00A6},
	{0x098C, 0x2415},
	{0x0990, 0x008B},
	{0x098C, 0x2417},
	{0x0990, 0x00A6},
	{0x098C, 0xA40D},
	{0x0990, 0x0002},
	{0x098C, 0xA40E},
	{0x0990, 0x0003},
	{0x098C, 0xA410},
	{0x0990, 0x000A},
	/* Micron lens Correction *//*Mu Lee update for new lens 0126*/
	{0x364E, 0x0330},
	{0x3650, 0x010B},
	{0x3652, 0x2312},
	{0x3654, 0xC2AF},
	{0x3656, 0x9F50},
	{0x3658, 0x0290},
	{0x365A, 0x0FEB},
	{0x365C, 0x4E52},
	{0x365E, 0xC0CF},
	{0x3660, 0xCB12},
	{0x3662, 0x02B0},
	{0x3664, 0x620C},
	{0x3666, 0x1AD2},
	{0x3668, 0xA7B0},
	{0x366A, 0xBB91},
	{0x366C, 0x0290},
	{0x366E, 0xCE2A},
	{0x3670, 0x2AD2},
	{0x3672, 0x8BAE},
	{0x3674, 0xABAF},
	{0x3676, 0xCB8D},
	{0x3678, 0xA24E},
	{0x367A, 0x2F91},
	{0x367C, 0x0991},
	{0x367E, 0xC594},
	{0x3680, 0xAC2B},
	{0x3682, 0xA4AC},
	{0x3684, 0x6891},
	{0x3686, 0xAD30},
	{0x3688, 0x9295},
	{0x368A, 0x380B},
	{0x368C, 0x464C},
	{0x368E, 0x4C2C},
	{0x3690, 0xE9AF},
	{0x3692, 0xC312},
	{0x3694, 0xA50D},
	{0x3696, 0xF6AD},
	{0x3698, 0x2E11},
	{0x369A, 0x11F0},
	{0x369C, 0xB534},
	{0x369E, 0x0573},
	{0x36A0, 0xA431},
	{0x36A2, 0x81B6},
	{0x36A4, 0x0895},
	{0x36A6, 0x5D19},
	{0x36A8, 0x17F3},
	{0x36AA, 0xF1F1},
	{0x36AC, 0x80D6},
	{0x36AE, 0x42B4},
	{0x36B0, 0x3499},
	{0x36B2, 0x55F2},
	{0x36B4, 0x9492},
	{0x36B6, 0x9456},
	{0x36B8, 0x22B5},
	{0x36BA, 0x4AB9},
	{0x36BC, 0x0093},
	{0x36BE, 0xA391},
	{0x36C0, 0x85B6},
	{0x36C2, 0x4C34},
	{0x36C4, 0x4BF9},
	{0x36C6, 0xBD0F},
	{0x36C8, 0x1DD1},
	{0x36CA, 0xEE54},
	{0x36CC, 0xAAB5},
	{0x36CE, 0x0CD7},
	{0x36D0, 0xA770},
	{0x36D2, 0x9C11},
	{0x36D4, 0xA635},
	{0x36D6, 0x1576},
	{0x36D8, 0x0058},
	{0x36DA, 0xC4F1},
	{0x36DC, 0xD3F1},
	{0x36DE, 0x5134},
	{0x36E0, 0x2696},
	{0x36E2, 0x8F19},
	{0x36E4, 0xD98F},
	{0x36E6, 0xA911},
	{0x36E8, 0xD1F4},
	{0x36EA, 0x7054},
	{0x36EC, 0x76D6},
	{0x36EE, 0x93D5},
	{0x36F0, 0x0934},
	{0x36F2, 0x63B9},
	{0x36F4, 0xC178},
	{0x36F6, 0xEA7C},
	{0x36F8, 0xA7D5},
	{0x36FA, 0x0A55},
	{0x36FC, 0x3979},
	{0x36FE, 0x8597},
	{0x3700, 0xA03C},
	{0x3702, 0xE194},
	{0x3704, 0x74F4},
	{0x3706, 0x7A19},
	{0x3708, 0xC5B6},
	{0x370A, 0xD9BC},
	{0x370C, 0x8F55},
	{0x370E, 0x6FF4},
	{0x3710, 0x01DA},
	{0x3712, 0xE317},
	{0x3714, 0xE93C},
	{0x3644, 0x0144},
	{0x3642, 0x00F0},
};

static struct msm_camera_i2c_reg_conf mt9v113_probe_settings_2[] = {
	{0x098C, 0xA354},
	{0x0990, 0x0048},
	{0x098C, 0xAB20},
	{0x0990, 0x0048},
	/* Micron AWB and CCMs */
	{0x098C, 0xA11F},
	{0x0990, 0x0001},
	{0x098C, 0x2306},
	{0x0990, 0x03C0},
	{0x098C, 0x2308},
	{0x0990, 0xFD7C},
	{0x098C, 0x230A},
	{0x0990, 0xFFF7},
	{0x098C, 0x230C},
	{0x0990, 0xFF25},
	{0x098C, 0x230E},
	{0x0990, 0x0384},
	{0x098C, 0x2310},
	{0x0990, 0xFFD6},
	{0x098C, 0x2312},
	{0x0990, 0xFED2},
	{0x098C, 0x2314},
	{0x0990, 0xFCB2},
	{0x098C, 0x2316},
	{0x0990, 0x068E},
	{0x098C, 0x2318},
	{0x0990, 0x001B},
	{0x098C, 0x231A},
	{0x0990, 0x0036},
	{0x098C, 0x231C},
	{0x0990, 0xFF65},
	{0x098C, 0x231E},
	{0x0990, 0x0052},
	{0x098C, 0x2320},
	{0x0990, 0x0012},
	{0x098C, 0x2322},
	{0x0990, 0x0007},
	{0x098C, 0x2324},
	{0x0990, 0xFFCF},
	{0x098C, 0x2326},
	{0x0990, 0x0037},
	{0x098C, 0x2328},
	{0x0990, 0x00D8},
	{0x098C, 0x232A},
	{0x0990, 0x01C8},
	{0x098C, 0x232C},
	{0x0990, 0xFC9F},
	{0x098C, 0x232E},
	{0x0990, 0x0010},
	{0x098C, 0x2330},
	{0x0990, 0xFFF7},
	{0x098C, 0xA348},
	{0x0990, 0x0008},
	{0x098C, 0xA349},
	{0x0990, 0x0002},
	{0x098C, 0xA34A},
	{0x0990, 0x0059},
	{0x098C, 0xA34B},
	{0x0990, 0x00E6},
	{0x098C, 0xA351},
	{0x0990, 0x0000},
	{0x098C, 0xA352},
	{0x0990, 0x007F},
	{0x098C, 0xA355},
	{0x0990, 0x0001},
	{0x098C, 0xA35D},
	{0x0990, 0x0078},
	{0x098C, 0xA35E},
	{0x0990, 0x0086},
	{0x098C, 0xA35F},
	{0x0990, 0x007E},
	{0x098C, 0xA360},
	{0x0990, 0x0082},
	{0x098C, 0x2361},
	{0x0990, 0x0040},
	{0x098C, 0xA363},
	{0x0990, 0x00D2},
	{0x098C, 0xA364},
	{0x0990, 0x00F6},
	{0x098C, 0xA302},
	{0x0990, 0x0000},
	{0x098C, 0xA303},
	{0x0990, 0x00EF},
	{0x098C, 0xA366},
	{0x0990, 0x00C0},
	{0x098C, 0xA367},
	{0x0990, 0x0073},
	{0x098C, 0xA368},
	{0x0990, 0x0038},
	/* Gamma Morph brightness setting */
	{0x098C, 0x2B1B},
	{0x0990, 0x0000},
	{0x098C, 0x2B28},
	{0x0990, 0x157C},
	{0x098C, 0x2B2A},
	{0x0990, 0x1B58},
	{0x098C, 0xAB37},
	{0x0990, 0x0003},
	{0x098C, 0x2B38},
	{0x0990, 0x157C},
	{0x098C, 0x2B3A},
	{0x0990, 0x1B58},
	/* Gamma Table Context A (normal light) */
	{0x098C, 0xAB3C},
	{0x0990, 0x0000},
	{0x098C, 0xAB3D},
	{0x0990, 0x0014},
	{0x098C, 0xAB3E},
	{0x0990, 0x0027},
	{0x098C, 0xAB3F},
	{0x0990, 0x0041},
	{0x098C, 0xAB40},
	{0x0990, 0x0074},
	{0x098C, 0xAB41},
	{0x0990, 0x0093},
	{0x098C, 0xAB42},
	{0x0990, 0x00AD},
	{0x098C, 0xAB43},
	{0x0990, 0x00C1},
	{0x098C, 0xAB44},
	{0x0990, 0x00CA},
	{0x098C, 0xAB45},
	{0x0990, 0x00D4},
	{0x098C, 0xAB46},
	{0x0990, 0x00DC},
	{0x098C, 0xAB47},
	{0x0990, 0x00E4},
	{0x098C, 0xAB48},
	{0x0990, 0x00E9},
	{0x098C, 0xAB49},
	{0x0990, 0x00EE},
	{0x098C, 0xAB4A},
	{0x0990, 0x00F2},
	{0x098C, 0xAB4B},
	{0x0990, 0x00F5},
	{0x098C, 0xAB4C},
	{0x0990, 0x00F8},
	{0x098C, 0xAB4D},
	{0x0990, 0x00FD},
	{0x098C, 0xAB4E},
	{0x0990, 0x00FF},
	/* Gamma Table Context B (dark scene) */
	{0x098C, 0xAB4F},
	{0x0990, 0x0000},
	{0x098C, 0xAB50},
	{0x0990, 0x000F},
	{0x098C, 0xAB51},
	{0x0990, 0x001A},
	{0x098C, 0xAB52},
	{0x0990, 0x002E},
	{0x098C, 0xAB53},
	{0x0990, 0x0050},
	{0x098C, 0xAB54},
	{0x0990, 0x006A},
	{0x098C, 0xAB55},
	{0x0990, 0x0080},
	{0x098C, 0xAB56},
	{0x0990, 0x0091},
	{0x098C, 0xAB57},
	{0x0990, 0x00A1},
	{0x098C, 0xAB58},
	{0x0990, 0x00AF},
	{0x098C, 0xAB59},
	{0x0990, 0x00BB},
	{0x098C, 0xAB5A},
	{0x0990, 0x00C6},
	{0x098C, 0xAB5B},
	{0x0990, 0x00D0},
	{0x098C, 0xAB5C},
	{0x0990, 0x00D9},
	{0x098C, 0xAB5D},
	{0x0990, 0x00E2},
	{0x098C, 0xAB5E},
	{0x0990, 0x00EA},
	{0x098C, 0xAB5F},
	{0x0990, 0x00F1},
	{0x098C, 0xAB60},
	{0x0990, 0x00F9},
	{0x098C, 0xAB61},
	{0x0990, 0x00FF},
	/* Mode-set up Preview (VGA) /Capture Mode (VGA) */
	{0x098C, 0x2703},
	{0x0990, 0x0280},
	{0x098C, 0x2705},
	{0x0990, 0x01E0},
	{0x098C, 0x270D},
	{0x0990, 0x0004},
	{0x098C, 0x270F},
	{0x0990, 0x0004},
	{0x098C, 0x2711},
	{0x0990, 0x01EB},
	{0x098C, 0x2713},
	{0x0990, 0x028B},
	{0x098C, 0x2715},
	{0x0990, 0x0001},
	{0x098C, 0x2717},
	{0x0990, 0x0027}, /* 0x0027: mirror/flip , 0x0024: none*/
	{0x098C, 0x2719},
	{0x0990, 0x001A},
	{0x098C, 0x271B},
	{0x0990, 0x006B},
	{0x098C, 0x271D},
	{0x0990, 0x006B},
	{0x098C, 0x271F},
	{0x0990, 0x022A},
	{0x098C, 0x2721},
	{0x0990, 0x034A},
	{0x098C, 0x2739},
	{0x0990, 0x0000},
	{0x098C, 0x273B},
	{0x0990, 0x027F},
	{0x098C, 0x273D},
	{0x0990, 0x0000},
	{0x098C, 0x273F},
	{0x0990, 0x01DF},
	{0x098C, 0x2707},
	{0x0990, 0x0280},
	{0x098C, 0x2709},
	{0x0990, 0x01E0},
	{0x098C, 0x2723},
	{0x0990, 0x0004},
	{0x098C, 0x2725},
	{0x0990, 0x0004},
	{0x098C, 0x2727},
	{0x0990, 0x01EB},
	{0x098C, 0x2729},
	{0x0990, 0x028B},
	{0x098C, 0x272B},
	{0x0990, 0x0001},
	{0x098C, 0x272D},
	{0x0990, 0x0027}, /* 0x0027: mirror/flip , 0x0024: none*/
	{0x098C, 0x272F},
	{0x0990, 0x001A},
	{0x098C, 0x2731},
	{0x0990, 0x006B},
	{0x098C, 0x2733},
	{0x0990, 0x006B},
	{0x098C, 0x2735},
	{0x0990, 0x022A},
	{0x098C, 0x2737},
	{0x0990, 0x034A},
	{0x098C, 0x2747},
	{0x0990, 0x0000},
	{0x098C, 0x2749},
	{0x0990, 0x027F},
	{0x098C, 0x274B},
	{0x0990, 0x0000},
	{0x098C, 0x274D},
	{0x0990, 0x01DF},
	{0x098C, 0x2755},
	{0x0990, 0x0002},
	{0x098C, 0x2757},
	{0x0990, 0x0002},
	{0x098C, 0xA20C},
	{0x0990, 0x0008}, /*Mu add for min frame rate =15fps 0126*/
	{0x098C, 0xA103},
	{0x0990, 0x0006},
};

static struct v4l2_subdev_info mt9v113_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
};

static int32_t mt9v113_write_bit(struct msm_camera_i2c_client *client,
	unsigned short raddr, unsigned short bit, unsigned short state)
{
	int32_t rc;
	unsigned short check_value;
	unsigned short check_bit;

	if (state)
		check_bit = 0x0001 << bit;
	else
		check_bit = 0xFFFF & (~(0x0001 << bit));
	rc = client->i2c_func_tbl->i2c_read(client, raddr,
		&check_value, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0)
		return rc;
	if (state)
		check_value |= check_bit;
	else
		check_value &= check_bit;
	return client->i2c_func_tbl->i2c_write(client, raddr, check_value,
		MSM_CAMERA_I2C_WORD_DATA);
}

static int32_t mt9v113_check_bit(struct msm_camera_i2c_client *client,
	unsigned short raddr, unsigned short bit, int check_state)
{
	int i;
	unsigned short check_value;
	unsigned short check_bit;

	check_bit = 0x1 << bit;
	for (i = 0; i < 100; i++) {
		client->i2c_func_tbl->i2c_read(client, raddr,
			&check_value, MSM_CAMERA_I2C_WORD_DATA);
		if (check_state) {
			if ((check_value & check_bit))
				break;
		} else {
			if (!(check_value & check_bit))
				break;
		}
		msleep(1);
	}
	if (i == 100) {
		pr_err("%s failed addr:0x%2x data check_bit:0x%2x",
			__func__, raddr, check_bit);
		return -1;
	}
	return 1;
}

static inline int mt9v113_suspend(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i, rc = 0;
	unsigned short value;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;
	enum msm_camera_i2c_reg_addr_type dt = MSM_CAMERA_I2C_WORD_ADDR;

	if (client->i2c_func_tbl->i2c_read(client, 0x0018, &value, dt) < 0)
		goto suspend_fail;
	value |= 0x0008;
	if (client->i2c_func_tbl->i2c_write(client, 0x0018, value, dt) < 0)
		goto suspend_fail;

	if (client->i2c_func_tbl->i2c_read(client, 0x0018, &value, dt) < 0)
		goto suspend_fail;
	value |= 0x0001;
	if (client->i2c_func_tbl->i2c_write(client, 0x0018, value, dt) < 0)
		goto suspend_fail;

	for (i = 0; i < 100; i++) {
		rc = client->i2c_func_tbl->i2c_read(client, 0x0018, &value, dt);
		if ((value & 0x4000))
			break;
		msleep(1);
	}
	if (i == 100)
		goto suspend_fail;
	msleep(2);
	return rc;
suspend_fail:
	pr_err("%s: suspend failed\n", __func__);
	return -EIO;
}

static int32_t mt9v113_resume(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i;
	int32_t rc = 0;
	unsigned short check_value;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;

	/* enter SW Active mode */
	if (client->i2c_func_tbl->i2c_read(client, 0x0016, &check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto resume_fail;
	check_value |= 0x0020;
	if (client->i2c_func_tbl->i2c_write(client, 0x0016, check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto resume_fail;

	if (client->i2c_func_tbl->i2c_read(client, 0x0018, &check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto resume_fail;
	check_value &= 0xFFFE;
	if (client->i2c_func_tbl->i2c_write(client, 0x0018, check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto resume_fail;

	for (i = 0; i < 100; i++) {
		client->i2c_func_tbl->i2c_read(client, 0x0018, &check_value,
			MSM_CAMERA_I2C_WORD_DATA);
		if (!(check_value & 0x4000))
			break;
		msleep(1);
	}
	if (i == 100)
		return -EIO;

	for (i = 0; i < 100; i++) {
		client->i2c_func_tbl->i2c_read(client, 0x31E0, &check_value,
			MSM_CAMERA_I2C_WORD_DATA);
		if (check_value == 0x0003)
			break;
		msleep(1);
	}
	if (i == 100)
		return -EIO;

	rc = client->i2c_func_tbl->i2c_write(client, 0x31E0, 0x0001,
		MSM_CAMERA_I2C_WORD_DATA);
	msleep(2);

	return rc;
resume_fail:
	return -EIO;
}

static int32_t mt9v113_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_SLAVE_READ_I2C: {
		struct msm_camera_i2c_read_config read_config;
		uint16_t local_data = 0;
		if (copy_from_user(&read_config,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_read_config))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s:CFG_SLAVE_READ_I2C:", __func__);
		CDBG("%s:slave_addr=0x%x reg_addr=0x%x, data_type=%d\n",
			__func__, read_config.slave_addr,
			read_config.reg_addr, read_config.data_type);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				read_config.reg_addr,
				&local_data, read_config.data_type);
		if (rc < 0) {
			pr_err("%s:%d: i2c_read failed\n", __func__, __LINE__);
			break;
		}
		if (copy_to_user((void __user *)read_config.data,
			(void *)&local_data, sizeof(uint16_t))) {
			pr_err("%s:%d copy failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		break;
	}
	case CFG_POWER_UP:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n", __func__,
					__LINE__, rc);
				break;
			}
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else
			rc = -EFAULT;
		break;
	case CFG_POWER_DOWN:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n", __func__,
					__LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			pr_err("%s:%d sensor state %d\n", __func__, __LINE__,
				s_ctrl->sensor_state);
		} else {
			rc = -EFAULT;
		}
		break;
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		struct msm_camera_i2c_reg_conf *reg_conf = NULL;
		struct msm_camera_i2c_reg_array *setting_ptr = NULL;
		struct msm_camera_i2c_reg_conf *reg_ptr = NULL;
		int i;

		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s:%d failed: invalid state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if (!conf_array.size) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		setting_ptr = reg_setting;
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}
		reg_conf = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_conf)), GFP_KERNEL);
		if (!reg_conf) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		reg_ptr = reg_conf;
		for (i = 0; i < conf_array.size; i++) {
			reg_ptr->reg_addr = setting_ptr->reg_addr;
			reg_ptr->reg_data = setting_ptr->reg_data;
			reg_ptr++;
			setting_ptr++;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, reg_conf, conf_array.size,
			conf_array.data_type);
		kfree(reg_setting);
		kfree(reg_conf);
		break;
	}
	default:
		mutex_unlock(s_ctrl->msm_sensor_mutex);
		return msm_sensor_config(s_ctrl, argp);
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static int32_t mt9v113_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i = 0;
	unsigned short check_value;
	struct msm_camera_sensor_info *sinfo = NULL;
	struct msm_camera_device_platform_data *camdev = NULL;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	enum msm_camera_i2c_reg_addr_type dt = MSM_CAMERA_I2C_WORD_ADDR;

	CDBG("%s:%d\n", __func__, __LINE__);
	sinfo = s_ctrl->pdev->dev.platform_data;
	if (!sinfo)
		return -ENODEV;

	camdev = sinfo->pdata;
	if (!camdev)
		return -ENODEV;

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	if (camdev->camera_gpio_on)
		camdev->camera_gpio_on();

	/*switch PCLK and MCLK to Front cam*/
	if (sinfo->camera_clk_switch) {
		CDBG("%s: switch clk\n", __func__);
		sinfo->camera_clk_switch();
		msleep(1);
	}

	if (!mt9v113_probe_done) {
		/*Config Reset*/
		if (!gpio_request(sinfo->sensor_reset, "mt9v113")) {
			gpio_direction_output(sinfo->sensor_reset, 1);
			msleep(2);
			gpio_direction_output(sinfo->sensor_reset, 0);
			msleep(1);
		} else {
			pr_err("%s: gpio request failed\n", __func__);
			goto power_on_i2c_error;
		}
		gpio_free(sinfo->sensor_reset);

		msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
			(struct clk **)&power_setting->data[0],
			s_ctrl->clk_info_size, 1);
		msleep(1);

		/* follow optical team Power Flow */
		if (!gpio_request(sinfo->sensor_reset, "mt9v113")) {
			gpio_direction_output(sinfo->sensor_reset, 1);
			msleep(1);
		} else {
			pr_err("%s: gpio request failed\n", __func__);
			goto power_on_i2c_error;
		}
		gpio_free(sinfo->sensor_reset);
		msleep(2);

		if (client->i2c_func_tbl->i2c_write(client, 0x001A, 0x0011, dt) < 0)
			goto power_on_i2c_error;
		msleep(1);
		if (client->i2c_func_tbl->i2c_write(client, 0x001A, 0x0000, dt) < 0)
			goto power_on_i2c_error;
		msleep(1);
		if (client->i2c_func_tbl->i2c_write(client, 0x301A, 0x1218, dt) < 0)
			goto power_on_i2c_error;
		msleep(10);
		if (client->i2c_func_tbl->i2c_write(client, 0x301A, 0x121C, dt) < 0)
			goto power_on_i2c_error;
		msleep(10);

		/* RESET and MISC Control */
		if (client->i2c_func_tbl->i2c_write(client, 0x0018,
			0x4028, dt) < 0)
			goto power_on_i2c_error;
		if (mt9v113_check_bit(client, 0x0018, 14, 0) < 0)
			goto power_on_i2c_error;
		if (client->i2c_func_tbl->i2c_write(client, 0x001A,
			0x0003, dt) < 0)
			goto power_on_i2c_error;
		mdelay(2);
		if (client->i2c_func_tbl->i2c_write(client, 0x001A,
			0x0000, dt) < 0)
			goto power_on_i2c_error;
		mdelay(2);
		if (client->i2c_func_tbl->i2c_write(client, 0x0018,
			0x4028, dt) < 0)
			goto power_on_i2c_error;
		if (mt9v113_check_bit(client, 0x0018, 14, 0) < 0)
			goto power_on_i2c_error;

		if (client->i2c_func_tbl->i2c_write_conf_tbl(client,
			&mt9v113_probe_settings_0[0],
			ARRAY_SIZE(mt9v113_probe_settings_0), dt) < 0)
			goto power_on_i2c_error;
		msleep(10);

		if (client->i2c_func_tbl->i2c_write(client, 0x0014,
			0x304B, dt) < 0)
			goto power_on_i2c_error;
		if (mt9v113_check_bit(client, 0x0014, 15, 1) < 0)
			goto power_on_i2c_error;

		if (client->i2c_func_tbl->i2c_write_conf_tbl(client,
			&mt9v113_probe_settings_1[0],
			ARRAY_SIZE(mt9v113_probe_settings_1), dt) < 0)
			goto power_on_i2c_error;

		if (mt9v113_write_bit(client, 0x3210, 3, 1) < 0)
			goto power_on_i2c_error;

		if (client->i2c_func_tbl->i2c_write_conf_tbl(client,
			&mt9v113_probe_settings_2[0],
			ARRAY_SIZE(mt9v113_probe_settings_2), dt) < 0)
			goto power_on_i2c_error;

		for (i = 0; i < 100; i++) {
			client->i2c_func_tbl->i2c_write(client,	0x098C,
				0xA103, dt);
			client->i2c_func_tbl->i2c_read(client, 0x0990,
				&check_value, dt);
			if (check_value == 0x0000)
				break;
			msleep(1);
		}
		if (i == 100)
			goto power_on_i2c_error;

		if (client->i2c_func_tbl->i2c_write(client, 0x098C,
			0xA103, dt) < 0)
			goto power_on_i2c_error;
		if (client->i2c_func_tbl->i2c_write(client, 0x0990,
			0x0005, dt) < 0)
			goto power_on_i2c_error;

		for (i = 0; i < 100; i++) {
			client->i2c_func_tbl->i2c_write(client,
				0x098C, 0xA103, dt);
			client->i2c_func_tbl->i2c_read(client,
				0x0990, &check_value, dt);
			if (check_value == 0x0000)
				break;
			msleep(1);
		}
		if (i == 100)
			goto power_on_i2c_error;

		if (client->i2c_func_tbl->i2c_write(client, 0x098C,
			0xA102, dt) < 0)
			goto power_on_i2c_error;
		if (client->i2c_func_tbl->i2c_write(client, 0x0990,
			0x000F, dt) < 0)
			goto power_on_i2c_error;

		if (mt9v113_suspend(s_ctrl) < 0)
			goto power_on_i2c_error;
		msleep(10);

		mt9v113_probe_done = 1;
	} else {
		msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
			(struct clk **)&power_setting->data[0],
			s_ctrl->clk_info_size, 1);
		msleep(3);

		if (mt9v113_resume(s_ctrl) < 0)
			goto power_on_i2c_error;
	}

	s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;

	return 0;
power_on_i2c_error:
	pr_err("%s: i2c error\n", __func__);
	return -EIO;
}

static int32_t mt9v113_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	unsigned short check_value;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;

	if (client->i2c_func_tbl->i2c_read(client, 0x0016, &check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto power_down_fail;
	check_value &= 0xFFDF;
	if (client->i2c_func_tbl->i2c_write(client, 0x0016, check_value,
		MSM_CAMERA_I2C_WORD_DATA) < 0)
		goto power_down_fail;
	s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	return rc;
power_down_fail:
	pr_err("%s: power down failure\n", __func__);
	return -EIO;
}

static int32_t mt9v113_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *sinfo = s_ctrl->pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	if (mt9v113_suspend(s_ctrl) < 0)
		goto power_off_i2c_error;

	mt9v113_sensor_power_down(s_ctrl);

	if (sinfo->camera_clk_switch != NULL && sinfo->cam_select_pin) {
		/*0709: optical ask : CLK switch to Main Cam after 2nd Cam release*/
		CDBG("%s: doing clk switch to Main CAM\n", __func__);
		rc = gpio_request(sinfo->cam_select_pin, "mt9v113");
		if (rc < 0)
			pr_err("[CAM]GPIO (%d) request fail\n", sinfo->cam_select_pin);
		else
			gpio_direction_output(sinfo->cam_select_pin, 0);
		gpio_free(sinfo->cam_select_pin);
	}

	msleep(1);

	camdev->camera_gpio_off();
	msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
		(struct clk **)&power_setting->data[0],
		s_ctrl->clk_info_size, 0);

	return rc;
power_off_i2c_error:
	pr_err("%s: i2c error\n", __func__);
	return -EIO;
}

static const struct i2c_device_id mt9v113_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mt9v113_s_ctrl},
	{ }
};

static int32_t mt9v113_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &mt9v113_s_ctrl);
}

static struct i2c_driver mt9v113_i2c_driver = {
	.id_table = mt9v113_i2c_id,
	.probe = mt9v113_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9v113_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct platform_driver mt9v113_platform_driver = {
	.driver = {
		.name = "msm_camera_mt9v113",
		.owner = THIS_MODULE,
	},
};

static int32_t mt9v113_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	mt9v113_s_ctrl.pdev = pdev;

	return rc;
}

static int __init mt9v113_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&mt9v113_platform_driver,
		mt9v113_platform_probe);
	if (rc < 0)
		return rc;
	return i2c_add_driver(&mt9v113_i2c_driver);
}

static void __exit mt9v113_exit_module(void)
{
	if (mt9v113_s_ctrl.pdev) {
		kfree(mt9v113_s_ctrl.clk_info);
		platform_driver_unregister(&mt9v113_platform_driver);
	}
	i2c_del_driver(&mt9v113_i2c_driver);
	return;
}

static struct msm_sensor_fn_t mt9v113_func_tbl = {
	.sensor_config = mt9v113_sensor_config,
	.sensor_power_up = mt9v113_sensor_power_up,
	.sensor_power_down = mt9v113_sensor_release,
};

static struct msm_sensor_ctrl_t mt9v113_s_ctrl = {
	.sensor_i2c_client = &mt9v113_sensor_i2c_client,
	.power_setting_array.power_setting = mt9v113_power_setting,
	.power_setting_array.size = ARRAY_SIZE(mt9v113_power_setting),
	.msm_sensor_mutex = &mt9v113_mut,
	.sensor_v4l2_subdev_info = mt9v113_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9v113_subdev_info),
	.func_tbl = &mt9v113_func_tbl,
};

module_init(mt9v113_init_module);
module_exit(mt9v113_exit_module);
MODULE_DESCRIPTION("Micron VGA CMOS sensor driver");
MODULE_LICENSE("GPL v2");
