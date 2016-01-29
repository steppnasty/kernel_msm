/*
 * Copyright (C) 2015-2016, Brian Stepp <steppnasty@gmail.com>
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <mach/clk.h>

#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"
#include "msm_isp.h"
#include "msm.h"
#include "msm_camera_io_util.h"

/*#define CONFIG_MSM_ISP_DBG*/
#undef CDBG
#ifdef CONFIG_MSM_ISP_DBG
#define CDBG(fmt, args...) pr_info(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define VFE31_BURST_LEN 2
#define VFE31_UB_SIZE 1024
#define VFE31_EQUAL_SLICE_UB 198
#define VFE31_WM_BASE(idx) (0x4C + 0x18 * idx)
#define VFE31_RDI_BASE(idx) (idx ? 0x734 + 0x4 * (idx - 1) : 0x06FC)
#define VFE31_XBAR_BASE(idx) (0x40 + 0x4 * (idx / 4))
#define VFE31_XBAR_SHIFT(idx) ((idx % 4) * 8)
#define VFE31_PING_PONG_BASE(wm, ping_pong) \
	(VFE31_WM_BASE(wm) + 0x4 * (1 + (~(ping_pong >> wm) & 0x1)))
#define VFE31_NUM_STATS_TYPE 7
#define VFE31_STATS_PING_PONG_OFFSET 7
#define VFE31_STATS_BASE(idx) (0xF4 + 0xC * idx)
#define VFE31_STATS_PING_PONG_BASE(idx, ping_pong) \
	(VFE31_STATS_BASE(idx) + 0x4 * \
	(~(ping_pong >> (idx + VFE31_STATS_PING_PONG_OFFSET)) & 0x1))

static struct clk *ebi1_clk;
static struct msm_cam_clk_info msm_vfe31_clk_info[] = {
	{"vfe_clk", 153600000},
	{"vfe_pclk", -1},
	{"camif_pad_pclk", -1},
	{"vfe_camif_clk", -1},
};

static void msm_vfe31_camif_pad_reg_reset(struct vfe_device *vfe_dev)
{
	uint32_t reg;
	struct clk *clk = NULL;

	clk = vfe_dev->vfe_clk[0];

	if (clk != NULL)
		clk_set_flags(clk, 0x00000100 << 1);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe_dev->camif_base)) & 0x1fffff;
	reg |= 0x3;
	msm_camera_io_w(reg, vfe_dev->camif_base);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe_dev->camif_base)) & 0x1fffff;
	reg |= 0x10;
	msm_camera_io_w(reg, vfe_dev->camif_base);
	usleep_range(10000, 15000);

	reg = (msm_camera_io_r(vfe_dev->camif_base)) & 0x1fffff;
	/* Need to be uninverted*/
	reg &= 0x03;
	msm_camera_io_w(reg, vfe_dev->camif_base);
	usleep_range(10000, 15000);
}

static uint32_t msm_vfe31_axi_get_plane_size(
	struct msm_vfe_axi_stream *stream_info, int plane_idx)
{
	uint32_t size = 0;
	struct msm_vfe_axi_plane_cfg *plane_cfg = stream_info->plane_cfg;
	switch (stream_info->output_format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
		/* TODO: fix me */
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
		/* TODO: fix me */
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		if (plane_cfg[plane_idx].output_plane_format == Y_PLANE)
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		else
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width / 2;
		break;
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
		if (plane_cfg[plane_idx].output_plane_format == Y_PLANE)
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		else
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width / 8;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		size = plane_cfg[plane_idx].output_height *
			plane_cfg[plane_idx].output_width;
		break;
	/*TD: Add more image format*/
	default:
		pr_err("%s: Invalid output format\n", __func__);
		break;
	}
	return size;
}

