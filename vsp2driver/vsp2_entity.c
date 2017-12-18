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

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_entity.h"
#include "vsp2_vspm.h"

static inline struct vsp2_entity *
media_entity_to_vsp2_entity(struct media_entity *entity)
{
	return container_of(entity, struct vsp2_entity, subdev.entity);
}

void vsp2_entity_route_setup(struct vsp2_entity *source)
{
	struct vsp2_entity *sink;
	u32 connect = 0;
	struct vsp_start_t *vsp_start = source->vsp2->vspm->ip_par.par.vsp;

	if (source->type == VSP2_ENTITY_WPF)
		return;

	sink = media_entity_to_vsp2_entity(source->sink);

	switch (sink->type) {
	case VSP2_ENTITY_WPF:
		connect = 0;
		break;
	case VSP2_ENTITY_UDS:
		vsp_start->use_module |= VSP_UDS_USE;
		connect = VSP_UDS_USE;
		break;
	case VSP2_ENTITY_BRU:
		vsp_start->use_module |= VSP_BRU_USE;
		connect = VSP_BRU_USE;
		break;
	case VSP2_ENTITY_BRS:
		vsp_start->use_module |= VSP_BRS_USE;
		connect = VSP_BRS_USE;
		break;
	case VSP2_ENTITY_LUT:
		vsp_start->use_module |= VSP_LUT_USE;
		connect = VSP_LUT_USE;
		break;
	case VSP2_ENTITY_CLU:
		vsp_start->use_module |= VSP_CLU_USE;
		connect = VSP_CLU_USE;
		break;
	default:
		dev_err(source->vsp2->dev,
			"Invalid sink type.(%d)\n",
			sink->type);
		/* Invalid type. */
		break;
	}

	switch (source->type) {
	case VSP2_ENTITY_RPF:
		if (source->index < 5) {
			vsp_start->src_par[source->index]->connect = connect;
		} else {
			dev_err(source->vsp2->dev,
				"Invalid PRF index.(%d)\n",
				source->index);
			/* Invalid index. */
		}
		break;
	case VSP2_ENTITY_UDS:
		vsp_start->ctrl_par->uds->connect = connect;
		break;
	case VSP2_ENTITY_LUT:
		vsp_start->ctrl_par->lut->connect = connect;
		break;
	case VSP2_ENTITY_CLU:
		vsp_start->ctrl_par->clu->connect = connect;
		break;
	case VSP2_ENTITY_BRU:
		vsp_start->ctrl_par->bru->connect = connect;
		break;
	case VSP2_ENTITY_BRS:
		vsp_start->ctrl_par->brs->connect = connect;
		break;
	default:
		dev_err(source->vsp2->dev,
			"Invalid source type.(%d)\n",
			source->type);
		/* Invalid type. */
		break;
	}
}

/* -----------------------------------------------------------------------------
 * V4L2 Subdevice Operations
 */

/**
 * vsp2_entity_get_pad_config - Get the pad configuration for an entity
 * @entity: the entity
 * @cfg: the TRY pad configuration
 * @which: configuration selector (ACTIVE or TRY)
 *
 * When called with which set to V4L2_SUBDEV_FORMAT_ACTIVE the caller must hold
 * the entity lock to access the returned configuration.
 *
 * Return the pad configuration requested by the which argument. The TRY
 * configuration is passed explicitly to the function through the cfg argument
 * and simply returned when requested. The ACTIVE configuration comes from the
 * entity structure.
 */
struct v4l2_subdev_pad_config *
vsp2_entity_get_pad_config(struct vsp2_entity *entity,
			   struct v4l2_subdev_pad_config *cfg,
			   enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return entity->config;
	case V4L2_SUBDEV_FORMAT_TRY:
	default:
		return cfg;
	}
}

/**
 * vsp2_entity_get_pad_format - Get a pad format from storage for an entity
 * @entity: the entity
 * @cfg: the configuration storage
 * @pad: the pad number
 *
 * Return the format stored in the given configuration for an entity's pad. The
 * configuration can be an ACTIVE or TRY configuration.
 */
struct v4l2_mbus_framefmt *
vsp2_entity_get_pad_format(struct vsp2_entity *entity,
			   struct v4l2_subdev_pad_config *cfg,
			   unsigned int pad)
{
	return v4l2_subdev_get_try_format(&entity->subdev, cfg, pad);
}

/**
 * vsp2_entity_get_pad_selection - Get a pad selection from storage for entity
 * @entity: the entity
 * @cfg: the configuration storage
 * @pad: the pad number
 * @target: the selection target
 *
 * Return the selection rectangle stored in the given configuration for an
 * entity's pad. The configuration can be an ACTIVE or TRY configuration. The
 * selection target can be COMPOSE or CROP.
 */
