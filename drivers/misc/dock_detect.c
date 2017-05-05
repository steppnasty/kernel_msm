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

#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/irq.h>

#include <mach/board.h>

#define DOCK_STATE_UNDOCKED     0
#define DOCK_STATE_DESK         (1 << 0)
#define DOCK_STATE_CAR          (1 << 1)

#define DOCK_DET_DELAY		HZ/4

static struct switch_dev dock_switch = {
	.name = "dock",
};

struct dock_detect_info {
	int dock_pin_gpio;
	int irq;
	int vbus;
	/* 0: none, 1: carkit, 2: usb headset, 4: mhl */
	u8 accessory_type;
	uint8_t dock_pin_state;
	struct platform_device *pdev;
	struct delayed_work dock_work_isr;
	struct delayed_work dock_work;
};
static struct dock_detect_info *the_dock_info;

static ssize_t dock_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dock_detect_info *dock = the_dock_info;
	if (dock->accessory_type == 3) /*desk dock*/
		return sprintf(buf, "online\n");
	else
		return sprintf(buf, "offline\n");
}
static DEVICE_ATTR(status, S_IRUGO | S_IWUSR, dock_status_show, NULL);

static irqreturn_t dock_interrupt(int irq, void *data)
{
	struct dock_detect_info *dock = data;

	disable_irq_nosync(dock->irq);
	cancel_delayed_work(&dock->dock_work);
	schedule_delayed_work(&dock->dock_work_isr, DOCK_DET_DELAY);

	return IRQ_HANDLED;
}
static void dock_isr_work(struct work_struct *w)
{
	struct dock_detect_info *dock = container_of(w,
			struct dock_detect_info, dock_work_isr.work);
	dock->dock_pin_state = gpio_get_value(dock->dock_pin_gpio);

	if (dock->dock_pin_state == 1)
		set_irq_type(dock->irq, IRQF_TRIGGER_LOW);
	else
		set_irq_type(dock->irq, IRQF_TRIGGER_HIGH);
	schedule_delayed_work(&dock->dock_work, DOCK_DET_DELAY);
	enable_irq(dock->irq);
}

static void dock_detect_work(struct work_struct *w)
{
	struct dock_detect_info *dock = container_of(w, struct dock_detect_info, dock_work.work);
	int value;

	value = gpio_get_value(dock->dock_pin_gpio);
	if (dock->dock_pin_state != value && (dock->dock_pin_state & 0x80) == 0) {
		pr_err("%s: dock_pin_state changed\n", __func__);
		return;
	}

	if (value == 0 && dock->vbus) {
		if (dock->accessory_type == 3)
			return;
		set_irq_type(dock->irq, IRQF_TRIGGER_HIGH);
		switch_set_state(&dock_switch, DOCK_STATE_DESK);
		dock->accessory_type = 3;
		pr_info("dock: set state %d\n", DOCK_STATE_DESK);
	} else {
		if (dock->accessory_type == 0)
			return;
		set_irq_type(dock->irq, IRQF_TRIGGER_LOW);
		switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
		dock->accessory_type = 0;
		pr_info("dock: set state %d\n", DOCK_STATE_UNDOCKED);
	}
}

void dock_detect_set_vbus_state(int vbus)
{
	struct dock_detect_info *dock = the_dock_info;

	if (!dock || dock->vbus == vbus)
		return;

	dock->vbus = vbus;

	if (vbus)
		enable_irq(dock->irq);
	else {
		disable_irq_nosync(dock->irq);
		if (cancel_delayed_work_sync(&dock->dock_work_isr))
			enable_irq(dock->irq);

		if (cancel_delayed_work_sync(&dock->dock_work)) {
			if (dock->dock_pin_state == 0)
				set_irq_type(dock->irq, IRQF_TRIGGER_LOW);
		}
		if (dock->accessory_type == 3) {
			dock->dock_pin_state |= 0x80;
			schedule_delayed_work(&dock->dock_work, 0);
		}
	}
}
EXPORT_SYMBOL(dock_detect_set_vbus_state);

static int dock_probe(struct platform_device *pdev)
{
	struct dock_detect_platform_data *pdata;
	struct dock_detect_info *dock;

	pdata = pdev->dev.platform_data;
	dock = kzalloc(sizeof(struct dock_detect_info), GFP_KERNEL);
	if (!dock)
		return -ENOMEM;

	dock->pdev = pdev;
	dock->dock_pin_gpio = pdata->dock_pin_gpio;

	if (!dock->dock_pin_gpio)
		goto err;

	dock->irq = gpio_to_irq(dock->dock_pin_gpio);
	set_irq_flags(dock->irq, IRQF_VALID | IRQF_NOAUTOEN);
	if (request_irq(dock->irq, dock_interrupt,
				IRQF_TRIGGER_LOW, "dock_irq", dock) < 0)
		goto err;

	if (switch_dev_register(&dock_switch) < 0)
		goto err;

	if (device_create_file(dock_switch.dev, &dev_attr_status) != 0)
		pr_err("[DOCK] dev_attr_status failed\n");

	INIT_DELAYED_WORK(&dock->dock_work_isr, dock_isr_work);
	INIT_DELAYED_WORK(&dock->dock_work, dock_detect_work);

	the_dock_info = dock;

	return 0;

err:
	if (dock->irq)
		free_irq(dock->irq, 0);
	if (dock)
		kfree(dock);
	pr_err("[DOCK] probe failed\n");
	return -EINVAL;
}

static int dock_remove(struct platform_device *pdev)
{
	struct dock_detect_info *dock = container_of(&pdev,
		struct dock_detect_info, pdev);

	if (dock->irq)
		free_irq(dock->irq, 0);
	if (dock)
		kfree(dock);

	return 0;
}

static struct platform_driver dock_driver = {
	.probe	= dock_probe,
	.remove	= dock_remove,
	.driver	= {
		.name = "dock_detect",
	},
};

static int __init init(void)
{
	return platform_driver_register(&dock_driver);
}

static void __exit cleanup(void)
{
	platform_driver_unregister(&dock_driver);
}

module_init(init);
module_exit(cleanup);
