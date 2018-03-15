/*
 * Copyright (C) 2017, Brian Stepp <steppnasty@gmail.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/mfd/pmic8058.h>

#include <mach/camera2.h>
#include <mach/msm_iomap.h>
#include <mach/vreg.h>
#include <mach/msm_flashlight.h>

#include "devices.h"
#include "board-glacier.h"
#include "proc_comm.h"

static void glacier_config_gpio_table(uint32_t *table, int len)
{
	int n;
	unsigned id;
	for (n = 0; n < len; n++) {
		id = table[n];
		if (msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0))
			printk(KERN_ERR "%s: config gpio fail\n", __func__);
	}
}

static uint32_t glacier_camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	/* RST1 */
	GPIO_CFG(31, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* PWD1 */
	GPIO_CFG(34, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* RST2 */
	GPIO_CFG(21, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* PWD2 */
	GPIO_CFG(24, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT2 */
	GPIO_CFG(2,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT3 */
	GPIO_CFG(3,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT4 */
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT5 */
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT6 */
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT7 */
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT8 */
	GPIO_CFG(8,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT9 */
	GPIO_CFG(9,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT10 */
	GPIO_CFG(10, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* DAT11 */
	GPIO_CFG(11, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* PCLK */
	GPIO_CFG(12, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* HSYNC_IN */
	GPIO_CFG(13, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* VSYNC_IN */
	GPIO_CFG(14, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),
	/* MCLK */
	GPIO_CFG(15, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static uint32_t glacier_camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
	/* RST1 */
	GPIO_CFG(31, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* PWD1 */
	GPIO_CFG(34, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* RST2 */
	GPIO_CFG(21, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* PWD2 */
	GPIO_CFG(24, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT2 */
	GPIO_CFG(2,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT3 */
	GPIO_CFG(3,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT4 */
	GPIO_CFG(4,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT5 */
	GPIO_CFG(5,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT6 */
	GPIO_CFG(6,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT7 */
	GPIO_CFG(7,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT8 */
	GPIO_CFG(8,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT9 */
	GPIO_CFG(9,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT10 */
	GPIO_CFG(10, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* DAT11 */
	GPIO_CFG(11, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* PCLK */
	GPIO_CFG(12, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* HSYNC_IN */
	GPIO_CFG(13, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* VSYNC_IN */
	GPIO_CFG(14, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	/* MCLK */
	GPIO_CFG(15, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA),
};

static int glacier_sensor_power(char *power, unsigned volt)
{
	struct vreg *sensor_power;
	int rc;

	if (power == NULL)
		return -EIO;

	sensor_power = vreg_get(NULL, power);
	if (IS_ERR(sensor_power)) {
		pr_err("%s: vreg_get(%s) failed (%ld)\n",
			__func__, power, PTR_ERR(sensor_power));
		return -EIO;
	}

	if (volt) {
		if (vreg_set_level(sensor_power, volt))
			pr_err("[CAM]%s: unable to set %s voltage to %d\n",
				__func__, power, volt);
		rc = vreg_enable(sensor_power);
	} else
		rc = vreg_disable(sensor_power);
	if (rc)
		pr_err("%s: vreg %s %s failed (%d)\n", __func__,
			volt ? "enable" : "disable", power, rc);
	vreg_put(sensor_power);
	return rc;
}

static int glacier_sensor_vreg_on(void)
{
	int rc;

	struct pm8058_gpio camera_analog_pw_on = {
		.direction		= PM_GPIO_DIR_OUT,
		.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
		.output_value	= 1,
		.pull			= PM_GPIO_PULL_NO,
		.out_strength	= PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_NORMAL,
	};

	pr_info("%s camera vreg on\n", __func__);

	/*camera VCM power*/
	if (system_rev >= 1)
		rc = glacier_sensor_power("gp4", 2850);
	else
		rc = glacier_sensor_power("wlan", 2850);

	/*camera IO power*/
	rc = glacier_sensor_power("gp2", 1800);


	/*camera analog power*/
	pm8058_gpio_config(GLACIER_CAM_A2V85_EN, &camera_analog_pw_on);

	/*camera digital power*/
	if (system_rev >= 1)
		rc = glacier_sensor_power("wlan", 1800);
	else
		rc = glacier_sensor_power("gp4", 1800);

	udelay(200);

	return rc;
}

static int glacier_sensor_vreg_off(void)
{
	int rc;
	/*camera analog power*/
	struct pm8058_gpio camera_analog_pw_off = {
		.direction		= PM_GPIO_DIR_OUT,
		.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
		.output_value	= 0,
		.pull			= PM_GPIO_PULL_NO,
		.out_strength	= PM_GPIO_STRENGTH_LOW,
		.function = PM_GPIO_FUNC_NORMAL,
	};

	pm8058_gpio_config(GLACIER_CAM_A2V85_EN, &camera_analog_pw_off);
	/*camera digital power*/
	rc = glacier_sensor_power("gp4", 0);

	/*camera IO power*/
	rc = glacier_sensor_power("gp2", 0);

	/*camera VCM power*/
	rc = glacier_sensor_power("wlan", 0);
	return rc;
}

static void glacier_camera_on_gpios(void)
{
	glacier_config_gpio_table(glacier_camera_on_gpio_table,
		ARRAY_SIZE(glacier_camera_on_gpio_table));
}

static void glacier_camera_off_gpios(void)
{
	glacier_config_gpio_table(glacier_camera_off_gpio_table,
		ARRAY_SIZE(glacier_camera_off_gpio_table));
}

static struct resource msm_camera_resources[] = {
	{
		.start	= 0xA6000000,
		.end	= 0xA6000000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct msm_camera_sensor_platform_info s5k4e1gx_sensor_platform_info = {
	.mount_angle = 90
};

static void glacier_s5k4e1gx_clk_switch(void){
	int rc = 0;
	pr_info("doing clk switch (glacier)(s5k4e1gx)\n");
	rc = gpio_request(GLACIER_CLK_SWITCH, "s5k4e1gx");
	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", GLACIER_CLK_SWITCH);
	else
		gpio_direction_output(GLACIER_CLK_SWITCH, 0);
	gpio_free(GLACIER_CLK_SWITCH);

	return;
}

static struct msm_camera_device_platform_data glacier_s5k4e1gx_platform_data = {
	.camera_gpio_on = glacier_camera_on_gpios,
	.camera_gpio_off = glacier_camera_off_gpios,
	.csid_core = 0,
	.ioclk = {
		.vfe_clk_rate = 122880000,
	},
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k4e1gx_info = {
	.sensor_name    = "s5k4e1gx",
	.sensor_reset   = GLACIER_CAM_RST,
	.vcm_pwd     = GLACIER_CAM_PWD,
	.camera_clk_switch	= glacier_s5k4e1gx_clk_switch,
/*	.camera_analog_pwd = "gp8", */
	.camera_io_pwd = "gp2",
	.camera_vcm_pwd = "wlan",
	.camera_digital_pwd = "gp4",
	.analog_pwd1_gpio = GLACIER_CAM_A2V85_EN,
	.camera_power_on = glacier_sensor_vreg_on,
	.camera_power_off = glacier_sensor_vreg_off,
	.pdata          = &glacier_s5k4e1gx_platform_data,
	.sensor_lc_disable = true, /* disable sensor lens correction */
	.cam_select_pin = GLACIER_CLK_SWITCH,
	.sensor_platform_info = &s5k4e1gx_sensor_platform_info,
	.csi_if = 0,
};

static struct msm_sensor_info_t msm_camera_sensor_s5k4e1gx_sensor_info = {
	.sensor_name = "s5k4e1gx",
};

static struct msm_camera_slave_info msm_camera_sensor_s5k4e1gx_slave_info = {
	.sensor_slave_addr = 0x20,
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x4E10,
};

static struct msm_sensor_init_params msm_camera_sensor_s5k4e1gx_init_params = {
	.modes_supported = CAMERA_MODE_2D_B,
	.position = BACK_CAMERA_B,
	.sensor_mount_angle = 90,
};

static struct msm_camera_sensor_board_info msm_camera_sensor_s5k4e1gx_data = {
	.sensor_name = "s5k4e1gx",
	.slave_info = &msm_camera_sensor_s5k4e1gx_slave_info,
	.sensor_info = &msm_camera_sensor_s5k4e1gx_sensor_info,
	.sensor_init_params = &msm_camera_sensor_s5k4e1gx_init_params,
};


static struct platform_device msm_camera_sensor_s5k4e1gx = {
	.name      = "msm_camera_s5k4e1gx",
	.dev       = {
		.platform_data = &msm_camera_sensor_s5k4e1gx_info,
	},
};

static struct msm_camera_sensor_platform_info mt9v113_sensor_platform_info = {
	.mount_angle = 0
};

static void glacier_mt9v113_clk_switch(void){
	int rc = 0;
	pr_info("doing clk switch (glacier)(mt9v113)\n");
	rc = gpio_request(GLACIER_CLK_SWITCH, "mt9v113");
	if (rc < 0)
		pr_err("GPIO (%d) request fail\n", GLACIER_CLK_SWITCH);
	else
		gpio_direction_output(GLACIER_CLK_SWITCH, 1);
	gpio_free(GLACIER_CLK_SWITCH);

	return;
}

static struct msm_camera_device_platform_data glacier_mt9v113_platform_data = {
	.camera_gpio_on = glacier_camera_on_gpios,
	.camera_gpio_off = glacier_camera_off_gpios,
	.csid_core = 0,
	.ioclk = {
		.vfe_clk_rate = 122880000,
	},
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9v113_info = {
	.sensor_name		= "mt9v113",
	.sensor_reset		= GLACIER_CAM2_RST,
	.vcm_pwd		= GLACIER_CAM2_PWD,
	.camera_clk_switch	= glacier_mt9v113_clk_switch,
	.pdata			= &glacier_mt9v113_platform_data,
	.flash_type		= MSM_CAMERA_FLASH_NONE,
	.resource		= msm_camera_resources,
	.num_resources		= ARRAY_SIZE(msm_camera_resources),
	.cam_select_pin		= GLACIER_CLK_SWITCH,
	.sensor_platform_info	= &mt9v113_sensor_platform_info,
	.csi_if			= 0,
};

static struct msm_sensor_info_t msm_camera_sensor_mt9v113_sensor_info = {
	.sensor_name = "mt9v113",
};

static struct msm_camera_slave_info msm_camera_sensor_mt9v113_slave_info = {
	.sensor_slave_addr = 0x3C << 1,
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x2280,
};

static struct msm_sensor_init_params msm_camera_sensor_mt9v113_init_params = {
	.modes_supported = CAMERA_MODE_2D_B,
	.position = FRONT_CAMERA_B,
	.sensor_mount_angle = 0,
};

static struct msm_camera_sensor_board_info msm_camera_sensor_mt9v113_data = {
	.sensor_name = "mt9v113",
	.slave_info = &msm_camera_sensor_mt9v113_slave_info,
	.sensor_info = &msm_camera_sensor_mt9v113_sensor_info,
	.sensor_init_params = &msm_camera_sensor_mt9v113_init_params,
};

static struct platform_device msm_camera_sensor_mt9v113 = {
	.name = "msm_camera_mt9v113",
	.dev = {
		.platform_data = &msm_camera_sensor_mt9v113_info,
	},
};

static struct msm_actuator_info s5k4e1gx_actuator_info = {
	.cam_name = MSM_ACTUATOR_MAIN_CAM_0,
	.vcm_pwd = GLACIER_CAM_PWD,
	.vcm_enable = 1,
};

static int flashlight_control(int mode)
{
	return aat1271_flashlight_control(mode);
}

static struct msm_camera_sensor_flash_src msm_camera_flash_pdata = {
	.camera_flash = flashlight_control,
};

static struct platform_device msm_camera_flash_device = {
	.name = "camera-led-flash",
	.id = 0,
	.dev = {
		.platform_data = &msm_camera_flash_pdata,
	},
};

static struct resource glacier_vfe_resources[] = {
	{
		.name	= "msm_vfe",
		.start	= 0xA6000000,
		.end	= 0xA6000000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "msm_vfe",
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "msm_camif",
		.start	= 0xAB000000,
		.end    = 0xAB000000 + SZ_1K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device msm_device_vfe = {
	.name		= "msm_vfe31",
	.id		= 0,
	.resource	= glacier_vfe_resources,
	.num_resources	= ARRAY_SIZE(glacier_vfe_resources),
};

static struct resource msm_gemini_resources[] = {
	{
		.start  = 0xA3A00000,
		.end    = 0xA3A00000 + 0x0150 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_JPEG,
		.end    = INT_JPEG,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_gemini_device = {
	.name           = "msm_gemini",
	.resource       = msm_gemini_resources,
	.num_resources  = ARRAY_SIZE(msm_gemini_resources),
};

static struct platform_device msm_device_cam = {
	.name		= "msm",
	.id		= 0,
};

void __init glacier_init_cam(void)
{
	platform_device_register(&msm_device_cam);
	platform_device_register(&msm_device_vfe);
	platform_device_register(&msm_device_csic0);
	platform_device_register(&msm_gemini_device);
	platform_device_register(&msm_camera_sensor_s5k4e1gx);
	platform_device_register(&msm_camera_sensor_mt9v113);
	platform_device_register(&msm_camera_flash_device);
	//platform_device_register(&msm_device_vpe);
}

static struct i2c_board_info glacier_camera_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("s5k4e1gx", 0x20 >> 1),
		.platform_data = &msm_camera_sensor_s5k4e1gx_data,
	},
	{
		I2C_BOARD_INFO("mt9v113", 0x3C), /* 0x78: w, 0x79 :r */
		.platform_data = &msm_camera_sensor_mt9v113_data,
	},
	{
		I2C_BOARD_INFO("qcom,actuator", 0x18),
		.platform_data = &s5k4e1gx_actuator_info,
	},
};

struct msm_camera_board_info glacier_camera_board_info = {
	.board_info = glacier_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(glacier_camera_i2c_boardinfo),
};
