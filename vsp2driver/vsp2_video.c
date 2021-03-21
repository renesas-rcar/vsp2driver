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

#include <linux/dma-fence.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/dma-resv.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/media-entity.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-dma-contig.h>

#include "vsp2_device.h"
#include "vsp2_bru.h"
#include "vsp2_brs.h"
#include "vsp2_entity.h"
#include "vsp2_pipe.h"
#include "vsp2_rwpf.h"
#include "vsp2_uds.h"
#include "vsp2_hgo.h"
#include "vsp2_hgt.h"
#include "vsp2_video.h"
#include "vsp2_vspm.h"
#include "vsp2_debug.h"

#define VSP2_VIDEO_DEF_FORMAT		V4L2_PIX_FMT_YUYV
#define VSP2_VIDEO_DEF_WIDTH		1024
#define VSP2_VIDEO_DEF_HEIGHT		768

#define VSP2_VIDEO_MIN_WIDTH		2U
#define VSP2_VIDEO_MAX_WIDTH		8190U
#define VSP2_VIDEO_MIN_HEIGHT		2U
#define VSP2_VIDEO_MAX_HEIGHT		8190U
#define VSP2_VIDEO_MIN_WIDTH_RGB	1U
#define VSP2_VIDEO_MIN_HEIGHT_RGB	1U

/* -----------------------------------------------------------------------------
 * Helper functions
 */

static struct v4l2_subdev *
vsp2_video_remote_subdev(struct media_pad *local, u32 *pad)
{
	struct media_pad *remote;

	remote = media_entity_remote_pad(local);
	if (!remote || !is_media_entity_v4l2_subdev(remote->entity))
		return NULL;

	if (pad)
		*pad = remote->index;

	return media_entity_to_v4l2_subdev(remote->entity);
}

static int vsp2_video_verify_format(struct vsp2_video *video)
{
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev *subdev;
	int ret;

	subdev = vsp2_video_remote_subdev(&video->pad, &fmt.pad);
	if (!subdev)
		return -EINVAL;

	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &fmt);
	if (ret < 0)
		return ret == -ENOIOCTLCMD ? -EINVAL : ret;

	if (video->rwpf->fmtinfo->mbus != fmt.format.code ||
	    video->rwpf->format.height != fmt.format.height ||
	    video->rwpf->format.width != fmt.format.width)
		return -EINVAL;

	return 0;
}

static int __vsp2_video_try_format(struct vsp2_video *video,
				   struct v4l2_pix_format_mplane *pix,
				   const struct vsp2_format_info **fmtinfo)
{
	static const u32 xrgb_formats[][2] = {
		{ V4L2_PIX_FMT_RGB444, V4L2_PIX_FMT_XRGB444 },
		{ V4L2_PIX_FMT_RGB555, V4L2_PIX_FMT_XRGB555 },
		{ V4L2_PIX_FMT_BGR32, V4L2_PIX_FMT_XBGR32 },
		{ V4L2_PIX_FMT_RGB32, V4L2_PIX_FMT_XRGB32 },
	};

	const struct vsp2_format_info *info;
	unsigned int width = pix->width;
	unsigned int height = pix->height;
	unsigned int i;

	/* Backward compatibility: replace deprecated RGB formats by their XRGB
	 * equivalent. This selects the format older userspace applications want
	 * while still exposing the new format.
	 */
	for (i = 0; i < ARRAY_SIZE(xrgb_formats); ++i) {
		if (xrgb_formats[i][0] == pix->pixelformat) {
			pix->pixelformat = xrgb_formats[i][1];
			break;
		}
	}

	/* Retrieve format information and select the default format if the
	 * requested format isn't supported.
	 */
	info = vsp2_get_format_info(pix->pixelformat);
	if (!info)
		info = vsp2_get_format_info(VSP2_VIDEO_DEF_FORMAT);

	pix->pixelformat = info->fourcc;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	pix->field = V4L2_FIELD_NONE;
	memset(pix->reserved, 0, sizeof(pix->reserved));

	/* Align the width and height for YUV 4:2:2 and 4:2:0 formats. */
	width = round_down(width, info->hsub);
	height = round_down(height, info->vsub);

	/* Clamp the width and height. */
	if (info->mbus == MEDIA_BUS_FMT_ARGB8888_1X32) {
		pix->width = clamp(width, VSP2_VIDEO_MIN_WIDTH_RGB,
				   VSP2_VIDEO_MAX_WIDTH);
		pix->height = clamp(height, VSP2_VIDEO_MIN_HEIGHT_RGB,
				    VSP2_VIDEO_MAX_HEIGHT);
	} else {
		pix->width = clamp(width, VSP2_VIDEO_MIN_WIDTH,
				   VSP2_VIDEO_MAX_WIDTH);
		pix->height = clamp(height, VSP2_VIDEO_MIN_HEIGHT,
				    VSP2_VIDEO_MAX_HEIGHT);
	}

	for (i = 0; i < min(info->planes, 2U); ++i) {
		unsigned int hsub = i > 0 ? info->hsub : 1;
		unsigned int vsub = i > 0 ? info->vsub : 1;
		unsigned int bpl;

		bpl = clamp_t(unsigned int, pix->plane_fmt[i].bytesperline,
			      pix->width / hsub * info->bpp[i] / 8, 65535U);

		pix->plane_fmt[i].bytesperline = bpl;
		pix->plane_fmt[i].sizeimage = pix->plane_fmt[i].bytesperline
					    * pix->height / vsub;
	}

	if (info->planes == 3) {
		/* The second and third planes must have the same stride. */
		pix->plane_fmt[2].bytesperline = pix->plane_fmt[1].bytesperline;
		pix->plane_fmt[2].sizeimage = pix->plane_fmt[1].sizeimage;
	}

	pix->num_planes = info->planes;

	if (fmtinfo)
		*fmtinfo = info;

	return 0;
}

