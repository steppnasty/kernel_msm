/* linux/arch/arm/mach-msm/board-glacier.c
 *
 * Copyright (C) 2009 Google, Inc.
 * Copyright (C) 2010 HTC Corporation.
 * Copyright (c) 2014-2017, Brian Stepp <steppnasty@gmail.com>
 * Author: Tony Liu <tony_liu@htc.com>
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/android_pmem.h>
#ifdef CONFIG_MSM_SSBI
#include <mach/msm_ssbi.h>
#endif
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/marimba.h>
#include <linux/i2c.h>
#include <linux/i2c-msm.h>
#include <linux/a1026.h>
#include <linux/spi/spi.h>
#include <mach/qdsp5v2/msm_lpa.h>
#include <linux/akm8975.h>
#include <linux/bma150.h>
#include <linux/capella_cm3602.h>
#include <linux/atmel_qt602240.h>
#include <linux/curcial_oj.h>
#include <linux/input/pmic8058-keypad.h>
#include <linux/proc_fs.h>
#include <linux/msm_ion.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/setup.h>
#include <mach/msm_flashlight.h>
#include <asm/mach/flash.h>

#include <mach/system.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/board_htc.h>
#include <mach/bcm_bt_lpm.h>

#include <mach/hardware.h>
#include <mach/msm_hsusb.h>
#include <mach/rpc_hsusb.h>
#include <mach/msm_spi.h>
#include <mach/dma.h>
#include <mach/msm_iomap.h>
#include <mach/perflock.h>
#include <mach/msm_serial_debugger.h>
#include <mach/rpc_pmapp.h>
#include <mach/remote_spinlock.h>
#include <mach/socinfo.h>

#include <mach/vreg.h>
#include <mach/atmega_microp.h>
#include <mach/htc_battery.h>
#include <linux/tps65200.h>
#ifdef CONFIG_BT
#include <mach/htc_bdaddress.h>
#endif
#include <mach/htc_fmtx_rfkill.h>
#include <mach/htc_headset_mgr.h>
#include <mach/htc_headset_gpio.h>
#include <mach/htc_headset_microp.h>
#include <mach/htc_headset_pmic.h>

#include "board-glacier.h"
#include "devices.h"
#ifdef CONFIG_USB_G_ANDROID
#include <linux/usb/android.h>
#include <mach/usbdiag.h>
#endif
#include "proc_comm.h"
#include "smd_private.h"
#include "pm.h"
#include "spm.h"
#include "acpuclock.h"
#include <mach/msm_serial_hs.h>
#include "gpio_chip.h"

#include "board-glacier-regulator.h"

#ifdef CONFIG_MICROP_COMMON
void __init glacier_microp_init(void);
#endif

static uint opt_disable_uart2;
static unsigned engineerid;

module_param_named(disable_uart2, opt_disable_uart2, uint, 0);

static uint32_t usb_ID_PIN_input_table[] = {
	GPIO_CFG(GLACIER_GPIO_USB_ID_PIN, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA),
};

static uint32_t usb_ID_PIN_ouput_table[] = {
	GPIO_CFG(GLACIER_GPIO_USB_ID_PIN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA),
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n;
	unsigned id;
	for (n = 0; n < len; n++) {
		id = table[n];
		if (msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0))
			printk(KERN_ERR "%s: config gpio fail\n", __func__);
	}
}

void config_glacier_usb_id_gpios(bool output)
{
	if (output) {
		config_gpio_table(usb_ID_PIN_ouput_table,
			ARRAY_SIZE(usb_ID_PIN_ouput_table));
		gpio_set_value(GLACIER_GPIO_USB_ID_PIN, 1);
	} else
		config_gpio_table(usb_ID_PIN_input_table,
			ARRAY_SIZE(usb_ID_PIN_input_table));
}

#ifdef CONFIG_USB_EHCI_MSM_72K
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{
	static int vbus_is_on;

	/* If VBUS is already on (or off), do nothing. */
	if (unlikely(on == vbus_is_on))
		return;

	vbus_is_on = on;
}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_45NM),
	.vbus_power	= msm_hsusb_vbus_power,
	.power_budget	= 180,
};
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
static struct vreg *vreg_3p3;
static int msm_hsusb_ldo_init(int init)
{
	uint32_t version = 0;
	int def_vol = 3400;

	version = socinfo_get_version();

	if (SOCINFO_VERSION_MAJOR(version) >= 2 &&
			SOCINFO_VERSION_MINOR(version) >= 1) {
		def_vol = 3075;
		pr_debug("%s: default voltage:%d\n", __func__, def_vol);
	}

	if (init) {
		vreg_3p3 = vreg_get(NULL, "usb");
		if (IS_ERR(vreg_3p3))
			return PTR_ERR(vreg_3p3);
		vreg_set_level(vreg_3p3, def_vol);
	} else
		vreg_put(vreg_3p3);

	return 0;
}

static int msm_hsusb_ldo_enable(int enable)
{
	static int ldo_status;

	if (!vreg_3p3 || IS_ERR(vreg_3p3))
		return -ENODEV;

	if (ldo_status == enable)
		return 0;

	ldo_status = enable;

	if (enable)
		return vreg_enable(vreg_3p3);

	return vreg_disable(vreg_3p3);
}

static int msm_hsusb_ldo_set_voltage(int mV)
{
	static int cur_voltage = 3400;

	if (!vreg_3p3 || IS_ERR(vreg_3p3))
		return -ENODEV;

	if (cur_voltage == mV)
		return 0;

	cur_voltage = mV;

	pr_debug("%s: (%d)\n", __func__, mV);

	return vreg_set_level(vreg_3p3, mV);
}
#endif

#ifndef CONFIG_USB_EHCI_MSM_72K
static int msm_hsusb_pmic_notif_init(void (*callback)(int online), int init);
#endif
static struct msm_otg_platform_data msm_otg_pdata = {
	.rpc_connect	= hsusb_rpc_connect,

#ifndef CONFIG_USB_EHCI_MSM_72K
	.pmic_vbus_notif_init	= msm_hsusb_pmic_notif_init,
#else
	.vbus_power	= msm_hsusb_vbus_power,
#endif
	.pemp_level		 = PRE_EMPHASIS_WITH_20_PERCENT,
	.cdr_autoreset		 = CDR_AUTO_RESET_DISABLE,
	.drv_ampl		 = HS_DRV_AMPLITUDE_DEFAULT,
	.se1_gating		 = SE1_GATING_DISABLE,
	.phy_reset	= (void *) msm_hsusb_phy_reset,
	.ldo_enable		 = msm_hsusb_ldo_enable,
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_set_voltage	 = msm_hsusb_ldo_set_voltage,
};

#ifdef CONFIG_USB_G_ANDROID
static struct msm_hsusb_gadget_platform_data msm_gadget_pdata = {
	.is_phy_status_timer_on = 1,
};

#ifdef CONFIG_USB_G_ANDROID
static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};
#endif

void glacier_add_usb_devices(void)
{
	config_glacier_usb_id_gpios(0);

	platform_device_register(&android_usb_device);
}
#endif

#ifdef CONFIG_DOCK_DETECT
static struct dock_detect_platform_data dock_detect_pdata = {
	.dock_pin_gpio	= GLACIER_GPIO_DOCK_PIN,
};

static struct platform_device dock_detect_device = {
	.name	= "dock_detect",
	.id	= -1,
	.dev	= {
		.platform_data = &dock_detect_pdata,
	},
};
#endif

