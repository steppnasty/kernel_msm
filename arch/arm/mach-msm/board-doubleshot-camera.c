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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include <mach/camera2.h>
#include <mach/msm_flashlight.h>

#include "board-doubleshot.h"

#define DS_CLK_SWITCH 141

static void doubleshot_config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("[CAM]%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static uint32_t doubleshot_camera_off_gpio_table[] = {
	GPIO_CFG(137, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM1_RST# */
	GPIO_CFG(138, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM2_RST# */
	GPIO_CFG(140, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM2_PWDN */
	GPIO_CFG(32, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),	/* CAM_MCLK */
	GPIO_CFG(DOUBLESHOT_CAM_I2C_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA),		/* CAM_I2C_SDA */
	GPIO_CFG(DOUBLESHOT_CAM_I2C_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),		/* CAM_I2C_SCL */
	GPIO_CFG(141, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM_SEL */
};

static uint32_t doubleshot_camera_on_gpio_table[] = {
	GPIO_CFG(DOUBLESHOT_CAM_I2C_SDA, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA),		/* CAM_I2C_SDA */
	GPIO_CFG(DOUBLESHOT_CAM_I2C_SCL, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA),		/* CAM_I2C_SCL */
	GPIO_CFG(32, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),	/* CAM_MCLK */
	GPIO_CFG(137, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM1_RST# */
	GPIO_CFG(138, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM2_RST# */
	GPIO_CFG(140, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM2_PWDN */
	GPIO_CFG(141, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),	/* CAM_SEL */
};

static int doubleshot_sensor_power(char *power, unsigned volt)
{
	struct regulator *sensor_power;
	int rc;

	if (power == NULL)
		return -ENODEV;

	sensor_power = regulator_get(NULL, power);
	if (IS_ERR(sensor_power))
		return -ENODEV;

	if (volt) {
		if (regulator_set_voltage(sensor_power, volt, volt))
			pr_err("[CAM]%s: unable to set %s voltage to %d\n",
				__func__, power, volt);
		rc = regulator_enable(sensor_power);
	} else
		rc = regulator_disable(sensor_power);

	if (rc)
		pr_err("[CAM]%s: %s regulator %s failed\n", __func__,
			volt ? "enable" : "disable", power);
	regulator_put(sensor_power);
	return rc;
}

static int doubleshot_sensor_vreg_on(void)
{
	int rc;
	pr_info("[CAM]%s\n", __func__);
	/* DOT Main camera VCM power */
	rc = doubleshot_sensor_power("8058_l10", 2850000);
	/* DOT Main/2nd IO and 2nd VDD*/
	rc = doubleshot_sensor_power("8058_l12", 1800000);
	udelay(50);
	/* DOT Main / 2nd camera Analog power */
	rc = doubleshot_sensor_power("8058_l15", 2800000);
	udelay(50);
	/* Main Digital power */
	if (system_rev >= 1) {
		/* XB board and after ... */
		rc = doubleshot_sensor_power("8058_l24", 1200000);
		pr_info("Apply XB board camera digital power pin L24\n");
	} else {
		/* XA board */
		rc = doubleshot_sensor_power("8058_l23", 1200000);
		pr_info("Apply XA board camera digital power pin L23\n");
	}

	mdelay(1);

	return rc;
}

static int doubleshot_sensor_vreg_off(void)
{
	int rc;
	pr_info("[CAM]%s\n", __func__);
	/* main / 2nd camera digital power */
	if (system_rev >= 1) {
		/* XB board and after ... */
		rc = doubleshot_sensor_power("8058_l24", 0);
	} else {
		/* XA board */
		rc = doubleshot_sensor_power("8058_l23", 0);
	}
	/* main / 2nd camera analog power */
	rc = doubleshot_sensor_power("8058_l15", 0);
	/* IO power off */
	rc = doubleshot_sensor_power("8058_l12", 0);
	/* main camera VCM power */
	rc = doubleshot_sensor_power("8058_l10", 0);

	mdelay(10);

	return rc;
}

static void doubleshot_camera_on_gpios(void)
{
	doubleshot_config_gpio_table(doubleshot_camera_on_gpio_table,
		ARRAY_SIZE(doubleshot_camera_on_gpio_table));
}

static void doubleshot_camera_off_gpios(void)
{
	doubleshot_config_gpio_table(doubleshot_camera_off_gpio_table,
		ARRAY_SIZE(doubleshot_camera_off_gpio_table));
}


static void imx105_clk_switch(void)
{
	int rc = 0;
	pr_info("[CAM]Doing clk switch (Main Cam)\n");
	rc = gpio_request(DS_CLK_SWITCH, "imx105");
	if (rc < 0)
		pr_err("[CAM]GPIO (%d) request fail\n", DS_CLK_SWITCH);
	else
		gpio_direction_output(DS_CLK_SWITCH, 0);
	gpio_free(DS_CLK_SWITCH);

	mdelay(1);
	return;
}

static struct msm_camera_sensor_flash_data imx105_flash = {
	.flash_type		= MSM_CAMERA_FLASH_LED,
};

static struct msm_camera_device_platform_data imx105_platform_data = {
	.camera_gpio_on  = doubleshot_camera_on_gpios,
	.camera_gpio_off = doubleshot_camera_off_gpios,
	.ioext.csiphy = 0x04800000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_0_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 266667000,
};

static struct msm_camera_sensor_info imx105_sensor_info = {
	.sensor_name		= "imx105",
	.sensor_reset		= 137,/*Main Cam RST*/
	.sensor_pwd		= 139,/*Main Cam PWD*/
	.vcm_pwd		= 58,/*VCM_PD*/
	.vcm_enable		= 1,
	.camera_clk_switch	= imx105_clk_switch,
	.camera_power_on	= doubleshot_sensor_vreg_on,
	.camera_power_off	= doubleshot_sensor_vreg_off,
	.pdata			= &imx105_platform_data,
	.flash_data		= &imx105_flash,
	.power_down_disable	= false, /* true: disable pwd down function */
	.mirror_mode		= 0,
	.cam_select_pin		= DS_CLK_SWITCH,
	.csi_if			= 1,
	.dev_node		= 0
};

static struct msm_sensor_info_t msm_camera_sensor_imx105_sensor_info = {
	.sensor_name		= "imx105"
};

static struct msm_camera_slave_info msm_camera_sensor_imx105_slave_info = {
	.sensor_slave_addr	= 0x1A << 1,
	.sensor_id_reg_addr	= 0x0000,
	.sensor_id		= 0x0105,
};

static struct msm_sensor_init_params msm_camera_sensor_imx105_init_params = {
	.modes_supported	= CAMERA_MODE_2D_B,
	.position		= BACK_CAMERA_B,
	.sensor_mount_angle	= 0,
};

static struct msm_camera_sensor_board_info msm_camera_sensor_imx105_data = {
	.sensor_name = "imx105",
	.slave_info = &msm_camera_sensor_imx105_slave_info,
	.sensor_info = &msm_camera_sensor_imx105_sensor_info,
	.sensor_init_params = &msm_camera_sensor_imx105_init_params,
};

static void mt9v113_clk_switch(void)
{
	int rc = 0;
	pr_info("[CAM]Doing clk switch (2nd Cam)\n");
	rc = gpio_request(DS_CLK_SWITCH, "mt9v113");

	if (rc < 0)
		pr_err("[CAM]GPIO (%d) request fail\n", DS_CLK_SWITCH);
	else
		gpio_direction_output(DS_CLK_SWITCH, 1);

	gpio_free(DS_CLK_SWITCH);
	return;
}

static struct msm_camera_sensor_flash_data mt9v113_flash = {
	.flash_type		= MSM_CAMERA_FLASH_NONE,
};

static struct msm_camera_device_platform_data mt9v113_platform_data = {
	.camera_gpio_on  = doubleshot_camera_on_gpios,
	.camera_gpio_off = doubleshot_camera_off_gpios,
	.ioext.csiphy = 0x04900000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_1_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
};

static struct msm_camera_sensor_info mt9v113_sensor_info = {
	.sensor_name		= "mt9v113",
	.sensor_reset		= 138,/*2nd Cam RST*/
	.sensor_pwd		= 140,/*2nd Cam PWD*/
	.camera_clk_switch	= mt9v113_clk_switch,
	.camera_power_on	= doubleshot_sensor_vreg_on,
	.camera_power_off	= doubleshot_sensor_vreg_off,
	.pdata			= &mt9v113_platform_data,
	.flash_data		= &mt9v113_flash,
	.power_down_disable	= false, /* true: disable pwd down function */
	.mirror_mode		= 0,
	.cam_select_pin		= DS_CLK_SWITCH,
	.csi_if			= 1,
	.dev_node		= 1
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

static struct resource doubleshot_csic0_resources[] = {
	{
		.name	= "csic",
		.start	= 0x04800000,
		.end	= 0x04800000 + 0x00000400 - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "csic",
		.start	= CSI_0_IRQ,
		.end	= CSI_0_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource doubleshot_csic1_resources[] = {
	{
		.name   = "csic",
		.start  = 0x04900000,
		.end    = 0x04900000 + 0x00000400 - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.name   = "csic",
		.start  = CSI_1_IRQ,
		.end    = CSI_1_IRQ,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct resource doubleshot_vfe_resources[] = {
	{
		.name	= "msm_vfe",
		.start	= 0x04500000,
		.end	= 0x04500000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "msm_vfe",
		.start	= VFE_IRQ,
		.end	= VFE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_device_cam = {
	.name		= "msm",
	.id		= 0,
};

static struct platform_device msm_device_vfe = {
	.name		= "msm_vfe31",
	.id		= 0,
	.resource	= doubleshot_vfe_resources,
	.num_resources	= ARRAY_SIZE(doubleshot_vfe_resources),
};

static struct platform_device msm_device_csic0 = {
	.name           = "msm_csic",
	.id             = 0,
	.resource       = doubleshot_csic0_resources,
	.num_resources  = ARRAY_SIZE(doubleshot_csic0_resources),
};

static struct platform_device msm_device_csic1 = {
	.name           = "msm_csic",
	.id             = 1,
	.resource       = doubleshot_csic1_resources,
	.num_resources  = ARRAY_SIZE(doubleshot_csic1_resources),
};

static struct platform_device msm_camera_sensor_imx105 = {
	.name		= "msm_camera_imx105",
	.dev		= {
		.platform_data = &imx105_sensor_info,
	},
};

static struct platform_device msm_camera_sensor_mt9v113 = {
	.name		= "msm_camera_mt9v113",
	.dev		= {
		.platform_data = &mt9v113_sensor_info,
	},
};

static int flashlight_control(int mode)
{
#if CONFIG_ARCH_MSM_FLASHLIGHT
	return aat1271_flashlight_control(mode);
#else
	return 0;
#endif
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

void __init doubleshot_init_cam(void)
{
	platform_device_register(&msm_device_cam);
	platform_device_register(&msm_device_csic0);
	platform_device_register(&msm_device_csic1);
	platform_device_register(&msm_device_vfe);
	platform_device_register(&msm_camera_sensor_imx105);
	platform_device_register(&msm_camera_sensor_mt9v113);
	platform_device_register(&msm_camera_flash_device);
}

#ifdef CONFIG_I2C
static struct i2c_board_info doubleshot_camera_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("imx105", 0x1A >> 1),
		.platform_data = &msm_camera_sensor_imx105_data,
	},
	{
		I2C_BOARD_INFO("mt9v113", 0x3C),
		.platform_data = &msm_camera_sensor_mt9v113_data,
	},
};

struct msm_camera_board_info doubleshot_camera_board_info = {
	.board_info = doubleshot_camera_i2c_boardinfo,
	.num_i2c_board_info = ARRAY_SIZE(doubleshot_camera_i2c_boardinfo),
};
#endif
