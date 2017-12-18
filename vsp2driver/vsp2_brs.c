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
#include <linux/gfp.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_brs.h"
#include "vsp2_pipe.h"
#include "vsp2_rwpf.h"
#include "vsp2_video.h"
#include "vsp2_vspm.h"

#define BRS_MIN_SIZE				1U
#define BRS_MAX_SIZE				8190U

/* -----------------------------------------------------------------------------
 * Controls
 */

static int brs_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct vsp2_brs *brs =
		container_of(ctrl->handler, struct vsp2_brs, ctrls);

	switch (ctrl->id) {
	case V4L2_CID_BG_COLOR:
		brs->bgcolor = ctrl->val;
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops brs_ctrl_ops = {
	.s_ctrl = brs_s_ctrl,
};

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

/*
 * The BRS can't perform format conversion, all sink and source formats must be
 * identical. We pick the format on the first sink pad (pad 0) and propagate it
 * to all other pads.
 */

static int brs_enum_mbus_code(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		MEDIA_BUS_FMT_ARGB8888_1X32,
		MEDIA_BUS_FMT_AYUV8_1X32,
	};

	return vsp2_subdev_enum_mbus_code(subdev, cfg, code, codes,
					  ARRAY_SIZE(codes));
}

static int brs_enum_frame_size(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	if (fse->index)
		return -EINVAL;

	if (fse->code != MEDIA_BUS_FMT_ARGB8888_1X32 &&
	    fse->code != MEDIA_BUS_FMT_AYUV8_1X32)
		return -EINVAL;

	fse->min_width = BRS_MIN_SIZE;
	fse->max_width = BRS_MAX_SIZE;
	fse->min_height = BRS_MIN_SIZE;
	fse->max_height = BRS_MAX_SIZE;

	return 0;
}

static struct v4l2_rect *brs_get_compose(struct vsp2_brs *brs,
					 struct v4l2_subdev_pad_config *cfg,
					 unsigned int pad)
{
	return v4l2_subdev_get_try_compose(&brs->entity.subdev, cfg, pad);
}

static void brs_try_format(struct vsp2_brs *brs,
			   struct v4l2_subdev_pad_config *config,
			   unsigned int pad, struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_mbus_framefmt *format;

	switch (pad) {
	case BRS_PAD_SINK(0):
		/* Default to YUV if the requested format is not supported. */
		if (fmt->code != MEDIA_BUS_FMT_ARGB8888_1X32 &&
		    fmt->code != MEDIA_BUS_FMT_AYUV8_1X32)
			fmt->code = MEDIA_BUS_FMT_AYUV8_1X32;
		break;

	default:
		/* The BRS can't perform format conversion. */
		format = vsp2_entity_get_pad_format(&brs->entity, config,
						    BRS_PAD_SINK(0));
		fmt->code = format->code;
		break;
	}

	fmt->width = clamp(fmt->width, BRS_MIN_SIZE, BRS_MAX_SIZE);
	fmt->height = clamp(fmt->height, BRS_MIN_SIZE, BRS_MAX_SIZE);
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = V4L2_COLORSPACE_SRGB;
}

static int brs_set_format(struct v4l2_subdev *subdev,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct vsp2_brs *brs = to_brs(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&brs->entity.lock);

	config = vsp2_entity_get_pad_config(&brs->entity, cfg, fmt->which);
	if (!config) {
		ret = -EINVAL;
		goto done;
	}

	brs_try_format(brs, config, fmt->pad, &fmt->format);

	format = vsp2_entity_get_pad_format(&brs->entity, config, fmt->pad);
	*format = fmt->format;

	/* Reset the compose rectangle */
	if (fmt->pad != BRS_PAD_SOURCE) {
		struct v4l2_rect *compose;

		compose = brs_get_compose(brs, config, fmt->pad);
		compose->left = 0;
		compose->top = 0;
		compose->width = format->width;
		compose->height = format->height;
	}

	/* Propagate the format code to all pads */
	if (fmt->pad == BRS_PAD_SINK(0)) {
		unsigned int i;

		for (i = 0; i <= BRS_PAD_SOURCE; ++i) {
			format = vsp2_entity_get_pad_format(&brs->entity,
							    config, i);
			format->code = fmt->format.code;
		}
	}

done:
	mutex_unlock(&brs->entity.lock);
	return ret;
}

