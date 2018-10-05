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

#define RPF_MAX_WIDTH				(8190)
#define RPF_MAX_HEIGHT				(8190)

static struct vsp_src_t *rpf_get_vsp_in(struct vsp2_rwpf *rpf)
{
	struct vsp_start_t *vsp_par =
		rpf->entity.vsp2->vspm->ip_par.par.vsp;

	if (rpf->entity.index >= 5)
		return NULL;

	return vsp_par->src_par[rpf->entity.index];
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static const struct v4l2_subdev_ops rpf_ops = {
	.pad    = &vsp2_rwpf_pad_ops,
};

/* -----------------------------------------------------------------------------
 * VSP2 Entity Operations
 */

static void rpf_set_memory(struct vsp2_entity *entity)
{
	struct vsp2_rwpf *rpf = entity_to_rwpf(entity);
	struct vsp_src_t *vsp_in = rpf_get_vsp_in(rpf);

	if (!vsp_in) {
		dev_err(rpf->entity.vsp2->dev,
			"failed to rpf queue. Invalid RPF index.\n");
		return;
	}

	vsp_in->addr = (unsigned int)rpf->mem.addr[0] + rpf->offsets[0];
	vsp_in->addr_c0 = (unsigned int)rpf->mem.addr[1] + rpf->offsets[1];
	vsp_in->addr_c1 = (unsigned int)rpf->mem.addr[2] + rpf->offsets[1];
}

static void rpf_configure(struct vsp2_entity *entity,
			  struct vsp2_pipeline *pipe)
{
	struct vsp2_rwpf *rpf = to_rwpf(&entity->subdev);
	const struct vsp2_format_info *fmtinfo = rpf->fmtinfo;
	const struct v4l2_pix_format_mplane *format = &rpf->format;
	const struct v4l2_mbus_framefmt *source_format;
	const struct v4l2_mbus_framefmt *sink_format;
	const struct v4l2_rect *crop;
	unsigned int left = 0;
	unsigned int top = 0;
	u32 infmt;
	u32 alph_sel, laya;
	u32 stride_y = 0;
	u32 stride_c = 0;
	struct vsp_src_t *vsp_in = rpf_get_vsp_in(rpf);
	u16 vspm_format;

	if (!vsp_in) {
		dev_err(rpf->entity.vsp2->dev,
			"failed to rpf stream. Invalid RPF index.\n");
		return;
	}

	/* Source size, stride and crop offsets.
	 *
	 * The crop offsets correspond to the location of the crop rectangle top
	 * left corner in the plane buffer. Only two offsets are needed, as
	 * planes 2 and 3 always have identical strides.
	 */
	crop = vsp2_rwpf_get_crop(rpf, rpf->entity.config);

	stride_y = format->plane_fmt[0].bytesperline;
	if (format->num_planes > 1)
		stride_c = format->plane_fmt[1].bytesperline;

	vsp_in->width		= crop->width;
	vsp_in->height		= crop->height;
	vsp_in->width_ex	= 0;
	vsp_in->height_ex	= 0;
	vsp_in->x_offset	= 0;
	vsp_in->y_offset	= 0;

	rpf->offsets[0] = crop->top * stride_y
			+ crop->left * fmtinfo->bpp[0] / 8;

	if (format->num_planes > 1) {
		rpf->offsets[1] = crop->top * stride_c / fmtinfo->vsub
				+ crop->left * fmtinfo->bpp[1] / fmtinfo->hsub
				/ 8;
	} else {
		rpf->offsets[1] = 0;
	}

	vsp_in->stride		= stride_y;
	vsp_in->stride_c	= stride_c;

	/* Format */
	sink_format = vsp2_entity_get_pad_format(&rpf->entity,
						 rpf->entity.config,
						 RWPF_PAD_SINK);
	source_format = vsp2_entity_get_pad_format(&rpf->entity,
						   rpf->entity.config,
						   RWPF_PAD_SOURCE);

	infmt = VI6_RPF_INFMT_CIPM
	      | (fmtinfo->hwfmt << VI6_RPF_INFMT_RDFMT_SHIFT);

	if (fmtinfo->swap_yc)
		infmt |= VI6_RPF_INFMT_SPYCS;
	if (fmtinfo->swap_uv)
		infmt |= VI6_RPF_INFMT_SPUVS;

	if (sink_format->code != source_format->code)
		infmt |= VI6_RPF_INFMT_CSC;

	infmt |= rpf->csc_mode << 9;

	vspm_format = (unsigned short)(infmt & 0x007F);
	if (vspm_format == 0x007F || vspm_format == 0x003F) {
		/* CLUT data. */
		/* Set bytes per pixel (1). */
		vspm_format	|= (1 << 8);
	} else if (vspm_format < 0x0040) {
		/* RGB format. */
		/* Set bytes per pixel. */
		vspm_format	|= (fmtinfo->bpp[0] / 8) << 8;
	} else {
		/* YUV format. */
		/* Set SPYCS and SPUVS. */
		vspm_format	|= (infmt & 0xC000);
	}
	vsp_in->format		= vspm_format;
	vsp_in->cipm		= (infmt & (1 << 16)) >> 16;
	vsp_in->cext		= (infmt & (3 << 12)) >> 12;
	vsp_in->csc		= (infmt & (1 <<  8)) >>  8;
	vsp_in->iturbt		= (infmt & (3 << 10)) >> 10;
	vsp_in->clrcng		= (infmt & (1 <<  9)) >>  9;

