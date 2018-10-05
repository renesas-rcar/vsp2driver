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

#ifndef __VSP2_RWPF_H__
#define __VSP2_RWPF_H__

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_entity.h"

#define RWPF_PAD_SINK				0
#define RWPF_PAD_SOURCE				1

#define FCP_FCNL_DEF_VALUE	(0x00)

#define CSC_MODE_601_LIMITED	(0)
#define CSC_MODE_601_FULL	(1)
#define CSC_MODE_709_LIMITED	(2)
#define CSC_MODE_709_FULL	(3)
#define CSC_MODE_DEFAULT	CSC_MODE_601_LIMITED

struct v4l2_ctrl;
struct vsp2_pipeline;
struct vsp2_rwpf;
struct vsp2_video;

struct vsp2_rwpf_memory {
	dma_addr_t addr[3];
};

struct vsp2_rwpf {
	struct vsp2_entity entity;
	struct v4l2_ctrl_handler ctrls;

	struct vsp2_pipeline *pipe;
	struct vsp2_video *video;

	unsigned int max_width;
	unsigned int max_height;

	struct v4l2_pix_format_mplane format;
	const struct vsp2_format_info *fmtinfo;
	unsigned int bru_input;
	unsigned int brs_input;

	unsigned int alpha;

	unsigned int offsets[2];
	struct vsp2_rwpf_memory mem;

	unsigned char fcp_fcnl;
	struct {
		struct v4l2_ctrl *rotangle;
		struct v4l2_ctrl *hflip;
		struct v4l2_ctrl *vflip;
		unsigned char rotation;
		bool swap_sizes;
	} rotinfo;

	int csc_mode;
};

static inline struct vsp2_rwpf *to_rwpf(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp2_rwpf, entity.subdev);
}

static inline struct vsp2_rwpf *entity_to_rwpf(struct vsp2_entity *entity)
{
	return container_of(entity, struct vsp2_rwpf, entity);
}

struct vsp2_rwpf *vsp2_rpf_create(struct vsp2_device *vsp2, unsigned int index);
struct vsp2_rwpf *vsp2_wpf_create(struct vsp2_device *vsp2, unsigned int index);

int vsp2_rwpf_init_ctrls(struct vsp2_rwpf *rwpf);

extern const struct v4l2_subdev_pad_ops vsp2_rwpf_pad_ops;

struct v4l2_rect *vsp2_rwpf_get_crop(struct vsp2_rwpf *rwpf,
				     struct v4l2_subdev_pad_config *config);
int vsp2_rwpf_check_compose_size(struct vsp2_entity *entity);
void vsp2_rwpf_get_csc_element(struct vsp2_entity *entity, unsigned int *mbus,
			       unsigned char *ycbcr_enc,
			       unsigned char *quantization);
void vsp2_rwpf_set_csc_mode(struct vsp2_entity *entity, int csc_mode);
/**
 * vsp2_rwpf_set_memory - Configure DMA addresses for a [RW]PF
 * @rwpf: the [RW]PF instance
 *
 * This function applies the cached memory buffer address to the hardware.
 */
static inline void vsp2_rwpf_set_memory(struct vsp2_rwpf *rwpf)
{
	rwpf->entity.ops->set_memory(&rwpf->entity);
}

#endif /* __VSP2_RWPF_H__ */
