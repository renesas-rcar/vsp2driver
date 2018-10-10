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

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_rwpf.h"
#include "vsp2_video.h"
#include "vsp2_pipe.h"

#define RWPF_MIN_WIDTH				1
#define RWPF_MIN_HEIGHT				1

struct v4l2_rect *vsp2_rwpf_get_crop(struct vsp2_rwpf *rwpf,
				     struct v4l2_subdev_pad_config *config)
{
	return v4l2_subdev_get_try_crop(&rwpf->entity.subdev, config,
					RWPF_PAD_SINK);
}

static struct v4l2_rect *vsp2_rwpf_get_compose(
	struct vsp2_rwpf *rwpf,
	struct v4l2_subdev_pad_config *config)
{
	return v4l2_subdev_get_try_compose(&rwpf->entity.subdev, config,
					   RWPF_PAD_SOURCE);
}

int vsp2_rwpf_check_compose_size(struct vsp2_entity *entity)
{
	struct vsp2_rwpf *wpf = entity_to_rwpf(entity);
	const struct v4l2_mbus_framefmt *format;
	const struct v4l2_rect *compose;
	int ret = 0;

	if (entity->type != VSP2_ENTITY_WPF)
		return 0;

	format = vsp2_entity_get_pad_format(entity, entity->config,
					    RWPF_PAD_SINK);

	compose = vsp2_entity_get_pad_selection(entity,
						entity->config,
						RWPF_PAD_SOURCE,
						V4L2_SEL_TGT_COMPOSE);

	if (!wpf->rotinfo.swap_sizes) {
		if (format->width != compose->width ||
		    format->height != compose->height)
			ret = -EINVAL;
	} else {
		if (format->width != compose->height ||
		    format->height != compose->width)
			ret = -EINVAL;
	}

	return ret;
}

void vsp2_rwpf_get_csc_element(struct vsp2_entity *entity, unsigned int *mbus,
			       unsigned char *ycbcr_enc,
			       unsigned char *quantization)
{
	struct vsp2_rwpf *rwpf = entity_to_rwpf(entity);
	const struct v4l2_pix_format_mplane *format = &rwpf->format;

	*mbus = rwpf->fmtinfo->mbus;
	*ycbcr_enc = format->ycbcr_enc;
	*quantization = format->quantization;
}

void vsp2_rwpf_set_csc_mode(struct vsp2_entity *entity, int csc_mode)
{
	struct vsp2_rwpf *rwpf = entity_to_rwpf(entity);

	rwpf->csc_mode = csc_mode;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

static int vsp2_rwpf_enum_mbus_code(struct v4l2_subdev *subdev,
				    struct v4l2_subdev_pad_config *cfg,
				    struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		MEDIA_BUS_FMT_ARGB8888_1X32,
		MEDIA_BUS_FMT_AYUV8_1X32,
	};

	if (code->index >= ARRAY_SIZE(codes))
		return -EINVAL;

	code->code = codes[code->index];

	return 0;
}

static int vsp2_rwpf_enum_frame_size(struct v4l2_subdev *subdev,
				     struct v4l2_subdev_pad_config *cfg,
				     struct v4l2_subdev_frame_size_enum *fse)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);

	return vsp2_subdev_enum_frame_size(subdev, cfg, fse, RWPF_MIN_WIDTH,
					   RWPF_MIN_HEIGHT, rwpf->max_width,
					   rwpf->max_height);
}

