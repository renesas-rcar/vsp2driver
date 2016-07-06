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

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
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
	struct vsp2_rwpf *wpf = to_rwpf(subdev);
	struct v4l2_pix_format_mplane *format = &wpf->format;
	const struct v4l2_mbus_framefmt *source_format;
	const struct v4l2_mbus_framefmt *sink_format;
	const struct v4l2_rect *crop = &wpf->crop;
	const struct vsp2_format_info *fmtinfo = wpf->fmtinfo;
	u32 outfmt = 0;
	u32 stride_y = 0;
	u32 stride_c = 0;
	struct vsp_start_t *vsp_par =
		wpf->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_dst_t *vsp_out = vsp_par->dst_par;
	u16 vspm_format;

	vsp2_entity_set_streaming(&wpf->entity, enable);

	if (!enable)
		return 0;

	/* Destination stride. */
	stride_y = format->plane_fmt[0].bytesperline;
	if (format->num_planes > 1)
		stride_c = format->plane_fmt[1].bytesperline;

	vsp_out->stride			= stride_y;
	if (format->num_planes > 1)
		vsp_out->stride_c	= stride_c;

	vsp_out->width		= crop->width;
	vsp_out->height		= crop->height;
	vsp_out->x_offset	= 0;
	vsp_out->y_offset	= 0;
	vsp_out->x_coffset	= crop->left;
	vsp_out->y_coffset	= crop->top;

	/* Format */
	sink_format = vsp2_entity_get_pad_format(&wpf->entity,
						 wpf->entity.config,
						 RWPF_PAD_SINK);
	source_format = vsp2_entity_get_pad_format(&wpf->entity,
						   wpf->entity.config,
						   RWPF_PAD_SOURCE);

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
	vsp_out->rotation	= 0;

	if (wpf->fcp_fcnl) {
		vsp_out->fcp->fcnl = FCP_FCNL_ENABLE;
		vsp_out->swap = VSP_SWAP_LL;
	} else
		vsp_out->fcp->fcnl = FCP_FCNL_DISABLE;

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

static struct v4l2_subdev_core_ops vsp2_debug_ops = {
	.ioctl = vsp2_debug_ioctl,
};
#endif

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static struct v4l2_subdev_video_ops wpf_video_ops = {
	.s_stream = wpf_s_stream,
};

static struct v4l2_subdev_pad_ops wpf_pad_ops = {
	.init_cfg = vsp2_entity_init_cfg,
	.enum_mbus_code = vsp2_rwpf_enum_mbus_code,
	.enum_frame_size = vsp2_rwpf_enum_frame_size,
	.get_fmt = vsp2_rwpf_get_format,
	.set_fmt = vsp2_rwpf_set_format,
	.get_selection = vsp2_rwpf_get_selection,
	.set_selection = vsp2_rwpf_set_selection,
};

static struct v4l2_subdev_ops wpf_ops = {
#ifdef VSP2_DEBUG
	.core	= &vsp2_debug_ops,
#endif
	.video	= &wpf_video_ops,
	.pad    = &wpf_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Video Device Operations
 */

static void wpf_set_memory(struct vsp2_rwpf *wpf)
{
	struct vsp_start_t *vsp_par =
		wpf->entity.vsp2->vspm->ip_par.par.vsp;
	struct vsp_dst_t *vsp_out = vsp_par->dst_par;

	vsp_out->addr = (void *)((unsigned long)wpf->buf_addr[0]);
	vsp_out->addr_c0 = (void *)((unsigned long)wpf->buf_addr[1]);
	vsp_out->addr_c1 = (void *)((unsigned long)wpf->buf_addr[2]);
}

static const struct vsp2_rwpf_operations wpf_vdev_ops = {
	.set_memory = wpf_set_memory,
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

	wpf->ops = &wpf_vdev_ops;

	wpf->max_width = WPF_MAX_WIDTH;
	wpf->max_height = WPF_MAX_HEIGHT;

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
		ret = wpf->ctrls.error;
		goto error;
	}

	wpf->fcp_fcnl = FCP_FCNL_DEF_VALUE;

	return wpf;

error:
	vsp2_entity_destroy(&wpf->entity);
	return ERR_PTR(ret);
}
