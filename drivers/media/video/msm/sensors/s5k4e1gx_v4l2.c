/*
 * Copyright (C) 2015 Brian Stepp <steppnasty@gmail.com>
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

#include <linux/clk.h>
#include "msm_sensor.h"
#include "msm.h"
#define SENSOR_NAME "s5k4e1gx"
#define MSB			1
#define LSB			0
#define S5K4E1GX_REG_MODEL_ID	0x0000
#define S5K4E1GX_MODEL_ID	0x4E10

DEFINE_MUTEX(s5k4e1gx_mut);

static struct clk *camio_cam_m_clk;
static struct msm_sensor_ctrl_t s5k4e1gx_s_ctrl;

static struct msm_camera_i2c_reg_conf s5k4e1gx_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_prev_settings[] = {
	{0x034C, 0x05},/* x_output size msb */
	{0x034D, 0x18},/* x_output size lsb */
	{0x034E, 0x03},/* y_output size msb */
	{0x034F, 0xD4},/* y_output size lsb */
	{0x0381, 0x01},/* x_even_inc */
	{0x0383, 0x01},/* x_odd_inc */
	{0x0385, 0x01},/* y_even_inc */
	{0x0387, 0x03},/* y_odd_inc 03(10b AVG) */
	{0x30A9, 0x02},/* Horizontal Binning On */
	{0x300E, 0xEB},/* Vertical Binning On */
	{0x0105, 0x01},/* mask corrupted frame */
	{0x0340, 0x03},/* Frame Length */
	{0x0341, 0xE0},
	{0x300F, 0x82},/* CDS Test */
	{0x3013, 0xC0},/* rst_offset1 */
	{0x3017, 0xA4},/* rmb_init */
	{0x301B, 0x88},/* comp_bias */
	{0x3110, 0x10},/* PCLK inv */
	{0x3117, 0x0C},/* PCLK delay */
	{0x3119, 0x0F},/* V H Sync Strength */
	{0x311A, 0xFA},/* Data PCLK Strength */
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_snap_settings[] = {
	/*Output Size (2608x1960)*/
	{0x30A9, 0x03},/* Horizontal Binning Off */
	{0x300E, 0xE8},/* Vertical Binning Off */
	{0x0387, 0x01},/* y_odd_inc */
	{0x034C, 0x0A},/* x_output size */
	{0x034D, 0x30},
	{0x034E, 0x07},/* y_output size */
	{0x034F, 0xA8},
	{0x30BF, 0xAB},/* outif_enable[7], data_type[5:0](2Bh = bayer 10bit} */
	{0x30C0, 0x80},/* video_offset[7:4] 3260%12 */
	{0x30C8, 0x0C},/* video_data_length 3260 = 2608 * 1.25 */
	{0x30C9, 0xBC},
	/*Timing configuration*/
	{0x0202, 0x06},
	{0x0203, 0x28},
	{0x0204, 0x00},
	{0x0205, 0x80},
	{0x0340, 0x07},/* Frame Length */
	{0x0341, 0xB4},
	{0x0342, 0x0A},/* 2738 Line Length */
	{0x0343, 0xB2},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_recommend_settings[] = {
	{0x302B, 0x00},
	{0x30BC, 0xA0},
	{0x30BE, 0x08},
	{0x0305, 0x06},
	{0x0306, 0x00},
	{0x0307, 0x50},
	{0x30B5, 0x00},
	{0x30F1, 0xB0},
	{0x0101, 0x00},
	{0x034C, 0x05},
	{0x034D, 0x18},
	{0x034E, 0x03},
	{0x034F, 0xD4},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x30A9, 0x02},
	{0x300E, 0xEB},
	{0x0340, 0x03},
	{0x0341, 0xE0},
	{0x0342, 0x0A},
	{0x0343, 0xB2},
	{0x3110, 0x10},
	{0x3117, 0x0C},
	{0x3119, 0x0F},
	{0x311A, 0xFA},
	{0x0104, 0x01},
	{0x0204, 0x00},
	{0x0205, 0x20},
	{0x0202, 0x03},
	{0x0203, 0x1F},
	{0x0104, 0x00},
	{0x3110, 0x10},
};

