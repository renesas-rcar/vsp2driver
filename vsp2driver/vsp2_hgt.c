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
#include <linux/vsp2.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_hgt.h"
#include "vsp2_vspm.h"
#include "vsp2_addr.h"

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
#include <linux/dma-mapping.h>	/* for dl_par */
#endif

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static void hgt_set_config(struct vsp2_hgt *hgt, struct vsp2_hgt_config *config)
{
	hgt->set_hgt = 1; /* set HGT parameter from user */

	memcpy(&hgt->config, config, sizeof(struct vsp2_hgt_config));
}

static long hgt_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct vsp2_hgt *hgt = to_hgt(subdev);

	switch (cmd) {
	case VIDIOC_VSP2_HGT_CONFIG:
		hgt_set_config(hgt, arg);
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

	/* not implemented */

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static const struct v4l2_subdev_core_ops hgt_core_ops = {
	.ioctl = hgt_ioctl,
};

static const struct v4l2_subdev_ops hgt_ops = {
	.core	= &hgt_core_ops,
	.pad    = NULL,
};

/* -----------------------------------------------------------------------------
 * VSP2 Entity Operations
 */

static void hgt_configure(struct vsp2_entity *entity,
			  struct vsp2_pipeline *pipe)
{
	struct vsp2_hgt *hgt = to_hgt(&entity->subdev);
	struct vsp_start_t *vsp_par =
		hgt->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_hgt_t *vsp_hgt = vsp_par->ctrl_par->hgt;

	if (hgt->set_hgt == 1) {
		int i;

		/* VSPM parameter */

		vsp_par->use_module |= VSP_HGT_USE;

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
		if (hgt->buff_v == NULL) {
			VSP2_PRINT_ALERT("%s() error!!", __func__);
			return;
		}
	#ifdef TYPE_GEN2
		vsp_hgt->addr = (void *)hgt->buff_v;
	#else
		vsp_hgt->virt_addr = (void *)hgt->buff_v;
		vsp_hgt->hard_addr = (unsigned int)hgt->buff_h;
	#endif
#else
	#ifdef TYPE_GEN2
		vsp_hgt->addr = (void *)vsp2_addr_uv2hd(
			(unsigned long)hgt->config.addr);
	#else
		vsp_hgt->virt_addr = NULL;
		vsp_hgt->hard_addr = (unsigned int)vsp2_addr_uv2hd(
			(unsigned long)hgt->config.addr);
	#endif
#endif
		vsp_hgt->width		= hgt->config.width;
		vsp_hgt->height		= hgt->config.height;
		vsp_hgt->x_offset	= 0;
		vsp_hgt->y_offset	= 0;
		vsp_hgt->x_skip		= VSP_SKIP_OFF;
		vsp_hgt->y_skip		= VSP_SKIP_OFF;

		for (i = 0; i < 6; i++) {
			vsp_hgt->area[i].lower	= hgt->config.area[i].lower;
			vsp_hgt->area[i].upper	= hgt->config.area[i].upper;
		}
		vsp_hgt->sampling	= hgt->config.sampling;
	}
}

static const struct vsp2_entity_operations hgt_entity_ops = {
	.configure = hgt_configure,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_hgt *vsp2_hgt_create(struct vsp2_device *vsp2)
{
	struct vsp2_hgt *hgt;
	int ret;

	hgt = devm_kzalloc(vsp2->dev, sizeof(*hgt), GFP_KERNEL);
	if (hgt == NULL)
		return ERR_PTR(-ENOMEM);

	hgt->entity.ops = &hgt_entity_ops;
	hgt->entity.type = VSP2_ENTITY_HGT;

	ret = vsp2_entity_init(vsp2, &hgt->entity, "hgt", 2, &hgt_ops,
			       MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret < 0)
		return ERR_PTR(ret);

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
	hgt->buff_v = dma_alloc_coherent(vsp2->dev,
					 HGT_BUFF_SIZE, &hgt->buff_h,
					 GFP_KERNEL|GFP_DMA);
#endif

	return hgt;
}

/* -----------------------------------------------------------------------------
 * buffer finish process
 */
void vsp2_hgt_buffer_finish(struct vsp2_hgt *hgt)
{
#ifndef USE_BUFFER /* TODO: delete USE_BUFFER */
	if (hgt->set_hgt == 1)
		hgt->set_hgt = 0;
#else
	int remain;
	int i;

	if (hgt->set_hgt == 1) {
		hgt->set_hgt = 0;

		remain = (int)copy_to_user(
			hgt->config.addr, hgt->buff_v, HGT_BUFF_SIZE);

		if (remain > 0) {
			VSP2_PRINT_ALERT(
				"hgt frame end error / remain = %d", remain);

			for (i = 0; i < 10 && remain > 0; i++) {
				remain = (int)copy_to_user(
					hgt->config.addr, hgt->buff_v,
						HGT_BUFF_SIZE);

				VSP2_PRINT_ALERT(
					"retry %d / remain = %d", i, remain);
			}

			if (remain > 0)
				VSP2_PRINT_ALERT(
					"hgt frame end / giveup !!");
		}
	}
#endif
}

