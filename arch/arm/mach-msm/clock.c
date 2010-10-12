/* arch/arm/mach-msm/clock.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/pm_qos.h>

#include <asm/clkdev.h>

#include "mach/socinfo.h"

#include "clock.h"
#include "proc_comm.h"

static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clocks_lock);
static LIST_HEAD(clocks);
struct clk *msm_clocks;
unsigned msm_num_clocks;

static struct notifier_block axi_freq_notifier_block;
static struct clk *pbus_clk;

struct clk* axi_clk;  /* hack */

static int clk_set_rate_locked(struct clk *clk, unsigned long rate);

static inline int pc_pll_request(unsigned id, unsigned on)
{
	on = !!on;
	return msm_proc_comm(PCOM_CLKCTL_RPC_PLL_REQUEST, &id, &on);
}

/*
 * Standard clock functions defined in include/linux/clk.h
 */
int clk_enable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&clocks_lock, flags);
	clk->count++;
	if (clk->count == 1)
		clk->ops->enable(clk->id);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return 0;
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&clocks_lock, flags);
	BUG_ON(clk->count == 0);
	clk->count--;
	if (clk->count == 0) {
		clk->ops->disable(clk->id);
	}
	spin_unlock_irqrestore(&clocks_lock, flags);
}
EXPORT_SYMBOL(clk_disable);

int clk_reset(struct clk *clk, enum clk_reset_action action)
{
	return clk->ops->reset(clk->remote_id, action);
}
EXPORT_SYMBOL(clk_reset);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->ops->get_rate(clk->id);
}
EXPORT_SYMBOL(clk_get_rate);

static int clk_set_rate_locked(struct clk *clk, unsigned long rate)
{
	int ret = 0;

	if (clk->flags & CLKFLAG_USE_MAX_TO_SET) {
		ret = clk->ops->set_max_rate(clk->id, rate);
		if (ret)
			goto err;
	}
	if (clk->flags & CLKFLAG_USE_MIN_TO_SET) {
		ret = clk->ops->set_min_rate(clk->id, rate);
		if (ret)
			goto err;
	}

	if (!(clk->flags & (CLKFLAG_USE_MAX_TO_SET | CLKFLAG_USE_MIN_TO_SET)))
		ret = clk->ops->set_rate(clk->id, rate);
err:
	return ret;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&clocks_lock, flags);
	ret = clk_set_rate_locked(clk, rate);
	spin_unlock_irqrestore(&clocks_lock, flags);

	return ret;
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->round_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_min_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->set_min_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_set_min_rate);

int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
        if (IS_ERR_OR_NULL(clk))
                return -EINVAL;

        if (!clk->ops->set_max_rate)
                return -ENOSYS;

        return clk->ops->set_max_rate(clk, rate);
}
EXPORT_SYMBOL(clk_set_max_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	return ERR_PTR(-ENOSYS);
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;
	return clk->ops->set_flags(clk->id, flags);
}
EXPORT_SYMBOL(clk_set_flags);

static int axi_freq_notifier_handler(struct notifier_block *block,
				unsigned long min_freq, void *v)
{
	/* convert min_freq from KHz to Hz, unless it's a magic value */
	if (min_freq != MSM_AXI_MAX_FREQ)
		min_freq *= 1000;

	/* On 7x30, ebi1_clk votes are dropped during power collapse, but
	 * pbus_clk votes are not. Use pbus_clk to implicitly request ebi1
	 * and AXI rates. */
	if (cpu_is_msm7x30() || cpu_is_msm8x55())
		return clk_set_rate(pbus_clk, min_freq/2);
	return 0;
}