static struct msm_camera_i2c_reg_conf s5k4e1gx_probe_settings[] = {
        {0x3000, 0x05},
        {0x3001, 0x03},
        {0x3002, 0x08},
        {0x3003, 0x09},
        {0x3004, 0x2E},
        {0x3005, 0x06},
        {0x3006, 0x34},
        {0x3007, 0x00},
        {0x3008, 0x3C},
        {0x3009, 0x3C},
        {0x300A, 0x28},
        {0x300B, 0x04},
        {0x300C, 0x0A},
        {0x300D, 0x02},
        {0x300F, 0x82},
        {0x3010, 0x00},
        {0x3011, 0x4C},
        {0x3012, 0x30},
        {0x3013, 0xC0},
        {0x3014, 0x00},
        {0x3015, 0x00},
        {0x3016, 0x2C},
        {0x3017, 0x94},
        {0x3018, 0x78},
        {0x301B, 0x83},
        {0x301D, 0xD4},
        {0x3021, 0x02},
        {0x3022, 0x24},
        {0x3024, 0x40},
        {0x3027, 0x08},
        {0x3029, 0xC6},
        {0x302B, 0x00},
        {0x30BC, 0xA0},
        {0x301C, 0x04},
        {0x30D8, 0x3F},
        {0x3070, 0x5F},
        {0x3071, 0x00},
        {0x3080, 0x04},
        {0x3081, 0x38},
        {0x3084, 0x15},
        {0x30BE, 0x08},
        {0x30E2, 0x01},
};

