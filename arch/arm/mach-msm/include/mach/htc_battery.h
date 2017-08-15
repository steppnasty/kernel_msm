/*
 * Copyright (C) 2007 HTC Incorporated
 * Author: Jay Tu (jay_tu@htc.com)
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
#ifndef _HTC_BATTERY_H_
#define _HTC_BATTERY_H_
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/rtc.h>
#include <mach/msm_hsusb.h>

#define BATT_LOG(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_INFO "[BATT] " x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

#define BATT_ERR(x...) do { \
struct timespec ts; \
struct rtc_time tm; \
getnstimeofday(&ts); \
rtc_time_to_tm(ts.tv_sec, &tm); \
printk(KERN_ERR "[BATT] err:" x); \
printk(" at %lld (%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n", \
ktime_to_ns(ktime_get()), tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec); \
} while (0)

/* information about the system we're running on */
extern unsigned int system_rev;

#ifdef CONFIG_ARCH_MSM8X60
#define ADC_REPLY_ARRAY_SIZE		5

/* ioctl define */
#define HTC_BATT_IOCTL_MAGIC		0xba

#define DEBUG_LOG_LENGTH		1024

#define HTC_BATT_IOCTL_READ_SOURCE \
	_IOR(HTC_BATT_IOCTL_MAGIC, 1, unsigned int)
#define HTC_BATT_IOCTL_SET_BATT_ALARM \
	_IOW(HTC_BATT_IOCTL_MAGIC, 2, unsigned int)
#define HTC_BATT_IOCTL_GET_ADC_VREF \
	_IOR(HTC_BATT_IOCTL_MAGIC, 3, unsigned int[ADC_REPLY_ARRAY_SIZE])
#define HTC_BATT_IOCTL_GET_ADC_ALL \
	_IOR(HTC_BATT_IOCTL_MAGIC, 4, struct battery_adc_reply)
#define HTC_BATT_IOCTL_CHARGER_CONTROL \
	_IOW(HTC_BATT_IOCTL_MAGIC, 5, unsigned int)
#define HTC_BATT_IOCTL_UPDATE_BATT_INFO \
	_IOW(HTC_BATT_IOCTL_MAGIC, 6, struct battery_info_reply)
#define HTC_BATT_IOCTL_BATT_DEBUG_LOG \
	_IOW(HTC_BATT_IOCTL_MAGIC, 7, char[DEBUG_LOG_LENGTH])
#define HTC_BATT_IOCTL_SET_VOLTAGE_ALARM \
	_IOW(HTC_BATT_IOCTL_MAGIC, 8, struct battery_vol_alarm)
#define HTC_BATT_IOCTL_SET_ALARM_TIMER_FLAG \
	_IOW(HTC_BATT_IOCTL_MAGIC, 9, unsigned int)

#define REGULAR_BATTERRY_TIMER		"regular_timer"
#define CABLE_DETECTION			"cable"
#define CHARGER_IC_INTERRUPT		"charger_int"

#define XOADC_MPP			0
#define PM_MPP_AIN_AMUX			1

#define MBAT_IN_LOW_TRIGGER		0
#define MBAT_IN_HIGH_TRIGGER		1

enum {
	HTC_BATT_DEBUG_UEVT = 1U << 1,
	HTC_BATT_DEBUG_USER_QUERY = 1U << 2,
	HTC_BATT_DEBUG_USB_NOTIFY = 1U << 3,
	HTC_BATT_DEBUG_FULL_LOG = 1U << 4,
};

struct battery_adc_reply {
	u32 adc_voltage[ADC_REPLY_ARRAY_SIZE];
	u32 adc_current[ADC_REPLY_ARRAY_SIZE];
	u32 adc_temperature[ADC_REPLY_ARRAY_SIZE];
	u32 adc_battid[ADC_REPLY_ARRAY_SIZE];
};

struct battery_vol_alarm {
	int lower_threshold;
	int upper_threshold;
	int enable;
};

struct battery_info_reply {
	u32 batt_vol;		/* Battery voltage from ADC */
	u32 batt_id;		/* Battery ID from ADC */
	s32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	s32 batt_current;	/* Battery current from ADC */
	u32 batt_discharg_current;
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
	u32 full_level;		/* Full Level */
	u32 over_vchg;		/* 0:normal, 1:over voltage charger */
	s32 temp_fault;
	u32 batt_state;
};
#else
enum {
	HTC_BATT_DEBUG_M2A_RPC = 1U << 0,
	HTC_BATT_DEBUG_A2M_RPC = 1U << 1,
	HTC_BATT_DEBUG_UEVT = 1U << 2,
	HTC_BATT_DEBUG_USER_QUERY = 1U << 3,
	HTC_BATT_DEBUG_USB_NOTIFY = 1U << 4,
	HTC_BATT_DEBUG_SMEM = 1U << 5,
};