int glacier_pm8058_gpios_init(struct pm8058_chip *pm_chip)
{
	int ret;

	struct pm8058_gpio glacier_oj_action = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_home_key = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_menu_key = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};


	struct pm8058_gpio glacier_back_key = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};


	struct pm8058_gpio glacier_send_key = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_tp_rstz = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_L5,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_sdmc_cd_n = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_L5,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_vol_up = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_vol_down = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_cam_step2 = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_cam_step1 = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_aud_hp_detz = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_31P5,
		.vin_sel        = PM_GPIO_VIN_S3,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_aud_spk_eno = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 1,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_S3,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	struct pm8058_gpio glacier_ps_shdn = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_L5,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
	};

	pm8058_gpio_config(GLACIER_OJ_ACTION, &glacier_oj_action);
	pm8058_gpio_config(GLACIER_HOME_KEY, &glacier_home_key);
	pm8058_gpio_config(GLACIER_MENU_KEY, &glacier_menu_key);
	pm8058_gpio_config(GLACIER_BACK_KEY, &glacier_back_key);
	pm8058_gpio_config(GLACIER_SEND_KEY, &glacier_send_key);
	pm8058_gpio_config(GLACIER_TP_RSTz, &glacier_tp_rstz);
	pm8058_gpio_config(GLACIER_SDMC_CD_N, &glacier_sdmc_cd_n);
	pm8058_gpio_config(GLACIER_VOL_UP, &glacier_vol_up);
	pm8058_gpio_config(GLACIER_VOL_DN, &glacier_vol_down);
	pm8058_gpio_config(GLACIER_CAM_STEP2, &glacier_cam_step2);
	pm8058_gpio_config(GLACIER_CAM_STEP1, &glacier_cam_step1);
	pm8058_gpio_config(GLACIER_AUD_HP_DETz, &glacier_aud_hp_detz);

	ret = pm8058_gpio_config(GLACIER_AUD_SPK_ENO, &glacier_aud_spk_eno);
	if (ret) {
		pr_err("%s PMIC GPIO 18 write failed\n", __func__);
		return ret;
	}
	
	ret = pm8058_gpio_config(GLACIER_PS_SHDN, &glacier_ps_shdn);
	if (ret) {
		pr_err("%s PMIC GPIO 20 write failed\n", __func__);
		return ret;
	}

	return 0;
}

/* Regulator API support */

#ifdef CONFIG_MSM_PROC_COMM_REGULATOR
static struct platform_device msm_proccomm_regulator_dev = {
	.name = PROCCOMM_REGULATOR_DEV_NAME,
	.id   = -1,
	.dev  = {
		.platform_data = &glacier_proccomm_regulator_data
	}
};
#endif

/* HTC_HEADSET_GPIO Driver */
static struct htc_headset_gpio_platform_data htc_headset_gpio_data = {
	.hpin_gpio		= 0,
	.key_enable_gpio	= 0,
	.mic_select_gpio	= GLACIER_AUD_MICPATH_SEL,
};

static struct platform_device htc_headset_gpio = {
	.name	= "HTC_HEADSET_GPIO",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_gpio_data,
	},
};

/* HTC_HEADSET_MICROP Driver */
static struct htc_headset_microp_platform_data htc_headset_microp_data = {
	.remote_int		= 1 << 13,
	.remote_irq		= MSM_uP_TO_INT(13),
	.remote_enable_pin	= 1 << 4,
	.adc_channel		= 0x01,
	.adc_remote		= {0, 33, 38, 82, 95, 167},
};

static struct platform_device htc_headset_microp = {
	.name	= "HTC_HEADSET_MICROP",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_microp_data,
	},
};

/* HTC_HEADSET_PMIC Driver */
static struct htc_headset_pmic_platform_data htc_headset_pmic_data = {
	.hpin_gpio	= PM8058_GPIO_PM_TO_SYS(GLACIER_AUD_HP_DETz),
	.hpin_irq	= MSM_GPIO_TO_INT(
			  PM8058_GPIO_PM_TO_SYS(GLACIER_AUD_HP_DETz)),
};

static struct platform_device htc_headset_pmic = {
	.name	= "HTC_HEADSET_PMIC",
	.id	= -1,
	.dev	= {
		.platform_data	= &htc_headset_pmic_data,
	},
};

/* HTC_HEADSET_MGR Driver */
static struct platform_device *headset_devices[] = {
	&htc_headset_gpio,
	&htc_headset_microp,
	&htc_headset_pmic,
	/* Please put the headset detection driver on the last */
};

static struct htc_headset_mgr_platform_data htc_headset_mgr_data = {
	.headset_devices_num	= ARRAY_SIZE(headset_devices),
	.headset_devices	= headset_devices,
	.headset_config_num	= 0,
	.headset_config		= 0,
};

static struct htc_battery_platform_data htc_battery_pdev_data = {
	.guage_driver = GUAGE_MODEM,
	.charger = SWITCH_CHARGER_TPS65200,
	.m2a_cable_detect = 1,
};

static struct platform_device htc_battery_pdev = {
	.name = "htc_battery",
	.id = -1,
	.dev	= {
		.platform_data = &htc_battery_pdev_data,
	},
};

static int capella_cm3602_power(int pwr_device, uint8_t enable);
static struct microp_function_config microp_functions[] = {
	{
		.name   = "microp_intrrupt",
		.category = MICROP_FUNCTION_INTR,
	},
	{
		.name   = "reset-int",
		.category = MICROP_FUNCTION_RESET_INT,
		.int_pin = 1 << 8,
	},
#if 0
	{
		.name   = "oj",
		.category = MICROP_FUNCTION_OJ,
		.int_pin = 1 << 12,
	},
#endif
};

static struct microp_function_config microp_lightsensor_function = {
	.name = "light_sensor",
	.category = MICROP_FUNCTION_LSENSOR,
	.levels = { 1, 3, 5, 17, 33, 172, 299, 326, 344, 1023 },
	.channel = 3,
	.int_pin = 1 << 9,
	.golden_adc = 0xD3,
	.ls_power = capella_cm3602_power,
};

static struct lightsensor_platform_data lightsensor_data = {
	.config = &microp_lightsensor_function,
	.irq = MSM_uP_TO_INT(9),
};

static struct microp_led_config led_config[] = {
	{
		.name = "amber",
		.type = LED_RGB,
	},
	{
		.name = "green",
		.type = LED_RGB,
	},
	{
		.name = "button-backlight",
		.type = LED_PWM,
		.led_pin = 1 << 2,
		.init_value = 0xFF,
		.fade_time = 5,
	},
};

static struct microp_led_platform_data microp_leds_data = {
	.num_leds = ARRAY_SIZE(led_config),
	.led_config = led_config,
};

static struct bma150_platform_data microp_g_sensor_pdata = {
	.microp_new_cmd = 1,
	.chip_layout = 0,
};

static struct platform_device microp_devices[] = {
	{
		.name = "lightsensor_microp",
		.dev = {
			.platform_data = &lightsensor_data,
		},
	},
	{
		.name = "leds-microp",
		.id = -1,
		.dev = {
			.platform_data = &microp_leds_data,
		},
	},
	{
		.name = BMA150_G_SENSOR_NAME,
		.dev = {
			.platform_data = &microp_g_sensor_pdata,
		},
	},
	{
		.name	= "HTC_HEADSET_MGR",
		.id	= -1,
		.dev	= {
			.platform_data	= &htc_headset_mgr_data,
		},
	},
};

static struct microp_i2c_platform_data microp_data = {
	.num_functions   = ARRAY_SIZE(microp_functions),
	.microp_function = microp_functions,
	.num_devices = ARRAY_SIZE(microp_devices),
	.microp_devices = microp_devices,
	.gpio_reset = GLACIER_GPIO_UP_RESET_N,
	.spi_devices = SPI_GSENSOR,
};

static struct akm8975_platform_data compass_platform_data = {
	.layouts = GLACIER_LAYOUTS,
};

static struct a1026_platform_data a1026_data = {
       .gpio_a1026_micsel = GLACIER_AUD_MICPATH_SEL,
       .gpio_a1026_wakeup = GLACIER_AUD_A1026_WAKEUP,
       .gpio_a1026_reset = GLACIER_AUD_A1026_RESET,
       .gpio_a1026_clk = GLACIER_AUD_A1026_CLK,
       /*.gpio_a1026_int = PASSIONC_AUD_A1026_INT,*/
};

static uint32_t proximity_on_gpio_table[] = {
	GPIO_CFG(GLACIER_GPIO_PROXIMITY_INT_N,
		0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, 0), /* PS_VOUT */
};

static uint32_t proximity_off_gpio_table[] = {
	GPIO_CFG(GLACIER_GPIO_PROXIMITY_INT_N,
		0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, 0) /* PS_VOUT */
};

void config_glacier_proximity_gpios(int on)
{
	if (on)
		config_gpio_table(proximity_on_gpio_table,
			ARRAY_SIZE(proximity_on_gpio_table));
	else
		config_gpio_table(proximity_off_gpio_table,
			ARRAY_SIZE(proximity_off_gpio_table));
}

static int __capella_cm3602_power(int on)
{
	int rc;
	struct vreg *vreg = vreg_get(0, "gp7");
	if (!vreg) {
		printk(KERN_ERR "%s: vreg error\n", __func__);
		return -EIO;
	}
	rc = vreg_set_level(vreg, 2850);

	printk(KERN_DEBUG "%s: Turn the capella_cm3602 power %s\n",
		__func__, (on) ? "on" : "off");

	if (on) {
		config_glacier_proximity_gpios(1);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(GLACIER_PS_SHDN), 1);
		rc = vreg_enable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg enable failed\n", __func__);
	} else {
		rc = vreg_disable(vreg);
		if (rc < 0)
			printk(KERN_ERR "%s: vreg disable failed\n", __func__);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(GLACIER_PS_SHDN), 0);
		config_glacier_proximity_gpios(0);
	}

	return rc;
}

