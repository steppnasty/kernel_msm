/*
 * Copyright (C) 2015-2016, Brian Stepp <steppnasty@gmail.com>
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

#define SENSOR_NAME "s5k4e1gx"
DEFINE_MSM_MUTEX(s5k4e1gx_mut);

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_sensor_ctrl_t s5k4e1gx_s_ctrl;

static struct msm_sensor_power_setting s5k4e1gx_power_setting[] = {

	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
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

int32_t s5k4e1gx_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
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
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
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
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static int32_t s5k4e1gx_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	unsigned short reg_status;
	struct msm_camera_sensor_info *sinfo = NULL;
	struct msm_camera_device_platform_data *camdev = NULL;
	struct msm_camera_i2c_client *client = s_ctrl->sensor_i2c_client;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	CDBG("%s:%d\n", __func__, __LINE__);
	sinfo = s_ctrl->pdev->dev.platform_data;
	if (!sinfo)
		return -ENODEV;

	camdev = sinfo->pdata;
	if (!camdev)
		return -ENODEV;

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	rc = sinfo->camera_power_on();
	if (rc < 0) {
		pr_err("%s: ERROR %d\n", __func__, rc);
		return rc;
	}

	/*switch PCLK and MCLK to Main cam*/
	if (sinfo->camera_clk_switch) {
		CDBG("%s: switch clk\n", __func__);
		sinfo->camera_clk_switch();
		msleep(10);
	}
	if (camdev->camera_gpio_on)
		camdev->camera_gpio_on();
	msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
		(struct clk **)&power_setting->data[0],
		s_ctrl->clk_info_size, 1);
	rc = gpio_request(sinfo->sensor_reset, "s5k4e1gx");
	if (!rc) {
		gpio_direction_output(sinfo->sensor_reset, 0);
		mdelay(5);
		gpio_direction_output(sinfo->sensor_reset, 1);
	} else {
		pr_err("%s: gpio request failed\n", __func__);
		return rc;
	}
	gpio_free(sinfo->sensor_reset);

	/* Reset sensor */
	if (client->i2c_func_tbl->i2c_write(client, 0x0103, 0x01,
		MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	rc = client->i2c_func_tbl->i2c_write_conf_tbl(client,
		&s5k4e1gx_probe_settings[0], ARRAY_SIZE(s5k4e1gx_probe_settings),
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
		goto power_on_i2c_error;
	if (client->i2c_func_tbl->i2c_read(client, 0x3110, &reg_status,
		MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	reg_status = (reg_status|0x01);

	if (client->i2c_func_tbl->i2c_write(client, 0x3110, reg_status,
		MSM_CAMERA_I2C_BYTE_DATA) < 0)
		goto power_on_i2c_error;
	client->i2c_func_tbl->i2c_write(client, 0x100, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;

	return rc;
power_on_i2c_error:
	pr_err("%s: i2c error\n", __func__);
	return -EIO;
}

static int32_t s5k4e1gx_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *sinfo = s_ctrl->pdev->dev.platform_data;

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0x100, 0x0, MSM_CAMERA_I2C_BYTE_DATA);
	mdelay(110);

	if (!sinfo->csi_if) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3110, 0x11, MSM_CAMERA_I2C_BYTE_DATA);
		mdelay(120);
	}
	rc = sinfo->camera_power_off();
	s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
	return rc;
}

static int s5k4e1gx_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_info *sinfo = s_ctrl->pdev->dev.platform_data;
	struct msm_camera_device_platform_data *camdev = sinfo->pdata;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	CDBG("%s:%d\n", __func__, __LINE__);
	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	/*SW stand by*/
	s5k4e1gx_sensor_power_down(s_ctrl);
	/*HW stand by*/
	if (sinfo) {
		gpio_request(sinfo->vcm_pwd, "s5k4e1gx");
		gpio_direction_output(sinfo->vcm_pwd, 0);
		gpio_free(sinfo->vcm_pwd);
	}

	if (sinfo->camera_clk_switch != NULL && sinfo->cam_select_pin) {
		/*0730: optical ask : CLK switch to Main Cam after 2nd Cam release*/
		CDBG("%s: doing clk switch to Main CAM\n", __func__);
		rc = gpio_request(sinfo->cam_select_pin, "s5k4e1gx");
		if (rc < 0)
			pr_err("[CAM]GPIO (%d) request fail\n", sinfo->cam_select_pin);
		else
			gpio_direction_output(sinfo->cam_select_pin, 0);
		gpio_free(sinfo->cam_select_pin);

		msleep(5);
		/* CLK switch set 0 */

		CDBG("%s msm_camio_probe_off()\n", __func__);

		camdev->camera_gpio_off();

		msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
			(struct clk **)&power_setting->data[0],
			s_ctrl->clk_info_size, 0);
	}			
	return rc;
}

static const struct i2c_device_id s5k4e1gx_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4e1gx_s_ctrl},
	{ }
};

static int32_t s5k4e1gx_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &s5k4e1gx_s_ctrl);
}

static struct i2c_driver s5k4e1gx_i2c_driver = {
	.id_table = s5k4e1gx_i2c_id,
	.probe  = s5k4e1gx_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e1gx_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct platform_driver s5k4e1gx_platform_driver = {
	.driver = {
		.name = "msm_camera_s5k4e1gx",
		.owner = THIS_MODULE,
	},
};

static int32_t s5k4e1gx_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	s5k4e1gx_s_ctrl.pdev = pdev;

	return rc;
}

static int __init s5k4e1gx_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&s5k4e1gx_platform_driver,
		s5k4e1gx_platform_probe);
	if (rc < 0)
		return rc;
	return i2c_add_driver(&s5k4e1gx_i2c_driver);
}

static void __exit s5k4e1gx_exit_module(void)
{
	
	if (s5k4e1gx_s_ctrl.pdev) {
		kfree(s5k4e1gx_s_ctrl.clk_info);
		platform_driver_unregister(&s5k4e1gx_platform_driver);
	}
	i2c_del_driver(&s5k4e1gx_i2c_driver);
	return;
}

static struct msm_sensor_fn_t s5k4e1gx_func_tbl = {
	.sensor_config = s5k4e1gx_sensor_config,
	.sensor_power_up = s5k4e1gx_sensor_power_up,
	.sensor_power_down = s5k4e1gx_sensor_release,
};

static struct msm_sensor_ctrl_t s5k4e1gx_s_ctrl = {
	.sensor_i2c_client = &s5k4e1gx_sensor_i2c_client,
	.power_setting_array.power_setting = s5k4e1gx_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k4e1gx_power_setting),
	.msm_sensor_mutex = &s5k4e1gx_mut,
	.sensor_v4l2_subdev_info = s5k4e1gx_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e1gx_subdev_info),
	.func_tbl = &s5k4e1gx_func_tbl,
};

module_init(s5k4e1gx_init_module);
module_exit(s5k4e1gx_exit_module);
MODULE_DESCRIPTION("Samsung 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