static int msm_vfe31_init_hardware(struct vfe_device *vfe_dev)
{
	int rc = -1;

	vfe_dev->vfe_clk_idx = 0;

	if (vfe_dev->fs_vfe) {
		rc = regulator_enable(vfe_dev->fs_vfe);
		if (rc) {
			pr_err("%s: Regulator enable failed\n", __func__);
			goto fs_failed;
		}
	}

	rc = msm_cam_clk_enable(&vfe_dev->pdev->dev, msm_vfe31_clk_info,
		vfe_dev->vfe_clk, ARRAY_SIZE(msm_vfe31_clk_info), 1);
	if (rc < 0)
		goto clk_enable_failed;
	else
		vfe_dev->vfe_clk_idx = 1;
	ebi1_clk = clk_get(NULL, "ebi1_clk");
	if (IS_ERR(ebi1_clk))
		ebi1_clk = NULL;
	else {
		clk_prepare(ebi1_clk);
		clk_enable(ebi1_clk);
		clk_set_rate(ebi1_clk, 192000000);
	}

	vfe_dev->vfe_base = ioremap(vfe_dev->vfe_mem->start,
		resource_size(vfe_dev->vfe_mem));
	if (!vfe_dev->vfe_base) {
		rc = -ENOMEM;
		pr_err("%s: vfe ioremap failed\n", __func__);
		goto vfe_remap_failed;
	}

	vfe_dev->camif_base = ioremap(vfe_dev->camif_mem->start,
		resource_size(vfe_dev->camif_mem));
	if (!vfe_dev->camif_base) {
		rc = -ENOMEM;
		pr_err("%s: camif ioremap failed\n", __func__);
		goto camif_remap_failed;
	}

	rc = request_irq(vfe_dev->vfe_irq->start, msm_isp_process_irq,
					IRQF_TRIGGER_RISING, "vfe", vfe_dev);
	if (rc < 0) {
		pr_err("%s: irq request failed\n", __func__);
		goto irq_req_failed;
	}
	msm_vfe31_camif_pad_reg_reset(vfe_dev);

	return rc;
irq_req_failed:
	iounmap(vfe_dev->camif_base);
camif_remap_failed:
	iounmap(vfe_dev->vfe_base);
vfe_remap_failed:
	if (vfe_dev->vfe_clk_idx == 1)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe31_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe31_clk_info), 0);
	if (ebi1_clk) {
		clk_disable(ebi1_clk);
		clk_unprepare(ebi1_clk);
		clk_put(ebi1_clk);
		ebi1_clk = NULL;
	}
clk_enable_failed:
	regulator_disable(vfe_dev->fs_vfe);
fs_failed:
	return rc;
}

static void msm_vfe31_release_hardware(struct vfe_device *vfe_dev)
{
	free_irq(vfe_dev->vfe_irq->start, vfe_dev);
	tasklet_kill(&vfe_dev->vfe_tasklet);
	iounmap(vfe_dev->vfe_base);
	iounmap(vfe_dev->camif_base);
	if (vfe_dev->vfe_clk_idx == 1)
		msm_cam_clk_enable(&vfe_dev->pdev->dev,
				msm_vfe31_clk_info, vfe_dev->vfe_clk,
				ARRAY_SIZE(msm_vfe31_clk_info), 0);
	if (vfe_dev->fs_vfe);
		regulator_disable(vfe_dev->fs_vfe);
	msm_isp_deinit_bandwidth_mgr(ISP_VFE0 + vfe_dev->pdev->id);
	if (!ebi1_clk)
		return;
	clk_disable(ebi1_clk);
	clk_unprepare(ebi1_clk);
	clk_put(ebi1_clk);
	ebi1_clk = NULL;
}

static void msm_vfe31_init_hardware_reg(struct vfe_device *vfe_dev)
{
	msm_camera_io_w(0x800080, vfe_dev->vfe_base + 0x288);
	msm_camera_io_w(0x800080, vfe_dev->vfe_base + 0x28C);
	/* CGC_OVERRIDE */
	msm_camera_io_w(0xFFFFF, vfe_dev->vfe_base + 0xC);
	/* default frame drop period and pattern */
	msm_camera_io_w(0x1f, vfe_dev->vfe_base + 0x504);
	msm_camera_io_w(0x1f, vfe_dev->vfe_base + 0x508);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x50C);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x510);
	msm_camera_io_w(0x1f, vfe_dev->vfe_base + 0x514);
	msm_camera_io_w(0x1f, vfe_dev->vfe_base + 0x518);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x51C);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x520);
	msm_camera_io_w(0, vfe_dev->vfe_base + 0x528);
	msm_camera_io_w(0xFFFFFF, vfe_dev->vfe_base + 0x524);
	/* stats UB config */
	msm_camera_io_w(0x3980007, vfe_dev->vfe_base + 0xFC);
	msm_camera_io_w(0x3A00007, vfe_dev->vfe_base + 0x108);
	msm_camera_io_w(0x3A8000F, vfe_dev->vfe_base + 0x114);
	msm_camera_io_w(0x3B80007, vfe_dev->vfe_base + 0x120);
	msm_camera_io_w(0x3C0001F, vfe_dev->vfe_base + 0x12c);
	msm_camera_io_w(0x3E0001F, vfe_dev->vfe_base + 0x138);
}

static void msm_vfe31_process_reset_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
	if (irq_status1 & BIT(23))
		return;
	else if (irq_status1 & BIT(22)) {
		msm_vfe31_init_hardware_reg(vfe_dev);
		msm_camera_io_w(0x7FFF, vfe_dev->vfe_base + 0x38);
	}
}

static void msm_vfe31_process_halt_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1)
{
}

