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

#include <linux/list.h>
#include <linux/wait.h>

#include <media/media-entity.h>
#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_bru.h"
#include "vsp2_entity.h"
#include "vsp2_pipe.h"
#include "vsp2_rwpf.h"
#include "vsp2_uds.h"
#include "vsp2_vspm.h"

/* -----------------------------------------------------------------------------
 * Helper Functions
 */

static const struct vsp2_format_info vsp2_video_formats[] = {
	{ V4L2_PIX_FMT_RGB332, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_RGB_332, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 8, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_ARGB444, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_4444, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, false, false, 1, 1, true },
	{ V4L2_PIX_FMT_XRGB444, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_XRGB_4444, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, false, false, 1, 1, true },
	{ V4L2_PIX_FMT_ARGB555, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_1555, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, false, false, 1, 1, true },
	{ V4L2_PIX_FMT_XRGB555, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_XRGB_1555, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_RGB565, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_RGB_565, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_BGR24, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_BGR_888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 24, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_RGB24, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_RGB_888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 24, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_ABGR32, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_8888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS,
	  1, { 32, 0, 0 }, false, false, 1, 1, true },
	{ V4L2_PIX_FMT_XBGR32, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_8888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS,
	  1, { 32, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_ARGB32, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_8888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 32, 0, 0 }, false, false, 1, 1, true },
	{ V4L2_PIX_FMT_XRGB32, MEDIA_BUS_FMT_ARGB8888_1X32,
	  VI6_FMT_ARGB_8888, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 32, 0, 0 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_UYVY, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_YUYV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 16, 0, 0 }, false, false, 2, 1, false },
	{ V4L2_PIX_FMT_VYUY, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_YUYV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS,
	  1, { 16, 0, 0 }, true, true, 2, 1, false },
	{ V4L2_PIX_FMT_YUYV, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_YUYV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 16, 0, 0 }, true, false, 2, 1, false },
	{ V4L2_PIX_FMT_YVYU, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_YUYV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  1, { 16, 0, 0 }, true, true, 2, 1, false },
	{ V4L2_PIX_FMT_NV12M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_UV_420, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  2, { 8, 16, 0 }, false, false, 2, 2, false },
	{ V4L2_PIX_FMT_NV21M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_UV_420, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  2, { 8, 16, 0 }, false, true, 2, 2, false },
	{ V4L2_PIX_FMT_NV16M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_UV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  2, { 8, 16, 0 }, false, false, 2, 1, false },
	{ V4L2_PIX_FMT_NV61M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_UV_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  2, { 8, 16, 0 }, false, true, 2, 1, false },
	{ V4L2_PIX_FMT_YUV420M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_420, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, false, 2, 2, false },
	{ V4L2_PIX_FMT_YVU420M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_420, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, true, 2, 2, false },
	{ V4L2_PIX_FMT_YUV422M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, false, 2, 1, false },
	{ V4L2_PIX_FMT_YVU422M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_422, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, true, 2, 1, false },
	{ V4L2_PIX_FMT_YUV444M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_444, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, false, 1, 1, false },
	{ V4L2_PIX_FMT_YVU444M, MEDIA_BUS_FMT_AYUV8_1X32,
	  VI6_FMT_Y_U_V_444, VI6_RPF_DSWAP_P_LLS | VI6_RPF_DSWAP_P_LWS |
	  VI6_RPF_DSWAP_P_WDS | VI6_RPF_DSWAP_P_BTS,
	  3, { 8, 8, 8 }, false, true, 1, 1, false },
};

/*
 * vsp2_get_format_info - Retrieve format information for a 4CC
 * @fourcc: the format 4CC
 *
 * Return a pointer to the format information structure corresponding to the
 * given V4L2 format 4CC, or NULL if no corresponding format can be found.
 */
const struct vsp2_format_info *vsp2_get_format_info(u32 fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(vsp2_video_formats); ++i) {
		const struct vsp2_format_info *info = &vsp2_video_formats[i];

		if (info->fourcc == fourcc)
			return info;
	}

	return NULL;
}

/* -----------------------------------------------------------------------------
 * Pipeline Management
 */

void vsp2_pipeline_reset(struct vsp2_pipeline *pipe)
{
	unsigned int i;

	if (pipe->bru) {
		struct vsp2_bru *bru = to_bru(&pipe->bru->subdev);

		for (i = 0; i < ARRAY_SIZE(bru->inputs); ++i)
			bru->inputs[i].rpf = NULL;
	}

	for (i = 0; i < ARRAY_SIZE(pipe->inputs); ++i)
		pipe->inputs[i] = NULL;

	INIT_LIST_HEAD(&pipe->entities);
	pipe->state = VSP2_PIPELINE_STOPPED;
	pipe->buffers_ready = 0;
	pipe->num_video = 0;
	pipe->num_inputs = 0;
	pipe->output = NULL;
	pipe->bru = NULL;
	pipe->uds = NULL;
}

void vsp2_pipeline_init(struct vsp2_pipeline *pipe)
{
	mutex_init(&pipe->lock);
	spin_lock_init(&pipe->irqlock);
	init_waitqueue_head(&pipe->wq);

	INIT_LIST_HEAD(&pipe->entities);
	pipe->state = VSP2_PIPELINE_STOPPED;
}