static DEFINE_MUTEX(capella_cm3602_lock);
static int als_power_control;


static int capella_cm3602_power(int pwr_device, uint8_t enable)
{
	unsigned int old_status = 0;
	int ret = 0, on = 0;
	mutex_lock(&capella_cm3602_lock);

	old_status = als_power_control;
	if (enable)
		als_power_control |= pwr_device;
	else
		als_power_control &= ~pwr_device;

	on = als_power_control ? 1 : 0;
	if (old_status == 0 && on)
		ret = __capella_cm3602_power(1);
	else if (!on)
		ret = __capella_cm3602_power(0);

	mutex_unlock(&capella_cm3602_lock);
	return ret;
}

static struct capella_cm3602_platform_data capella_cm3602_pdata = {
	.power = capella_cm3602_power,
	.p_en = PM8058_GPIO_PM_TO_SYS(GLACIER_PS_SHDN),
	.p_out = GLACIER_GPIO_PROXIMITY_INT_N,
	.irq = MSM_GPIO_TO_INT(GLACIER_GPIO_PROXIMITY_INT_N),
};

static struct platform_device capella_cm3602 = {
	.name = CAPELLA_CM3602,
	.id = -1,
	.dev = {
		.platform_data = &capella_cm3602_pdata
	}
};

static int glacier_ts_atmel_power(int on)
{
	pr_info("%s: power %d\n", __func__, on);

	if (on == 1)
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(GLACIER_TP_RSTz), 1);
	else if (on == 2) {
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(GLACIER_TP_RSTz), 0);
		msleep(5);
		gpio_set_value(PM8058_GPIO_PM_TO_SYS(GLACIER_TP_RSTz), 1);
		msleep(40);
	}

	return 0;
}

struct atmel_i2c_platform_data glacier_ts_atmel_data[] = {
	{
		.version = 0x020,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 1020,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = GLACIER_GPIO_TP_ATT_N,
		.power = glacier_ts_atmel_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 25},
		.config_T8 = {8, 0, 5, 5, 0, 0, 10, 35, 5, 192},
		.config_T9 = {139, 0, 0, 18, 12, 0, 16, 40, 3, 1, 10, 10, 5, 15, 4, 10, 20, 0, 0, 0, 0, 0, 0, 0, 40, 20, 155, 48, 152, 79, 25, 12},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 35, 0, 0, 10, 15, 18, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 16, 60},
		.object_crc = {0x04, 0xE0, 0xBA},
		.cable_config = {40, 35, 8, 16},
		.GCAF_level = {20, 24, 28, 40, 63},
		.filter_level = {10, 60, 963, 1013},
	},
	{
		.version = 0x015,
		.abs_x_min = 0,
		.abs_x_max = 1023,
		.abs_y_min = 0,
		.abs_y_max = 1020,
		.abs_pressure_min = 0,
		.abs_pressure_max = 255,
		.abs_width_min = 0,
		.abs_width_max = 20,
		.gpio_irq = GLACIER_GPIO_TP_ATT_N,
		.power = glacier_ts_atmel_power,
		.config_T6 = {0, 0, 0, 0, 0, 0},
		.config_T7 = {50, 15, 25},
		.config_T8 = {8, 0, 5, 5, 0, 0, 10, 35},
		.config_T9 = {139, 0, 0, 18, 12, 0, 16, 40, 3, 1, 10, 10, 5, 15, 4, 10, 20, 0, 0, 0, 0, 0, 0, 0, 40, 20, 155, 48, 152, 79, 25},
		.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T20 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T22 = {15, 0, 0, 0, 0, 0, 0, 0, 35, 0, 0, 10, 15, 18, 255, 255, 0},
		.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T24 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T25 = {3, 0, 200, 50, 64, 31, 0, 0, 0, 0, 0, 0, 0, 0},
		.config_T27 = {0, 0, 0, 0, 0, 0, 0},
		.config_T28 = {0, 0, 2, 4, 16, 60},
		.object_crc = {0x48, 0x5E, 0x95},
		.cable_config = {40, 35, 8, 16},
		.GCAF_level = {20, 24, 28, 40, 63},
		.filter_level = {10, 60, 963, 1013},
	},
};

static struct i2c_board_info i2c_a1026_devices[] = {
	{
			I2C_BOARD_INFO("audience_a1026", 0x3E),
			.platform_data = &a1026_data,
			/*.irq = MSM_GPIO_TO_INT(PASSIONC_AUD_A1026_INT)*/
	},
};

static struct i2c_board_info msm_i2c_gsbi5_info[] = {
	{
		I2C_BOARD_INFO(ATMEL_QT602240_NAME, 0x94 >> 1),
		.platform_data = &glacier_ts_atmel_data,
		.irq = MSM_GPIO_TO_INT(GLACIER_GPIO_TP_ATT_N)
	},	
};

static struct i2c_board_info i2c_microp_devices[] = {
	{
		I2C_BOARD_INFO(MICROP_I2C_NAME, 0xCC >> 1),
		.platform_data = &microp_data,
		.irq = MSM_GPIO_TO_INT(GLACIER_GPIO_UP_INT_N)
	},
};

static struct i2c_board_info i2c_akm8975_devices[] = {
	{
		I2C_BOARD_INFO(AKM8975_I2C_NAME, 0x1A >> 1),
		.platform_data = &compass_platform_data,
		.irq = MSM_GPIO_TO_INT(GLACIER_GPIO_COMPASS_INT),
	},
};

static struct tps65200_platform_data tps65200_data = {
	.gpio_chg_int = MSM_GPIO_TO_INT(PM8058_GPIO_PM_TO_SYS(GLACIER_GPIO_CHG_INT)),
};

static struct i2c_board_info msm_tps_65200_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("tps65200", 0xD4 >> 1),
		.platform_data = &tps65200_data,
	},
};

struct i2c_registry {
	int                   bus;
	struct i2c_board_info *info;
	int                   len;
};

static struct i2c_registry glacier_i2c_devices[] __initdata = {
	{
		0,
		msm_i2c_gsbi5_info,
		ARRAY_SIZE(msm_i2c_gsbi5_info),
	},
	{
		0,
		i2c_microp_devices,
		ARRAY_SIZE(i2c_microp_devices),
	},
	{
		0,
		i2c_akm8975_devices,
		ARRAY_SIZE(i2c_akm8975_devices),
	},
	{
		0,
		msm_tps_65200_boardinfo,
		ARRAY_SIZE(msm_tps_65200_boardinfo),
	},
};

static void register_i2c_devices(void)
{
#ifdef CONFIG_I2C
	int i;
#ifdef CONFIG_MSMB_CAMERA
	struct i2c_registry glacier_camera_i2c_devices = {
		4, /* QUP ID */
		glacier_camera_board_info.board_info,
		glacier_camera_board_info.num_i2c_board_info,
	};
#endif
	for (i = 0; i < ARRAY_SIZE(glacier_i2c_devices); ++i)
		i2c_register_board_info(glacier_i2c_devices[i].bus,
					glacier_i2c_devices[i].info,
					glacier_i2c_devices[i].len);

#ifdef CONFIG_MSMB_CAMERA
	i2c_register_board_info(glacier_camera_i2c_devices.bus,
		glacier_camera_i2c_devices.info,
		glacier_camera_i2c_devices.len);
#endif
#endif
}

static struct vreg *vreg_marimba_1;
static struct vreg *vreg_marimba_2;

static unsigned int msm_marimba_setup_power(void)
{
	int rc;

	rc = vreg_enable(vreg_marimba_1);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto out;
	}
	rc = vreg_enable(vreg_marimba_2);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto out;
	}

out:
	return rc;
};

static void msm_marimba_shutdown_power(void)
{
	int rc;

	rc = vreg_disable(vreg_marimba_1);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	rc = vreg_disable(vreg_marimba_2);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
};