struct csc_element {
	unsigned int mbus;
	unsigned char ycbcr_enc;
	unsigned char quant;
};

static unsigned char determine_ycbcr_enc(unsigned char ycbcr_enc)
{
	if (ycbcr_enc == V4L2_YCBCR_ENC_709)
		return V4L2_YCBCR_ENC_709;
	return V4L2_YCBCR_ENC_601;
}

static unsigned char determine_quantization(unsigned int mbus,
					    unsigned char quant)
{
	if (mbus == MEDIA_BUS_FMT_ARGB8888_1X32) {
		if (quant == V4L2_QUANTIZATION_LIM_RANGE)
			return V4L2_QUANTIZATION_LIM_RANGE;
		return V4L2_QUANTIZATION_FULL_RANGE;
	}

	/* MEDIA_BUS_FMT_AYUV8_1X32 */
	if (quant == V4L2_QUANTIZATION_FULL_RANGE)
		return V4L2_QUANTIZATION_FULL_RANGE;
	return V4L2_QUANTIZATION_LIM_RANGE;
}

static bool need_csc(struct csc_element *wpf, struct csc_element *rpf,
		     int rpf_cnt)
{
	int i;

	for (i = 0; i < rpf_cnt; i++) {
		if (wpf->mbus != rpf[i].mbus)
			return true;	/* need color space conversion */
	}

	return false;
}

static int get_csc_mode(struct csc_element *wpf, struct csc_element *rpf)
{
	struct csc_element *rgb;
	struct csc_element *yuv;

	if (wpf->mbus == MEDIA_BUS_FMT_ARGB8888_1X32) {
		rgb = wpf;
		yuv = rpf;
	} else {
		rgb = rpf;
		yuv = wpf;
	}

	if (rgb->quant == V4L2_QUANTIZATION_FULL_RANGE) {
		if (yuv->quant == V4L2_QUANTIZATION_LIM_RANGE) {
			if (yuv->ycbcr_enc == V4L2_YCBCR_ENC_601)
				return CSC_MODE_601_LIMITED;
			else
				return CSC_MODE_709_LIMITED;
		}

		/* yuv->quant == V4L2_QUANTIZATION_FULL_RANGE */
		if (yuv->ycbcr_enc == V4L2_YCBCR_ENC_601)
			return CSC_MODE_601_FULL;

		return -1;	/* wrong */
	}

	/* rgb->quant == V4L2_QUANTIZATION_LIM_RANGE */
	if (yuv->quant != V4L2_QUANTIZATION_LIM_RANGE)
		return -1;	/* wrong */
	if (yuv->ycbcr_enc != V4L2_YCBCR_ENC_709)
		return -1;	/* wrong */

	return CSC_MODE_709_FULL;
}

static int vsp2_determine_csc_mode(struct vsp2_pipeline *pipe)
{
	struct vsp2_entity *entity;
	struct csc_element csc_wpf;
	struct csc_element csc_rpf[5];
	unsigned int mbus;
	unsigned char ycbcr_enc;
	unsigned char quant;
	int rpf_cnt;
	int csc_mode = -1;
	int ret;
	int i;

	rpf_cnt = 0;
	list_for_each_entry(entity, &pipe->entities, list_pipe) {
		if (entity->type == VSP2_ENTITY_WPF) {
			/* clear csc mode */
			vsp2_rwpf_set_csc_mode(entity, CSC_MODE_DEFAULT);

			vsp2_rwpf_get_csc_element(entity, &mbus, &ycbcr_enc,
						  &quant);
			csc_wpf.mbus = mbus;
			csc_wpf.ycbcr_enc = determine_ycbcr_enc(ycbcr_enc);
			csc_wpf.quant = determine_quantization(mbus, quant);
		} else if (entity->type == VSP2_ENTITY_RPF) {
			/* clear csc mode */
			vsp2_rwpf_set_csc_mode(entity, CSC_MODE_DEFAULT);

			vsp2_rwpf_get_csc_element(entity, &mbus, &ycbcr_enc,
						  &quant);
			csc_rpf[rpf_cnt].mbus = mbus;
			csc_rpf[rpf_cnt].ycbcr_enc =
					determine_ycbcr_enc(ycbcr_enc);
			csc_rpf[rpf_cnt].quant =
					determine_quantization(mbus, quant);
			rpf_cnt++;
		}
	}

	if (!need_csc(&csc_wpf, csc_rpf, rpf_cnt))
		return 0;	/* not required */

	for (i = 0; i < rpf_cnt; i++) {
		if (csc_wpf.mbus == csc_rpf[i].mbus)
			continue;

		ret = get_csc_mode(&csc_wpf, &csc_rpf[i]);
		if (ret < 0)
			return -1;	/* wrong combination */

		if (csc_mode == -1)
			csc_mode = ret;
		else if (csc_mode != ret)
			return -1;	/* wrong combination */
	}

	/* set csc mode */
	list_for_each_entry(entity, &pipe->entities, list_pipe) {
		if (entity->type == VSP2_ENTITY_WPF ||
		    entity->type == VSP2_ENTITY_RPF)
			vsp2_rwpf_set_csc_mode(entity, csc_mode);
	}

	return 0;
}

