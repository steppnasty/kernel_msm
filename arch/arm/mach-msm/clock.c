/* arch/arm/mach-msm/clock.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007 QUALCOMM Incorporated
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
#include "mach/socinfo.h"

#include "clock.h"
#include "proc_comm.h"

static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(clocks_lock);
static HLIST_HEAD(clocks);
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

static struct clk *clk_allocate_handle(struct clk *sclk)
{
	unsigned long flags;
	struct clk_handle *clkh = kzalloc(sizeof(*clkh), GFP_KERNEL);
	if (!clkh)
		return ERR_PTR(ENOMEM);
	clkh->clk.flags = CLKFLAG_HANDLE;
	clkh->source = sclk;

	spin_lock_irqsave(&clocks_lock, flags);
	hlist_add_head(&clkh->clk.list, &sclk->handles);
	spin_unlock_irqrestore(&clocks_lock, flags);
	return &clkh->clk;
}

static struct clk *source_clk(struct clk *clk)
{
	struct clk_handle *clkh;

	if (clk->flags & CLKFLAG_HANDLE) {
		clkh = container_of(clk, struct clk_handle, clk);
		clk = clkh->source;
	}
	return clk;
}

/*
 * Standard clock functions defined in include/linux/clk.h
 */
struct clk *clk_get(struct device *dev, const char *id)
{
	struct clk *clk;
	struct hlist_node *pos;

	mutex_lock(&clocks_mutex);

	hlist_for_each_entry(clk, pos, &clocks, list)
		if (!strcmp(id, clk->name) && clk->dev == dev)
			goto found_it;

	hlist_for_each_entry(clk, pos, &clocks, list)
		if (!strcmp(id, clk->name) && clk->dev == NULL)
			goto found_it;

	clk = ERR_PTR(-ENOENT);
found_it:
	if (!IS_ERR(clk) && (clk->flags & CLKFLAG_SHARED))
		clk = clk_allocate_handle(clk);
	mutex_unlock(&clocks_mutex);
	return clk;
}
EXPORT_SYMBOL(clk_get);

void clk_put(struct clk *clk)
{
	struct clk_handle *clkh;
	unsigned long flags;

	if (WARN_ON(IS_ERR(clk)))
		return;

	if (!(clk->flags & CLKFLAG_HANDLE))
		return;

	clk_set_rate(clk, 0);

	spin_lock_irqsave(&clocks_lock, flags);
	clkh = container_of(clk, struct clk_handle, clk);
	hlist_del(&clk->list);
	kfree(clkh);
	spin_unlock_irqrestore(&clocks_lock, flags);
}
EXPORT_SYMBOL(clk_put);

int clk_enable(struct clk *clk)
{
	unsigned long flags;
	spin_lock_irqsave(&clocks_lock, flags);
	clk = source_clk(clk);
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
	clk = source_clk(clk);
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
	clk = source_clk(clk);
	return clk->ops->get_rate(clk->id);
}
EXPORT_SYMBOL(clk_get_rate);

static unsigned long clk_find_min_rate_locked(struct clk *clk)
{
	unsigned long rate = 0;
	struct clk_handle *clkh;
	struct hlist_node *pos;

	hlist_for_each_entry(clkh, pos, &clk->handles, clk.list)
		if (clkh->rate > rate)
			rate = clkh->rate;
	return rate;
}

static int clk_set_rate_locked(struct clk *clk, unsigned long rate)
{
	int ret = 0;

	if (clk->flags & CLKFLAG_HANDLE) {
		struct clk_handle *clkh;
		clkh = container_of(clk, struct clk_handle, clk);
		clkh->rate = rate;
		clk = clkh->source;
		rate = clk_find_min_rate_locked(clk);
	}

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
	clk = source_clk(clk);
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

void __init msm_clock_init(struct clk *clock_tbl, unsigned num_clocks)
{
	unsigned n;
	struct clk *clk;

	/* Do SoC-speficic clock init operations. */
	msm_clk_soc_init();

	spin_lock_init(&clocks_lock);
	mutex_lock(&clocks_mutex);
	msm_clocks = clock_tbl;
	msm_num_clocks = num_clocks;
	for (n = 0; n < msm_num_clocks; n++) {
		msm_clk_soc_set_ops(&msm_clocks[n]);
		hlist_add_head(&msm_clocks[n].list, &clocks);
	}
	mutex_unlock(&clocks_mutex);

	for (n = 0; n < num_clocks; n++) {
		clk = &clock_tbl[n];
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

static void *clk_info_seq_start(struct seq_file *seq, loff_t *ppos)
{
	struct hlist_node *pos;
	int i = *ppos;
	mutex_lock(&clocks_mutex);
	hlist_for_each(pos, &clocks)
		if (i-- == 0)
			return hlist_entry(pos, struct clk, list);
	return NULL;
}

static void *clk_info_seq_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct clk *clk = v;
	++*pos;
	return hlist_entry(clk->list.next, struct clk, list);
}

static void clk_info_seq_stop(struct seq_file *seq, void *v)
{
	mutex_unlock(&clocks_mutex);
}

static int clk_info_seq_show(struct seq_file *seq, void *v)
{
	struct clk *clk = v;
	unsigned long flags;
	struct clk_handle *clkh;
	struct hlist_node *pos;

	seq_printf(seq, "Clock %s\n", clk->name);
	seq_printf(seq, "  Id          %d\n", clk->id);
	seq_printf(seq, "  Count       %d\n", clk->count);
	seq_printf(seq, "  Flags       %x\n", clk->flags);
	seq_printf(seq, "  Dev         %p %s\n",
			clk->dev, clk->dev ? dev_name(clk->dev) : "");
	seq_printf(seq, "  Handles     %p\n", clk->handles.first);
	spin_lock_irqsave(&clocks_lock, flags);
	hlist_for_each_entry(clkh, pos, &clk->handles, clk.list)
		seq_printf(seq, "    Requested rate    %ld\n", clkh->rate);
	spin_unlock_irqrestore(&clocks_lock, flags);

	seq_printf(seq, "  Enabled     %d\n", pc_clk_is_enabled(clk->id));
	seq_printf(seq, "  Rate        %ld\n", clk_get_rate(clk));

	seq_printf(seq, "\n");
	return 0;
}

static struct seq_operations clk_info_seqops = {
	.start = clk_info_seq_start,
	.next = clk_info_seq_next,
	.stop = clk_info_seq_stop,
	.show = clk_info_seq_show,
};

static int clk_info_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &clk_info_seqops);
}

static const struct file_operations clk_info_fops = {
	.owner = THIS_MODULE,
	.open = clk_info_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

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
	struct hlist_node *pos;

	dent = debugfs_create_dir("clk", 0);
	if (IS_ERR(dent)) {
		pr_err("%s: Unable to create debugfs dir (%ld)\n", __func__,
		       PTR_ERR(dent));
		return;
	}

	debugfs_create_file("all", 0x444, dent, NULL, &clk_info_fops);

	mutex_lock(&clocks_mutex);
	hlist_for_each_entry(clk, pos, &clocks, list) {
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
	struct hlist_node *pos;
	unsigned count = 0;

	mutex_lock(&clocks_mutex);
	hlist_for_each_entry(clk, pos, &clocks, list) {
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