/*
static int fm_radio_setup(struct marimba_fm_platform_data *pdata)
{
	int rc;
	uint32_t irqcfg;
	const char *id = "FMPW";

	pdata->vreg_s2 = vreg_get(NULL, "s2");
	if (IS_ERR(pdata->vreg_s2)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(pdata->vreg_s2));
		return -1;
	}

	rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 1300);
	if (rc < 0) {
		printk(KERN_ERR "%s: voltage level vote failed (%d)\n",
			__func__, rc);
		return rc;
	}

	rc = vreg_enable(pdata->vreg_s2);
	if (rc) {
		printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		return rc;
	}

	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
					  PMAPP_CLOCK_VOTE_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s: clock vote failed (%d)\n",
			__func__, rc);
		goto fm_clock_vote_fail;
	}
	irqcfg = GPIO_CFG(147, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL,
					GPIO_CFG_2MA);
	rc = gpio_tlmm_config(irqcfg, GPIO_CFG_ENABLE);
	if (rc) {
		printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, irqcfg, rc);
		rc = -EIO;
		goto fm_gpio_config_fail;

	}
	return 0;
fm_gpio_config_fail:
	pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
				  PMAPP_CLOCK_VOTE_OFF);
fm_clock_vote_fail:
	vreg_disable(pdata->vreg_s2);
	return rc;

};

static void fm_radio_shutdown(struct marimba_fm_platform_data *pdata)
{
	int rc;
	const char *id = "FMPW";
	uint32_t irqcfg = GPIO_CFG(147, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP,
					GPIO_CFG_2MA);
	rc = gpio_tlmm_config(irqcfg, GPIO_CFG_ENABLE);
	if (rc) {
		printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, irqcfg, rc);
	}
	rc = vreg_disable(pdata->vreg_s2);
	if (rc) {
		printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	rc = pmapp_clock_vote(id, PMAPP_CLOCK_ID_DO,
					  PMAPP_CLOCK_VOTE_OFF);
	if (rc < 0)
		printk(KERN_ERR "%s: clock_vote return val: %d \n",
						__func__, rc);
	rc = pmapp_vreg_level_vote(id, PMAPP_VREG_S2, 0);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level vote return val: %d \n",
						__func__, rc);
}

static struct marimba_fm_platform_data marimba_fm_pdata = {
	.fm_setup =  fm_radio_setup,
	.fm_shutdown = fm_radio_shutdown,
	.irq = MSM_GPIO_TO_INT(147),
	.vreg_s2 = NULL,
	.vreg_xo_out = NULL,
};
*/

/* Slave id address for FM/CDC/QMEMBIST
 * Values can be programmed using Marimba slave id 0
 * should there be a conflict with other I2C devices
 * */
/*#define MARIMBA_SLAVE_ID_FM_ADDR	0x2A*/
#define MARIMBA_SLAVE_ID_CDC_ADDR	0x77
#define MARIMBA_SLAVE_ID_QMEMBIST_ADDR	0X66

static const char *tsadc_id = "MADC";
static const char *vregs_tsadc_name[] = {
	"gp12",
	"s2",
};
static struct vreg *vregs_tsadc[ARRAY_SIZE(vregs_tsadc_name)];

static int marimba_tsadc_power(int vreg_on)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		if (!vregs_tsadc[i]) {
			printk(KERN_ERR "%s: vreg_get %s failed (%d)\n",
				__func__, vregs_tsadc_name[i], rc);
			goto vreg_fail;
		}

		rc = vreg_on ? vreg_enable(vregs_tsadc[i]) :
			  vreg_disable(vregs_tsadc[i]);
		if (rc < 0) {
			printk(KERN_ERR "%s: vreg %s %s failed (%d)\n",
				__func__, vregs_tsadc_name[i],
			       vreg_on ? "enable" : "disable", rc);
			goto vreg_fail;
		}
	}
	/* vote for D0 buffer */
	rc = pmapp_clock_vote(tsadc_id, PMAPP_CLOCK_ID_DO,
		vreg_on ? PMAPP_CLOCK_VOTE_ON : PMAPP_CLOCK_VOTE_OFF);
	if (rc)	{
		printk(KERN_ERR "%s: unable to %svote for d0 clk\n",
			__func__, vreg_on ? "" : "de-");
		goto do_vote_fail;
	}

	mdelay(5); /* ensure power is stable */

	return 0;

do_vote_fail:
vreg_fail:
	while (i)
		vreg_disable(vregs_tsadc[--i]);
	return rc;
}

static int marimba_tsadc_vote(int vote_on)
{
	int rc, level;

	level = vote_on ? 1300 : 0;

	rc = pmapp_vreg_level_vote(tsadc_id, PMAPP_VREG_S2, level);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level %s failed (%d)\n",
			__func__, vote_on ? "on" : "off", rc);

	return rc;
}

static int marimba_tsadc_init(void)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		vregs_tsadc[i] = vreg_get(NULL, vregs_tsadc_name[i]);
		if (IS_ERR(vregs_tsadc[i])) {
			printk(KERN_ERR "%s: vreg get %s failed (%ld)\n",
			       __func__, vregs_tsadc_name[i],
			       PTR_ERR(vregs_tsadc[i]));
			rc = PTR_ERR(vregs_tsadc[i]);
			goto vreg_get_fail;
		}
	}

	return rc;

vreg_get_fail:
	while (i)
		vreg_put(vregs_tsadc[--i]);
	return rc;
}

static int marimba_tsadc_exit(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(vregs_tsadc_name); i++) {
		if (vregs_tsadc[i])
			vreg_put(vregs_tsadc[i]);
	}

	rc = pmapp_vreg_level_vote(tsadc_id, PMAPP_VREG_S2, 0);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg level off failed (%d)\n",
			__func__, rc);

	return rc;
}

static struct marimba_tsadc_platform_data marimba_tsadc_pdata = {
	.marimba_tsadc_power = marimba_tsadc_power,
	.init		     =  marimba_tsadc_init,
	.exit		     =  marimba_tsadc_exit,
	.level_vote	     =  marimba_tsadc_vote,
	.tsadc_prechg_en = true,
	.setup = {
		.pen_irq_en	=	true,
		.tsadc_en	=	true,
	},
	.params2 = {
		.input_clk_khz		=	2400,
		.sample_prd		=	TSADC_CLK_3,
	},
	.params3 = {
		.prechg_time_nsecs	=	6400,
		.stable_time_nsecs	=	6400,
		.tsadc_test_mode	=	0,
	},
};

static struct vreg *vreg_codec_s4;
static int msm_marimba_codec_power(int vreg_on)
{
	int rc = 0;

	if (!vreg_codec_s4) {

		vreg_codec_s4 = vreg_get(NULL, "s4");

		if (IS_ERR(vreg_codec_s4)) {
			printk(KERN_ERR "%s: vreg_get() failed (%ld)\n",
				__func__, PTR_ERR(vreg_codec_s4));
			rc = PTR_ERR(vreg_codec_s4);
			goto  vreg_codec_s4_fail;
		}
	}

	if (vreg_on) {
		rc = vreg_enable(vreg_codec_s4);
		if (rc)
			printk(KERN_ERR "%s: vreg_enable() = %d \n",
					__func__, rc);
		goto vreg_codec_s4_fail;
	} else {
		rc = vreg_disable(vreg_codec_s4);
		if (rc)
			printk(KERN_ERR "%s: vreg_disable() = %d \n",
					__func__, rc);
		goto vreg_codec_s4_fail;
	}

vreg_codec_s4_fail:
	return rc;
}

static struct marimba_codec_platform_data mariba_codec_pdata = {
	.marimba_codec_power =  msm_marimba_codec_power,
};

static struct marimba_platform_data marimba_pdata = {
	/*.slave_id[MARIMBA_SLAVE_ID_FM]       = MARIMBA_SLAVE_ID_FM_ADDR,*/
	.slave_id[MARIMBA_SLAVE_ID_CDC]	     = MARIMBA_SLAVE_ID_CDC_ADDR,
	.slave_id[MARIMBA_SLAVE_ID_QMEMBIST] = MARIMBA_SLAVE_ID_QMEMBIST_ADDR,
	.marimba_setup = msm_marimba_setup_power,
	.marimba_shutdown = msm_marimba_shutdown_power,
	/*.fm = &marimba_fm_pdata,*/
	.tsadc = &marimba_tsadc_pdata,
	.codec = &mariba_codec_pdata,
};

static void __init msm7x30_init_marimba(void)
{
	vreg_marimba_1 = vreg_get(NULL, "s2");
	if (IS_ERR(vreg_marimba_1)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_marimba_1));
		return;
	}
	vreg_marimba_2 = vreg_get(NULL, "gp16");
	if (IS_ERR(vreg_marimba_2)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_marimba_2));
		return;
	}
}
#ifdef CONFIG_MSM7KV2_1X_AUDIO
static struct resource msm_aictl_resources[] = {
	{
		.name = "aictl",
		.start = 0xa5000100,
		.end = 0xa5000100,
		.flags = IORESOURCE_MEM,
	}
};