/* -----------------------------------------------------------------------------
 * Pipeline Management
 */

/*
 * vsp2_video_complete_buffer - Complete the current buffer
 * @video: the video node
 *
 * This function completes the current buffer by filling its sequence number,
 * time stamp and payload size, and hands it back to the videobuf core.
 *
 * When operating in DU output mode (deep pipeline to the DU through the LIF),
 * the VSP2 needs to constantly supply frames to the display. In that case, if
 * no other buffer is queued, reuse the one that has just been processed instead
 * of handing it back to the videobuf core.
 *
 * Return the next queued buffer or NULL if the queue is empty.
 */
static struct vsp2_vb2_buffer *
vsp2_video_complete_buffer(struct vsp2_video *video)
{
	struct vsp2_pipeline *pipe = video->rwpf->pipe;
	struct vsp2_vb2_buffer *next = NULL;
	struct vsp2_vb2_buffer *done;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&video->irqlock, flags);

	if (list_empty(&video->irqqueue)) {
		spin_unlock_irqrestore(&video->irqlock, flags);
		return NULL;
	}

	done = list_first_entry(&video->irqqueue,
				struct vsp2_vb2_buffer, queue);

	list_del(&done->queue);

	if (!list_empty(&video->irqqueue))
		next = list_first_entry(&video->irqqueue,
					struct vsp2_vb2_buffer, queue);

	spin_unlock_irqrestore(&video->irqlock, flags);

	done->buf.sequence = pipe->sequence;
	done->buf.vb2_buf.timestamp = ktime_get_ns();
	for (i = 0; i < done->buf.vb2_buf.num_planes; ++i)
		vb2_set_plane_payload(&done->buf.vb2_buf, i,
				      vb2_plane_size(&done->buf.vb2_buf, i));
	vb2_buffer_done(&done->buf.vb2_buf, VB2_BUF_STATE_DONE);

	return next;
}

static void vsp2_video_frame_end(struct vsp2_pipeline *pipe,
				 struct vsp2_rwpf *rwpf)
{
	struct vsp2_video *video = rwpf->video;
	struct vsp2_vb2_buffer *buf;

	buf = vsp2_video_complete_buffer(video);
	if (!buf)
		return;

	video->rwpf->mem = buf->mem;
	pipe->buffers_ready |= 1 << video->pipe_index;
}

static void vsp2_video_pipeline_run(struct vsp2_pipeline *pipe)
{
	struct vsp2_device *vsp2 = pipe->output->entity.vsp2;
	unsigned int i;

	for (i = 0; i < vsp2->pdata.rpf_count; ++i) {
		struct vsp2_rwpf *rwpf = pipe->inputs[i];

		if (rwpf)
			vsp2_rwpf_set_memory(rwpf);
	}

	vsp2_rwpf_set_memory(pipe->output);

	vsp2_pipeline_run(pipe);
}

static void vsp2_video_pipeline_frame_end(struct vsp2_pipeline *pipe)
{
	struct vsp2_device *vsp2 = pipe->output->entity.vsp2;
	enum vsp2_pipeline_state state;
	unsigned long flags;
	unsigned int i;

	spin_lock_irqsave(&pipe->irqlock, flags);

	/* Complete buffers on all video nodes. */
	for (i = 0; i < vsp2->pdata.rpf_count; ++i) {
		if (!pipe->inputs[i])
			continue;

		vsp2_video_frame_end(pipe, pipe->inputs[i]);
	}

	vsp2_video_frame_end(pipe, pipe->output);

	state = pipe->state;
	pipe->state = VSP2_PIPELINE_STOPPED;

	/* If a stop has been requested, mark the pipeline as stopped and
	 * return. Otherwise restart the pipeline if ready.
	 */
	if (state == VSP2_PIPELINE_STOPPING)
		wake_up(&pipe->wq);
	else if (vsp2_pipeline_ready(pipe))
		vsp2_video_pipeline_run(pipe);

	spin_unlock_irqrestore(&pipe->irqlock, flags);
}

static int vsp2_video_pipeline_build_branch(struct vsp2_pipeline *pipe,
					    struct vsp2_rwpf *input,
					    struct vsp2_rwpf *output)
{
	struct media_entity_enum ent_enum;
	struct vsp2_entity *entity;
	struct media_pad *pad;
	bool bru_found = false;
	int ret;

	ret = media_entity_enum_init(&ent_enum, &input->entity.vsp2->media_dev);
	if (ret < 0)
		return ret;

	pad = media_entity_remote_pad(&input->entity.pads[RWPF_PAD_SOURCE]);

