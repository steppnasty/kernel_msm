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

#ifndef MSM_CSIC_H
#define MSM_CSIC_H

#include <linux/clk.h>
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include <media/msm_cam_sensor.h>
#include "msm_sd.h"

#define CSIC_7X 0x1
#define CSIC_8X (0x1 << 1)

enum msm_csic_state_t {
	CSIC_POWER_UP,
	CSIC_POWER_DOWN,
};

struct csic_device {
	struct platform_device *pdev;
	struct msm_sd_subdev msm_sd;
	struct resource *mem;
	struct resource *irq;
	struct resource *io;
	void __iomem *base;
	struct mutex mutex;
	uint32_t hw_version;
	enum msm_csic_state_t csic_state;

	struct clk *csic_clk[5];
};

enum msm_camera_csic_data_format {
	CSIC_8BIT,
	CSIC_10BIT,
	CSIC_12BIT,
};

struct msm_camera_csic_params {
	enum msm_camera_csic_data_format data_format;
	uint8_t lane_cnt;
	uint8_t lane_assign;
	uint8_t settle_cnt;
	uint8_t dpcm_scheme;
};

enum csic_cfg_type_t {
	CSIC_INIT,
	CSIC_CFG,
	CSIC_RELEASE,
};

struct csic_cfg_data {
	enum csic_cfg_type_t cfgtype;
	struct msm_camera_csic_params *csic_params;
};

#define MSM_CAMERA_SUBDEV_CSIC 15

#define VIDIOC_MSM_CSIC_IO_CFG \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct csic_cfg_data)

#define VIDIOC_MSM_CSIC_RELEASE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_subdev)

#endif