static struct v4l2_subdev_info s5k4e1gx_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SGRBG10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k4e1gx_init_conf[] = {
	{&s5k4e1gx_recommend_settings[0],
	ARRAY_SIZE(s5k4e1gx_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k4e1gx_probe_conf[] = {
	{&s5k4e1gx_probe_settings[0],
	ARRAY_SIZE(s5k4e1gx_probe_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k4e1gx_confs[] = {
	{
		&s5k4e1gx_snap_settings[0],
		ARRAY_SIZE(s5k4e1gx_snap_settings),
		0,
		MSM_CAMERA_I2C_BYTE_DATA
	},
	{
		&s5k4e1gx_prev_settings[0],
		ARRAY_SIZE(s5k4e1gx_prev_settings),
		0,
		MSM_CAMERA_I2C_BYTE_DATA
	},
};

static struct msm_sensor_output_info_t s5k4e1gx_dimensions[] = {
	{
		.x_output = 0xA30,
		.y_output = 0x7A8,
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x3E0,
		.vt_pixel_clk = 816000000,
		.op_pixel_clk = 816000000,
		.binning_factor = 0,
	},
	{
		.x_output = 0x518,
		.y_output = 0x3D4,
		.line_length_pclk = 0xAB2,
		.frame_length_lines = 0x3E0,
		.vt_pixel_clk = 816000000,
		.op_pixel_clk = 816000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csi_params s5k4e1gx_csi_params = {
	.data_format = CSI_10BIT,
	.lane_cnt    = 1,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 24,
};

static struct msm_camera_csi_params *s5k4e1gx_csi_params_array[] = {
	&s5k4e1gx_csi_params,
	&s5k4e1gx_csi_params,
};

static struct msm_sensor_output_reg_addr_t s5k4e1gx_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t s5k4e1gx_id_info = {
	.sensor_id_reg_addr = S5K4E1GX_REG_MODEL_ID,
	.sensor_id = S5K4E1GX_MODEL_ID,
};

static struct msm_sensor_exp_gain_info_t s5k4e1gx_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 4,
};

static inline uint8_t s5k4e1gx_byte(uint16_t word, uint8_t offset)
{
	return word >> (offset * BITS_PER_BYTE);
}

static int32_t s5k4e1gx_write_prev_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t gain, uint32_t line)
{
	int32_t rc = 0;

	uint16_t max_legal_gain = 0x0200;
	uint32_t ll_ratio;
	uint32_t fl_lines = 992;
	uint32_t ll_pck = 2738;
	uint32_t offset;

	CDBG("s5k4e1gx_write_prev_exp_gain :%d %d\n", gain, line);
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	line = (line * 0x400);

	if (fl_lines < (line / 0x400))
		ll_ratio = (line / (fl_lines - offset));
	else
		ll_ratio = 0x400;

	/* Analogue Gain */
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr,
		s5k4e1gx_byte(gain, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
		s5k4e1gx_byte(gain, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);

	ll_pck = ll_pck * ll_ratio;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk,
		s5k4e1gx_byte((ll_pck / 0x400), MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk + 1,
		s5k4e1gx_byte((ll_pck / 0x400), LSB),
		MSM_CAMERA_I2C_BYTE_DATA);

	line = line / ll_ratio;
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		s5k4e1gx_byte(line, MSB),
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		s5k4e1gx_byte(line, LSB),
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return rc;
}

static int32_t s5k4e1gx_write_pict_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x0200;
	uint16_t min_ll_pck = 0x0AB2;
	uint32_t ll_pck, fl_lines;
	uint32_t ll_ratio;
	uint8_t gain_msb, gain_lsb;
	uint8_t intg_time_msb, intg_time_lsb;
	uint8_t ll_pck_msb, ll_pck_lsb;

	if (gain > max_legal_gain) {
		CDBG("Max legal gain Line:%d\n", __LINE__);
		gain = max_legal_gain;
	}

	gain = 32;
	line = 1465;
	CDBG("%s: gain = %d line = %d\n", __func__, gain, line);
	line = (uint32_t) (line * s_ctrl->fps_divider);
	fl_lines = s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider / Q10;
	ll_pck = s_ctrl->curr_line_length_pclk;

	if (fl_lines < (line / Q10))
		ll_ratio = (line / (fl_lines - 4));
	else
		ll_ratio = Q10;

	ll_pck = ll_pck * ll_ratio / Q10;
	line = line / ll_ratio;
	if (ll_pck < min_ll_pck)
		ll_pck = min_ll_pck;

	gain_msb = (uint8_t) ((gain & 0xFF00) >> 8);
	gain_lsb = (uint8_t) (gain & 0x00FF);

	intg_time_msb = (uint8_t) ((line & 0xFF00) >> 8);
	intg_time_lsb = (uint8_t) (line & 0x00FF);

	ll_pck_msb = (uint8_t) ((ll_pck & 0xFF00) >> 8);
	ll_pck_lsb = (uint8_t) (ll_pck & 0x00FF);

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr,
		gain_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr + 1,
		gain_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk,
		ll_pck_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->line_length_pclk + 1,
		ll_pck_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* Coarse Integration Time */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		intg_time_msb,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr + 1,
		intg_time_lsb,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return 0;
}

static int32_t s5k4e1gx_sensor_csi_setting(struct msm_sensor_ctrl_t *s_ctrl,
	int update_type, int res)
{
	int32_t rc = 0;
	static int csi_config;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);
	if (update_type == MSM_SENSOR_REG_INIT) {
		CDBG("Register INIT\n");
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_enable_debugfs(s_ctrl);
		msm_sensor_write_init_settings(s_ctrl);
		csi_config = 0;
	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		CDBG("PERIODIC : %d\n", res);
		msm_sensor_write_conf_array(
			s_ctrl->sensor_i2c_client,
			s_ctrl->msm_sensor_reg->mode_settings, res);
		msleep(30);
		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE,
			&s_ctrl->sensordata->pdata->ioclk.vfe_clk_rate);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(50);
	}
	return rc;
}

static int32_t s5k4e1gx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	unsigned short reg_status;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;
	struct msm_camera_device_platform_data *camdev = data->pdata;

	rc = data->camera_power_on();
	if (rc < 0) {
		pr_err("%s: ERROR %d\n", __func__, rc);
		return rc;
	}

	/*switch PCLK and MCLK to Main cam*/
	if (data && data->camera_clk_switch != NULL) {
		CDBG("%s: switch clk\n", __func__);
		data->camera_clk_switch();
		msleep(10);
	}
	if (camdev->camera_gpio_on)
		camdev->camera_gpio_on();
	camio_cam_m_clk = clk_get(NULL, "cam_clk");
	clk_set_rate(camio_cam_m_clk, 24000000);
	if (camio_cam_m_clk && !IS_ERR(camio_cam_m_clk))
		clk_enable(camio_cam_m_clk);
	else
		return -1;
	rc = gpio_request(data->sensor_reset, "s5k4e1gx");
	if (!rc) {
		gpio_direction_output(data->sensor_reset, 0);
		mdelay(5);
		gpio_direction_output(data->sensor_reset, 1);
	} else {
		pr_err("%s: gpio request failed\n", __func__);
		return rc;
	}
	gpio_free(data->sensor_reset);

	/* Reset sensor */
	if (msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0103,
			0x01, MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	if (msm_sensor_write_all_conf_array(s_ctrl->sensor_i2c_client,
			&s5k4e1gx_probe_conf[0],
			ARRAY_SIZE(s5k4e1gx_probe_conf)) < 0)
		goto power_on_i2c_error;
	if (msm_camera_i2c_read(s_ctrl->sensor_i2c_client, 0x3110,
			&reg_status, MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	reg_status = (reg_status|0x01);

	if (msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x3110,
			reg_status, MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);

	return rc;
power_on_i2c_error:
	pr_err("%s: i2c error\n", __func__);
	return -EIO;
}

static int32_t s5k4e1gx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *data = s_ctrl->sensordata;

	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	mdelay(110);

	if (!data->csi_if) {
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x3110, 0x11, MSM_CAMERA_I2C_BYTE_DATA);
		mdelay(120);
	}
	rc = data->camera_power_off();
	return rc;
}

static int s5k4e1gx_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *sdata = s_ctrl->sensordata;
	struct msm_camera_device_platform_data *camdev = sdata->pdata;

	/*SW stand by*/
	s5k4e1gx_sensor_power_down(s_ctrl);
	/*HW stand by*/
	if (sdata) {
		gpio_request(sdata->vcm_pwd, "s5k4e1gx");
		gpio_direction_output(sdata->vcm_pwd, 0);
		gpio_free(sdata->vcm_pwd);
	}

	if (sdata->camera_clk_switch != NULL && sdata->cam_select_pin) {
		/*0730: optical ask : CLK switch to Main Cam after 2nd Cam release*/
		CDBG("%s: doing clk switch to Main CAM\n", __func__);
		rc = gpio_request(sdata->cam_select_pin, "s5k4e1gx");
		if (rc < 0)
			pr_err("[CAM]GPIO (%d) request fail\n", sdata->cam_select_pin);
		else
			gpio_direction_output(sdata->cam_select_pin, 0);
		gpio_free(sdata->cam_select_pin);

		msleep(5);
		/* CLK switch set 0 */

		CDBG("%s msm_camio_probe_off()\n", __func__);

		camdev->camera_gpio_off();

		if (camio_cam_m_clk && !IS_ERR(camio_cam_m_clk)) {
			clk_disable(camio_cam_m_clk);
			clk_put(camio_cam_m_clk);
		}
	}			
	return rc;
}

static const struct i2c_device_id s5k4e1gx_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4e1gx_s_ctrl},
	{ }
};

static struct i2c_driver s5k4e1gx_i2c_driver = {
	.id_table = s5k4e1gx_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e1gx_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k4e1gx_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k4e1gx_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k4e1gx_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k4e1gx_subdev_ops = {
	.core = &s5k4e1gx_subdev_core_ops,
	.video  = &s5k4e1gx_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k4e1gx_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k4e1gx_write_prev_exp_gain,
	.sensor_write_snapshot_exp_gain = s5k4e1gx_write_pict_exp_gain,
	.sensor_csi_setting = s5k4e1gx_sensor_csi_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = s5k4e1gx_sensor_power_up,
	.sensor_power_down = s5k4e1gx_sensor_release,
};

static struct msm_sensor_reg_t s5k4e1gx_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k4e1gx_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k4e1gx_start_settings),
	.stop_stream_conf = s5k4e1gx_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k4e1gx_stop_settings),
	.group_hold_on_conf = s5k4e1gx_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k4e1gx_groupon_settings),
	.group_hold_off_conf = s5k4e1gx_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k4e1gx_groupoff_settings),
	.init_settings = &s5k4e1gx_init_conf[0],
	.init_size = ARRAY_SIZE(s5k4e1gx_init_conf),
	.mode_settings = &s5k4e1gx_confs[0],
	.output_settings = &s5k4e1gx_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k4e1gx_confs),
};

static struct msm_sensor_ctrl_t s5k4e1gx_s_ctrl = {
	.msm_sensor_reg = &s5k4e1gx_regs,
	.sensor_i2c_client = &s5k4e1gx_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &s5k4e1gx_reg_addr,
	.sensor_id_info = &s5k4e1gx_id_info,
	.sensor_exp_gain_info = &s5k4e1gx_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csic_params = &s5k4e1gx_csi_params_array[0],
	.msm_sensor_mutex = &s5k4e1gx_mut,
	.sensor_i2c_driver = &s5k4e1gx_i2c_driver,
	.sensor_v4l2_subdev_info = s5k4e1gx_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e1gx_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k4e1gx_subdev_ops,
	.func_tbl = &s5k4e1gx_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


