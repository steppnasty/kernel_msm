/*
 * Copyright (c) 2017, Brian Stepp <steppnasty@gmail.com>
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

#define SENSOR_NAME "imx105"
DEFINE_MSM_MUTEX(imx105_mut);

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

//static int imx105_probe_done = 0;
static struct msm_sensor_ctrl_t imx105_s_ctrl;

static struct msm_sensor_power_setting imx105_power_setting[] = {
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
};

static struct v4l2_subdev_info imx105_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static int32_t imx105_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;

	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	mutex_lock(s_ctrl->msm_sensor_mutex);
	switch (cdata->cfgtype) {
	case CFG_POWER_UP:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_DOWN) {
			pr_err("%s:%d failed: invalid state %d\n",
				__func__, __LINE__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_up) {
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n",
					__func__, __LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_UP;
			pr_err("%s:%d sensor state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
		} else
			rc = -EFAULT;
		break;
	case CFG_POWER_DOWN:
		if (s_ctrl->sensor_state != MSM_SENSOR_POWER_UP) {
			pr_err("%s: failed: invalid state %d\n",
				__func__, s_ctrl->sensor_state);
			rc = -EFAULT;
			break;
		}
		if (s_ctrl->func_tbl->sensor_power_down) {
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
			if (rc < 0) {
				pr_err("%s:%d failed rc %ld\n",
					__func__, __LINE__, rc);
				break;
			}
			s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
			pr_err("%s:%d sensor state %d\n", __func__,
				__LINE__, s_ctrl->sensor_state);
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

static int imx105_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *sinfo = NULL;
	struct msm_camera_device_platform_data *camdev = NULL;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;
	int rc = 0;

	CDBG("%s:%d\n", __func__, __LINE__);
	sinfo = s_ctrl->pdev->dev.platform_data;
	if (!sinfo)
		return -ENODEV;

	camdev = sinfo->pdata;
	if (!camdev)
		return -ENODEV;

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	if (sinfo->camera_power_on &&
		sinfo->camera_power_on() < 0)
		goto power_on_i2c_error;

	if (sinfo->camera_clk_switch)
		sinfo->camera_clk_switch();

	if (camdev->camera_gpio_on)
		camdev->camera_gpio_on();

	msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
		(struct clk **)&power_setting->data[0],
		s_ctrl->clk_info_size, 1);
	mdelay(1);

	if (!gpio_request(sinfo->sensor_reset, "imx105"))
		gpio_direction_output(sinfo->sensor_reset, 1);
	else {
		pr_err("%s: gpio request failed\n", __func__);
		goto power_on_i2c_error;
	}
	gpio_free(sinfo->sensor_reset);
	mdelay(1);

	if (s_ctrl->func_tbl->sensor_match_id(s_ctrl) < 0) {
		if (!gpio_request(sinfo->sensor_reset, "imx105"))
			gpio_direction_output(sinfo->sensor_reset, 0);
		gpio_free(sinfo->sensor_reset);
		mdelay(1);
		if (camdev->camera_gpio_off)
			camdev->camera_gpio_off();
		mdelay(1);
		if (sinfo->camera_power_off)
			sinfo->camera_power_off();
		rc = -ENODEV;
	}

	return rc;
power_on_i2c_error:
	pr_err("%s: i2c error\n", __func__);
	return -EIO;
}

static void imx105_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	enum msm_camera_i2c_data_type dt = MSM_CAMERA_I2C_BYTE_DATA;

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x3400, 0x02, dt);
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, 0x0100, 0x00, dt);
	msleep(20);
	s_ctrl->sensor_state = MSM_SENSOR_POWER_DOWN;
}

static int imx105_sensor_release(struct msm_sensor_ctrl_t *s_ctrl)
{
	struct msm_camera_sensor_info *sinfo = NULL;
	struct msm_camera_device_platform_data *camdev = NULL;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	CDBG("%s:%d\n", __func__, __LINE__);

	sinfo = s_ctrl->pdev->dev.platform_data;
	if (!sinfo)
		return -EINVAL;
	camdev = sinfo->pdata;
	if (!camdev)
		return -EINVAL;

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	imx105_sensor_power_down(s_ctrl);

	if (!gpio_request(sinfo->sensor_reset, "imx105"))
		gpio_direction_output(sinfo->sensor_reset, 0);
	else
		pr_err("%s: gpio request failed\n", __func__);
	gpio_free(sinfo->sensor_reset);
	mdelay(1);
	camdev->camera_gpio_off();
	mdelay(1);
	if (sinfo->camera_power_off)
		sinfo->camera_power_off();

	return 0;
}

static const struct i2c_device_id imx105_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx105_s_ctrl},
	{ }
};

static int32_t imx105_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx105_s_ctrl);
}

static struct i2c_driver imx105_i2c_driver = {
	.id_table = imx105_i2c_id,
	.probe = imx105_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx105_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static struct platform_driver imx105_platform_driver = {
	.driver = {
		.name = "msm_camera_imx105",
		.owner = THIS_MODULE,
	},
};

static int32_t imx105_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	imx105_s_ctrl.pdev = pdev;

	return rc;
}

static int __init imx105_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&imx105_platform_driver,
		imx105_platform_probe);
	if (rc < 0)
		return rc;
	return i2c_add_driver(&imx105_i2c_driver);
}

static void __exit imx105_exit_module(void)
{
	if (imx105_s_ctrl.pdev) {
		kfree(imx105_s_ctrl.clk_info);
		platform_driver_unregister(&imx105_platform_driver);
	}
	i2c_del_driver(&imx105_i2c_driver);
	return;
}

static struct msm_sensor_fn_t imx105_func_tbl = {
	.sensor_config = imx105_sensor_config,
	.sensor_power_up = imx105_sensor_power_up,
	.sensor_power_down = imx105_sensor_release,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t imx105_s_ctrl = {
	.sensor_i2c_client = &imx105_sensor_i2c_client,
	.power_setting_array.power_setting = imx105_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx105_power_setting),
	.msm_sensor_mutex = &imx105_mut,
	.sensor_v4l2_subdev_info = imx105_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx105_subdev_info),
	.func_tbl = &imx105_func_tbl,
};

module_init(imx105_init_module);
module_exit(imx105_exit_module);
MODULE_DESCRIPTION("imx105");
MODULE_LICENSE("GPL v2");