struct battery_info_reply {
	u32 batt_id;		/* Battery ID from ADC */
	u32 batt_vol;		/* Battery voltage from ADC */
	s32 batt_temp;		/* Battery Temperature (C) from formula and ADC */
	s32 batt_current;	/* Battery current from ADC */
	u32 level;		/* formula */
	u32 charging_source;	/* 0: no cable, 1:usb, 2:AC */
	u32 charging_enabled;	/* 0: Disable, 1: Enable */
	u32 full_bat;		/* Full capacity of battery (mAh) */
	u32 full_level;		/* Full Level */
	u32 over_vchg;		/* 0:normal, 1:over voltage charger */
	u32 force_high_power_charging;
	s32 eval_current;	/* System loading current from ADC */
};
#endif /* CONFIG_ARCH_MSM8X60 */

struct mpp_config_data {
	u32 vol[2];
	u32 curr[2];
	u32 temp[2];
	u32 battid[2];
};

enum batt_ctl_t {
	DISABLE = 0,
	ENABLE_SLOW_CHG,
	ENABLE_FAST_CHG,
	ENABLE_SUPER_CHG,
	ENABLE_WIRELESS_CHG,
	CHARGER_CHK,
	TOGGLE_CHARGER,
	ENABLE_MIN_TAPER,
	DISABLE_MIN_TAPER
};

/* This order is the same as htc_power_supplies[]
 * And it's also the same as htc_cable_status_update()
 */
enum charger_type_t {
	CHARGER_UNKNOWN = -1,
	CHARGER_BATTERY = 0,
	CHARGER_USB,
	CHARGER_AC,
	CHARGER_9V_AC,
	CHARGER_WIRELESS
};

enum power_supplies_type {
	BATTERY_SUPPLY,
	USB_SUPPLY,
	AC_SUPPLY,
	WIRELESS_SUPPLY
};

enum charger_control_flag {
	STOP_CHARGER = 0,
	ENABLE_CHARGER,
	ENABLE_LIMIT_CHARGER,
	DISABLE_LIMIT_CHARGER,
	END_CHARGER
};

enum {
	GUAGE_NONE,
	GUAGE_MODEM,
};

enum {
	LINEAR_CHARGER,
	SWITCH_CHARGER_TPS65200,
};

enum {
	BATT_TIMER_WAKE_LOCK = 0,
	BATT_IOCTL_WAKE_LOCK,
};

enum {
	BATT_ID = 0,
	BATT_VOL,
	BATT_TEMP,
	BATT_CURRENT,
	CHARGING_SOURCE,
	CHARGING_ENABLED,
	FULL_BAT,
	OVER_VCHG,
	BATT_STATE,
	FORCE_HIGH_POWER_CHARGING = BATT_STATE,
};

struct htc_battery_platform_data {
	int gpio_mbat_in;
	int gpio_mbat_in_trigger_level;
	int gpio_mchg_en_n;
	int gpio_iset;
	int gpio_adp_9v;
	int guage_driver;
	int m2a_cable_detect;
	int charger;
	int (*is_wireless_charger)(void);
	struct mpp_config_data mpp_data;
};

/* START: add USB connected notify function */
struct htc_usb_status_notifier {
	struct list_head notifier_link;
	const char *name;
	void (*func)(int cable_type);
};

int htc_usb_register_notifier(struct htc_usb_status_notifier *);
int htc_usb_get_connect_type(void);
int htc_battery_charger_disable(void);
void htc_usb_chg_connected(enum chg_type chg_type);

struct htc_battery_core {
	int (*func_show_batt_attr)(struct device_attribute *attr, char *buf);
	int (*func_get_battery_info)(struct battery_info_reply *buffer);
	int (*func_charger_control)(enum charger_control_flag);
	void (*func_set_full_level)(int full_level);
};
#ifdef CONFIG_HTC_BATT_CORE
extern int htc_battery_core_update_changed(void);
extern int htc_battery_core_register(struct device *dev,
	struct htc_battery_core *htc_battery);
#else
static inline int htc_battery_core_update_changed(void)
{
	return 0;
}

static inline int htc_battery_core_register(struct device *dev,
	struct htc_battery_core *htc_battery)
{
	return 0;
}
#endif

#endif
