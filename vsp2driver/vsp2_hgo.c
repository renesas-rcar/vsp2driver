/*************************************************************************/ /*
 VSP2

 Copyright (C) 2015 Renesas Electronics Corporation

 License        Dual MIT/GPLv2

 The contents of this file are subject to the MIT license as set out below.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 Alternatively, the contents of this file may be used under the terms of
 the GNU General Public License Version 2 ("GPL") in which case the provisions
 of GPL are applicable instead of those above.

 If you wish to allow use of your version of this file only under the terms of
 GPL, and not to allow others to use your version of this file under the terms
 of the MIT license, indicate your decision by deleting the provisions above
 and replace them with the notice and other provisions required by GPL as set
 out in the file called "GPL-COPYING" included in this distribution. If you do
 not delete the provisions above, a recipient may use your version of this file
 under the terms of either the MIT license or GPL.

 This License is also included in this distribution in the file called
 "MIT-COPYING".

 EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


 GPLv2:
 If you wish to use this file under the terms of GPL, following terms are
 effective.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/ /*************************************************************************/

#include <linux/device.h>
#include <linux/gfp.h>
#include <linux/vsp2.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_hgo.h"
#include "vsp2_vspm.h"
#include "vsp2_addr.h"

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
#include <linux/dma-mapping.h>	/* for dl_par */
#endif

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static void hgo_set_config(struct vsp2_hgo *hgo, struct vsp2_hgo_config *config)
{
	hgo->set_hgo = 1;/* set HGT parameter from user */

	memcpy(&hgo->config, config, sizeof(struct vsp2_hgo_config));
}

static long hgo_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct vsp2_hgo *hgo = to_hgo(subdev);

	switch (cmd) {
	case VIDIOC_VSP2_HGO_CONFIG:
		hgo_set_config(hgo, arg);
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

static struct v4l2_subdev_core_ops hgo_core_ops = {
	.ioctl = hgo_ioctl,
};

static struct v4l2_subdev_ops hgo_ops = {
	.core	= &hgo_core_ops,
	.pad    = NULL,
};

/* -----------------------------------------------------------------------------
 * VSP2 Entity Operations
 */

static void hgo_configure(struct vsp2_entity *entity)
{
	struct vsp2_hgo *hgo = to_hgo(&entity->subdev);
	struct vsp_start_t *vsp_par =
		hgo->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_hgo_t *vsp_hgo = vsp_par->ctrl_par->hgo;

	if (hgo->set_hgo == 1) {

		/* VSPM parameter */

		vsp_par->use_module |= VSP_HGO_USE;

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
		if (hgo->buff_v == NULL) {
			VSP2_PRINT_ALERT("hgo_configure() error!!");
			return;
		}
	#ifdef TYPE_GEN2
		vsp_hgo->addr = (void *)hgo->buff_v;
	#else
		vsp_hgo->virt_addr = (void *)hgo->buff_v;
		vsp_hgo->hard_addr = (void *)hgo->buff_h;
	#endif
#else
	#ifdef TYPE_GEN2
		vsp_hgo->addr = (void *)vsp2_addr_uv2hd(
			(unsigned long)hgo->config.addr);
	#else
		vsp_hgo->virt_addr = NULL;
		vsp_hgo->hard_addr = (void *)vsp2_addr_uv2hd(
			(unsigned long)hgo->config.addr);
	#endif
#endif
		vsp_hgo->width			= hgo->config.width;
		vsp_hgo->height			= hgo->config.height;
		vsp_hgo->x_offset		= hgo->config.x_offset;
		vsp_hgo->y_offset		= hgo->config.y_offset;
		vsp_hgo->binary_mode	= hgo->config.binary_mode;
		vsp_hgo->maxrgb_mode	= hgo->config.maxrgb_mode;
		vsp_hgo->step_mode		= hgo->config.step_mode;
		vsp_hgo->x_skip			= VSP_SKIP_OFF;
		vsp_hgo->y_skip			= VSP_SKIP_OFF;
		vsp_hgo->sampling		= hgo->config.sampling;
	}
}

static const struct vsp2_entity_operations hgo_entity_ops = {
	.configure = hgo_configure,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_hgo *vsp2_hgo_create(struct vsp2_device *vsp2)
{
	struct vsp2_hgo *hgo;
	int ret;

	hgo = devm_kzalloc(vsp2->dev, sizeof(*hgo), GFP_KERNEL);
	if (hgo == NULL)
		return ERR_PTR(-ENOMEM);

	hgo->entity.ops = &hgo_entity_ops;
	hgo->entity.type = VSP2_ENTITY_HGO;

	ret = vsp2_entity_init(vsp2, &hgo->entity, "hgo", 2, &hgo_ops,
			       MEDIA_ENT_F_PROC_VIDEO_STATISTICS);
	if (ret < 0)
		return ERR_PTR(ret);

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
	hgo->buff_v = dma_alloc_coherent(vsp2->dev,
		HGO_BUFF_SIZE, &hgo->buff_h, GFP_KERNEL|GFP_DMA);
#endif

	return hgo;
}

/* -----------------------------------------------------------------------------
 * buffer finish process
 */
void vsp2_hgo_buffer_finish(struct vsp2_hgo *hgo)
{
#ifndef USE_BUFFER /* TODO: delete USE_BUFFER */
	if (hgo->set_hgo == 1)
		hgo->set_hgo = 0;
#else
	int remain;
	int i;
	int copy_size = HGO_BUFF_SIZE;

	if (hgo->set_hgo == 1) {
		hgo->set_hgo = 0;

		if (hgo->config.step_mode == 0x00 /*VSP_STEP_64*/)
			copy_size = sizeof(unsigned int) * 192;
		remain = (int)copy_to_user(
			hgo->config.addr, hgo->buff_v, copy_size);

		if (remain > 0) {

			VSP2_PRINT_ALERT(
				"hgo frame end error / remain = %d", remain);

			for (i = 0; i < 10 && remain > 0; i++) {

				remain = (int)copy_to_user(
					hgo->config.addr, hgo->buff_v,
						copy_size);

				VSP2_PRINT_ALERT(
					"retry %d / remain = %d", i, remain);
			}

			if (remain > 0)
				VSP2_PRINT_ALERT(
					"hgo frame end / giveup !!");
		}
	}
#endif
}

