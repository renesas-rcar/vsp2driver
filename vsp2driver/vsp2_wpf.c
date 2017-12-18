/*************************************************************************/ /*
 * VSP2
 *
 * Copyright (C) 2015-2017 Renesas Electronics Corporation
 *
 * License        Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 * IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * GPLv2:
 * If you wish to use this file under the terms of GPL, following terms are
 * effective.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */ /*************************************************************************/

#include <linux/device.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_pipe.h"
#include "vsp2_rwpf.h"
#include "vsp2_video.h"
#include "vsp2_vspm.h"
#include "vsp2_debug.h"

#define WPF_MAX_WIDTH				8190
#define WPF_MAX_HEIGHT				8190

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static int wpf_s_stream(struct v4l2_subdev *subdev, int enable)
{
	return 0;
}

/* -----------------------------------------------------------------------------
 * for debug
 */

#ifdef VSP2_DEBUG
static long vsp2_debug_ioctl(
	struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct vsp2_rwpf	*wpf  = to_rwpf(subdev);
	struct vsp2_device	*vsp2 = wpf->entity.vsp2;

	switch (cmd) {
	case VIDIOC_VSP2_DEBUG:
		vsp2_debug(vsp2, arg);
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

static const struct v4l2_subdev_core_ops vsp2_debug_ops = {
	.ioctl = vsp2_debug_ioctl,
};
#endif

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static const struct v4l2_subdev_video_ops wpf_video_ops = {
	.s_stream = wpf_s_stream,
};

static const struct v4l2_subdev_ops wpf_ops = {
#ifdef VSP2_DEBUG
	.core	= &vsp2_debug_ops,
#endif
	.video	= &wpf_video_ops,
	.pad    = &vsp2_rwpf_pad_ops,
};

struct v4l2tovspm_rotation {
	s32 rotangle;
	bool hflip;
	bool vflip;
	unsigned char vspm_rotation;
};

static struct v4l2tovspm_rotation v4l2tovspm[16] = {
	{ 0, 0, 0,		VSP_ROT_OFF },
	{ 90, 0, 0,		VSP_ROT_90 },
	{ 180, 0, 0,		VSP_ROT_180 },
	{ 270, 0, 0,		VSP_ROT_270 },

	{ 0, 1, 0,		VSP_ROT_H_FLIP },
	{ 90, 1, 0,		VSP_ROT_90_H_FLIP },
	{ 180, 1, 0,		VSP_ROT_V_FLIP },
	{ 270, 1, 0,		VSP_ROT_90_V_FLIP },

	{ 0, 1, 1,		VSP_ROT_180 },
	{ 90, 1, 1,		VSP_ROT_270 },
	{ 180, 1, 1,		VSP_ROT_OFF },
	{ 270, 1, 1,		VSP_ROT_90 },

	{ 0, 0, 1,		VSP_ROT_V_FLIP },
	{ 90, 0, 1,		VSP_ROT_90_V_FLIP },
	{ 180, 0, 1,		VSP_ROT_H_FLIP },
	{ 270, 0, 1,		VSP_ROT_90_H_FLIP },
};

/* -----------------------------------------------------------------------------
 * VSP2 Entity Operations
 */

static void vsp2_wpf_destroy(struct vsp2_entity *entity)
{
}

static void wpf_set_memory(struct vsp2_entity *entity)
{
	struct vsp2_rwpf *wpf = entity_to_rwpf(entity);
	struct v4l2_pix_format_mplane *format = &wpf->format;
	const struct vsp2_format_info *fmtinfo = wpf->fmtinfo;
	const struct v4l2_rect *compose;

	struct vsp_start_t *vsp_par =
		wpf->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_dst_t *vsp_out = vsp_par->dst_par;

	compose = vsp2_entity_get_pad_selection(&wpf->entity,
						wpf->entity.config,
						RWPF_PAD_SOURCE,
						V4L2_SEL_TGT_COMPOSE);
	vsp_out->addr = ((unsigned int)wpf->mem.addr[0])
			+ (format->plane_fmt[0].bytesperline * compose->top)
			+ compose->left * (fmtinfo->bpp[0] / 8);

	vsp_out->addr_c0 = (unsigned int)wpf->mem.addr[1];
	vsp_out->addr_c1 = (unsigned int)wpf->mem.addr[2];

	if (vsp_out->addr_c0) {
		vsp_out->addr_c0 +=
			(format->plane_fmt[1].bytesperline * compose->top
			 / fmtinfo->vsub)
			+ (compose->left * (fmtinfo->bpp[1] / 8)
			   / fmtinfo->hsub);
	}
	if (vsp_out->addr_c1) {
		vsp_out->addr_c1 +=
			(format->plane_fmt[2].bytesperline * compose->top
			 / fmtinfo->vsub)
			+ (compose->left * (fmtinfo->bpp[2] / 8)
			   / fmtinfo->hsub);
	}
}

static void wpf_configure(struct vsp2_entity *entity,
			  struct vsp2_pipeline *pipe)
{
	struct vsp2_rwpf *wpf = to_rwpf(&entity->subdev);
	struct v4l2_pix_format_mplane *format = &wpf->format;
	const struct v4l2_mbus_framefmt *source_format;
	const struct v4l2_mbus_framefmt *sink_format;
	const struct vsp2_format_info *fmtinfo = wpf->fmtinfo;
	u32 outfmt = 0;
	u32 stride_y = 0;
	u32 stride_c = 0;
	struct vsp_start_t *vsp_par =
		wpf->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_dst_t *vsp_out = vsp_par->dst_par;
	const struct v4l2_rect *compose;
	u16 vspm_format;

	/* Destination stride. */
	stride_y = format->plane_fmt[0].bytesperline;
	if (format->num_planes > 1)
		stride_c = format->plane_fmt[1].bytesperline;

	vsp_out->stride			= stride_y;
	if (format->num_planes > 1)
		vsp_out->stride_c	= stride_c;

	/* Format */
	sink_format = vsp2_entity_get_pad_format(&wpf->entity,
						 wpf->entity.config,
						 RWPF_PAD_SINK);
	source_format = vsp2_entity_get_pad_format(&wpf->entity,
						   wpf->entity.config,
						   RWPF_PAD_SOURCE);

	compose = vsp2_entity_get_pad_selection(&wpf->entity,
						wpf->entity.config,
						RWPF_PAD_SOURCE,
						V4L2_SEL_TGT_COMPOSE);
	vsp_out->width		= compose->width;
	vsp_out->height		= compose->height;
	vsp_out->x_offset	= 0;
	vsp_out->y_offset	= 0;
	vsp_out->x_coffset	= 0;
	vsp_out->y_coffset	= 0;

	outfmt = fmtinfo->hwfmt << VI6_WPF_OUTFMT_WRFMT_SHIFT;

	if (fmtinfo->alpha)
		outfmt |= VI6_WPF_OUTFMT_PXA;
	if (fmtinfo->swap_yc)
		outfmt |= VI6_WPF_OUTFMT_SPYCS;
	if (fmtinfo->swap_uv)
		outfmt |= VI6_WPF_OUTFMT_SPUVS;

	vsp_out->swap		= fmtinfo->swap;

	if (sink_format->code != source_format->code)
		outfmt |= VI6_WPF_OUTFMT_CSC;

	outfmt |= wpf->alpha << VI6_WPF_OUTFMT_PDV_SHIFT;

	/* Take the control handler lock to ensure that the PDV value won't be
	 * changed behind our back by a set control operation.
	 */
	vspm_format = (u16)(outfmt & 0x007F);
	if (vspm_format < 0x0040) {
		/* RGB format. */
		/* Set bytes per pixel. */
		vspm_format	|= (fmtinfo->bpp[0] / 8) << 8;
	} else {
		/* YUV format. */
		/* Set SPYCS and SPUVS */
		vspm_format	|= (outfmt & 0xC000);
	}
	vsp_out->format		= vspm_format;
	vsp_out->csc		= (outfmt & (1 <<  8)) >>  8;
	vsp_out->clrcng		= (outfmt & (1 <<  9)) >>  9;
	vsp_out->iturbt		= (outfmt & (3 << 10)) >> 10;
	vsp_out->dith		= (outfmt & (3 << 12)) >> 12;
	vsp_out->pxa		= (outfmt & (1 << 23)) >> 23;

	vsp_out->pad = (outfmt & (0xff << 24)) >> 24;

	vsp_out->cbrm		= VSP_CSC_ROUND_DOWN;
	vsp_out->abrm		= VSP_CONVERSION_ROUNDDOWN;
	vsp_out->athres		= 0;
	vsp_out->clmd		= VSP_CLMD_NO;
	vsp_out->rotation	= wpf->rotinfo.rotation;
	if (wpf->fcp_fcnl) {
		vsp_out->fcp->fcnl = FCP_FCNL_ENABLE;
		vsp_out->swap = VSP_SWAP_LL;
	} else {
		vsp_out->fcp->fcnl = FCP_FCNL_DISABLE;
	}
}

static void set_rotation(struct vsp2_rwpf *wpf, bool hflip,
			 bool vflip, s32 rotangle)
{
	struct v4l2_mbus_framefmt *sink_format;
	struct v4l2_mbus_framefmt *source_format;
	struct v4l2_rect *compose;
	bool swap_work;
	int i = 0;

	wpf->rotinfo.rotation = VSP_ROT_OFF;
	for (i = 0; i  < ARRAY_SIZE(v4l2tovspm); i++) {
		if (v4l2tovspm[i].hflip == hflip &&
		    v4l2tovspm[i].vflip == vflip &&
		    v4l2tovspm[i].rotangle == rotangle) {
			wpf->rotinfo.rotation = v4l2tovspm[i].vspm_rotation;
		}
	}

	swap_work = wpf->rotinfo.swap_sizes;
	sink_format = vsp2_entity_get_pad_format(&wpf->entity,
						 wpf->entity.config,
						 RWPF_PAD_SINK);
	source_format = vsp2_entity_get_pad_format(&wpf->entity,
						   wpf->entity.config,
						   RWPF_PAD_SOURCE);
	switch (wpf->rotinfo.rotation) {
	case VSP_ROT_90:
	case VSP_ROT_90_V_FLIP:
	case VSP_ROT_90_H_FLIP:
	case VSP_ROT_270:
		wpf->rotinfo.swap_sizes = true;
		break;
	default:
		wpf->rotinfo.swap_sizes = false;
		break;
	}
	/*
	 * If different sizes are set for sink pad and source pad, do not auto
	 * size swap.
	 */
	if (swap_work) {
		if (sink_format->width != source_format->height ||
		    sink_format->height != source_format->width)
			return;
	} else {
		if (sink_format->width != source_format->width ||
		    sink_format->height != source_format->height)
			return;
	}

	mutex_lock(&wpf->entity.lock);
	compose = vsp2_entity_get_pad_selection(&wpf->entity,
						wpf->entity.config,
						RWPF_PAD_SOURCE,
						V4L2_SEL_TGT_COMPOSE);
	if (wpf->rotinfo.swap_sizes) {
		source_format->width = sink_format->height;
		source_format->height = sink_format->width;
	} else {
		source_format->width = sink_format->width;
		source_format->height = sink_format->height;
	}
	compose->width = source_format->width;
	compose->height = source_format->height;
	mutex_unlock(&wpf->entity.lock);
}

static int vsp2_wpf_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vsp2_rwpf *wpf = container_of(ctrl->handler,
					     struct vsp2_rwpf, ctrls);
	struct vsp2_video *video = wpf->video;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_ROTATE:
		mutex_lock(&video->lock);
		if (vb2_is_busy(&video->queue))
			ret = -EBUSY;
		else
			set_rotation(wpf, wpf->rotinfo.hflip->val,
				     wpf->rotinfo.vflip->val,
				     wpf->rotinfo.rotangle->val);
		mutex_unlock(&video->lock);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static const struct vsp2_entity_operations wpf_entity_ops = {
	.destroy = vsp2_wpf_destroy,
	.set_memory = wpf_set_memory,
	.configure = wpf_configure,
};

static const struct v4l2_ctrl_ops vsp2_wpf_ctrl_ops = {
	.s_ctrl = vsp2_wpf_s_ctrl,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_rwpf *vsp2_wpf_create(struct vsp2_device *vsp2, unsigned int index)
{
	struct vsp2_rwpf *wpf;
	char name[6];
	int ret;

	wpf = devm_kzalloc(vsp2->dev, sizeof(*wpf), GFP_KERNEL);
	if (wpf == NULL)
		return ERR_PTR(-ENOMEM);

	wpf->max_width = WPF_MAX_WIDTH;
	wpf->max_height = WPF_MAX_HEIGHT;

	wpf->entity.ops = &wpf_entity_ops;
	wpf->entity.type = VSP2_ENTITY_WPF;
	wpf->entity.index = index;

	sprintf(name, "wpf.%u", index);
	ret = vsp2_entity_init(vsp2, &wpf->entity, name, 2, &wpf_ops,
			       MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the control handler. */
	ret = vsp2_rwpf_init_ctrls(wpf);
	if (ret < 0) {
		dev_err(vsp2->dev, "wpf%u: failed to initialize controls\n",
			index);
		goto error;
	}
	wpf->rotinfo.vflip = v4l2_ctrl_new_std(&wpf->ctrls,
					       &vsp2_wpf_ctrl_ops,
					       V4L2_CID_VFLIP, 0, 1, 1, 0);
	wpf->rotinfo.hflip = v4l2_ctrl_new_std(&wpf->ctrls,
					       &vsp2_wpf_ctrl_ops,
					       V4L2_CID_HFLIP, 0, 1, 1, 0);
	wpf->rotinfo.rotangle = v4l2_ctrl_new_std(&wpf->ctrls,
						  &vsp2_wpf_ctrl_ops,
						  V4L2_CID_ROTATE,
						  0, 270, 90, 0);
	if (wpf->ctrls.error) {
		ret = wpf->ctrls.error;
		goto error;
	}
	wpf->fcp_fcnl = FCP_FCNL_DEF_VALUE;

	return wpf;

error:
	vsp2_entity_destroy(&wpf->entity);
	return ERR_PTR(ret);
}
