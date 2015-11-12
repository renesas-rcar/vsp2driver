/*
 * vsp2_clu.c  --  R-Car VSP2 Look-Up Table
 *
 * Copyright (C) 2013 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*TODO: add GPL comment*/

#include <linux/device.h>
#include <linux/gfp.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_clu.h"
#include "vsp2_vspm.h"
#include "vsp2_addr.h"

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
#include <linux/dma-mapping.h>	/* for dl_par */
#endif

#define CLU_MIN_SIZE	(1U)
#define CLU_MAX_SIZE	(8190U)

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Core Operations
 */

static void clu_configure(struct vsp2_clu *clu, struct vsp2_clu_config *config)
{
	memcpy(&clu->config, config, sizeof(struct vsp2_clu_config));
}

static long clu_ioctl(struct v4l2_subdev *subdev, unsigned int cmd, void *arg)
{
	struct vsp2_clu *clu = to_clu(subdev);

	switch (cmd) {
	case VIDIOC_VSP2_CLU_CONFIG:
		clu_configure(clu, arg);
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

static int clu_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct vsp2_clu     *clu     = to_clu(subdev);
	struct vsp_start_t  *vsp_par = to_vsp_par(&clu->entity);
	struct vsp_clu_t    *vsp_clu = vsp_par->ctrl_par->clu;

	if (!enable)
		return 0;

	/* VSPM parameter */

	vsp_clu->mode           = clu->config.mode;

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */

	if (clu->buff_v == NULL) {
		VSP2_PRINT_ALERT("clu_s_stream() error<1>!!");
		return 0;
	}
	if (copy_from_user(clu->buff_v,
					(void __user *)clu->config.addr,
					clu->config.tbl_num * 8))
		VSP2_PRINT_ALERT("clu_s_stream() error<2>!!");

	vsp_clu->clu.hard_addr  = (void *)clu->buff_h;
	vsp_clu->clu.virt_addr	= (void *)clu->buff_v;
#else
	vsp_clu->clu.hard_addr  =
		(void *)vsp2_addr_uv2hd((unsigned long)clu->config.addr);

	vsp_clu->clu.virt_addr  =
		(void *)vsp2_addr_uv2kv((unsigned long)clu->config.addr);
#endif
	vsp_clu->clu.tbl_num    = clu->config.tbl_num;
	vsp_clu->fxa            = 0;
	/*vsp_clu->connect      = 0;  set by vsp2_entity_route_setup() */

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Pad Operations
 */

static int clu_enum_mbus_code(struct v4l2_subdev *subdev,
			      struct v4l2_subdev_pad_config *cfg,
			      struct v4l2_subdev_mbus_code_enum *code)
{
	static const unsigned int codes[] = {
		MEDIA_BUS_FMT_ARGB8888_1X32,
		MEDIA_BUS_FMT_AHSV8888_1X32,
		MEDIA_BUS_FMT_AYUV8_1X32,
	};
	struct vsp2_clu *clu = to_clu(subdev);
	struct v4l2_mbus_framefmt *format;

	if (code->pad == CLU_PAD_SINK) {

		if (code->index >= ARRAY_SIZE(codes))
			return -EINVAL;

		code->code = codes[code->index];

	} else {

		/* The CLU can't perform format conversion, the sink format is
		 * always identical to the source format.
		 */
		if (code->index)
			return -EINVAL;

		format = vsp2_entity_get_pad_format(&clu->entity, cfg,
						    CLU_PAD_SINK, code->which);
		code->code  = format->code;
	}

	return 0;
}

static int clu_enum_frame_size(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_frame_size_enum *fse)
{
	struct vsp2_clu *clu = to_clu(subdev);
	struct v4l2_mbus_framefmt *format;

	format = vsp2_entity_get_pad_format(&clu->entity, cfg,
					    fse->pad, fse->which);

	if (fse->index || fse->code != format->code)
		return -EINVAL;

	if (fse->pad == CLU_PAD_SINK) {

		fse->min_width  = CLU_MIN_SIZE;
		fse->max_width  = CLU_MAX_SIZE;
		fse->min_height = CLU_MIN_SIZE;
		fse->max_height = CLU_MAX_SIZE;

	} else {

		/* The size on the source pad are fixed and always identical to
		 * the size on the sink pad.
		 */
		fse->min_width  = format->width;
		fse->max_width  = format->width;
		fse->min_height = format->height;
		fse->max_height = format->height;
	}

	return 0;
}

static int clu_get_format(
	struct v4l2_subdev *subdev, struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	struct vsp2_clu *clu = to_clu(subdev);

	fmt->format = *vsp2_entity_get_pad_format(&clu->entity, cfg, fmt->pad,
						  fmt->which);

	return 0;
}

static int clu_set_format(
	struct v4l2_subdev *subdev, struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	struct vsp2_clu *clu = to_clu(subdev);
	struct v4l2_mbus_framefmt *format;


	/* Default to YUV if the requested format is not supported. */
	if (fmt->format.code != MEDIA_BUS_FMT_ARGB8888_1X32 &&
/*	    fmt->format.code != V4L2_MBUS_FMT_AHSV8888_1X32 && TODO:del chk */
	    fmt->format.code != MEDIA_BUS_FMT_AYUV8_1X32)
		fmt->format.code = MEDIA_BUS_FMT_AYUV8_1X32;

	format = vsp2_entity_get_pad_format(&clu->entity, cfg, fmt->pad,
					    fmt->which);

	if (fmt->pad == CLU_PAD_SOURCE) {

		/* The CLU output format can't be modified. */

		format->code    = fmt->format.code; /* TODO: need check */
		fmt->format     = *format;

		return 0;
	}

	format->code = fmt->format.code; /* TODO: need check */

	format->width = clamp_t(unsigned int, fmt->format.width,
				CLU_MIN_SIZE, CLU_MAX_SIZE);
	format->height = clamp_t(unsigned int, fmt->format.height,
				 CLU_MIN_SIZE, CLU_MAX_SIZE);
	format->field = V4L2_FIELD_NONE;
	format->colorspace = V4L2_COLORSPACE_SRGB;

	fmt->format = *format;

	/* Propagate the format to the source pad. */

	format = vsp2_entity_get_pad_format(&clu->entity, cfg, CLU_PAD_SOURCE,
					    fmt->which);
	*format = fmt->format;

	return 0;
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

static struct v4l2_subdev_core_ops clu_core_ops = {
	.ioctl = clu_ioctl,
};

static struct v4l2_subdev_video_ops clu_video_ops = {
	.s_stream = clu_s_stream,
};

static struct v4l2_subdev_pad_ops clu_pad_ops = {
	.enum_mbus_code     = clu_enum_mbus_code,
	.enum_frame_size    = clu_enum_frame_size,
	.get_fmt            = clu_get_format,
	.set_fmt            = clu_set_format,
};

static struct v4l2_subdev_ops clu_ops = {
	.core   = &clu_core_ops,
	.video  = &clu_video_ops,
	.pad    = &clu_pad_ops,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_clu *vsp2_clu_create(struct vsp2_device *vsp2)
{
	struct v4l2_subdev  *subdev;
	struct vsp2_clu     *clu;
	int                 ret;

	clu = devm_kzalloc(vsp2->dev, sizeof(*clu), GFP_KERNEL);
	if (clu == NULL)
		return ERR_PTR(-ENOMEM);

	clu->entity.type = VSP2_ENTITY_CLU;

	ret = vsp2_entity_init(vsp2, &clu->entity, 2);
	if (ret < 0)
		return ERR_PTR(ret);

	/* Initialize the V4L2 subdev. */

	subdev = &clu->entity.subdev;
	v4l2_subdev_init(subdev, &clu_ops);

	subdev->entity.ops   = &vsp2->media_ops;
	subdev->internal_ops = &vsp2_subdev_internal_ops;

	snprintf(subdev->name, sizeof(subdev->name), "%s clu",
		 dev_name(vsp2->dev));

	v4l2_set_subdevdata(subdev, clu);

	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	vsp2_entity_init_formats(subdev, NULL);

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
	clu->buff_v = dma_alloc_coherent(vsp2->dev,
		CLU_BUFF_SIZE, &clu->buff_h, GFP_KERNEL|GFP_DMA);
#endif

	return clu;
}