void __init msm_clock_init(struct clk_lookup *clock_tbl, unsigned num_clocks)
{
	unsigned n;
	struct clk *clk;

	/* Do SoC-speficic clock init operations. */
	msm_clk_soc_init();

	spin_lock_init(&clocks_lock);
	mutex_lock(&clocks_mutex);
	for (n = 0; n < num_clocks; n++) {
		msm_clk_soc_set_ops(clock_tbl[n].clk);
		clkdev_add(&clock_tbl[n]);
		list_add_tail(&clock_tbl[n].clk->list, &clocks);
	}
	mutex_unlock(&clocks_mutex);

	for (n = 0; n < num_clocks; n++) {
		clk = clock_tbl[n].clk;
		if (clk->flags & CLKFLAG_VOTER) {
			struct clk *agg_clk = clk_get(NULL, clk->aggregator);
			BUG_ON(IS_ERR(agg_clk));

			clk_set_parent(clk, agg_clk);
		}
	}

	if (cpu_is_msm7x30() || cpu_is_msm8x55()) {
		pbus_clk = clk_get(NULL, "pbus_clk");
		BUG_ON(IS_ERR(pbus_clk));
	}

	axi_freq_notifier_block.notifier_call = axi_freq_notifier_handler;
	pm_qos_add_notifier(PM_QOS_SYSTEM_BUS_FREQ, &axi_freq_notifier_block);
}

#if defined(CONFIG_MSM_CLOCK_CTRL_DEBUG)
static int clk_debug_set(void *data, u64 val)
{
	struct clk *clk = data;
	int ret;

	ret = clk_set_rate(clk, val);
	if (ret != 0)
		pr_err("%s: can't set rate of '%s' to %llu (%d)\n",
		       __func__, clk->name, val, ret);
	return ret;
}

static int clk_debug_get(void *data, u64 *val)
{
	*val = clk_get_rate((struct clk *) data);
	return *val == 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clk_debug_fops, clk_debug_get, clk_debug_set, "%llu\n");

static int clock_debug_measure_get(void *data, u64 *val)
{
	struct clk *clock = data;
	*val = clock->ops->measure_rate(clock->id);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(clock_measure_fops, clock_debug_measure_get,
			NULL, "%lld\n");

static void __init clock_debug_init(void)
{
	struct dentry *dent;
	struct clk *clk;

	dent = debugfs_create_dir("clk", 0);
	if (IS_ERR(dent)) {
		pr_err("%s: Unable to create debugfs dir (%ld)\n", __func__,
		       PTR_ERR(dent));
		return;
	}

	mutex_lock(&clocks_mutex);
	list_for_each_entry(clk, &clocks, list) {
		debugfs_create_file(clk->name, 0644, dent, clk,
				    &clk_debug_fops);
	}
	mutex_unlock(&clocks_mutex);
}
#else
static inline void __init clock_debug_init(void) {}
#endif

static struct clk *axi_clk_userspace;
static int min_axi_khz;
static int param_set_min_axi(const char *val, struct kernel_param *kp)
{
	int ret;
	ret = param_set_int(val, kp);
	if (min_axi_khz >= 0) {
		ret = clk_set_rate_locked(axi_clk_userspace,
			min_axi_khz * 1000);
	}
	return ret;
}

static int param_get_min_axi(char *buffer, struct kernel_param *kp)
{
	unsigned long rate;
	int len;
	rate = clk_get_rate(axi_clk_userspace);
	len = sprintf(buffer, "%d %ld", min_axi_khz, rate / 1000);
	return len;
}

module_param_call(min_axi_khz, param_set_min_axi,
	param_get_min_axi, &min_axi_khz, S_IWUSR | S_IRUGO);

/* The bootloader and/or AMSS may have left various clocks enabled.
 * Disable any clocks that belong to us (CLKFLAG_AUTO_OFF) but have
 * not been explicitly enabled by a clk_enable() call.
 */
static int __init clock_late_init(void)
{
	unsigned long flags;
	struct clk *clk;
	unsigned count = 0;

	mutex_lock(&clocks_mutex);
	list_for_each_entry(clk, &clocks, list) {
		if (clk->flags & CLKFLAG_AUTO_OFF) {
			spin_lock_irqsave(&clocks_lock, flags);
			if (!clk->count) {
				count++;
				clk->ops->auto_off(clk->id);
			}
			spin_unlock_irqrestore(&clocks_lock, flags);
		}
	}
	mutex_unlock(&clocks_mutex);
	pr_info("clock_late_init() disabled %d unused clocks\n", count);

	clock_debug_init();

	axi_clk = clk_get(NULL, "ebi1_clk");
	axi_clk_userspace = clk_get(NULL, "ebi1_clk");

	return 0;
}

late_initcall(clock_late_init);