static struct resource msm_mi2s_resources[] = {
	{
		.name = "hdmi",
		.start = 0xac900000,
		.end = 0xac900038,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "codec_rx",
		.start = 0xac940040,
		.end = 0xac940078,
		.flags = IORESOURCE_MEM,
	},
	{
		.name = "codec_tx",
		.start = 0xac980080,
		.end = 0xac9800B8,
		.flags = IORESOURCE_MEM,
	}

};

static struct msm_lpa_platform_data lpa_pdata = {
	.obuf_hlb_size = 0x2BFF8,
	.dsp_proc_id = 0,
	.app_proc_id = 2,
	.nosb_config = {
		.llb_min_addr = 0,
		.llb_max_addr = 0x3ff8,
		.sb_min_addr = 0,
		.sb_max_addr = 0,
	},
	.sb_config = {
		.llb_min_addr = 0,
		.llb_max_addr = 0x37f8,
		.sb_min_addr = 0x3800,
		.sb_max_addr = 0x3ff8,
	}
};

static struct resource msm_lpa_resources[] = {
	{
		.name = "lpa",
		.start = 0xa5000000,
		.end = 0xa50000a0,
		.flags = IORESOURCE_MEM,
	}
};

static struct resource msm_aux_pcm_resources[] = {

	{
		.name = "aux_codec_reg_addr",
		.start = 0xac9c00c0,
		.end = 0xac9c00c8,
		.flags = IORESOURCE_MEM,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 138,
		.end    = 138,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 139,
		.end    = 139,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 140,
		.end    = 140,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 141,
		.end    = 141,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_aux_pcm_device = {
	.name   = "msm_aux_pcm",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_aux_pcm_resources),
	.resource       = msm_aux_pcm_resources,
};

static struct platform_device msm_aictl_device = {
	.name = "audio_interct",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_aictl_resources),
	.resource = msm_aictl_resources,
};

static struct platform_device msm_mi2s_device = {
	.name = "mi2s",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_mi2s_resources),
	.resource = msm_mi2s_resources,
};

static struct platform_device msm_lpa_device = {
	.name = "lpa",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_lpa_resources),
	.resource = msm_lpa_resources,
	.dev		= {
		.platform_data = &lpa_pdata,
	},
};

#define DEC0_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC1_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
 #define DEC2_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
 #define DEC3_FORMAT ((1<<MSM_ADSP_CODEC_MP3)| \
	(1<<MSM_ADSP_CODEC_AAC)|(1<<MSM_ADSP_CODEC_WMA)| \
	(1<<MSM_ADSP_CODEC_WMAPRO)|(1<<MSM_ADSP_CODEC_AMRWB)| \
	(1<<MSM_ADSP_CODEC_AMRNB)|(1<<MSM_ADSP_CODEC_WAV)| \
	(1<<MSM_ADSP_CODEC_ADPCM)|(1<<MSM_ADSP_CODEC_YADPCM)| \
	(1<<MSM_ADSP_CODEC_EVRC)|(1<<MSM_ADSP_CODEC_QCELP))
#define DEC4_FORMAT (1<<MSM_ADSP_CODEC_MIDI)

static unsigned int dec_concurrency_table[] = {
	/* Audio LP */
	0, 0, 0, 0,
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_MODE_LP)|
	(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 1 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),

	 /* Concurrency 2 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 3 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 4 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 5 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_TUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),

	/* Concurrency 6 */
	(DEC4_FORMAT),
	(DEC3_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC2_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC1_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
	(DEC0_FORMAT|(1<<MSM_ADSP_MODE_NONTUNNEL)|(1<<MSM_ADSP_OP_DM)),
};

#define DEC_INFO(name, queueid, decid, nr_codec) { .module_name = name, \
	.module_queueid = queueid, .module_decid = decid, \
	.nr_codec_support = nr_codec}

#define DEC_INSTANCE(max_instance_same, max_instance_diff) { \
	.max_instances_same_dec = max_instance_same, \
	.max_instances_diff_dec = max_instance_diff}

static struct msm_adspdec_info dec_info_list[] = {
	DEC_INFO("AUDPLAY4TASK", 17, 4, 1),  /* AudPlay4BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY3TASK", 16, 3, 11),  /* AudPlay3BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY2TASK", 15, 2, 11),  /* AudPlay2BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY1TASK", 14, 1, 11),  /* AudPlay1BitStreamCtrlQueue */
	DEC_INFO("AUDPLAY0TASK", 13, 0, 11), /* AudPlay0BitStreamCtrlQueue */
};

static struct dec_instance_table dec_instance_list[][MSM_MAX_DEC_CNT] = {
	/* Non Turbo Mode */
	{
		DEC_INSTANCE(4, 3), /* WAV */
		DEC_INSTANCE(4, 3), /* ADPCM */
		DEC_INSTANCE(4, 2), /* MP3 */
		DEC_INSTANCE(0, 0), /* Real Audio */
		DEC_INSTANCE(4, 2), /* WMA */
		DEC_INSTANCE(3, 2), /* AAC */
		DEC_INSTANCE(0, 0), /* Reserved */
		DEC_INSTANCE(0, 0), /* MIDI */
		DEC_INSTANCE(4, 3), /* YADPCM */
		DEC_INSTANCE(4, 3), /* QCELP */
		DEC_INSTANCE(4, 3), /* AMRNB */
		DEC_INSTANCE(1, 1), /* AMRWB/WB+ */
		DEC_INSTANCE(4, 3), /* EVRC */
		DEC_INSTANCE(1, 1), /* WMAPRO */
	},
	/* Turbo Mode */
	{
		DEC_INSTANCE(4, 3), /* WAV */
		DEC_INSTANCE(4, 3), /* ADPCM */
		DEC_INSTANCE(4, 3), /* MP3 */
		DEC_INSTANCE(0, 0), /* Real Audio */
		DEC_INSTANCE(4, 3), /* WMA */
		DEC_INSTANCE(4, 3), /* AAC */
		DEC_INSTANCE(0, 0), /* Reserved */
		DEC_INSTANCE(0, 0), /* MIDI */
		DEC_INSTANCE(4, 3), /* YADPCM */
		DEC_INSTANCE(4, 3), /* QCELP */
		DEC_INSTANCE(4, 3), /* AMRNB */
		DEC_INSTANCE(2, 3), /* AMRWB/WB+ */
		DEC_INSTANCE(4, 3), /* EVRC */
		DEC_INSTANCE(1, 2), /* WMAPRO */
	},
};

static struct msm_adspdec_database msm_device_adspdec_database = {
	.num_dec = ARRAY_SIZE(dec_info_list),
	.num_concurrency_support = (ARRAY_SIZE(dec_concurrency_table) / \
					ARRAY_SIZE(dec_info_list)),
	.dec_concurrency_table = dec_concurrency_table,
	.dec_info_list = dec_info_list,
	.dec_instance_list = &dec_instance_list[0][0],
};

static struct platform_device msm_device_adspdec = {
	.name = "msm_adspdec",
	.id = -1,
	.dev    = {
		.platform_data = &msm_device_adspdec_database
	},
};

static unsigned aux_pcm_gpio_off[] = {
	GPIO_CFG(138, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_DOUT */
	GPIO_CFG(139, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),   /* PCM_DIN  */
	GPIO_CFG(140, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_SYNC */
	GPIO_CFG(141, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),   /* PCM_CLK  */
};

static int __init aux_pcm_gpio_init(void)
{
	config_gpio_table(aux_pcm_gpio_off,
	ARRAY_SIZE(aux_pcm_gpio_off));
	return 0;
}

static void __init audience_gpio_reset(void)
{
	gpio_configure(GLACIER_AUD_A1026_INT,
				GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_configure(GLACIER_AUD_MICPATH_SEL,
				GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_configure(GLACIER_AUD_A1026_RESET,
				GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	gpio_configure(GLACIER_AUD_A1026_WAKEUP,
				GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	pr_info("Configure audio codec gpio for devices without audience.\n");
}

#endif /* CONFIG_MSM7KV2_1X_AUDIO */

static struct i2c_board_info msm_marimba_board_info[] = {
	{
		I2C_BOARD_INFO("marimba", 0xc),
		.platform_data = &marimba_pdata,
	}
};

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
		.latency = 8594,
		.residency = 23740,
	},
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
		.latency = 4594,
		.residency = 23740,
	},
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE] = {
#ifdef CONFIG_MSM_STANDALONE_POWER_COLLAPSE
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 0,
#else /* CONFIG_MSM_STANDALONE_POWER_COLLAPSE */
		.idle_supported = 0,
		.suspend_supported = 0,
		.idle_enabled = 0,
		.suspend_enabled = 0,
#endif /* CONFIG_MSM_STANDALONE_POWER_COLLAPSE */
		.latency = 500,
		.residency = 6000,
	},
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 1,
		.latency = 443,
		.residency = 1098,
	},
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
		.latency = 2,
		.residency = 0,
	},
};