static void msm_vfe31_process_camif_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0x1F))
		return;

	if (irq_status0 & BIT(0)) {
		ISP_DBG("%s: SOF IRQ\n", __func__);
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].raw_stream_count > 0
			&& vfe_dev->axi_data.src_info[VFE_PIX_0].
			pix_stream_count == 0) {
			msm_isp_sof_notify(vfe_dev, VFE_PIX_0, ts);
			if (vfe_dev->axi_data.stream_update)
				msm_isp_axi_stream_update(vfe_dev);
		}
	}
}

static void msm_vfe31_process_violation_status(struct vfe_device *vfe_dev)
{
	uint32_t violation_status = vfe_dev->error_info.violation_status;
	if (!violation_status)
		return;

	if (violation_status & BIT(0))
		pr_err("%s: black violation\n", __func__);
	if (violation_status & BIT(1))
		pr_err("%s: rolloff violation\n", __func__);
	if (violation_status & BIT(2))
		pr_err("%s: demux violation\n", __func__);
	if (violation_status & BIT(3))
		pr_err("%s: demosaic violation\n", __func__);
	if (violation_status & BIT(4))
		pr_err("%s: crop violation\n", __func__);
	if (violation_status & BIT(5))
		pr_err("%s: scale violation\n", __func__);
	if (violation_status & BIT(6))
		pr_err("%s: wb violation\n", __func__);
	if (violation_status & BIT(7))
		pr_err("%s: clf violation\n", __func__);
	if (violation_status & BIT(8))
		pr_err("%s: matrix violation\n", __func__);
	if (violation_status & BIT(9))
		pr_err("%s: rgb lut violation\n", __func__);
	if (violation_status & BIT(10))
		pr_err("%s: la violation\n", __func__);
	if (violation_status & BIT(11))
		pr_err("%s: chroma enhance violation\n", __func__);
	if (violation_status & BIT(12))
		pr_err("%s: chroma supress mce violation\n", __func__);
	if (violation_status & BIT(13))
		pr_err("%s: skin enhance violation\n", __func__);
	if (violation_status & BIT(14))
		pr_err("%s: asf violation\n", __func__);
	if (violation_status & BIT(15))
		pr_err("%s: scale y violation\n", __func__);
	if (violation_status & BIT(16))
		pr_err("%s: scale cbcr violation\n", __func__);
	if (violation_status & BIT(17))
		pr_err("%s: chroma subsample violation\n", __func__);
	if (violation_status & BIT(18))
		pr_err("%s: framedrop enc y violation\n", __func__);
	if (violation_status & BIT(19))
		pr_err("%s: framedrop enc cbcr violation\n", __func__);
	if (violation_status & BIT(20))
		pr_err("%s: framedrop view y violation\n", __func__);
	if (violation_status & BIT(21))
		pr_err("%s: framedrop view cbcr violation\n", __func__);
	if (violation_status & BIT(22))
		pr_err("%s: realign buf y violation\n", __func__);
	if (violation_status & BIT(23))
		pr_err("%s: realign buf cb violation\n", __func__);
	if (violation_status & BIT(24))
		pr_err("%s: realign buf cr violation\n", __func__);
}

static void msm_vfe31_process_error_status(struct vfe_device *vfe_dev)
{
	uint32_t error_status1 = vfe_dev->error_info.error_mask1;

	if (error_status1 & BIT(0))
		pr_err("%s: camif error status: 0x%x\n",
			__func__, vfe_dev->error_info.camif_status);
	if (error_status1 & BIT(1))
		pr_err("%s: stats bhist overwrite\n", __func__);
	if (error_status1 & BIT(2))
		pr_err("%s: stats cs overwrite\n", __func__);
	if (error_status1 & BIT(3))
		pr_err("%s: stats ihist overwrite\n", __func__);
	if (error_status1 & BIT(4))
		pr_err("%s: realign buf y overflow\n", __func__);
	if (error_status1 & BIT(5))
		pr_err("%s: realign buf cb overflow\n", __func__);
	if (error_status1 & BIT(6))
		pr_err("%s: realign buf cr overflow\n", __func__);
	if (error_status1 & BIT(7)) {
		pr_err("%s: violation\n", __func__);
		msm_vfe31_process_violation_status(vfe_dev);
	}
	if (error_status1 & BIT(8))
		pr_err("%s: image master 0 bus overflow\n", __func__);
	if (error_status1 & BIT(9))
		pr_err("%s: image master 1 bus overflow\n", __func__);
	if (error_status1 & BIT(10))
		pr_err("%s: image master 2 bus overflow\n", __func__);
	if (error_status1 & BIT(11))
		pr_err("%s: image master 3 bus overflow\n", __func__);
	if (error_status1 & BIT(12))
		pr_err("%s: image master 4 bus overflow\n", __func__);
	if (error_status1 & BIT(13))
		pr_err("%s: image master 5 bus overflow\n", __func__);
	if (error_status1 & BIT(14))
		pr_err("%s: image master 6 bus overflow\n", __func__);
	if (error_status1 & BIT(15))
		pr_err("%s: status ae/bg bus overflow\n", __func__);
	if (error_status1 & BIT(16))
		pr_err("%s: status af/bf bus overflow\n", __func__);
	if (error_status1 & BIT(17))
		pr_err("%s: status awb bus overflow\n", __func__);
	if (error_status1 & BIT(18))
		pr_err("%s: status rs bus overflow\n", __func__);
	if (error_status1 & BIT(19))
		pr_err("%s: status cs bus overflow\n", __func__);
	if (error_status1 & BIT(20))
		pr_err("%s: status ihist bus overflow\n", __func__);
	if (error_status1 & BIT(21))
		pr_err("%s: status skin bhist bus overflow\n", __func__);
	if (error_status1 & BIT(22))
		pr_err("%s: axi error\n", __func__);
}

