/*************************************************************************/ /*
 VSP2

 Copyright (C) 2015-2016 Renesas Electronics Corporation

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

#ifndef __VSP2_PIPE_H__
#define __VSP2_PIPE_H__

#include <linux/kref.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <media/media-entity.h>

struct vsp2_rwpf;

/*
 * struct vsp2_format_info - VSP2 video format description
 * @mbus: media bus format code
 * @fourcc: V4L2 pixel format FCC identifier
 * @planes: number of planes
 * @bpp: bits per pixel
 * @hwfmt: VSP2 hardware format
 * @swap_yc: the Y and C components are swapped (Y comes before C)
 * @swap_uv: the U and V components are swapped (V comes before U)
 * @hsub: horizontal subsampling factor
 * @vsub: vertical subsampling factor
 * @alpha: has an alpha channel
 */
struct vsp2_format_info {
	u32 fourcc;
	unsigned int mbus;
	unsigned int hwfmt;
	unsigned int swap;
	unsigned int planes;
	unsigned int bpp[3];
	bool swap_yc;
	bool swap_uv;
	unsigned int hsub;
	unsigned int vsub;
	bool alpha;
};

enum vsp2_pipeline_state {
	VSP2_PIPELINE_STOPPED,
	VSP2_PIPELINE_RUNNING,
	VSP2_PIPELINE_STOPPING,
};

/*
 * struct vsp2_pipeline - A VSP2 hardware pipeline
 * @pipe: the media pipeline
 * @irqlock: protects the pipeline state
 * @state: current state
 * @wq: wait queue to wait for state change completion
 * @frame_end: frame end interrupt handler
 * @lock: protects the pipeline use count and stream count
 * @kref: pipeline reference count
 * @stream_count: number of streaming video nodes
 * @buffers_ready: bitmask of RPFs and WPFs with at least one buffer available
 * @sequence: frame sequence number
 * @num_video: number of video devices
 * @num_inputs: number of RPFs
 * @inputs: array of RPFs in the pipeline (indexed by RPF index)
 * @output: WPF at the output of the pipeline
 * @bru: BRU entity, if present
 * @uds: UDS entity, if present
 * @uds_input: entity at the input of the UDS, if the UDS is present
 * @entities: list of entities in the pipeline
 */
struct vsp2_pipeline {
	struct media_pipeline pipe;

	spinlock_t irqlock;
	enum vsp2_pipeline_state state;
	wait_queue_head_t wq;

	void (*frame_end)(struct vsp2_pipeline *pipe);

	struct mutex lock;
	struct kref kref;
	unsigned int stream_count;
	unsigned int buffers_ready;
	unsigned int sequence;

	unsigned int num_video;
	unsigned int num_inputs;
	struct vsp2_rwpf *inputs[VSP2_COUNT_RPF];
	struct vsp2_rwpf *output;
	struct vsp2_entity *bru;
	struct vsp2_entity *uds;
	struct vsp2_entity *uds_input;

	struct list_head entities;
};

static inline struct vsp2_pipeline *to_vsp2_pipeline(struct media_entity *e)
{
	if (likely(e->pipe))
		return container_of(e->pipe, struct vsp2_pipeline, pipe);
	else
		return NULL;
}

void vsp2_pipeline_reset(struct vsp2_pipeline *pipe);
void vsp2_pipeline_init(struct vsp2_pipeline *pipe);

void vsp2_pipeline_run(struct vsp2_pipeline *pipe);
bool vsp2_pipeline_stopped(struct vsp2_pipeline *pipe);
int vsp2_pipeline_stop(struct vsp2_pipeline *pipe);
bool vsp2_pipeline_ready(struct vsp2_pipeline *pipe);

void vsp2_pipeline_frame_end(struct vsp2_pipeline *pipe);

void vsp2_pipeline_propagate_alpha(struct vsp2_pipeline *pipe,
				   unsigned int alpha);

void vsp2_pipelines_suspend(struct vsp2_device *vsp2);
void vsp2_pipelines_resume(struct vsp2_device *vsp2);

const struct vsp2_format_info *vsp2_get_format_info(u32 fourcc);

#endif /* __VSP2_PIPE_H__ */