struct v4l2_rect *
vsp2_entity_get_pad_selection(struct vsp2_entity *entity,
			      struct v4l2_subdev_pad_config *cfg,
			      unsigned int pad, unsigned int target)
{
	switch (target) {
	case V4L2_SEL_TGT_COMPOSE:
		return v4l2_subdev_get_try_compose(&entity->subdev, cfg, pad);
	case V4L2_SEL_TGT_CROP:
		return v4l2_subdev_get_try_crop(&entity->subdev, cfg, pad);
	default:
		return NULL;
	}
}

/*
 * vsp2_entity_init_cfg - Initialize formats on all pads
 * @subdev: V4L2 subdevice
 * @cfg: V4L2 subdev pad configuration
 *
 * Initialize all pad formats with default values in the given pad config. This
 * function can be used as a handler for the subdev pad::init_cfg operation.
 */
int vsp2_entity_init_cfg(struct v4l2_subdev *subdev,
			 struct v4l2_subdev_pad_config *cfg)
{
	struct v4l2_subdev_format format;
	unsigned int pad;

	for (pad = 0; pad < subdev->entity.num_pads - 1; ++pad) {
		memset(&format, 0, sizeof(format));

		format.pad = pad;
		format.which = cfg ? V4L2_SUBDEV_FORMAT_TRY
			     : V4L2_SUBDEV_FORMAT_ACTIVE;

		v4l2_subdev_call(subdev, pad, set_fmt, cfg, &format);
	}

	return 0;
}

/*
 * vsp2_subdev_get_pad_format - Subdev pad get_fmt handler
 * @subdev: V4L2 subdevice
 * @cfg: V4L2 subdev pad configuration
 * @fmt: V4L2 subdev format
 *
 * This function implements the subdev get_fmt pad operation. It can be used as
 * a direct drop-in for the operation handler.
 */
int vsp2_subdev_get_pad_format(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_format *fmt)
{
	struct vsp2_entity *entity = to_vsp2_entity(subdev);
	struct v4l2_subdev_pad_config *config;

	config = vsp2_entity_get_pad_config(entity, cfg, fmt->which);
	if (!config)
		return -EINVAL;

	mutex_lock(&entity->lock);
	fmt->format = *vsp2_entity_get_pad_format(entity, config, fmt->pad);
	mutex_unlock(&entity->lock);

	return 0;
}

/*
 * vsp2_subdev_enum_mbus_code - Subdev pad enum_mbus_code handler
 * @subdev: V4L2 subdevice
 * @cfg: V4L2 subdev pad configuration
 * @code: Media bus code enumeration
 * @codes: Array of supported media bus codes
 * @ncodes: Number of supported media bus codes
 *
 * This function implements the subdev enum_mbus_code pad operation for entities
 * that do not support format conversion. It enumerates the given supported
 * media bus codes on the sink pad and reports a source pad format identical to
 * the sink pad.
 */
int vsp2_subdev_enum_mbus_code(struct v4l2_subdev *subdev,
			       struct v4l2_subdev_pad_config *cfg,
			       struct v4l2_subdev_mbus_code_enum *code,
			       const unsigned int *codes, unsigned int ncodes)
{
	struct vsp2_entity *entity = to_vsp2_entity(subdev);

	if (code->pad == 0) {
		if (code->index >= ncodes)
			return -EINVAL;

		code->code = codes[code->index];
	} else {
		struct v4l2_subdev_pad_config *config;
		struct v4l2_mbus_framefmt *format;

		/* The entity can't perform format conversion, the sink format
		 * is always identical to the source format.
		 */
		if (code->index)
			return -EINVAL;

		config = vsp2_entity_get_pad_config(entity, cfg, code->which);
		if (!config)
			return -EINVAL;

		mutex_lock(&entity->lock);
		format = vsp2_entity_get_pad_format(entity, config, 0);
		code->code = format->code;
		mutex_unlock(&entity->lock);
	}

	return 0;
}

/*
 * vsp2_subdev_enum_frame_size - Subdev pad enum_frame_size handler
 * @subdev: V4L2 subdevice
 * @cfg: V4L2 subdev pad configuration
 * @fse: Frame size enumeration
 * @min_width: Minimum image width
 * @min_height: Minimum image height
 * @max_width: Maximum image width
 * @max_height: Maximum image height
 *
 * This function implements the subdev enum_frame_size pad operation for
 * entities that do not support scaling or cropping. It reports the given
 * minimum and maximum frame width and height on the sink pad, and a fixed
 * source pad size identical to the sink pad.
 */
