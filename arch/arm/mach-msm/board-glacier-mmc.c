/* linux/arch/arm/mach-msm/board-glacier-mmc.c
 *
 * Copyright (C) 2008 HTC Corporation.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>

#include <asm/gpio.h>
#include <asm/io.h>

#include <mach/vreg.h>
#include <mach/htc_pwrsink.h>

#include <asm/mach/mmc.h>

#include "devices.h"
#include "board-glacier.h"
#include "proc_comm.h"

#define GLACIER_SDMC_CD_N_SYS	PM8058_GPIO_PM_TO_SYS(GLACIER_SDMC_CD_N)

extern int msm_add_sdcc(unsigned int controller, struct mmc_platform_data *plat,
			unsigned int stat_irq, unsigned long stat_irq_flags);

/* ---- SDCARD ---- */

static uint32_t sdcard_on_gpio_table[] = {
	GPIO_CFG(58, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), /* CLK */
	GPIO_CFG(59, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* CMD */
	GPIO_CFG(60, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* DAT3 */
	GPIO_CFG(61, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* DAT2 */
	GPIO_CFG(62, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* DAT1 */
	GPIO_CFG(63, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* DAT0 */
};

static uint32_t sdcard_off_gpio_table[] = {
	GPIO_CFG(58, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* CLK */
	GPIO_CFG(59, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* CMD */
	GPIO_CFG(60, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* DAT3 */
	GPIO_CFG(61, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* DAT2 */
	GPIO_CFG(62, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* DAT1 */
	GPIO_CFG(63, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* DAT0 */
};

static uint opt_disable_sdcard;

static uint32_t movinand_on_gpio_table[] = {
	GPIO_CFG(64, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* CLK */
	GPIO_CFG(65, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* CMD */
	GPIO_CFG(66, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT3 */
	GPIO_CFG(67, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT2 */
	GPIO_CFG(68, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT1 */
	GPIO_CFG(69, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT0 */
	GPIO_CFG(115, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT4 */
	GPIO_CFG(114, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT5 */
	GPIO_CFG(113, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT6 */
	GPIO_CFG(112, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* DAT7 */
};
static int __init glacier_disablesdcard_setup(char *str)
{
	int cal = simple_strtol(str, NULL, 0);

	opt_disable_sdcard = cal;
	return 1;
}

__setup("board_glacier.disable_sdcard=", glacier_disablesdcard_setup);

static struct vreg *vreg_sdslot;	/* SD slot power */

struct mmc_vdd_xlat {
	int mask;
	int level;
};

static struct mmc_vdd_xlat mmc_vdd_table[] = {
	{ MMC_VDD_28_29,	2850 },
	{ MMC_VDD_29_30,	2900 },
};

static unsigned int sdslot_vdd = 0xffffffff;
static unsigned int sdslot_vreg_enabled;

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

static uint32_t glacier_sdslot_switchvdd(struct device *dev, unsigned int vdd)
{
	int i;

	BUG_ON(!vreg_sdslot);

	if (vdd == sdslot_vdd)
		return 0;

	sdslot_vdd = vdd;

	if (vdd == 0) {
		printk(KERN_INFO "%s: Disabling SD slot power\n", __func__);
		config_gpio_table(sdcard_off_gpio_table,
				  ARRAY_SIZE(sdcard_off_gpio_table));
		vreg_disable(vreg_sdslot);
		sdslot_vreg_enabled = 0;
		return 0;
	}

	if (!sdslot_vreg_enabled) {
		mdelay(5);
		vreg_enable(vreg_sdslot);
		udelay(500);
		config_gpio_table(sdcard_on_gpio_table,
				  ARRAY_SIZE(sdcard_on_gpio_table));
		sdslot_vreg_enabled = 1;
	}

	for (i = 0; i < ARRAY_SIZE(mmc_vdd_table); i++) {
		if (mmc_vdd_table[i].mask == (1 << vdd)) {
			printk(KERN_INFO "%s: Setting level to %u\n",
				__func__, mmc_vdd_table[i].level);
			vreg_set_level(vreg_sdslot, mmc_vdd_table[i].level);
			return 0;
		}
	}

	printk(KERN_ERR "%s: Invalid VDD %d specified\n", __func__, vdd);
	return 0;
}

static unsigned int glacier_sdslot_status(struct device *dev)
{
	unsigned int status;

	status = (unsigned int) gpio_get_value(GLACIER_SDMC_CD_N_SYS);
	return (!status);
}

#define GLACIER_MMC_VDD		(MMC_VDD_28_29 | MMC_VDD_29_30)

static unsigned int glacier_sdslot_type = MMC_TYPE_SD;

static struct mmc_platform_data glacier_sdslot_data = {
	.ocr_mask	= GLACIER_MMC_VDD,
	.status_irq	= MSM_GPIO_TO_INT(GLACIER_SDMC_CD_N_SYS),
	.status		= glacier_sdslot_status,
	.translate_vdd	= glacier_sdslot_switchvdd,
	.slot_type	= &glacier_sdslot_type,
	.dat0_gpio	= 63,
};

static unsigned int glacier_emmcslot_type = MMC_TYPE_MMC;
static struct mmc_platform_data glacier_movinand_data = {
	.ocr_mask	= GLACIER_MMC_VDD,
	.slot_type	= &glacier_emmcslot_type,
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
};

/* ---- WIFI ---- */

static uint32_t wifi_on_gpio_table[] = {
	GPIO_CFG(116, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT3 */
	GPIO_CFG(117, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT2 */
	GPIO_CFG(118, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT1 */
	GPIO_CFG(119, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT0 */
	GPIO_CFG(111, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), /* CMD */
	GPIO_CFG(110, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), /* CLK */
	GPIO_CFG(147, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* WLAN IRQ */
};

static uint32_t wifi_off_gpio_table[] = {
	GPIO_CFG(116, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT3 */
	GPIO_CFG(117, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT2 */
	GPIO_CFG(118, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT1 */
	GPIO_CFG(119, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* DAT0 */
	GPIO_CFG(111, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA), /* CMD */
	GPIO_CFG(110, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* CLK */
	GPIO_CFG(147, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* WLAN IRQ */
};

/* BCM4329 returns wrong sdio_vsn(1) when we read cccr,
 * we use predefined value (sdio_vsn=2) here to initial sdio driver well
 */
static struct embedded_sdio_data glacier_wifi_emb_data = {
	.cccr	= {
		.sdio_vsn	= 2,
		.multi_block	= 1,
		.low_speed	= 0,
		.wide_bus	= 0,
		.high_power	= 1,
		.high_speed	= 1,
	},
};

static void (*wifi_status_cb)(int card_present, void *dev_id);
static void *wifi_status_cb_devid;

static int
glacier_wifi_status_register(void (*callback)(int card_present, void *dev_id),
				void *dev_id)
{
	if (wifi_status_cb)
		return -EAGAIN;
	wifi_status_cb = callback;
	wifi_status_cb_devid = dev_id;
	return 0;
}

static int glacier_wifi_cd;	/* WiFi virtual 'card detect' status */

static unsigned int glacier_wifi_status(struct device *dev)
{
	return glacier_wifi_cd;
}

static struct mmc_platform_data glacier_wifi_data = {
	.ocr_mask		= MMC_VDD_20_21,
	.status			= glacier_wifi_status,
	.register_status_notify	= glacier_wifi_status_register,
	.embedded_sdio		= &glacier_wifi_emb_data,
};

int glacier_wifi_set_carddetect(int val)
{
	printk(KERN_INFO "%s: %d\n", __func__, val);
	glacier_wifi_cd = val;
	if (wifi_status_cb)
		wifi_status_cb(val, wifi_status_cb_devid);
	else
		printk(KERN_WARNING "%s: Nobody to notify\n", __func__);
	return 0;
}
EXPORT_SYMBOL(glacier_wifi_set_carddetect);

int glacier_wifi_power(int on)
{
	printk(KERN_INFO "%s: %d\n", __func__, on);

	if (on) {
		config_gpio_table(wifi_on_gpio_table,
				  ARRAY_SIZE(wifi_on_gpio_table));
	} else {
		config_gpio_table(wifi_off_gpio_table,
				  ARRAY_SIZE(wifi_off_gpio_table));
	}

	gpio_set_value(GLACIER_GPIO_WIFI_SHUTDOWN_N, on); /* WIFI_SHUTDOWN */

	mdelay(120);
	return 0;
}
EXPORT_SYMBOL(glacier_wifi_power);

int glacier_wifi_reset(int on)
{
	printk(KERN_INFO "%s: do nothing\n", __func__);
	return 0;
}

int __init glacier_init_mmc(unsigned int sys_rev)
{
	uint32_t id;
	wifi_status_cb = NULL;
	sdslot_vreg_enabled = 0;

	printk(KERN_INFO "glacier: %s\n", __func__);
	/* SDC2: MoviNAND */
	config_gpio_table(movinand_on_gpio_table,
			  ARRAY_SIZE(movinand_on_gpio_table));
	msm_add_sdcc(2, &glacier_movinand_data, 0, 0);

	/* initial WIFI_SHUTDOWN# */
	id = GPIO_CFG(GLACIER_GPIO_WIFI_SHUTDOWN_N, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX, &id, 0);
	gpio_set_value(GLACIER_GPIO_WIFI_SHUTDOWN_N, 0);

	msm_add_sdcc(3, &glacier_wifi_data, 0, 0);

	/* SDCC4: SD card */
	if (opt_disable_sdcard) {
		printk(KERN_INFO "glacier: SD-Card interface disabled\n");
		goto done;
	}

	vreg_sdslot = vreg_get(0, "gp10");
	if (IS_ERR(vreg_sdslot))
		return PTR_ERR(vreg_sdslot);

	set_irq_wake(MSM_GPIO_TO_INT(GLACIER_SDMC_CD_N_SYS), 1);

	msm_add_sdcc(4, &glacier_sdslot_data,
		MSM_GPIO_TO_INT(GLACIER_SDMC_CD_N_SYS),
		IORESOURCE_IRQ_LOWEDGE | IORESOURCE_IRQ_HIGHEDGE);

done:

	/* reset eMMC for write protection test */
	gpio_set_value(GLACIER_GPIO_EMMC_RST, 0);	/* this should not work!!! */
	udelay(100);
	gpio_set_value(GLACIER_GPIO_EMMC_RST, 1);

	return 0;
}