static void msm_vfe31_read_irq_status(struct vfe_device *vfe_dev,
	uint32_t *irq_status0, uint32_t *irq_status1)
{
	*irq_status0 = msm_camera_io_r(vfe_dev->vfe_base + 0x2C);
	*irq_status1 = msm_camera_io_r(vfe_dev->vfe_base + 0x30);
        if (*irq_status1 & BIT(0))
		vfe_dev->error_info.camif_status =
			msm_camera_io_r(vfe_dev->vfe_base + 0x204);
	msm_camera_io_w(*irq_status0, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w(*irq_status1, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w_mb(1, vfe_dev->vfe_base + 0x18);
}

static void msm_vfe31_process_reg_update(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	if (!(irq_status0 & 0x20) && !(irq_status1 & 0x1C000000))
		return;

	if (irq_status0 & BIT(5))
		msm_isp_sof_notify(vfe_dev, VFE_PIX_0, ts);
	if (irq_status1 & BIT(26))
		msm_isp_sof_notify(vfe_dev, VFE_RAW_0, ts);
	if (irq_status1 & BIT(27))
		msm_isp_sof_notify(vfe_dev, VFE_RAW_1, ts);
	if (irq_status1 & BIT(28))
		msm_isp_sof_notify(vfe_dev, VFE_RAW_2, ts);

	if (vfe_dev->axi_data.stream_update)
		msm_isp_axi_stream_update(vfe_dev);
	if (atomic_read(&vfe_dev->stats_data.stats_update))
		msm_isp_stats_stream_update(vfe_dev);
	msm_isp_update_framedrop_reg(vfe_dev);
	msm_isp_update_error_frame_count(vfe_dev);

	vfe_dev->hw_info->vfe_ops.core_ops.
		reg_update(vfe_dev);
	return;
}

static void msm_vfe31_reg_update(
	struct vfe_device *vfe_dev)
{
	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x260);
}

static long msm_vfe31_reset_hardware(struct vfe_device *vfe_dev)
{
	return 0;
}

static void msm_vfe31_axi_reload_wm(
	struct vfe_device *vfe_dev, uint32_t reload_mask)
{
}

static void msm_vfe31_axi_enable_wm(struct vfe_device *vfe_dev,
	uint8_t wm_idx, uint8_t enable)
{
	uint32_t val = msm_camera_io_r(
	   vfe_dev->vfe_base + VFE31_WM_BASE(wm_idx));
	if (enable)
		val |= 0x1;
	else
		val &= ~0x1;
	msm_camera_io_w(val,
		vfe_dev->vfe_base + VFE31_WM_BASE(wm_idx));
}

static void msm_vfe31_axi_cfg_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t comp_mask, comp_mask_index =
		stream_info->comp_mask_index;
	uint32_t irq_mask;
	int offset = 8;

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x34);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	comp_mask |= (axi_data->composite_info[comp_mask_index].
		stream_composite_mask << (comp_mask_index * offset));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x34);

	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
#if 0
	irq_mask |= BIT(comp_mask_index + 21);
#else
	irq_mask = 0x00EFE021;
#endif
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe31_axi_clear_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t comp_mask, comp_mask_index = stream_info->comp_mask_index;
	uint32_t irq_mask;

	comp_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x34);
	comp_mask &= ~(0x7F << (comp_mask_index * 8));
	msm_camera_io_w(comp_mask, vfe_dev->vfe_base + 0x34);

	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~BIT(comp_mask_index + 21);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe31_axi_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= BIT(stream_info->wm[0] + 6);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
}