int vsp2_subdev_enum_frame_size(struct v4l2_subdev *subdev,
				struct v4l2_subdev_pad_config *cfg,
				struct v4l2_subdev_frame_size_enum *fse,
				unsigned int min_width, unsigned int min_height,
				unsigned int max_width, unsigned int max_height)
{
	struct vsp2_entity *entity = to_vsp2_entity(subdev);
	struct v4l2_subdev_pad_config *config;
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	config = vsp2_entity_get_pad_config(entity, cfg, fse->which);
	if (!config)
		return -EINVAL;

	format = vsp2_entity_get_pad_format(entity, config, fse->pad);

	mutex_lock(&entity->lock);
	if (fse->index || fse->code != format->code) {
		ret = -EINVAL;
		goto done;
	}

	if (fse->pad == 0) {
		fse->min_width = min_width;
		fse->max_width = max_width;
		fse->min_height = min_height;
		fse->max_height = max_height;
	} else {
		/* The size on the source pad are fixed and always identical to
		 * the size on the sink pad.
		 */
		fse->min_width = format->width;
		fse->max_width = format->width;
		fse->min_height = format->height;
		fse->max_height = format->height;
	}

done:
	mutex_unlock(&entity->lock);
	return ret;
}

/* -----------------------------------------------------------------------------
 * Media Operations
 */

int vsp2_entity_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct vsp2_entity *source;

	if (!(local->flags & MEDIA_PAD_FL_SOURCE))
		return 0;

	source = media_entity_to_vsp2_entity(local->entity);

	if (flags & MEDIA_LNK_FL_ENABLED) {
		if (source->sink)
			return -EBUSY;
		source->sink = remote->entity;
		source->sink_pad = remote->index;
	} else {
		source->sink = NULL;
		source->sink_pad = 0;
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * Initialization
 */

struct vsp2_route {
	enum		vsp2_entity_type	type;
	unsigned	int					index;
};

/* suppurt entities... */

static const struct vsp2_route vsp2_routes[] = {
	{ VSP2_ENTITY_BRU, 0 },
	{ VSP2_ENTITY_LUT, 0 },
	{ VSP2_ENTITY_CLU, 0 },
	{ VSP2_ENTITY_HGO, 0 },
	{ VSP2_ENTITY_HGT, 0 },
	{ VSP2_ENTITY_RPF, 0 },
	{ VSP2_ENTITY_RPF, 1 },
	{ VSP2_ENTITY_RPF, 2 },
	{ VSP2_ENTITY_RPF, 3 },
#ifdef TYPE_GEN2  /* TODO: delete TYPE_GEN2 */
/*	{ VSP2_ENTITY_RPF, 4 },*/
#else
	{ VSP2_ENTITY_RPF, 4 },
#endif
	{ VSP2_ENTITY_UDS, 0 },
	{ VSP2_ENTITY_BRS, 0 },
	{ VSP2_ENTITY_WPF, 0 },
};

int vsp2_entity_init(struct vsp2_device *vsp2, struct vsp2_entity *entity,
		     const char *name, unsigned int num_pads,
		     const struct v4l2_subdev_ops *ops, u32 function)
{
	struct v4l2_subdev *subdev;
	unsigned int i;
	bool flag = false;
	int ret;

	for (i = 0; i < ARRAY_SIZE(vsp2_routes); ++i) {
		if (vsp2_routes[i].type == entity->type &&
		    vsp2_routes[i].index == entity->index) {
			flag = true; /* found !! */
			break;
		}
	}

	if (!flag)
		return -EINVAL;

	mutex_init(&entity->lock);

	entity->vsp2 = vsp2;
	entity->source_pad = num_pads - 1;

	/* Allocate and initialize pads. */
	entity->pads = devm_kzalloc(vsp2->dev, num_pads * sizeof(*entity->pads),
				    GFP_KERNEL);
	if (!entity->pads)
		return -ENOMEM;

	for (i = 0; i < num_pads - 1; ++i)
		entity->pads[i].flags = MEDIA_PAD_FL_SINK;

	entity->pads[num_pads - 1].flags = MEDIA_PAD_FL_SOURCE;

	/* Initialize the media entity. */
	ret = media_entity_pads_init(&entity->subdev.entity, num_pads,
				     entity->pads);
	if (ret < 0)
		return ret;

	/* Initialize the V4L2 subdev. */
	subdev = &entity->subdev;
	v4l2_subdev_init(subdev, ops);

	subdev->entity.function = function;
	subdev->entity.ops = &vsp2->media_ops;
	subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	snprintf(subdev->name, sizeof(subdev->name), "%s %s",
		 dev_name(vsp2->dev), name);

	vsp2_entity_init_cfg(subdev, NULL);

	/* Allocate the pad configuration to store formats and selection
	 * rectangles.
	 */
	entity->config = v4l2_subdev_alloc_pad_config(&entity->subdev);
	if (!entity->config) {
		media_entity_cleanup(&entity->subdev.entity);
		return -ENOMEM;
	}

	return 0;
}

void vsp2_entity_destroy(struct vsp2_entity *entity)
{
	if (entity->ops && entity->ops->destroy)
		entity->ops->destroy(entity);
	if (entity->subdev.ctrl_handler)
		v4l2_ctrl_handler_free(entity->subdev.ctrl_handler);
	v4l2_subdev_free_pad_config(entity->config);
	media_entity_cleanup(&entity->subdev.entity);
}