	while (1) {
		if (!pad) {
			ret = -EPIPE;
			goto out;
		}

		/* We've reached a video node, that shouldn't have happened. */
		if (!is_media_entity_v4l2_subdev(pad->entity)) {
			ret = -EPIPE;
			goto out;
		}

		entity = to_vsp2_entity(
			media_entity_to_v4l2_subdev(pad->entity));

		/* A BRU is present in the pipeline, store the BRU input pad
		 * number in the input RPF for use when configuring the RPF.
		 */
		if (entity->type == VSP2_ENTITY_BRU) {
			struct vsp2_bru *bru = to_bru(&entity->subdev);

			bru->inputs[pad->index].rpf = input;
			input->bru_input = pad->index;

			bru_found = true;
		}

		if (entity->type == VSP2_ENTITY_BRS) {
			struct vsp2_brs *brs = to_brs(&entity->subdev);

			brs->inputs[pad->index].rpf = input;
			input->brs_input = pad->index;
		}

		/* We've reached the WPF, we're done. */
		if (entity->type == VSP2_ENTITY_WPF)
			break;

		/* Ensure the branch has no loop. */
		if (media_entity_enum_test_and_set(&ent_enum,
						   &entity->subdev.entity)) {
			ret = -EPIPE;
			goto out;
		}

		/* UDS can't be chained. */
		if (entity->type == VSP2_ENTITY_UDS) {
			if (pipe->uds) {
				ret = -EPIPE;
				goto out;
			}

			pipe->uds = entity;
			pipe->uds_input = bru_found ? pipe->bru
					: &input->entity;
		}

		/* Follow the source link. The link setup operations ensure
		 * that the output fan-out can't be more than one, there is thus
		 * no need to verify here that only a single source link is
		 * activated.
		 */
		pad = &entity->pads[entity->source_pad];
		pad = media_entity_remote_pad(pad);
	}

	/* The last entity must be the output WPF. */
	if (entity != &output->entity)
		ret = -EPIPE;

out:
	media_entity_enum_cleanup(&ent_enum);

	return ret;
}

static int vsp2_video_pipeline_build(struct vsp2_pipeline *pipe,
				     struct vsp2_video *video)
{
	struct media_graph graph;
	struct media_entity *entity = &video->video.entity;
	struct media_device *mdev = entity->graph_obj.mdev;
	unsigned int i;
	int ret;

	/* Walk the graph to locate the entities and video nodes. */
	ret = media_graph_walk_init(&graph, mdev);
	if (ret)
		return ret;

	media_graph_walk_start(&graph, entity);

	while ((entity = media_graph_walk_next(&graph))) {
		struct v4l2_subdev *subdev;
		struct vsp2_rwpf *rwpf;
		struct vsp2_entity *e;

		if (!is_media_entity_v4l2_subdev(entity)) {
			pipe->num_video++;
			continue;
		}

		subdev = media_entity_to_v4l2_subdev(entity);
		e = to_vsp2_entity(subdev);
		list_add_tail(&e->list_pipe, &pipe->entities);

		if (e->type == VSP2_ENTITY_RPF) {
			rwpf = to_rwpf(subdev);
			pipe->inputs[rwpf->entity.index] = rwpf;
			rwpf->video->pipe_index = ++pipe->num_inputs;
			rwpf->pipe = pipe;
		} else if (e->type == VSP2_ENTITY_WPF) {
			rwpf = to_rwpf(subdev);
			pipe->output = rwpf;
			rwpf->video->pipe_index = 0;
			rwpf->pipe = pipe;
		} else if (e->type == VSP2_ENTITY_BRU) {
			pipe->bru = e;
		} else if (e->type == VSP2_ENTITY_BRS) {
			pipe->brs = e;
		}
	}

	media_graph_walk_cleanup(&graph);

	/* We need one output and at least one input. */
	if (pipe->num_inputs == 0 || !pipe->output)
		return -EPIPE;

	/* Follow links downstream for each input and make sure the graph
	 * contains no loop and that all branches end at the output WPF.
	 */
	for (i = 0; i < video->vsp2->pdata.rpf_count; ++i) {
		if (!pipe->inputs[i])
			continue;

		ret = vsp2_video_pipeline_build_branch(pipe, pipe->inputs[i],
						       pipe->output);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int vsp2_video_pipeline_init(struct vsp2_pipeline *pipe,
				    struct vsp2_video *video)
{
	vsp2_pipeline_init(pipe);

	pipe->frame_end = vsp2_video_pipeline_frame_end;

	return vsp2_video_pipeline_build(pipe, video);
}

static struct vsp2_pipeline *vsp2_video_pipeline_get(struct vsp2_video *video)
{
	struct vsp2_pipeline *pipe;
	int ret;

	/* Get a pipeline object for the video node. If a pipeline has already
	 * been allocated just increment its reference count and return it.
	 * Otherwise allocate a new pipeline and initialize it, it will be freed
	 * when the last reference is released.
	 */
	if (!video->rwpf->pipe) {
		pipe = kzalloc(sizeof(*pipe), GFP_KERNEL);
		if (!pipe)
			return ERR_PTR(-ENOMEM);

		ret = vsp2_video_pipeline_init(pipe, video);
		if (ret < 0) {
			vsp2_pipeline_reset(pipe);
			kfree(pipe);
			return ERR_PTR(ret);
		}
	} else {
		pipe = video->rwpf->pipe;
		kref_get(&pipe->kref);
	}

	return pipe;
}

static void vsp2_video_pipeline_release(struct kref *kref)
{
	struct vsp2_pipeline *pipe = container_of(kref, typeof(*pipe), kref);

	vsp2_pipeline_reset(pipe);
	kfree(pipe);
}

static void vsp2_video_pipeline_put(struct vsp2_pipeline *pipe)
{
	struct media_device *mdev = &pipe->output->entity.vsp2->media_dev;

	mutex_lock(&mdev->graph_mutex);
	kref_put(&pipe->kref, vsp2_video_pipeline_release);
	mutex_unlock(&mdev->graph_mutex);
}

/* -----------------------------------------------------------------------------
 * videobuf2 Queue Operations
 */

static int
vsp2_video_queue_setup(struct vb2_queue *vq,
		       unsigned int *nbuffers, unsigned int *nplanes,
		       unsigned int sizes[], struct device *alloc_devs[])
{
	struct vsp2_video *video = vb2_get_drv_priv(vq);
	const struct v4l2_pix_format_mplane *format = &video->rwpf->format;
	unsigned int i;

	if (*nplanes) {
		if (*nplanes != format->num_planes)
			return -EINVAL;

		for (i = 0; i < *nplanes; i++)
			if (sizes[i] < format->plane_fmt[i].sizeimage)
				return -EINVAL;
		return 0;
	}

	*nplanes = format->num_planes;

	for (i = 0; i < format->num_planes; ++i)
		sizes[i] = format->plane_fmt[i].sizeimage;

	return 0;
}

static int vsp2_video_buffer_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vsp2_video *video = vb2_get_drv_priv(vq);
	struct vsp2_vb2_buffer *buf = to_vsp2_vb2_buffer(vbuf);
	const struct v4l2_pix_format_mplane *format = &video->rwpf->format;
	unsigned int i;

	if (vb->num_planes < format->num_planes)
		return -EINVAL;

	if (vq->memory == VB2_MEMORY_DMABUF &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		struct dma_resv *resv = vb->planes[0].dbuf->resv;

		if (resv) {
			struct dma_fence *fence;

			fence = dma_resv_get_excl_rcu(resv);
			if (fence) {
				int ret = dma_fence_wait(fence, true);

				if (ret)
					return ret;
				dma_fence_put(fence);
			}
		}
	}

	for (i = 0; i < vb->num_planes; ++i) {
		buf->mem.addr[i] = vb2_dma_contig_plane_dma_addr(vb, i) +
                           vb->planes[i].data_offset;

		if (vb2_plane_size(vb, i) < format->plane_fmt[i].sizeimage)
			return -EINVAL;
	}

	for ( ; i < 3; ++i)
		buf->mem.addr[i] = 0;

	return 0;
}

static void vsp2_video_buffer_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct vsp2_video *video = vb2_get_drv_priv(vb->vb2_queue);
	struct vsp2_pipeline *pipe = video->rwpf->pipe;
	struct vsp2_vb2_buffer *buf = to_vsp2_vb2_buffer(vbuf);
	unsigned long flags;
	bool empty;

