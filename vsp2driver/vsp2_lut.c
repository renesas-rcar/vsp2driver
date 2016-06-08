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


#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_lut.h"
#include "vsp2_vspm.h"
#include "vsp2_addr.h"

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
#include <linux/dma-mapping.h>	/* for dl_par */
#endif

#define LUT_MIN_SIZE	(1U)
#define LUT_MAX_SIZE	(8190U)

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static void lut_configure(struct vsp2_lut *lut, struct vsp2_lut_config *config)
{
	memcpy(&lut->config, config, sizeof(struct vsp2_lut_config));
}

static long lut_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct vsp2_lut *lut = to_lut(subdev);

	switch (cmd) {
	case VIDIOC_VSP2_LUT_CONFIG:
		lut_configure(lut, arg);
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Video Operations
 */
static struct vsp_start_t *to_vsp_par(struct vsp2_entity *entity)
{
	return entity->vsp2->vspm->ip_par.par.vsp;
}

static int lut_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct vsp2_lut       *lut     = to_lut(subdev);
	struct vsp_start_t    *vsp_par = to_vsp_par(&lut->entity);
	struct vsp_lut_t      *vsp_lut = vsp_par->ctrl_par->lut;

	if (!enable)
		return 0;

	/* VSPM parameter */

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */

	if (lut->buff_v == NULL) {
		VSP2_PRINT_ALERT("lut_s_stream() error!!<1>");
		return 0;
	}
	if (copy_from_user(lut->buff_v,
					(void __user *)lut->config.addr,
					lut->config.tbl_num * 8))
		VSP2_PRINT_ALERT("lut_s_stream() error<2>!!");

	vsp_lut->lut.hard_addr  = (void *)lut->buff_h;
	vsp_lut->lut.virt_addr  = (void *)lut->buff_v;
#else
	vsp_lut->lut.hard_addr  =
		(void *)vsp2_addr_uv2hd((unsigned long)lut->config.addr);

	vsp_lut->lut.virt_addr  =
		(void *)vsp2_addr_uv2kv((unsigned long)lut->config.addr);

#endif
	vsp_lut->lut.tbl_num    = lut->config.tbl_num;
	vsp_lut->fxa            = lut->config.fxa;
	/*vsp_lut->connect      = 0;    set by vsp2_entity_route_setup() */

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

static int lut_enum_mbus_code(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		MEDIA_BUS_FMT_ARGB8888_1X32,
		MEDIA_BUS_FMT_AHSV8888_1X32,
		MEDIA_BUS_FMT_AYUV8_1X32,
	};
	struct vsp2_lut *lut = to_lut(subdev);
	struct v4l2_mbus_framefmt *format;

	if (code->pad == LUT_PAD_SINK) {
		if (code->index >= ARRAY_SIZE(codes))
			return -EINVAL;

		code->code = codes[code->index];
	} else {
		/* The LUT can't perform format conversion, the sink format is
		 * always identical to the source format.
		 */
		if (code->index)
			return -EINVAL;

		format = vsp2_entity_get_pad_format(&lut->entity, cfg,
						    LUT_PAD_SINK, code->which);
		code->code = format->code;
	}

	return 0;
}

static int lut_enum_frame_size(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	struct vsp2_lut *lut = to_lut(subdev);
	struct v4l2_mbus_framefmt *format;

	format = vsp2_entity_get_pad_format(&lut->entity, cfg,
					    fse->pad, fse->which);

	if (fse->index || fse->code != format->code)
		return -EINVAL;

	if (fse->pad == LUT_PAD_SINK) {
		fse->min_width = LUT_MIN_SIZE;
		fse->max_width = LUT_MAX_SIZE;
		fse->min_height = LUT_MIN_SIZE;
		fse->max_height = LUT_MAX_SIZE;
	} else {
		/* The size on the source pad are fixed and always identical to
		 * the size on the sink pad.
		 */
		fse->min_width = format->width;
		fse->max_width = format->width;
		fse->min_height = format->height;
		fse->max_height = format->height;
	}

	return 0;
}

static int lut_get_format(
	struct v4l2_subdev *subdev, struct v4l2_subdev_pad_config *cfg,
	 struct v4l2_subdev_format *fmt)
{
	struct vsp2_lut *lut = to_lut(subdev);

	fmt->format = *vsp2_entity_get_pad_format(&lut->entity, cfg, fmt->pad,
						  fmt->which);

	return 0;
}

static int lut_set_format(
	struct v4l2_subdev *subdev, struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	struct vsp2_lut *lut = to_lut(subdev);
	struct v4l2_mbus_framefmt *format;


	/* Default to YUV if the requested format is not supported. */
	if (fmt->format.code != MEDIA_BUS_FMT_ARGB8888_1X32 &&
	    fmt->format.code != MEDIA_BUS_FMT_AYUV8_1X32)
		fmt->format.code = MEDIA_BUS_FMT_AYUV8_1X32;

	format = vsp2_entity_get_pad_format(&lut->entity, cfg, fmt->pad,
					    fmt->which);

	if (fmt->pad == LUT_PAD_SOURCE) {
		/* The LUT output format can't be modified. */
		format->code = fmt->format.code;
		fmt->format = *format;
		return 0;
	}

	format->code = fmt->format.code;

	format->width = clamp_t(unsigned int, fmt->format.width,
				LUT_MIN_SIZE, LUT_MAX_SIZE);
	format->height = clamp_t(unsigned int, fmt->format.height,
				 LUT_MIN_SIZE, LUT_MAX_SIZE);
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	fmt->format = *format;

	/* Propagate the format to the source pad. */
	format = vsp2_entity_get_pad_format(&lut->entity, cfg, LUT_PAD_SOURCE,
					    fmt->which);
	*format = fmt->format;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static struct v4l2_subdev_core_ops lut_core_ops = {
	.ioctl = lut_ioctl,
};

static struct v4l2_subdev_video_ops lut_video_ops = {
	.s_stream = lut_s_stream,
};

static struct v4l2_subdev_pad_ops lut_pad_ops = {
	.enum_mbus_code = lut_enum_mbus_code,
	.enum_frame_size = lut_enum_frame_size,
	.get_fmt = lut_get_format,
	.set_fmt = lut_set_format,
};

static struct v4l2_subdev_ops lut_ops = {
	.core	= &lut_core_ops,
	.video	= &lut_video_ops,
	.pad    = &lut_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_lut *vsp2_lut_create(struct vsp2_device *vsp2)
{
	struct vsp2_lut *lut;
	int ret;

	lut = devm_kzalloc(vsp2->dev, sizeof(*lut), GFP_KERNEL);
	if (lut == NULL)
		return ERR_PTR(-ENOMEM);

	lut->entity.type = VSP2_ENTITY_LUT;

	ret = vsp2_entity_init(vsp2, &lut->entity, "lut", 2, &lut_ops);
	if (ret < 0)
		return ERR_PTR(ret);

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
	lut->buff_v = dma_alloc_coherent(vsp2->dev,
		LUT_BUFF_SIZE, &lut->buff_h, GFP_KERNEL|GFP_DMA);
#endif

	return lut;
}