static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start	= 0xA8000000,
		.end	= 0xA8000000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "spidm_channels",
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "spidm_crci",
		.flags  = IORESOURCE_DMA,
	},
};

static struct platform_device qsd_device_spi = {
	.name		= "spi_qsd",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qsd_spi_resources),
	.resource	= qsd_spi_resources,
};

static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "spi_oj",
		.mode		= SPI_MODE_3,
		.bus_num	= 0,
		.chip_select	= 3,
		.max_speed_hz	= 512000,
	}
};


static int msm_qsd_spi_gpio_config(void)
{
	/* Move SPI configuration to OJ initialization to
	   prevent from power leakage through OJ compoment */
/*
	unsigned id;
	id = GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = GPIO_CFG(47, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	id = GPIO_CFG(48, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);

	id = GPIO_CFG(89, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
*/
	return 0;
}

static void msm_qsd_spi_gpio_release(void)
{
	/* Move SPI configuration to OJ initialization to
	   prevent from power leakage through OJ compoment */
/*
	unsigned id;
	id = GPIO_CFG(45, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	gpio_set_value(45, 0);
	id = GPIO_CFG(47, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	gpio_set_value(47, 0);
	id = GPIO_CFG(48, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);

	id = GPIO_CFG(89, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	gpio_set_value(89, 0);
*/
}

#if 0
#define AMDH0_BASE_PHYS		0xAC200000
#define ADMH0_GP_CTL		(ct_adm_base + 0x3D8)
static int msm_qsd_spi_dma_config(void)
{
	void __iomem *ct_adm_base = 0;
	u32 spi_mux = 0;
	int ret = 0;

	ct_adm_base = ioremap(AMDH0_BASE_PHYS, PAGE_SIZE);
	if (!ct_adm_base) {
		pr_err("%s: Could not remap %x\n", __func__, AMDH0_BASE_PHYS);
		return -ENOMEM;
	}

	spi_mux = (ioread32(ADMH0_GP_CTL) & (0x3 << 12)) >> 12;

	qsd_spi_resources[4].start  = DMOV_USB_CHAN;
	qsd_spi_resources[4].end    = DMOV_TSIF_CHAN;

	switch (spi_mux) {
	case (1):
		qsd_spi_resources[5].start  = DMOV_HSUART1_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART1_TX_CRCI;
		break;
	case (2):
		qsd_spi_resources[5].start  = DMOV_HSUART2_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART2_TX_CRCI;
		break;
	case (3):
		qsd_spi_resources[5].start  = DMOV_CE_OUT_CRCI;
		qsd_spi_resources[5].end    = DMOV_CE_IN_CRCI;
		break;
	default:
		ret = -ENOENT;
	}

	iounmap(ct_adm_base);

	return ret;
}
#endif
static struct msm_spi_platform_data qsd_spi_pdata = {
	.max_clock_speed = 26331429,
	.gpio_config  = msm_qsd_spi_gpio_config,
	.gpio_release = msm_qsd_spi_gpio_release,
//	.dma_config = msm_qsd_spi_dma_config,
};

static void __init msm_qsd_spi_init(void)
{
	qsd_device_spi.dev.platform_data = &qsd_spi_pdata;
}

#ifndef CONFIG_MSM_SSBI
static struct pm8058_platform_data pm8058_glacier_data = {
	.init = &glacier_pm8058_gpios_init,
	.num_subdevs = 4,
	.sub_devices = {
		{	.name = "pm8058-gpio",
		},
		{	.name = "pm8058-mpp",
		},
		{	.name = "pm8058-pwm",
		},
		{	.name = "pm8058-nfc",
		},
	},
};

static struct i2c_board_info pm8058_boardinfo[] __initdata = {
	{
		I2C_BOARD_INFO("pm8058-core", 0),
		.irq = MSM_GPIO_TO_INT(GLACIER_PMIC_GPIO_INT),
		.platform_data = &pm8058_glacier_data,
	},
};
#endif