	spin_lock_irqsave(&video->irqlock, flags);
	empty = list_empty(&video->irqqueue);
	list_add_tail(&buf->queue, &video->irqqueue);
	spin_unlock_irqrestore(&video->irqlock, flags);

	if (!empty)
		return;

	spin_lock_irqsave(&pipe->irqlock, flags);

	video->rwpf->mem = buf->mem;
	pipe->buffers_ready |= 1 << video->pipe_index;

	if (vb2_is_streaming(&video->queue) &&
	    vsp2_pipeline_ready(pipe))
		vsp2_video_pipeline_run(pipe);

	spin_unlock_irqrestore(&pipe->irqlock, flags);
}

static void vsp2_video_buffer_finish(struct vb2_buffer *vb)
{
	struct vsp2_video *video = vb2_get_drv_priv(vb->vb2_queue);

	/* subdevice return proccess */

	if (video->vsp2->hgo)
		vsp2_hgo_buffer_finish(video->vsp2->hgo);

	if (video->vsp2->hgt)
		vsp2_hgt_buffer_finish(video->vsp2->hgt);
}

static struct vsp_start_t *to_vsp_par(struct vsp2_video	*video)
{
	return video->vsp2->vspm->ip_par.par.vsp;
}

static int vsp2_video_setup_pipeline(struct vsp2_pipeline *pipe,
				     struct vsp2_video *video)
{
	struct vsp2_entity *entity;
	int ret;
	int max_index_rpf = -1;
	struct vsp_start_t *vsp_start = to_vsp_par(video);

	/* reset vspm use module */
	vsp_start->use_module = 0;

	if (pipe->uds) {
		struct vsp2_uds *uds = to_uds(&pipe->uds->subdev);

		/* If a BRU is present in the pipeline before the UDS, the alpha
		 * component doesn't need to be scaled as the BRU output alpha
		 * value is fixed to 255. Otherwise we need to scale the alpha
		 * component only when available at the input RPF.
		 */
		if (pipe->uds_input->type == VSP2_ENTITY_BRU ||
		    pipe->uds_input->type == VSP2_ENTITY_BRS) {
			uds->scale_alpha = false;
		} else {
			struct vsp2_rwpf *rpf =
				to_rwpf(&pipe->uds_input->subdev);

			uds->scale_alpha = rpf->fmtinfo->alpha;
		}
	}

	if (vsp2_determine_csc_mode(pipe) < 0)
		VSP2_PRINT_ALERT("CSC mode is wrong. Use default.");

	list_for_each_entry(entity, &pipe->entities, list_pipe) {
		vsp2_entity_route_setup(entity);

		if (entity->type == VSP2_ENTITY_UDS) {
			ret = vsp2_uds_check_ratio(entity);
			if (ret < 0)
				goto error;
		}
		if (entity->type == VSP2_ENTITY_WPF) {
			ret = vsp2_rwpf_check_compose_size(entity);
			if (ret < 0)
				goto error;
		}

		if (entity->ops->configure)
			entity->ops->configure(entity, pipe);

		if (entity->type == VSP2_ENTITY_RPF) {
			if ((int)entity->index > max_index_rpf)
				max_index_rpf = entity->index;
		}
	}

	/* check rpf setting */