static void msm_vfe31_axi_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
}

static void msm_vfe31_cfg_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t framedrop_pattern = 0, framedrop_period = 0;

	if (stream_info->stream_type == BURST_STREAM &&
		stream_info->runtime_burst_frame_count == 0) {
		framedrop_pattern = 0;
		framedrop_period = 0;
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x4C);
		msm_camera_io_w(0, vfe_dev->vfe_base + 0xAC);
		msm_camera_io_w(0, vfe_dev->vfe_base + 0x64);
		msm_camera_io_w(0, vfe_dev->vfe_base + 0xC4);
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, DISABLE_CAMIF);
	}
}

static void msm_vfe31_clear_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
}

static void msm_vfe31_cfg_io_format(struct vfe_device *vfe_dev,
	enum msm_vfe_axi_stream_src stream_src, uint32_t io_format)
{
}

static void msm_vfe31_cfg_camif(struct vfe_device *vfe_dev,
	struct msm_vfe_pix_cfg *pix_cfg)
{
}

static void msm_vfe31_update_camif_state(
	struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state update_state)
{
	uint32_t val;
	bool bus_en, vfe_en;
	if (update_state == NO_UPDATE)
		return;

	if (update_state == ENABLE_CAMIF) {
		msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 1;
	} else if (update_state == DISABLE_CAMIF) {
		msm_camera_io_w_mb(0x0, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
	} else if (update_state == DISABLE_CAMIF_IMMEDIATELY) {
		msm_camera_io_w_mb(0x2, vfe_dev->vfe_base + 0x1E0);
		vfe_dev->axi_data.src_info[VFE_PIX_0].active = 0;
	}
}

static void msm_vfe31_cfg_rdi_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_rdi_cfg *rdi_cfg, enum msm_vfe_input_src input_src)
{
}

void msm_vfe31_axi_reserve_wm(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
        struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int i;

	if (axi_data->free_wm[0])
		stream_info->wm[0] = 1;
	else
		stream_info->wm[0] = 0;
	if (axi_data->free_wm[4])
		stream_info->wm[1] = 5;
	else
		stream_info->wm[1] = 4;
	for (i = 0; i < stream_info->num_planes; i++) {
		axi_data->free_wm[stream_info->wm[i]] =
			stream_info->stream_handle;
		axi_data->wm_image_size[stream_info->wm[i]] =
			msm_vfe31_axi_get_plane_size(
			stream_info, i);
		axi_data->num_used_wm++;
	}
}

static void msm_vfe31_axi_cfg_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint8_t plane_idx)
{
	uint32_t val;
	uint32_t wm_base = VFE31_WM_BASE(stream_info->wm[plane_idx]);

	if (!stream_info->frame_based) {
		/*WR_IMAGE_SIZE*/
		val =
			((msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_width)+1)/2 - 1) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1);
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);

		/*WR_BUFFER_CFG*/
		val =
			msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_stride) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1) << 4 | VFE31_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	} else {
		msm_camera_io_w(0x2, vfe_dev->vfe_base + wm_base);
		val =
			msm_isp_cal_word_per_line(
			stream_info->output_format,
			stream_info->plane_cfg[plane_idx].
			output_width) << 16 |
			(stream_info->plane_cfg[plane_idx].
			output_height - 1) << 4 | VFE31_BURST_LEN;
		msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	}
	return;
}

static void msm_vfe31_axi_clear_wm_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint32_t val = 0;
	uint32_t wm_base = VFE31_WM_BASE(stream_info->wm[plane_idx]);
	/*WR_IMAGE_SIZE*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x10);
	/*WR_BUFFER_CFG*/
	msm_camera_io_w(val, vfe_dev->vfe_base + wm_base + 0x14);
	return;
}

static void msm_vfe31_axi_cfg_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
}

static void msm_vfe31_axi_clear_wm_xbar_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint8_t plane_idx)
{
	uint8_t wm = stream_info->wm[plane_idx];
	uint32_t xbar_reg_cfg = 0;

	xbar_reg_cfg = msm_camera_io_r(vfe_dev->vfe_base + VFE31_XBAR_BASE(wm));
	xbar_reg_cfg &= ~(0xFF << VFE31_XBAR_SHIFT(wm));
	msm_camera_io_w(xbar_reg_cfg, vfe_dev->vfe_base + VFE31_XBAR_BASE(wm));
}

static void msm_vfe31_cfg_axi_ub(struct vfe_device *vfe_dev)
{
}

static void msm_vfe31_update_ping_pong_addr(struct vfe_device *vfe_dev,
		uint8_t wm_idx, uint32_t pingpong_status, unsigned long paddr)
{
	msm_camera_io_w(paddr, vfe_dev->vfe_base +
		VFE31_PING_PONG_BASE(wm_idx, pingpong_status));
}

