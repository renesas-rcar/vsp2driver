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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */ /*************************************************************************/

#ifndef __VSP2_VIDEO_H__
#define __VSP2_VIDEO_H__

#include <linux/list.h>
#include <linux/spinlock.h>

#include <media/videobuf2-v4l2.h>

#include "vsp2_rwpf.h"

struct vsp2_vb2_buffer {
	struct vb2_v4l2_buffer buf;
	struct list_head queue;

	struct vsp2_rwpf_memory mem;
};

static inline struct vsp2_vb2_buffer *
to_vsp2_vb2_buffer(struct vb2_v4l2_buffer *vbuf)
{
	return container_of(vbuf, struct vsp2_vb2_buffer, buf);
}

struct vsp2_video {
	struct list_head list;
	struct vsp2_device *vsp2;
	struct vsp2_rwpf *rwpf;

	struct video_device video;
	enum v4l2_buf_type type;
	struct media_pad pad;

	struct mutex lock;

	unsigned int pipe_index;

	struct vb2_queue queue;
	spinlock_t irqlock;
	struct list_head irqqueue;
};

static inline struct vsp2_video *to_vsp2_video(struct video_device *vdev)
{
	return container_of(vdev, struct vsp2_video, video);
}

struct vsp2_video *vsp2_video_create(struct vsp2_device *vsp2,
				     struct vsp2_rwpf *rwpf);
void vsp2_video_cleanup(struct vsp2_video *video);

#endif /* __VSP2_VIDEO_H__ */