static int brs_get_selection(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
{
	struct vsp2_brs *brs = to_brs(subdev);
	struct v4l2_subdev_pad_config *config;

	if (sel->pad == BRS_PAD_SOURCE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = BRS_MAX_SIZE;
		sel->r.height = BRS_MAX_SIZE;
		return 0;

	case V4L2_SEL_TGT_COMPOSE:
		config = vsp2_entity_get_pad_config(&brs->entity, cfg,
						    sel->which);
		if (!config)
			return -EINVAL;

		mutex_lock(&brs->entity.lock);
		sel->r = *brs_get_compose(brs, config, sel->pad);
		mutex_unlock(&brs->entity.lock);
		return 0;

	default:
		return -EINVAL;
	}
}

static int brs_set_selection(struct v4l2_subdev *subdev,
			     struct v4l2_subdev_pad_config *cfg,
			     struct v4l2_subdev_selection *sel)
{
	struct vsp2_brs *brs = to_brs(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	struct v4l2_rect *compose;
	int ret = 0;

	if (sel->pad == BRS_PAD_SOURCE)
		return -EINVAL;

	if (sel->target != V4L2_SEL_TGT_COMPOSE)
		return -EINVAL;

	mutex_lock(&brs->entity.lock);

	config = vsp2_entity_get_pad_config(&brs->entity, cfg, sel->which);
	if (!config) {
		ret = -EINVAL;
		goto done;
	}

	/* The compose rectangle top left corner must be inside the output
	 * frame.
	 */
	format = vsp2_entity_get_pad_format(&brs->entity, config,
					    BRS_PAD_SOURCE);
	sel->r.left = clamp_t(unsigned int, sel->r.left, 0, format->width - 1);
	sel->r.top = clamp_t(unsigned int, sel->r.top, 0, format->height - 1);

	/* Scaling isn't supported, the compose rectangle size must be identical
	 * to the sink format size.
	 */
	format = vsp2_entity_get_pad_format(&brs->entity, config, sel->pad);
	sel->r.width = format->width;
	sel->r.height = format->height;

	compose = brs_get_compose(brs, config, sel->pad);
	*compose = sel->r;

done:
	mutex_unlock(&brs->entity.lock);
	return ret;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static const struct v4l2_subdev_pad_ops brs_pad_ops = {
	.init_cfg = vsp2_entity_init_cfg,
	.enum_mbus_code = brs_enum_mbus_code,
	.enum_frame_size = brs_enum_frame_size,
	.get_fmt = vsp2_subdev_get_pad_format,
	.set_fmt = brs_set_format,
	.get_selection = brs_get_selection,
	.set_selection = brs_set_selection,
};

static const struct v4l2_subdev_ops brs_ops = {
	.pad    = &brs_pad_ops,
};

/* -----------------------------------------------------------------------------
 * VSP2 Entity Operations
 */

static void brs_configure(struct vsp2_entity *entity,
			  struct vsp2_pipeline *pipe)
{
	struct vsp2_brs *brs = to_brs(&entity->subdev);
	struct v4l2_mbus_framefmt *format;
	unsigned int flags;
	unsigned int i;
	struct vsp_start_t *vsp_par =
		brs->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_brs_t *vsp_brs = vsp_par->ctrl_par->brs;
	u32 inctrl;

	format = vsp2_entity_get_pad_format(&brs->entity, brs->entity.config,
					    BRS_PAD_SOURCE);

	/* The hardware is extremely flexible but we have no userspace API to
	 * expose all the parameters, nor is it clear whether we would have use
	 * cases for all the supported modes. Let's just harcode the parameters
	 * to sane default values for now.
	 */

	/* Disable dithering and enable color data normalization unless the
	 * format at the pipeline output is premultiplied.
	 */
	flags = pipe->output ? pipe->output->format.flags : 0;
	inctrl = flags & V4L2_PIX_FMT_FLAG_PREMUL_ALPHA ?
		 0 : VI6_BRS_INCTRL_NRM;
	vsp_brs->adiv    = (inctrl & (1 << 28)) >> 28;

	/* Set the background position to cover the whole output image and
	 * configure its color.
	 */
	vsp_brs->blend_virtual->width		= format->width;
	vsp_brs->blend_virtual->height		= format->height;
	vsp_brs->blend_virtual->x_position	= 0;
	vsp_brs->blend_virtual->y_position	= 0;
	vsp_brs->blend_virtual->pwd		= VSP_LAYER_PARENT;
	vsp_brs->blend_virtual->color =
		brs->bgcolor | (0xff << VI6_BRS_VIRRPF_COL_A_SHIFT);

	for (i = 0; i < BRS_PAD_SOURCE; ++i) {
		bool premultiplied = false;
		u32 ctrl = 0;
		struct vsp_bld_ctrl_t *vsp_brs_ctrl = NULL;

		switch (i) {
		case 0:
			vsp_brs_ctrl = vsp_brs->blend_unit_a;
			break;
		case 1:
		default:
			vsp_brs_ctrl = vsp_brs->blend_unit_b;
			break;
		}

		/* Configure all Blend/ROP units corresponding to an enabled BRS
		 * input for alpha blending. Blend/ROP units corresponding to
		 * disabled BRS inputs are used in ROP NOP mode to ignore the
		 * SRC input.
		 */
		if (brs->inputs[i].rpf) {
			ctrl |= VI6_BRS_CTRL_RBC;

			premultiplied = brs->inputs[i].rpf->format.flags
				      & V4L2_PIX_FMT_FLAG_PREMUL_ALPHA;
		} else {
			ctrl |= VI6_BRS_CTRL_CROP(VI6_ROP_NOP)
			     |  VI6_BRS_CTRL_AROP(VI6_ROP_NOP);
		}

		/* Select the virtual RPF as the Blend/ROP unit A DST input to
		 * serve as a background color.
		 */
		if (i == 0)
			ctrl |= VI6_BRS_CTRL_DSTSEL_VRPF;

		ctrl |= VI6_BRS_CTRL_SRCSEL_BRSIN(i);

		vsp_brs_ctrl->rbc  = (ctrl & (1 << 31)) >> 31;
		vsp_brs_ctrl->crop = (ctrl & (0xF <<  4)) >>  4;
		vsp_brs_ctrl->arop = (ctrl & (0xF <<  0)) >>  0;

		/* Harcode the blending formula to
		 *
		 *	DSTc = DSTc * (1 - SRCa) + SRCc * SRCa
		 *	DSTa = DSTa * (1 - SRCa) + SRCa
		 *
		 * when the SRC input isn't premultiplied, and to
		 *
		 *	DSTc = DSTc * (1 - SRCa) + SRCc
		 *	DSTa = DSTa * (1 - SRCa) + SRCa
		 *
		 * otherwise.
		 */
		vsp_brs_ctrl->blend_formula = VSP_FORM_BLEND0;
		vsp_brs_ctrl->blend_coefx = VSP_COEFFICIENT_BLENDX4;
		vsp_brs_ctrl->blend_coefy = (premultiplied) ?
			VSP_COEFFICIENT_BLENDY5 : VSP_COEFFICIENT_BLENDY3;
		vsp_brs_ctrl->aformula = VSP_FORM_ALPHA0;
		vsp_brs_ctrl->acoefx = VSP_COEFFICIENT_ALPHAX4;
		vsp_brs_ctrl->acoefy = VSP_COEFFICIENT_ALPHAY5;
		vsp_brs_ctrl->acoefx_fix = 0;    /* Set coefficient x. */
		vsp_brs_ctrl->acoefy_fix = 0xFF; /* Set coefficient y. */
	}
}

static const struct vsp2_entity_operations brs_entity_ops = {
	.configure = brs_configure,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_brs *vsp2_brs_create(struct vsp2_device *vsp2)
{
	struct vsp2_brs *brs;
	int ret;

	brs = devm_kzalloc(vsp2->dev, sizeof(*brs), GFP_KERNEL);
	if (!brs)
		return ERR_PTR(-ENOMEM);

	brs->entity.ops = &brs_entity_ops;
	brs->entity.type = VSP2_ENTITY_BRS;

	ret = vsp2_entity_init(vsp2, &brs->entity, "brs",
			       (BRS_PAD_SOURCE + 1), &brs_ops,
			       MEDIA_ENT_F_PROC_VIDEO_COMPOSER);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the control handler. */
	v4l2_ctrl_handler_init(&brs->ctrls, 1);
	v4l2_ctrl_new_std(&brs->ctrls, &brs_ctrl_ops, V4L2_CID_BG_COLOR,
			  0, 0xffffff, 1, 0);

	brs->bgcolor = 0;

	brs->entity.subdev.ctrl_handler = &brs->ctrls;

	if (brs->ctrls.error) {
		dev_err(vsp2->dev, "brs: failed to initialize controls\n");
		ret = brs->ctrls.error;
		vsp2_entity_destroy(&brs->entity);
		return ERR_PTR(ret);
	}

	return brs;
}