#define OJ_SHUTDOWN            (35)
static void curcial_oj_shutdown(int enable)
{
	/* set suspend state as output low as logic suggest.*/
	unsigned id;

	if (enable) { /*enter early suspend*/
		id = GPIO_CFG(GLACIER_OJ_MOTION, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		gpio_set_value(GLACIER_OJ_MOTION, 0);

		gpio_set_value(GLACIER_OJ_RSTz, 0);

		/* Set SPI to suspend state */
		id = GPIO_CFG(45, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		gpio_set_value(45, 0);
		id = GPIO_CFG(47, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		gpio_set_value(47, 0);
		id = GPIO_CFG(48, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(89, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		gpio_set_value(89, 0);
	} else {
		/* Configure SPI interface */
		id = GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(47, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(48, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
		id = GPIO_CFG(89, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);

		gpio_set_value(GLACIER_OJ_RSTz, 1);
		msleep(4);

		id = GPIO_CFG(GLACIER_OJ_MOTION, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
		msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	}

	gpio_set_value(OJ_SHUTDOWN, 0);
}

static int curcial_oj_poweron(int on)
{
	return 1;
}

static void curcial_oj_adjust_xy(uint8_t *data, int16_t *mSumDeltaX, int16_t *mSumDeltaY)
{
	int8_t 	deltaX;
	int8_t 	deltaY;


	if (data[2] == 0x80)
		data[2] = 0x81;
	if (data[1] == 0x80)
		data[1] = 0x81;
	if (0) {
		deltaX = (-1)*((int8_t) data[2]); /*X=2*/
		deltaY = (1)*((int8_t) data[1]); /*Y=1*/
	} else {
		deltaX = (1)*((int8_t) data[1]);
		deltaY = (1)*((int8_t) data[2]);
	}
	*mSumDeltaX = -((int16_t)deltaX);
	*mSumDeltaY = -((int16_t)deltaY);
}

static struct curcial_oj_platform_data glacier_oj_data = {
	.oj_poweron = curcial_oj_poweron,
	.oj_shutdown = curcial_oj_shutdown,
	.oj_adjust_xy = curcial_oj_adjust_xy,
	.mdelay_time = 7,
	.normal_th = 10,
	.xy_ratio = 15,
	.interval = 20,
	.swap = false,
	.x = -1,
	.y = 1,
	.share_power = false,
	.debugflag = 0,
	.ap_code = false,
	.sht_tbl = {30, 200, 250, 300, 350, 400, 450},
	.pxsum_tbl = {0, 0, 70, 80, 90, 100, 110},
	.degree = 7,
	.Xsteps = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		10, 10, 10, 10, 10, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
	.Ysteps = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
		10, 10, 10, 10, 10, 9, 9, 9, 9, 9,
		9, 9, 9, 9, 9, 9, 9, 9, 9, 9},
	.irq_gpio = 26,
	.rst_gpio = GLACIER_OJ_RSTz,
};

static struct platform_device glacier_oj = {
	.name = CURCIAL_OJ_NAME,
	.id = -1,
	.dev = {
		.platform_data	= &glacier_oj_data,
	}
};

static struct platform_device glacier_rfkill = {
	.name = "glacier_rfkill",
	.id = -1,
};

static struct htc_fmtx_platform_data htc_fmtx_data = {
	.switch_pin	= GLACIER_WFM_ANT_SW,

};

static struct platform_device glacier_fmtx_rfkill = {
	.name = "htc_fmtx_rfkill",
	.id = -1,
	.dev = {
		.platform_data = &htc_fmtx_data,
	},
};

#ifdef CONFIG_ION_MSM
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct ion_co_heap_pdata co_ion_pdata = {
	.adjacent_mem_id = INVALID_HEAP_ID,
	.align = PAGE_SIZE,
};
#endif

/**
 * These heaps are listed in the order they will be allocated.
 * Don't swap the order unless you know what you are doing!
 */
struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
        .heaps = {
		{
			.id	= ION_SYSTEM_HEAP_ID,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= ION_VMALLOC_HEAP_NAME,
		},
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		/* PMEM_ADSP = CAMERA */
		{
			.id	= ION_CAMERA_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_CAMERA_HEAP_NAME,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
			.base = MSM_PMEM_ADSP_BASE,
			.size = MSM_PMEM_ADSP_SIZE,
		},
		/* PMEM_AUDIO */
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
			.base = PMEM_KERNEL_EBI1_BASE,
			.size = PMEM_KERNEL_EBI1_SIZE,
		},
		/* PMEM_MDP = SF */
		{
			.id	= ION_SF_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_SF_HEAP_NAME,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
			.base = MSM_PMEM_MDP_BASE,
			.size = MSM_ION_SF_SIZE,
		},
#endif
	}
};

#if 0
static struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
	.heaps = glacier_heaps,
};
#endif

static struct platform_device ion_dev = {
	.name = "ion-msm",
	.id = 1,
	.dev = { .platform_data = &ion_pdata },
};
#endif

static uint32_t fl_gpio_table[] = {
	GPIO_CFG(GLACIER_GPIO_FLASHLIGHT_TORCH, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GLACIER_GPIO_FLASHLIGHT_FLASH, 0,
					GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),

};

static void config_glacier_flashlight_gpios(void)
{
	config_gpio_table(fl_gpio_table, ARRAY_SIZE(fl_gpio_table));
}

static void config_glacier_emmc_gpios(void)
{
	uint32_t emmc_gpio_table[] = {
		GPIO_CFG(GLACIER_GPIO_EMMC_RST, 0, GPIO_CFG_OUTPUT,
						GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	};
	config_gpio_table(emmc_gpio_table,
		ARRAY_SIZE(emmc_gpio_table));
}

static struct flashlight_platform_data glacier_flashlight_data = {
	.gpio_init		= config_glacier_flashlight_gpios,
	.torch			= GLACIER_GPIO_FLASHLIGHT_TORCH,
	.flash			= GLACIER_GPIO_FLASHLIGHT_FLASH,
	.flash_duration_ms	= 600,
};

static struct platform_device glacier_flashlight_device = {
	.name = FLASHLIGHT_NAME,
	.dev		= {
		.platform_data	= &glacier_flashlight_data,
	},
};

#ifdef CONFIG_SERIAL_MSM_HS
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
        .wakeup_irq = -1,
        .inject_rx_on_wakeup = 0,
        .exit_lpm_cb = bcm_bt_lpm_exit_lpm_locked,
};

static struct bcm_bt_lpm_platform_data bcm_bt_lpm_pdata = {
        .gpio_wake = GLACIER_GPIO_BT_CHIP_WAKE,
        .gpio_host_wake = GLACIER_GPIO_BT_HOST_WAKE,
        .request_clock_off_locked = msm_hs_request_clock_off_locked,
        .request_clock_on_locked = msm_hs_request_clock_on_locked,
};

struct platform_device glacier_bcm_bt_lpm_device = {
        .name = "bcm_bt_lpm",
        .id = 0,
        .dev = {
                .platform_data = &bcm_bt_lpm_pdata,
        },
};
#endif

static struct resource ram_console_resources[] = {
	{
		.start	= MSM_RAM_CONSOLE_BASE,
		.end	= MSM_RAM_CONSOLE_BASE + MSM_RAM_CONSOLE_SIZE -1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};

static struct platform_device *devices[] __initdata = {
	&ram_console_device,
	&msm_device_uart2,
#ifdef CONFIG_MSM_PROC_COMM_REGULATOR
	&msm_proccomm_regulator_dev,
#endif
#ifdef CONFIG_SERIAL_MSM_HS
	&msm_device_uart_dm1,
	&glacier_bcm_bt_lpm_device,
#endif
	&msm_device_smd,
	&msm_device_dmov,
#ifdef CONFIG_DOCK_DETECT
	&dock_detect_device,
#endif
#ifdef CONFIG_USB_MSM_OTG_72K
	&msm_device_otg,
#endif
#ifdef CONFIG_USB_GADGET
	&msm_device_gadget_peripheral,
#endif
	&glacier_rfkill,
	&glacier_fmtx_rfkill,
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi6,
	&msm_device_ssbi7,
#endif
	&qsd_device_spi,
	&msm_device_i2c,
	&msm_device_i2c_2,
	&qup_device_i2c,
	&htc_battery_pdev,
#ifdef CONFIG_INPUT_CAPELLA_CM3602
	&capella_cm3602,
#endif
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	&msm_aictl_device,
	&msm_mi2s_device,
	&msm_lpa_device,
	&msm_device_adspdec,
#endif
#ifdef CONFIG_MSM_KGSL
	&msm_kgsl_3d0,
#ifdef CONFIG_MSM_KGSL_2D
	&msm_kgsl_2d0,
#endif
#endif
#ifdef CONFIG_MSM_VIDC_720P
	&msm_device_vidc_720p,
#endif
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	&msm_aux_pcm_device,
#endif
#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&glacier_flashlight_device,
#ifdef CONFIG_ION_MSM
	&ion_dev,
#endif
};

static struct msm_gpio msm_i2c_gpios_hw[] = {
	{ GPIO_CFG(70, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "i2c_scl" },
	{ GPIO_CFG(71, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "i2c_sda" },
};

static struct msm_gpio msm_i2c_gpios_io[] = {
	{ GPIO_CFG(70, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "i2c_scl" },
	{ GPIO_CFG(71, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "i2c_sda" },
};

static struct msm_gpio qup_i2c_gpios_io[] = {
	{ GPIO_CFG(16, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), "qup_scl" },
	{ GPIO_CFG(17, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), "qup_sda" },
};
static struct msm_gpio qup_i2c_gpios_hw[] = {
	{ GPIO_CFG(16, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), "qup_scl" },
	{ GPIO_CFG(17, 2, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), "qup_sda" },
};

static void
msm_i2c_gpio_config(int adap_id, int config_type)
{
	struct msm_gpio *msm_i2c_table;

	/* Each adapter gets 2 lines from the table */
	if (adap_id > 0)
		return;
	if (config_type)
		msm_i2c_table = &msm_i2c_gpios_hw[adap_id*2];
	else
		msm_i2c_table = &msm_i2c_gpios_io[adap_id*2];
	msm_gpios_enable(msm_i2c_table, 2);
}
/*This needs to be enabled only for OEMS*/
#ifndef CONFIG_QUP_EXCLUSIVE_TO_CAMERA
static struct vreg *qup_vreg;
#endif
static void
qup_i2c_gpio_config(int adap_id, int config_type)
{
	int rc = 0;
	struct msm_gpio *qup_i2c_table;
	/* Each adapter gets 2 lines from the table */
	if (adap_id != 4)
		return;
	if (config_type)
		qup_i2c_table = qup_i2c_gpios_hw;
	else
		qup_i2c_table = qup_i2c_gpios_io;
	rc = msm_gpios_enable(qup_i2c_table, 2);
	if (rc < 0)
		printk(KERN_ERR "QUP GPIO enable failed: %d\n", rc);
	/*This needs to be enabled only for OEMS*/
#ifndef CONFIG_QUP_EXCLUSIVE_TO_CAMERA
	if (qup_vreg) {
		int rc = vreg_set_level(qup_vreg, 1800);
		if (rc) {
			pr_err("%s: vreg LVS1 set level failed (%d)\n",
			__func__, rc);
		}
		rc = vreg_enable(qup_vreg);
		if (rc) {
			pr_err("%s: vreg_enable() = %d \n",
			__func__, rc);
		}
	}
#endif
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.pri_clk = 70,
	.pri_dat = 71,
	.rmutex  = 1,
	.rsl_id = "D:I2C02000021",
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (msm_gpios_request(msm_i2c_gpios_hw, ARRAY_SIZE(msm_i2c_gpios_hw)))
		pr_err("failed to request I2C gpios\n");

	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

static struct msm_i2c_platform_data msm_i2c_2_pdata = {
	.clk_freq = 100000,
	.rmutex   = 1,
	.rsl_id = "D:I2C02000022",
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_2_init(void)
{
	msm_device_i2c_2.dev.platform_data = &msm_i2c_2_pdata;
}

static struct msm_i2c_platform_data qup_i2c_pdata = {
	.clk_freq = 384000,
	.msm_i2c_config_gpio = qup_i2c_gpio_config,
};

static void __init qup_device_i2c_init(void)
{
	qup_device_i2c.dev.platform_data = &qup_i2c_pdata;
	/*This needs to be enabled only for OEMS*/
#ifndef CONFIG_QUP_EXCLUSIVE_TO_CAMERA
	qup_vreg = vreg_get(NULL, "lvsw1");
	if (IS_ERR(qup_vreg)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(qup_vreg));
	}
#endif
}

#ifdef CONFIG_I2C_SSBI
static struct msm_i2c_platform_data msm_i2c_ssbi6_pdata = {
	.rsl_id = "D:PMIC_SSBI"
};

static struct msm_i2c_platform_data msm_i2c_ssbi7_pdata = {
	.rsl_id = "D:CODEC_SSBI"
};
#endif

static unsigned glacier_perf_acpu_table[] = {
	245000000,
	768000000,
	1024000000,
};

static struct perflock_platform_data glacier_perflock_data = {
	.perf_acpu_table = glacier_perf_acpu_table,
	.table_size = ARRAY_SIZE(glacier_perf_acpu_table),
};
#ifdef CONFIG_MSM_SSBI
static int glacier_pmic_init(struct device *dev)
{
	struct pm8058_chip *pm_chip = NULL;

	glacier_pm8058_gpios_init(pm_chip);
	return 0;
}
static struct pm8058_platform_data glacier_pm8058_pdata = {
	.irq_base	= PM8058_FIRST_IRQ,
	.gpio_base	= FIRST_BOARD_GPIO,
	.init		= glacier_pmic_init,
	.num_subdevs = 3,
	.sub_devices_htc = {
		{	.name = "pm8058-gpio",
		},
		{	.name = "pm8058-mpp",
		},
		{	.name = "pm8058-pwm",
		},
	},
};

static struct msm_ssbi_platform_data glacier_ssbi_pmic_pdata = {
	.slave		= {
		.name		= "pm8058-core",
		.irq		= MSM_GPIO_TO_INT(GLACIER_PMIC_GPIO_INT),
		.platform_data	= &glacier_pm8058_pdata,
	},
	.rspinlock_name	= "D:PMIC_SSBI",
};

static int __init glacier_ssbi_pmic_init(void)
{
	int ret;
	u32 id;

	pr_info("%s()\n", __func__);
	id = GPIO_CFG(GLACIER_PMIC_GPIO_INT, 1, GPIO_CFG_INPUT,
			   GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
	ret = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	if (ret)
		pr_err("%s: gpio %d cfg failed\n", __func__,
		       GLACIER_PMIC_GPIO_INT);

	ret = gpiochip_reserve(glacier_pm8058_pdata.gpio_base,
			       PM8058_GPIOS);
	WARN(ret, "can't reserve pm8058 gpios. badness will ensue...\n");
	msm_device_ssbi_pmic.dev.platform_data = &glacier_ssbi_pmic_pdata;
	return platform_device_register(&msm_device_ssbi_pmic);
}
#endif

static struct msm_spm_platform_data msm_spm_data __initdata = {
	.reg_base_addr = MSM_SAW_BASE,

	.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x05,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x18,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x00006666,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFF000666,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x03,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

	.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
	.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
	.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

	.awake_vlevel = 0xF2,
	.retention_vlevel = 0xE0,
	.collapse_vlevel = 0x72,
	.retention_mid_vlevel = 0xE0,
	.collapse_mid_vlevel = 0xE0,

	.vctl_timeout_us = 50,
};

static void __init glacier_init(void)
{
	int ret = 0;
	uint32_t soc_version = 0;

	printk("glacier_init() reglacier=%d\n", system_rev);
	printk(KERN_INFO "%s: microp version = %s\n", __func__, microp_ver);

	soc_version = socinfo_get_version();

	msm_clock_init(&msm7x30_clock_init_data);

#ifdef CONFIG_BT
	bt_export_bd_address();
#endif

#if defined(CONFIG_MSM_SERIAL_DEBUGGER)
	if (!opt_disable_uart2)
		msm_serial_debug_init(MSM_UART2_PHYS, INT_UART2,
		&msm_device_uart2.dev, 23, MSM_GPIO_TO_INT(GLACIER_GPIO_UART2_RX));
#endif

#ifdef CONFIG_SERIAL_MSM_HS
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
#endif

#ifdef CONFIG_USB_MSM_OTG_72K
	if (SOCINFO_VERSION_MAJOR(soc_version) >= 2 &&
			SOCINFO_VERSION_MINOR(soc_version) >= 1) {
		pr_debug("%s: SOC Version:2.(1 or more)\n", __func__);
		msm_otg_pdata.ldo_set_voltage = 0;
	}

	msm_device_otg.dev.platform_data = &msm_otg_pdata;
#ifdef CONFIG_USB_GADGET
	msm_device_gadget_peripheral.dev.platform_data = &msm_gadget_pdata;
#endif
#endif
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_device_i2c_init();
	msm_device_i2c_2_init();
	qup_device_i2c_init();
	msm7x30_init_marimba();
#ifdef CONFIG_MSM7KV2_1X_AUDIO
	msm_snddev_init();
	aux_pcm_gpio_init();
#endif
	i2c_register_board_info(2, msm_marimba_board_info,
			ARRAY_SIZE(msm_marimba_board_info));
	msm_spm_init(&msm_spm_data, 1);
	acpuclk_init(&acpuclk_7x30_soc_data);
	perflock_init(&glacier_perflock_data);

	msm_init_pmic_vibrator(3000);
#ifdef CONFIG_MICROP_COMMON
	glacier_microp_init();
#endif
	platform_add_devices(msm_footswitch_devices,
			     msm_num_footswitch_devices);
	platform_add_devices(devices, ARRAY_SIZE(devices));
#ifdef CONFIG_USB_EHCI_MSM_72K
	msm_add_host(0, &msm_usb_host_pdata);
#endif
#ifdef CONFIG_MSMB_CAMERA
	glacier_init_cam();
#endif

#ifdef CONFIG_USB_G_ANDROID
	glacier_add_usb_devices();
#endif

	if (board_emmc_boot()) {
		rmt_storage_add_ramfs();
		create_proc_read_entry("emmc", 0, NULL, emmc_partition_read_proc, NULL);
	} else
		platform_device_register(&msm_device_nand);

#ifdef CONFIG_MSM_SSBI
	glacier_ssbi_pmic_init();
#endif

	config_glacier_emmc_gpios();	/* for emmc gpio reset test */
	ret = glacier_init_mmc(system_rev);
	if (ret != 0)
		pr_crit("%s: Unable to initialize MMC\n", __func__);

	msm_qsd_spi_init();
	spi_register_board_info(msm_spi_board_info, ARRAY_SIZE(msm_spi_board_info));
	glacier_init_panel();
	if ((system_rev >= 0x80) || (engineerid & 0x2))
		glacier_oj_data.ap_code = true;
	platform_device_register(&glacier_oj);
	if (!panel_type) {
		glacier_ts_atmel_data[0].config_T9[9] = 5;
		glacier_ts_atmel_data[0].abs_y_max = 954;
	}
	register_i2c_devices();
#ifdef CONFIG_I2C_SSBI
	msm_device_ssbi6.dev.platform_data = &msm_i2c_ssbi6_pdata;

	msm_device_ssbi7.dev.platform_data = &msm_i2c_ssbi7_pdata;
#endif
	/*Bit2: 0: with audience. 1: without audience*/
	if (engineerid & 0x4)
		audience_gpio_reset();
	else
		i2c_register_board_info(0, i2c_a1026_devices, ARRAY_SIZE(i2c_a1026_devices));

	glacier_audio_init();
	glacier_init_keypad();
	glacier_wifi_init();
}

unsigned int glacier_get_engineerid(void)
{
	return engineerid;
}

static void __init glacier_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	int mem = parse_tag_memsize((const struct tag *)tags);

	printk(KERN_INFO "[%s]\n", __func__);
	engineerid = parse_tag_engineerid(tags);

	mi->nr_banks = 2;
	mi->bank[0].start = MSM_LINUX_BASE1;
	mi->bank[0].size = MSM_LINUX_SIZE1;
	mi->bank[1].start = MSM_LINUX_BASE2;
	mi->bank[1].size = MSM_LINUX_SIZE2;

	if (mem == 768)
		mi->bank[0].size += MSM_MEM_256MB_OFFSET;
}

static void __init glacier_map_io(void)
{
	printk(KERN_INFO "[%s]\n", __func__);

	msm_map_common_io();
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
}

extern struct sys_timer msm_timer;

MACHINE_START(GLACIER, "glacier")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io        = MSM_DEBUG_UART_PHYS,
	.io_pg_offst    = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params	= 0x04000100,
	.fixup		= glacier_fixup,
	.map_io		= glacier_map_io,
	.init_irq	= msm_init_irq,
	.init_machine	= glacier_init,
	.timer		= &msm_timer,
MACHINE_END