	vsp_in->swap		= fmtinfo->swap;

	/* Output location */
	if (pipe->bru) {
		const struct v4l2_rect *compose;

		compose = vsp2_entity_get_pad_selection(pipe->bru,
							pipe->bru->config,
							rpf->bru_input,
							V4L2_SEL_TGT_COMPOSE);
		left = compose->left;
		top = compose->top;
	}

	if (pipe->brs) {
		const struct v4l2_rect *compose;

		compose = vsp2_entity_get_pad_selection(pipe->brs,
							pipe->brs->config,
							rpf->brs_input,
							V4L2_SEL_TGT_COMPOSE);
		left = compose->left;
		top = compose->top;
	}

	vsp_in->x_position	= left;
	vsp_in->y_position	= top;

	vsp_in->pwd		= VSP_LAYER_CHILD;
	vsp_in->vir		= VSP_NO_VIR;
	vsp_in->vircolor	= 0;

	switch (fmtinfo->fourcc) {
	case V4L2_PIX_FMT_ARGB555:
		if (CONFIG_VIDEO_RENESAS_VSP_ALPHA_BIT_ARGB1555 == 1)
			alph_sel = (2 << 28) | (1 << 18) |
				   (0xFF << 8) | (rpf->alpha & 0xFF);
		else
			alph_sel = (2 << 28) | (1 << 18) |
				   ((rpf->alpha & 0xFF) << 8) | 0xFF;
		laya = 0;
		break;
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_XRGB444:
		alph_sel = (1 << 18);
		laya = 0;
		break;
	default:
		alph_sel = (4 << 28) | (1 << 18);
		laya = rpf->alpha;
		break;
	}

	vsp_in->alpha->afix = laya;
	vsp2_pipeline_propagate_alpha(pipe, rpf->alpha);

	vsp_in->alpha->addr_a = 0;
	vsp_in->alpha->stride_a = 0;
	vsp_in->alpha->swap = VSP_SWAP_NO;
	vsp_in->alpha->asel = (alph_sel & (7 << 28)) >> 28;
	vsp_in->alpha->aext = (alph_sel & (3 << 18)) >> 18;
	vsp_in->alpha->anum0 = (alph_sel & (0xff << 0)) >> 0;
	vsp_in->alpha->anum1 = (alph_sel & (0xff << 8)) >> 8;
	vsp_in->alpha->irop = NULL;
	vsp_in->alpha->ckey = NULL;

	if (source_format->code == MEDIA_BUS_FMT_AYUV8_1X32) {
		vsp_in->alpha->mult->a_mmd = VSP_MULT_THROUGH;
		vsp_in->alpha->mult->p_mmd = VSP_MULT_THROUGH;
		vsp_in->alpha->mult->ratio = 0;
	} else {
		vsp_in->alpha->mult->a_mmd = VSP_MULT_RATIO;
		if (rpf->format.flags & V4L2_PIX_FMT_FLAG_PREMUL_ALPHA)
			vsp_in->alpha->mult->p_mmd = VSP_MULT_RATIO;
		else
			vsp_in->alpha->mult->p_mmd = VSP_MULT_THROUGH;
		vsp_in->alpha->mult->ratio = rpf->alpha;
	}

	/* Count rpf_num. */
	rpf->entity.vsp2->vspm->ip_par.par.vsp->rpf_num++;
}

static const struct vsp2_entity_operations rpf_entity_ops = {
	.set_memory = rpf_set_memory,
	.configure = rpf_configure,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_rwpf *vsp2_rpf_create(struct vsp2_device *vsp2, unsigned int index)
{
	struct vsp2_rwpf *rpf;
	char name[6];
	int ret;

	rpf = devm_kzalloc(vsp2->dev, sizeof(*rpf), GFP_KERNEL);
	if (!rpf)
		return ERR_PTR(-ENOMEM);

	rpf->max_width = RPF_MAX_WIDTH;
	rpf->max_height = RPF_MAX_HEIGHT;

	rpf->entity.ops = &rpf_entity_ops;
	rpf->entity.type = VSP2_ENTITY_RPF;
	rpf->entity.index = index;

	sprintf(name, "rpf.%u", index);
	ret = vsp2_entity_init(vsp2, &rpf->entity, name, 2, &rpf_ops,
			       MEDIA_ENT_F_PROC_VIDEO_PIXEL_FORMATTER);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the control handler. */
	ret = vsp2_rwpf_init_ctrls(rpf);
	if (ret < 0) {
		dev_err(vsp2->dev, "rpf%u: failed to initialize controls\n",
			index);
		goto error;
	}

	return rpf;

error:
	vsp2_entity_destroy(&rpf->entity);
	return ERR_PTR(ret);
}