	if (vsp_start->rpf_num > 0 &&
	    vsp_start->rpf_num != (max_index_rpf + 1)) {
		VSP2_PRINT_ALERT("rpf setting error !!");

		ret = -EINVAL;
		goto error;
	}

	/* control entity setup -> set vspm params */

	/* - HGO */

	entity = &video->vsp2->hgo->entity;
	if (entity) {
		if (entity->ops->configure)
			entity->ops->configure(entity, pipe);
	}

	/* - HGT */

	entity = &video->vsp2->hgt->entity;
	if (entity) {
		if (entity->ops->configure)
			entity->ops->configure(entity, pipe);
	}

	/* We know that the WPF s_stream operation never fails. */
	v4l2_subdev_call(&pipe->output->entity.subdev, video, s_stream, 1);

	return 0;

error:
	return ret;
}

static int vsp2_video_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct vsp2_video *video = vb2_get_drv_priv(vq);
	struct vsp2_pipeline *pipe = video->rwpf->pipe;
	bool start_pipeline = false;
	unsigned long flags;
	int ret;

	mutex_lock(&pipe->lock);
	if (pipe->stream_count == pipe->num_video - 1) {
		ret = vsp2_video_setup_pipeline(pipe, video);
		if (ret < 0) {
			mutex_unlock(&pipe->lock);
			goto error_end;
		}

		start_pipeline = true;
	}

	pipe->stream_count++;
	mutex_unlock(&pipe->lock);

	/*
	 * vsp2_pipeline_ready() is not sufficient to establish that all streams
	 * are prepared and the pipeline is configured, as multiple streams
	 * can race through streamon with buffers already queued; Therefore we
	 * don't even attempt to start the pipeline until the last stream has
	 * called through here.
	 */
	if (!start_pipeline)
		return 0;

	spin_lock_irqsave(&pipe->irqlock, flags);
	if (vsp2_pipeline_ready(pipe))
		vsp2_video_pipeline_run(pipe);
	spin_unlock_irqrestore(&pipe->irqlock, flags);

	return 0;

error_end:
	spin_lock_irqsave(&video->irqlock, flags);

	while (!list_empty(&video->irqqueue)) {
		struct vsp2_vb2_buffer *buffer;

		buffer = list_entry(video->irqqueue.next,
				    struct vsp2_vb2_buffer, queue);
		list_del(&buffer->queue);
		vb2_buffer_done(&buffer->buf.vb2_buf, VB2_BUF_STATE_QUEUED);
	}

	spin_unlock_irqrestore(&video->irqlock, flags);

	return ret;
}

static void vsp2_video_stop_streaming(struct vb2_queue *vq)
{
	struct vsp2_video *video = vb2_get_drv_priv(vq);
	struct vsp2_pipeline *pipe = video->rwpf->pipe;
	struct vsp2_vb2_buffer *buffer;
	unsigned long flags;
	int ret;

	/* Clear the buffers ready flag to make sure the device won't be started
	 * by a QBUF on the video node on the other side of the pipeline.
	 */
	spin_lock_irqsave(&video->irqlock, flags);
	pipe->buffers_ready &= ~(1 << video->pipe_index);
	spin_unlock_irqrestore(&video->irqlock, flags);

	mutex_lock(&pipe->lock);
	if (--pipe->stream_count == pipe->num_inputs) {
		/* Stop the pipeline. */
		ret = vsp2_pipeline_stop(pipe);
		if (ret == -ETIMEDOUT)
			dev_err(video->vsp2->dev, "pipeline stop timeout\n");

		/* Initialize the VSPM parameters. */
		vsp2_vspm_param_init(&video->vsp2->vspm->ip_par);
	}
	mutex_unlock(&pipe->lock);

	media_pipeline_stop(&video->video.entity);
	vsp2_video_pipeline_put(pipe);

	/* Remove all buffers from the IRQ queue. */
	spin_lock_irqsave(&video->irqlock, flags);
	list_for_each_entry(buffer, &video->irqqueue, queue)
		vb2_buffer_done(&buffer->buf.vb2_buf, VB2_BUF_STATE_ERROR);
	INIT_LIST_HEAD(&video->irqqueue);
	spin_unlock_irqrestore(&video->irqlock, flags);
}

static const struct vb2_ops vsp2_video_queue_qops = {
	.queue_setup = vsp2_video_queue_setup,
	.buf_prepare = vsp2_video_buffer_prepare,
	.buf_queue = vsp2_video_buffer_queue,
	.buf_finish = vsp2_video_buffer_finish,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
	.start_streaming = vsp2_video_start_streaming,
	.stop_streaming = vsp2_video_stop_streaming,
};

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

static int
vsp2_video_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);

	cap->capabilities = V4L2_CAP_DEVICE_CAPS | V4L2_CAP_STREAMING
			  | V4L2_CAP_VIDEO_CAPTURE_MPLANE
			  | V4L2_CAP_VIDEO_OUTPUT_MPLANE;

	if (video->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE
				 | V4L2_CAP_STREAMING;
	else
		cap->device_caps = V4L2_CAP_VIDEO_OUTPUT_MPLANE
				 | V4L2_CAP_STREAMING;

	strlcpy(cap->driver, "vsp2", sizeof(cap->driver));
	strlcpy(cap->card, video->video.name, sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(video->vsp2->dev));

	return 0;
}

static int
vsp2_video_get_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);

	if (format->type != video->queue.type)
		return -EINVAL;

	mutex_lock(&video->lock);
	format->fmt.pix_mp = video->rwpf->format;
	mutex_unlock(&video->lock);

	return 0;
}