static int vsp2_rwpf_set_format(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_format *fmt)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *compose;
	int ret = 0;

	mutex_lock(&rwpf->entity.lock);

	config = vsp2_entity_get_pad_config(&rwpf->entity, cfg, fmt->which);
	if (!config) {
		ret = -EINVAL;
		goto done;
	}

	/* Default to YUV if the requested format is not supported. */
	if (fmt->format.code != MEDIA_BUS_FMT_ARGB8888_1X32 &&
	    fmt->format.code != MEDIA_BUS_FMT_AYUV8_1X32)
		fmt->format.code = MEDIA_BUS_FMT_AYUV8_1X32;

	format = vsp2_entity_get_pad_format(&rwpf->entity, config, fmt->pad);

	if (fmt->pad == RWPF_PAD_SOURCE) {
		/* for WPF compose */
		if (rwpf->entity.type == VSP2_ENTITY_WPF) {
			struct v4l2_mbus_framefmt *sink;
			unsigned int width;
			unsigned int height;

			width = clamp_t(unsigned int, fmt->format.width,
					RWPF_MIN_WIDTH, rwpf->max_width);
			height = clamp_t(unsigned int, fmt->format.height,
					 RWPF_MIN_HEIGHT, rwpf->max_height);
			sink = vsp2_entity_get_pad_format(&rwpf->entity, config,
							  RWPF_PAD_SINK);
			if (width == sink->width && height == sink->height) {
				/*
				 * If same sizes are set for sink pad and source
				 * pad, width and height are depend on rotaion
				 * angle.
				 */
				if (rwpf->rotinfo.swap_sizes) {
					format->width = height;
					format->height = width;
				} else {
					format->width = width;
					format->height = height;
				}
			} else {
				/*
				 * If different sizes are set for sink pad and
				 * source pad, do not auto size swap.
				 */
				format->width = width;
				format->height = height;
			}
			compose = vsp2_rwpf_get_compose(rwpf, config);
			compose->left = 0;
			compose->top = 0;
			compose->width = format->width;
			compose->height = format->height;
		}
		/* The RWPF performs format conversion, the format code can be
		 * changed on the source pad.
		 * Can't scale except compose in WPF
		 */
		format->code = fmt->format.code;
		fmt->format = *format;
		goto done;
	}

	format->code = fmt->format.code;
	format->width = clamp_t(unsigned int, fmt->format.width,
				RWPF_MIN_WIDTH, rwpf->max_width);
	format->height = clamp_t(unsigned int, fmt->format.height,
				 RWPF_MIN_HEIGHT, rwpf->max_height);
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	fmt->format = *format;

	if (rwpf->entity.type == VSP2_ENTITY_RPF) {
		struct v4l2_rect *crop;

		/* Update the sink crop rectangle. */
		crop = vsp2_rwpf_get_crop(rwpf, config);
		crop->left = 0;
		crop->top = 0;
		crop->width = fmt->format.width;
		crop->height = fmt->format.height;
	}

	/* Propagate the format to the source pad. */
	format = vsp2_entity_get_pad_format(&rwpf->entity, config,
					    RWPF_PAD_SOURCE);
	*format = fmt->format;
	if (rwpf->rotinfo.swap_sizes) {
		format->width = fmt->format.height;
		format->height = fmt->format.width;
	}
	if (rwpf->entity.type == VSP2_ENTITY_WPF) {
		/* Propagate the format to the compose size. */
		compose = vsp2_rwpf_get_compose(rwpf, config);
		compose->left = 0;
		compose->top = 0;
		compose->width = format->width;
		compose->height = format->height;
	}

done:
	mutex_unlock(&rwpf->entity.lock);
	return ret;
}

static int vsp2_rwpf_get_selection(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_selection *sel)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	/* Cropping is only supported on the RPF and is implemented on the sink
	 * pad.
	 * Composing is only supported on the WPF and is implemented on the
	 * source pad.
	 */
	if (rwpf->entity.type == VSP2_ENTITY_WPF) {
		if (sel->pad != RWPF_PAD_SOURCE)
			return -EINVAL;
		if (sel->target != V4L2_SEL_TGT_COMPOSE &&
		    sel->target != V4L2_SEL_TGT_COMPOSE_BOUNDS)
			return -EINVAL;
	} else {
		if (sel->pad != RWPF_PAD_SINK)
			return -EINVAL;
		if (sel->target != V4L2_SEL_TGT_CROP &&
		    sel->target != V4L2_SEL_TGT_CROP_BOUNDS)
			return -EINVAL;
	}

	mutex_lock(&rwpf->entity.lock);

	config = vsp2_entity_get_pad_config(&rwpf->entity, cfg, sel->which);
	if (!config) {
		ret = -EINVAL;
		goto done;
	}

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *vsp2_rwpf_get_crop(rwpf, config);
		break;

	case V4L2_SEL_TGT_CROP_BOUNDS:
		format = vsp2_entity_get_pad_format(&rwpf->entity, config,
						    RWPF_PAD_SINK);
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = format->width;
		sel->r.height = format->height;
		break;

	case V4L2_SEL_TGT_COMPOSE:
		sel->r = *vsp2_rwpf_get_compose(rwpf, config);
		break;

	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		format = vsp2_entity_get_pad_format(&rwpf->entity, config,
						    RWPF_PAD_SOURCE);
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = format->width;
		sel->r.height = format->height;
		break;

	default:
		ret = -EINVAL;
		break;
	}

done:
	mutex_unlock(&rwpf->entity.lock);
	return ret;
}