static long msm_vfe31_axi_halt(struct vfe_device *vfe_dev)
{
	uint32_t halt_mask;
	uint32_t axi_busy_flag = true;

	msm_camera_io_w(0, vfe_dev->vfe_base + 0x1C);
	msm_camera_io_w(0, vfe_dev->vfe_base + 0x20);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x24);
	msm_camera_io_w(0xFFFFFFFF, vfe_dev->vfe_base + 0x28);
	msm_camera_io_w(0x1, vfe_dev->vfe_base + 0x18);
	msm_camera_io_w_mb(0x2, vfe_dev->vfe_base + 0x1E0);
	msm_camera_io_w_mb(0x1, vfe_dev->vfe_base + 0x1D8);
	while (axi_busy_flag) {
		if (msm_camera_io_r(
			vfe_dev->vfe_base + 0x1DC) & 0x1)
			axi_busy_flag = false;
	}
	msm_camera_io_w_mb(0, vfe_dev->vfe_base + 0x1D8);
	halt_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x20);
	halt_mask &= 0xFEFFFFFF;
	/* Disable AXI IRQ */
	msm_camera_io_w_mb(halt_mask, vfe_dev->vfe_base + 0x20);
	return 0;
}

static uint32_t msm_vfe31_get_wm_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 6) & 0x7F;
}

static uint32_t msm_vfe31_get_comp_mask(
	uint32_t irq_status0, uint32_t irq_status1)
{
	return (irq_status0 >> 21) & 0x7;
}

static uint32_t msm_vfe31_get_pingpong_status(struct vfe_device *vfe_dev)
{
	return msm_camera_io_r(vfe_dev->vfe_base + 0x180);
}

static int msm_vfe31_get_stats_idx(enum msm_isp_stats_type stats_type)
{
	switch (stats_type) {
	case MSM_ISP_STATS_AEC:
	case MSM_ISP_STATS_BG:
		return 0;
	case MSM_ISP_STATS_AF:
	case MSM_ISP_STATS_BF:
		return 1;
	case MSM_ISP_STATS_AWB:
		return 2;
	case MSM_ISP_STATS_RS:
		return 3;
	case MSM_ISP_STATS_CS:
		return 4;
	case MSM_ISP_STATS_IHIST:
		return 5;
	case MSM_ISP_STATS_SKIN:
	case MSM_ISP_STATS_BHIST:
		return 6;
	default:
		pr_err("%s: Invalid stats type\n", __func__);
		return -EINVAL;
	}
}

static void msm_vfe31_stats_cfg_comp_mask(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	return;
}

static void msm_vfe31_stats_cfg_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask |= BIT(STATS_IDX(stream_info->stream_handle) + 13);
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
	return;
}

static void msm_vfe31_stats_clear_wm_irq_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	uint32_t irq_mask;
	irq_mask = msm_camera_io_r(vfe_dev->vfe_base + 0x1C);
	irq_mask &= ~(BIT(STATS_IDX(stream_info->stream_handle) + 13));
	msm_camera_io_w(irq_mask, vfe_dev->vfe_base + 0x1C);
	return;
}

static void msm_vfe31_stats_cfg_wm_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	/*Nothing to configure for VFE3.x*/
	return;
}

static void msm_vfe31_stats_clear_wm_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	/*Nothing to configure for VFE3.x*/
	return;
}

static void msm_vfe31_stats_cfg_ub(struct vfe_device *vfe_dev)
{
	return;
}

static void msm_vfe31_stats_enable_module(struct vfe_device *vfe_dev,
	uint32_t stats_mask, uint8_t enable)
{
	int i;
	uint32_t module_cfg, module_cfg_mask = 0;

	for (i = 0; i < VFE31_NUM_STATS_TYPE; i++) {
		if ((stats_mask >> i) & 0x1) {
			switch (i) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				module_cfg_mask |= 1 << (5 + i);
				break;
			case 5:
				module_cfg_mask |= 1 << 16;
				break;
			case 6:
				module_cfg_mask |= 1 << 19;
				break;
			default:
				pr_err("%s: Invalid stats mask\n", __func__);
				return;
			}
		}
	}

	module_cfg = msm_camera_io_r(vfe_dev->vfe_base + 0x10);
	if (enable)
		module_cfg |= module_cfg_mask;
	else
		module_cfg &= ~module_cfg_mask;
	msm_camera_io_w(module_cfg, vfe_dev->vfe_base + 0x10);
}

static void msm_vfe31_stats_update_ping_pong_addr(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status,
	unsigned long paddr)
{
	int stats_idx = STATS_IDX(stream_info->stream_handle);
	msm_camera_io_w(paddr, vfe_dev->vfe_base +
		VFE31_STATS_PING_PONG_BASE(stats_idx, pingpong_status));
}