static int
vsp2_video_try_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);

	if (format->type != video->queue.type)
		return -EINVAL;

	return __vsp2_video_try_format(video, &format->fmt.pix_mp, NULL);
}

static int
vsp2_video_set_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);
	const struct vsp2_format_info *info;
	int ret;

	if (format->type != video->queue.type)
		return -EINVAL;

	ret = __vsp2_video_try_format(video, &format->fmt.pix_mp, &info);
	if (ret < 0)
		return ret;

	mutex_lock(&video->lock);

	if (vb2_is_busy(&video->queue)) {
		ret = -EBUSY;
		goto done;
	}

	video->rwpf->format = format->fmt.pix_mp;
	video->rwpf->fmtinfo = info;

done:
	mutex_unlock(&video->lock);
	return ret;
}

static int
vsp2_video_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);
	struct media_device *mdev = &video->vsp2->media_dev;
	struct vsp2_pipeline *pipe;
	int ret;

	if (video->queue.owner && video->queue.owner != file->private_data)
		return -EBUSY;

	/* Get a pipeline for the video node and start streaming on it. No link
	 * touching an entity in the pipeline can be activated or deactivated
	 * once streaming is started.
	 */
	mutex_lock(&mdev->graph_mutex);

	pipe = vsp2_video_pipeline_get(video);
	if (IS_ERR(pipe)) {
		mutex_unlock(&mdev->graph_mutex);
		return PTR_ERR(pipe);
	}

	ret = __media_pipeline_start(&video->video.entity, &pipe->pipe);
	if (ret < 0) {
		mutex_unlock(&mdev->graph_mutex);
		goto err_pipe;
	}

	mutex_unlock(&mdev->graph_mutex);

	/* Verify that the configured format matches the output of the connected
	 * subdev.
	 */
	ret = vsp2_video_verify_format(video);
	if (ret < 0)
		goto err_stop;

	/* Start the queue. */
	ret = vb2_streamon(&video->queue, type);
	if (ret < 0)
		goto err_stop;

	return 0;

err_stop:
	media_pipeline_stop(&video->video.entity);
err_pipe:
	vsp2_video_pipeline_put(pipe);
	return ret;
}

static int vsp2_g_ext_ctrls(struct file *file, void *fh,
			    struct v4l2_ext_controls *ctrls)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);
	struct v4l2_ext_control *ctrl;
	unsigned int i;

	/* wpf only */
	if (video->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	/* Only one control ID to support at this stage */
	if (ctrls->count != 1)
		return -EINVAL;

	for (i = 0; i < ctrls->count; i++) {
		ctrl = ctrls->controls + i;
		if (ctrl->id == VSP2_CID_COMPRESS) {
			if (ctrls->which == V4L2_CTRL_WHICH_DEF_VAL)
				ctrl->value = FCP_FCNL_DEF_VALUE;
			else
				ctrl->value = video->rwpf->fcp_fcnl;
		}
	}
	return 0;
}

static int vsp2_s_ext_ctrls(struct file *file, void *fh,
			    struct v4l2_ext_controls *ctrls)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);
	struct v4l2_ext_control *ctrl;
	unsigned int i;

	/* Default value cannot be changed */
	if (ctrls->which == V4L2_CTRL_WHICH_DEF_VAL)
		return -EINVAL;

	/* wpf only */
	if (video->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	/* Only one control ID to support at this stage */
	if (ctrls->count != 1)
		return -EINVAL;

	for (i = 0; i < ctrls->count; i++) {
		ctrl = ctrls->controls + i;
		if (ctrl->id == VSP2_CID_COMPRESS) {
			if (ctrl->value != 0x00 && ctrl->value != 0x01)
				return -EINVAL;
			video->rwpf->fcp_fcnl = ctrl->value;
		}
	}
	return 0;
}

static int vsp2_try_ext_ctrls(struct file *file, void *fh,
			      struct v4l2_ext_controls *ctrls)
{
	struct v4l2_fh *vfh = file->private_data;
	struct vsp2_video *video = to_vsp2_video(vfh->vdev);
	struct v4l2_ext_control *ctrl;
	unsigned int i;