static int vsp2_rwpf_set_selection(struct v4l2_subdev *subdev,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_selection *sel)
{
	struct vsp2_rwpf *rwpf = to_rwpf(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *crop;
	int ret = 0;

	/* Cropping is only supported on the RPF and is implemented on the sink
	 * pad.
	 * Composing is only supported on the WPF and is implemented on the
	 * source pad.
	 */
	if (rwpf->entity.type == VSP2_ENTITY_WPF) {
		if (sel->pad != RWPF_PAD_SOURCE)
			return -EINVAL;
		if (sel->target != V4L2_SEL_TGT_COMPOSE)
			return -EINVAL;
	} else {
		if (sel->pad != RWPF_PAD_SINK)
			return -EINVAL;
		if (sel->target != V4L2_SEL_TGT_CROP)
			return -EINVAL;
	}

	mutex_lock(&rwpf->entity.lock);

	config = vsp2_entity_get_pad_config(&rwpf->entity, cfg, sel->which);
	if (!config) {
		ret = -EINVAL;
		goto done;
	}

	if (sel->target == V4L2_SEL_TGT_COMPOSE) {
		/* WPF compose */
		struct v4l2_rect *compose;

		/* check size, not adjust size */
		format = vsp2_entity_get_pad_format(&rwpf->entity, config,
						    RWPF_PAD_SOURCE);
		if (sel->r.left + sel->r.width > format->width) {
			ret = -EINVAL;
			goto done;
		}
		if (sel->r.top + sel->r.height > format->height) {
			ret = -EINVAL;
			goto done;
		}
		compose = vsp2_rwpf_get_compose(rwpf, config);
		*compose = sel->r;
		goto done;
	}

	/* Make sure the crop rectangle is entirely contained in the image. */
	format = vsp2_entity_get_pad_format(&rwpf->entity, config,
					    RWPF_PAD_SINK);

	if (format->code == MEDIA_BUS_FMT_AYUV8_1X32) {
		sel->r.left = (sel->r.left + 1) & ~1;
		sel->r.top = (sel->r.top + 1) & ~1;
		sel->r.width = (sel->r.width) & ~1;
		sel->r.height = (sel->r.height) & ~1;
	}

	sel->r.left = min_t(unsigned int, sel->r.left, format->width - 2);
	sel->r.top = min_t(unsigned int, sel->r.top, format->height - 2);
	sel->r.width = min_t(unsigned int, sel->r.width,
			     format->width - sel->r.left);
	sel->r.height = min_t(unsigned int, sel->r.height,
			      format->height - sel->r.top);

	crop = vsp2_rwpf_get_crop(rwpf, config);
	*crop = sel->r;

	/* Propagate the format to the source pad. */
	format = vsp2_entity_get_pad_format(&rwpf->entity, config,
					    RWPF_PAD_SOURCE);
	format->width = crop->width;
	format->height = crop->height;

done:
	mutex_unlock(&rwpf->entity.lock);
	return ret;
}

const struct v4l2_subdev_pad_ops vsp2_rwpf_pad_ops = {
	.init_cfg = vsp2_entity_init_cfg,
	.enum_mbus_code = vsp2_rwpf_enum_mbus_code,
	.enum_frame_size = vsp2_rwpf_enum_frame_size,
	.get_fmt = vsp2_subdev_get_pad_format,
	.set_fmt = vsp2_rwpf_set_format,
	.get_selection = vsp2_rwpf_get_selection,
	.set_selection = vsp2_rwpf_set_selection,
};

/* -----------------------------------------------------------------------------
 * Controls
 */

static int vsp2_rwpf_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vsp2_rwpf *rwpf =
		container_of(ctrl->handler, struct vsp2_rwpf, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_ALPHA_COMPONENT:
		rwpf->alpha = ctrl->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops vsp2_rwpf_ctrl_ops = {
	.s_ctrl = vsp2_rwpf_s_ctrl,
};

int vsp2_rwpf_init_ctrls(struct vsp2_rwpf *rwpf)
{
	rwpf->alpha = 255;

	v4l2_ctrl_handler_init(&rwpf->ctrls, 1);
	v4l2_ctrl_new_std(&rwpf->ctrls, &vsp2_rwpf_ctrl_ops,
			  V4L2_CID_ALPHA_COMPONENT, 0, 255, 1, 255);

	rwpf->entity.subdev.ctrl_handler = &rwpf->ctrls;

	return rwpf->ctrls.error;
}