void vsp2_pipeline_run(struct vsp2_pipeline *pipe)
{
	struct vsp2_device *vsp2 = pipe->output->entity.vsp2;

	vsp2_vspm_drv_entry(vsp2);

	pipe->state = VSP2_PIPELINE_RUNNING;
	pipe->buffers_ready = 0;
}

bool vsp2_pipeline_stopped(struct vsp2_pipeline *pipe)
{
	unsigned long flags;
	bool stopped;

	spin_lock_irqsave(&pipe->irqlock, flags);
	stopped = pipe->state == VSP2_PIPELINE_STOPPED;
	spin_unlock_irqrestore(&pipe->irqlock, flags);

	return stopped;
}

int vsp2_pipeline_stop(struct vsp2_pipeline *pipe)
{
	struct vsp2_entity *entity;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&pipe->irqlock, flags);
	if (pipe->state == VSP2_PIPELINE_RUNNING)
		pipe->state = VSP2_PIPELINE_STOPPING;
	spin_unlock_irqrestore(&pipe->irqlock, flags);

	ret = wait_event_timeout(pipe->wq, vsp2_pipeline_stopped(pipe),
				 msecs_to_jiffies(500));
	ret = ret == 0 ? -ETIMEDOUT : 0;

	list_for_each_entry(entity, &pipe->entities, list_pipe) {
		v4l2_subdev_call(&entity->subdev, video, s_stream, 0);
	}

	return ret;
}

bool vsp2_pipeline_ready(struct vsp2_pipeline *pipe)
{
	unsigned int mask;

	mask = ((1 << pipe->num_inputs) - 1) << 1;
	mask |= 1 << 0;

	return pipe->buffers_ready == mask;
}

void vsp2_pipeline_frame_end(struct vsp2_pipeline *pipe)
{
	enum vsp2_pipeline_state state;
	unsigned long flags;

	if (pipe == NULL)
		return;

	/* Signal frame end to the pipeline handler. */
	pipe->frame_end(pipe);

	spin_lock_irqsave(&pipe->irqlock, flags);

	state = pipe->state;
	pipe->state = VSP2_PIPELINE_STOPPED;

	/* If a stop has been requested, mark the pipeline as stopped and
	 * return.
	 */
	if (state == VSP2_PIPELINE_STOPPING) {
		wake_up(&pipe->wq);
		goto done;
	}

	/* Restart the pipeline if ready. */
	if (vsp2_pipeline_ready(pipe))
		vsp2_pipeline_run(pipe);

done:
	spin_unlock_irqrestore(&pipe->irqlock, flags);
}

/*
 * Propagate the alpha value through the pipeline.
 *
 * As the UDS has restricted scaling capabilities when the alpha component needs
 * to be scaled, we disable alpha scaling when the UDS input has a fixed alpha
 * value. The UDS then outputs a fixed alpha value which needs to be programmed
 * from the input RPF alpha.
 */
void vsp2_pipeline_propagate_alpha(struct vsp2_pipeline *pipe,
				   struct vsp2_entity *input,
				   unsigned int alpha)
{
	struct vsp2_entity *entity;
	struct media_pad *pad;

	pad = media_entity_remote_pad(&input->pads[RWPF_PAD_SOURCE]);

	while (pad) {
		if (!is_media_entity_v4l2_subdev(pad->entity))
			break;

		entity = to_vsp2_entity(
			media_entity_to_v4l2_subdev(pad->entity));

		/* The BRU background color has a fixed alpha value set to 255,
		 * the output alpha value is thus always equal to 255.
		 */
		if (entity->type == VSP2_ENTITY_BRU)
			alpha = 255;

		if (entity->type == VSP2_ENTITY_UDS) {
			struct vsp2_uds *uds = to_uds(&entity->subdev);

			vsp2_uds_set_alpha(uds, alpha);
			break;
		}

		pad = &entity->pads[entity->source_pad];
		pad = media_entity_remote_pad(pad);
	}
}

void vsp2_pipelines_suspend(struct vsp2_device *vsp2)
{
	unsigned long flags;
	unsigned int i;
	int ret;

	/* To avoid increasing the system suspend time needlessly, loop over the
	 * pipelines twice, first to set them all to the stopping state, and
	 * then to wait for the stop to complete.
	 */
	for (i = 0; i < vsp2->pdata.wpf_count; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);
		if (pipe == NULL)
			continue;

		spin_lock_irqsave(&pipe->irqlock, flags);
		if (pipe->state == VSP2_PIPELINE_RUNNING)
			pipe->state = VSP2_PIPELINE_STOPPING;
		spin_unlock_irqrestore(&pipe->irqlock, flags);
	}

	for (i = 0; i < vsp2->pdata.wpf_count; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);
		if (pipe == NULL)
			continue;

		ret = wait_event_timeout(pipe->wq, vsp2_pipeline_stopped(pipe),
					 msecs_to_jiffies(500));
		if (ret == 0)
			dev_warn(vsp2->dev, "pipeline %u stop timeout\n",
				 wpf->entity.index);
	}
}

void vsp2_pipelines_resume(struct vsp2_device *vsp2)
{
	unsigned int i;

	/* Resume pipeline all running pipelines. */
	for (i = 0; i < vsp2->pdata.wpf_count; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);
		if (pipe == NULL)
			continue;

		if (vsp2_pipeline_ready(pipe))
			vsp2_pipeline_run(pipe);
	}
}