	/* wpf only */
	if (video->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	/* Only one control ID to support at this stage */
	if (ctrls->count != 1)
		return -EINVAL;

	for (i = 0; i < ctrls->count; i++) {
		ctrl = ctrls->controls + i;
		if (ctrl->id == VSP2_CID_COMPRESS) {
			if (ctrl->value != 0x00 && ctrl->value != 0x01) {
				ctrls->error_idx = i;
				return -EINVAL;
			}
		} else {
			ctrls->error_idx = i;
			return -EINVAL;
		}
	}
	return 0;
}

static const struct v4l2_ioctl_ops vsp2_video_ioctl_ops = {
	.vidioc_querycap		= vsp2_video_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= vsp2_video_get_format,
	.vidioc_s_fmt_vid_cap_mplane	= vsp2_video_set_format,
	.vidioc_try_fmt_vid_cap_mplane	= vsp2_video_try_format,
	.vidioc_g_fmt_vid_out_mplane	= vsp2_video_get_format,
	.vidioc_s_fmt_vid_out_mplane	= vsp2_video_set_format,
	.vidioc_try_fmt_vid_out_mplane	= vsp2_video_try_format,
	.vidioc_reqbufs			= vb2_ioctl_reqbufs,
	.vidioc_querybuf		= vb2_ioctl_querybuf,
	.vidioc_qbuf			= vb2_ioctl_qbuf,
	.vidioc_dqbuf			= vb2_ioctl_dqbuf,
	.vidioc_create_bufs		= vb2_ioctl_create_bufs,
	.vidioc_prepare_buf		= vb2_ioctl_prepare_buf,
	.vidioc_expbuf			= vb2_ioctl_expbuf,
	.vidioc_streamon		= vsp2_video_streamon,
	.vidioc_streamoff		= vb2_ioctl_streamoff,
	.vidioc_g_ext_ctrls		= vsp2_g_ext_ctrls,
	.vidioc_s_ext_ctrls		= vsp2_s_ext_ctrls,
	.vidioc_try_ext_ctrls		= vsp2_try_ext_ctrls,
};

/* -----------------------------------------------------------------------------
 * V4L2 File Operations
 */

static int vsp2_video_open(struct file *file)
{
	struct vsp2_video *video = video_drvdata(file);
	struct v4l2_fh *vfh;
	int ret = 0;

	vfh = kzalloc(sizeof(*vfh), GFP_KERNEL);
	if (!vfh)
		return -ENOMEM;

	v4l2_fh_init(vfh, &video->video);
	v4l2_fh_add(vfh);

	file->private_data = vfh;

	ret = vsp2_device_get(video->vsp2);
	if (ret < 0) {
		v4l2_fh_del(vfh);
		v4l2_fh_exit(vfh);
		kfree(vfh);
	}

	return ret;
}

static int vsp2_video_release(struct file *file)
{
	struct vsp2_video *video = video_drvdata(file);
	struct v4l2_fh *vfh = file->private_data;

	mutex_lock(&video->lock);
	if (video->queue.owner == vfh) {
		vb2_queue_release(&video->queue);
		video->queue.owner = NULL;
	}
	mutex_unlock(&video->lock);

	vsp2_device_put(video->vsp2);

	v4l2_fh_release(file);

	file->private_data = NULL;

	return 0;
}

static const struct v4l2_file_operations vsp2_video_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = vsp2_video_open,
	.release = vsp2_video_release,
	.poll = vb2_fop_poll,
	.mmap = vb2_fop_mmap,
};

/* -----------------------------------------------------------------------------
 * Initialization and Cleanup
 */

struct vsp2_video *vsp2_video_create(struct vsp2_device *vsp2,
				     struct vsp2_rwpf *rwpf)
{
	struct vsp2_video *video;
	const char *direction;
	int ret;

	video = devm_kzalloc(vsp2->dev, sizeof(*video), GFP_KERNEL);
	if (!video)
		return ERR_PTR(-ENOMEM);

	rwpf->video = video;

	video->vsp2 = vsp2;
	video->rwpf = rwpf;

	if (rwpf->entity.type == VSP2_ENTITY_RPF) {
		direction = "input";
		video->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		video->pad.flags = MEDIA_PAD_FL_SOURCE;
		video->video.vfl_dir = VFL_DIR_TX;
	} else {
		direction = "output";
		video->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		video->pad.flags = MEDIA_PAD_FL_SINK;
		video->video.vfl_dir = VFL_DIR_RX;
	}

	mutex_init(&video->lock);
	spin_lock_init(&video->irqlock);
	INIT_LIST_HEAD(&video->irqqueue);

	/* Initialize the media entity... */
	ret = media_entity_pads_init(&video->video.entity, 1, &video->pad);
	if (ret < 0)
		return ERR_PTR(ret);

	/* ... and the format ... */
	rwpf->format.pixelformat = VSP2_VIDEO_DEF_FORMAT;
	rwpf->format.width = VSP2_VIDEO_DEF_WIDTH;
	rwpf->format.height = VSP2_VIDEO_DEF_HEIGHT;
	__vsp2_video_try_format(video, &rwpf->format, &rwpf->fmtinfo);

	/* ... and the video node... */
	video->video.v4l2_dev = &video->vsp2->v4l2_dev;
	video->video.fops = &vsp2_video_fops;
	snprintf(video->video.name, sizeof(video->video.name), "%s %s",
		 rwpf->entity.subdev.name, direction);
	video->video.vfl_type = VFL_TYPE_VIDEO;
	video->video.release = video_device_release_empty;
	video->video.ioctl_ops = &vsp2_video_ioctl_ops;

	video_set_drvdata(&video->video, video);

	video->queue.type = video->type;
	video->queue.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	video->queue.lock = &video->lock;
	video->queue.drv_priv = video;
	video->queue.buf_struct_size = sizeof(struct vsp2_vb2_buffer);
	video->queue.ops = &vsp2_video_queue_qops;
	video->queue.mem_ops = &vb2_dma_contig_memops;
	video->queue.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	video->queue.dev = video->vsp2->dev;
	ret = vb2_queue_init(&video->queue);
	if (ret < 0) {
		dev_err(video->vsp2->dev, "failed to initialize vb2 queue\n");
		goto error;
	}

	/* ... and register the video device. */
	video->video.queue = &video->queue;
	ret = video_register_device(&video->video, VFL_TYPE_VIDEO, -1);
	if (ret < 0) {
		dev_err(video->vsp2->dev, "failed to register video device\n");
		goto error;
	}

	return video;

error:
	vsp2_video_cleanup(video);
	return ERR_PTR(ret);
}

void vsp2_video_cleanup(struct vsp2_video *video)
{
	if (video_is_registered(&video->video))
		video_unregister_device(&video->video);

	media_entity_cleanup(&video->video.entity);
}