static uint32_t msm_vfe31_stats_get_wm_mask(uint32_t irq_status0,
	uint32_t irq_status1)
{
	return (irq_status0 >> 13) & 0x7F;
}

static uint32_t msm_vfe31_stats_get_comp_mask(uint32_t irq_status0,
	uint32_t irq_status1)
{
	return (irq_status0 >> 24) & 0x1;
}

static uint32_t msm_vfe31_stats_get_frame_id(struct vfe_device *vfe_dev)
{
	return vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
}

static int msm_vfe31_get_platform_data(struct vfe_device *vfe_dev)
{
	int rc = 0;
	vfe_dev->vfe_mem = platform_get_resource_byname(vfe_dev->pdev,
					IORESOURCE_MEM, "msm_vfe");
	if (!vfe_dev->vfe_mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}

	vfe_dev->vfe_irq = platform_get_resource_byname(vfe_dev->pdev,
					IORESOURCE_IRQ, "msm_vfe");
	if (!vfe_dev->vfe_irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto vfe_no_resource;
	}
	vfe_dev->camif_mem = platform_get_resource_byname(vfe_dev->pdev,
		IORESOURCE_MEM, "msm_camif");
	if (!vfe_dev->camif_mem)
		pr_err("%s: camif not supported\n", __func__);

vfe_no_resource:
	return rc;
}

int msm_vfe31_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vfe_device *vfe_dev = v4l2_get_subdevdata(sd);
	ISP_DBG("%s\n", __func__);

	mutex_lock(&vfe_dev->realtime_mutex);
	mutex_lock(&vfe_dev->core_mutex);
	if (vfe_dev->vfe_open_cnt == 1) {
		pr_err("VFE already open\n");
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return -ENODEV;
	}

	if (vfe_dev->hw_info->vfe_ops.core_ops.init_hw(vfe_dev) < 0) {
		pr_err("%s: init hardware failed\n", __func__);
		mutex_unlock(&vfe_dev->core_mutex);
		mutex_unlock(&vfe_dev->realtime_mutex);
		return -EBUSY;
	}
	vfe_dev->vfe_hw_version = msm_camera_io_r(vfe_dev->vfe_base);
	ISP_DBG("%s: HW Version: 0x%x\n", __func__, vfe_dev->vfe_hw_version);

	vfe_dev->buf_mgr->ops->buf_mgr_init(vfe_dev->buf_mgr, "msm_isp", 28);

	vfe_dev->soc_hw_version = 0x00;

	memset(&vfe_dev->axi_data, 0, sizeof(struct msm_vfe_axi_shared_data));
	memset(&vfe_dev->stats_data, 0,
		sizeof(struct msm_vfe_stats_shared_data));
	memset(&vfe_dev->error_info, 0, sizeof(vfe_dev->error_info));
	vfe_dev->axi_data.hw_info = vfe_dev->hw_info->axi_hw_info;
	vfe_dev->vfe_open_cnt++;
	vfe_dev->taskletq_idx = 0;
	mutex_unlock(&vfe_dev->core_mutex);
	mutex_unlock(&vfe_dev->realtime_mutex);
	return 0;
}

static void msm_vfe31_get_error_mask(uint32_t *error_mask0,
	uint32_t *error_mask1)
{
	*error_mask0 = 0x00000000;
	*error_mask1 = 0x003FFFFF;
}

struct msm_vfe_axi_hardware_info msm_vfe31_axi_hw_info = {
	.num_wm = 6,
	.num_comp_mask = 3,
	.num_rdi = 3,
	.num_rdi_master = 3,
};

static struct msm_vfe_stats_hardware_info msm_vfe31_stats_hw_info = {
	.stats_capability_mask =
		1 << MSM_ISP_STATS_AEC | 1 << MSM_ISP_STATS_BG |
		1 << MSM_ISP_STATS_AF | 1 << MSM_ISP_STATS_BF |
		1 << MSM_ISP_STATS_AWB | 1 << MSM_ISP_STATS_IHIST |
		1 << MSM_ISP_STATS_RS | 1 << MSM_ISP_STATS_CS |
		1 << MSM_ISP_STATS_SKIN | 1 << MSM_ISP_STATS_BHIST,
	.stats_ping_pong_offset = VFE31_STATS_PING_PONG_OFFSET,
	.num_stats_type = VFE31_NUM_STATS_TYPE,
	.num_stats_comp_mask = 0,
};

static struct v4l2_subdev_core_ops msm_vfe31_subdev_core_ops = {
	.ioctl = msm_isp_ioctl,
	.subscribe_event = msm_isp_subscribe_event,
	.unsubscribe_event = msm_isp_unsubscribe_event,
};

