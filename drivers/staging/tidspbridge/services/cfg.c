/*
 * cfg.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * Implementation of platform specific config services.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/types.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */

/*  ----------------------------------- This */
#include <dspbridge/cfg.h>
#include <dspbridge/drv.h>

/*
 *  ======== cfg_get_object ========
 *  Purpose:
 *      Retrieve the Object handle from the Registry
 */
int cfg_get_object(u32 *value, u8 dw_type)
{
	int status = -EINVAL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	DBC_REQUIRE(value != NULL);

	if (!drv_datap)
		return -EPERM;

	switch (dw_type) {
	case (REG_DRV_OBJECT):
		if (drv_datap->drv_object) {
			*value = (u32)drv_datap->drv_object;
			status = 0;
		} else {
			status = -ENODATA;
		}
		break;
	case (REG_MGR_OBJECT):
		if (drv_datap->mgr_object) {
			*value = (u32)drv_datap->mgr_object;
			status = 0;
		} else {
			status = -ENODATA;
		}
		break;

	default:
		break;
	}
	if (status) {
		*value = 0;
		pr_err("%s: Failed, status 0x%x\n", __func__, status);
	}
	DBC_ENSURE((!status && *value != 0) || (status && *value == 0));
	return status;
}

/*
 *  ======== cfg_set_dev_object ========
 *  Purpose:
 *      Store the Device Object handle and dev_node pointer for a given devnode.
 */
int cfg_set_dev_object(struct cfg_devnode *dev_node_obj, u32 value)
{
	int status = 0;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	if (!drv_datap) {
		pr_err("%s: Failed, status 0x%x\n", __func__, status);
		return -EPERM;
	}

	if (!dev_node_obj)
		status = -EFAULT;

	if (!status) {
		/* Store the Bridge device object in the Registry */

		if (!(strcmp((char *)dev_node_obj, "TIOMAP1510")))
			drv_datap->dev_object = (void *) value;
	}
	if (status)
		pr_err("%s: Failed, status 0x%x\n", __func__, status);

	return status;
}

/*
 *  ======== cfg_set_object ========
 *  Purpose:
 *      Store the Driver Object handle
 */
int cfg_set_object(u32 value, u8 dw_type)
{
	int status = -EINVAL;
	struct drv_data *drv_datap = dev_get_drvdata(bridge);

	if (!drv_datap)
		return -EPERM;

	switch (dw_type) {
	case (REG_DRV_OBJECT):
		drv_datap->drv_object = (void *)value;
		status = 0;
		break;
	case (REG_MGR_OBJECT):
		drv_datap->mgr_object = (void *)value;
		status = 0;
		break;
	default:
		break;
	}
	if (status)
		pr_err("%s: Failed, status 0x%x\n", __func__, status);
	return status;
}