static struct v4l2_subdev_ops msm_vfe31_subdev_ops = {
	.core = &msm_vfe31_subdev_core_ops,
};

static struct v4l2_subdev_internal_ops msm_vfe31_internal_ops = {
	.open = msm_vfe31_open_node,
	.close = msm_isp_close_node,
};

struct msm_vfe_hardware_info vfe31_hw_info = {
	.vfe_ops = {
		.irq_ops = {
			.read_irq_status = msm_vfe31_read_irq_status,
			.process_camif_irq = msm_vfe31_process_camif_irq,
			.process_reset_irq = msm_vfe31_process_reset_irq,
			.process_halt_irq = msm_vfe31_process_halt_irq,
			.process_reg_update = msm_vfe31_process_reg_update,
			.process_axi_irq = msm_isp_process_axi_irq,
			.process_stats_irq = msm_isp_process_stats_irq,
		},
		.axi_ops = {
			.reload_wm = msm_vfe31_axi_reload_wm,
			.enable_wm = msm_vfe31_axi_enable_wm,
			.cfg_io_format = msm_vfe31_cfg_io_format,
			.cfg_comp_mask = msm_vfe31_axi_cfg_comp_mask,
			.clear_comp_mask = msm_vfe31_axi_clear_comp_mask,
			.cfg_wm_irq_mask = msm_vfe31_axi_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe31_axi_clear_wm_irq_mask,
			.cfg_framedrop = msm_vfe31_cfg_framedrop,
			.clear_framedrop = msm_vfe31_clear_framedrop,
			.reserve_wm = msm_vfe31_axi_reserve_wm,
			.cfg_wm_reg = msm_vfe31_axi_cfg_wm_reg,
			.clear_wm_reg = msm_vfe31_axi_clear_wm_reg,
			.cfg_wm_xbar_reg = msm_vfe31_axi_cfg_wm_xbar_reg,
			.clear_wm_xbar_reg = msm_vfe31_axi_clear_wm_xbar_reg,
			.cfg_ub = msm_vfe31_cfg_axi_ub,
			.update_ping_pong_addr =
				msm_vfe31_update_ping_pong_addr,
			.get_comp_mask = msm_vfe31_get_comp_mask,
			.get_wm_mask = msm_vfe31_get_wm_mask,
			.get_pingpong_status = msm_vfe31_get_pingpong_status,
			.halt = msm_vfe31_axi_halt,
		},
		.core_ops = {
			.reg_update = msm_vfe31_reg_update,
			.cfg_camif = msm_vfe31_cfg_camif,
			.update_camif_state = msm_vfe31_update_camif_state,
			.cfg_rdi_reg = msm_vfe31_cfg_rdi_reg,
			.reset_hw = msm_vfe31_reset_hardware,
			.init_hw = msm_vfe31_init_hardware,
			.init_hw_reg = msm_vfe31_init_hardware_reg,
			.release_hw = msm_vfe31_release_hardware,
			.get_platform_data = msm_vfe31_get_platform_data,
			.get_error_mask = msm_vfe31_get_error_mask,
			.process_error_status = msm_vfe31_process_error_status,
		},
		.stats_ops = {
			.get_stats_idx = msm_vfe31_get_stats_idx,
			.cfg_comp_mask = msm_vfe31_stats_cfg_comp_mask,
			.cfg_wm_irq_mask = msm_vfe31_stats_cfg_wm_irq_mask,
			.clear_wm_irq_mask = msm_vfe31_stats_clear_wm_irq_mask,
			.cfg_wm_reg = msm_vfe31_stats_cfg_wm_reg,
			.clear_wm_reg = msm_vfe31_stats_clear_wm_reg,
			.cfg_ub = msm_vfe31_stats_cfg_ub,
			.enable_module = msm_vfe31_stats_enable_module,
			.update_ping_pong_addr =
				msm_vfe31_stats_update_ping_pong_addr,
			.get_comp_mask = msm_vfe31_stats_get_comp_mask,
			.get_wm_mask = msm_vfe31_stats_get_wm_mask,
			.get_frame_id = msm_vfe31_stats_get_frame_id,
			.get_pingpong_status = msm_vfe31_get_pingpong_status,
		},
	},
	.dmi_reg_offset = 0x5A0,
	.axi_hw_info = &msm_vfe31_axi_hw_info,
	.stats_hw_info = &msm_vfe31_stats_hw_info,
	.subdev_ops = &msm_vfe31_subdev_ops,
	.subdev_internal_ops = &msm_vfe31_internal_ops,
};
EXPORT_SYMBOL(vfe31_hw_info);
