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

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>

#include <media/v4l2-subdev.h>

#include "vsp2_device.h"
#include "vsp2_bru.h"
#include "vsp2_brs.h"
#include "vsp2_lut.h"
#include "vsp2_clu.h"
#include "vsp2_pipe.h"
#include "vsp2_rwpf.h"
#include "vsp2_uds.h"
#include "vsp2_video.h"
#include "vsp2_hgo.h"
#include "vsp2_hgt.h"
#include "vsp2_vspm.h"
#include "vsp2_debug.h"

/* -----------------------------------------------------------------------------
 * frame end proccess
 */

void vsp2_frame_end(struct vsp2_device *vsp2)
{
	unsigned int i;

	/* pipeline flame end */

	for (i = 0; i < VSP2_COUNT_WPF; ++i) {
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		struct vsp2_pipeline *pipe;

		if (wpf == NULL)
			continue;

		pipe = to_vsp2_pipeline(&wpf->entity.subdev.entity);

		vsp2_pipeline_frame_end(pipe);
	}
}

/* -----------------------------------------------------------------------------
 * Entities
 */

/*
 * vsp2_create_sink_links - Create links from all sources to the given sink
 *
 * This function creates media links from all valid sources to the given sink
 * pad. Links that would be invalid according to the VSP2 hardware capabilities
 * are skipped. Those include all links
 *
 * - from a UDS to a UDS (UDS entities can't be chained)
 * - from an entity to itself (no loops are allowed)
 */
static int vsp2_create_sink_links(struct vsp2_device *vsp2,
				  struct vsp2_entity *sink)
{
	struct media_entity *entity = &sink->subdev.entity;
	struct vsp2_entity *source;
	unsigned int pad;
	int ret;

	list_for_each_entry(source, &vsp2->entities, list_dev) {
		u32 flags;

		if (source->type == sink->type)
			continue;

		if (source->type == VSP2_ENTITY_WPF)
			continue;

		if (source->type == VSP2_ENTITY_HGO)
			continue;

		if (source->type == VSP2_ENTITY_HGT)
			continue;

		flags = source->type == VSP2_ENTITY_RPF &&
			sink->type == VSP2_ENTITY_WPF &&
			source->index == sink->index
		      ? MEDIA_LNK_FL_ENABLED : 0;

		for (pad = 0; pad < entity->num_pads; ++pad) {
			if (!(entity->pads[pad].flags & MEDIA_PAD_FL_SINK))
				continue;

			ret = media_create_pad_link(&source->subdev.entity,
						    source->source_pad,
						    entity, pad, flags);
			if (ret < 0)
				return ret;

			if (flags & MEDIA_LNK_FL_ENABLED)
				source->sink = entity;
		}
	}

	return 0;
}

static int vsp2_create_links(struct vsp2_device *vsp2)
{
	struct vsp2_entity *entity;
	unsigned int i;
	int ret;

	list_for_each_entry(entity, &vsp2->entities, list_dev) {
		if (entity->type == VSP2_ENTITY_HGO ||
		    entity->type == VSP2_ENTITY_HGT ||
		    entity->type == VSP2_ENTITY_RPF)
			continue;

		ret = vsp2_create_sink_links(vsp2, entity);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < vsp2->pdata.rpf_count; ++i) {
		struct vsp2_rwpf *rpf = vsp2->rpf[i];

		ret = media_create_pad_link(&rpf->video->video.entity, 0,
					    &rpf->entity.subdev.entity,
					    RWPF_PAD_SINK,
					    MEDIA_LNK_FL_ENABLED |
					    MEDIA_LNK_FL_IMMUTABLE);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < vsp2->pdata.wpf_count; ++i) {
		/* Connect the video device to the WPF. All connections are
		 * immutable except for the WPF0 source link.
		 */
		struct vsp2_rwpf *wpf = vsp2->wpf[i];
		unsigned int flags = MEDIA_LNK_FL_ENABLED;

		flags |= MEDIA_LNK_FL_IMMUTABLE;

		ret = media_create_pad_link(&wpf->entity.subdev.entity,
					    RWPF_PAD_SOURCE,
					    &wpf->video->video.entity,
					    0, flags);
		if (ret < 0)
			return ret;
	}

	return 0;
}

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
static void vsp2_free_buffers(struct vsp2_device *vsp2)
{
	if (vsp2->lut != NULL && vsp2->lut->buff_v != NULL)
		dma_free_coherent(vsp2->dev,
				  LUT_BUFF_SIZE,
				  vsp2->lut->buff_v,
				  vsp2->lut->buff_h);

	if (vsp2->clu != NULL && vsp2->clu->buff_v != NULL)
		dma_free_coherent(vsp2->dev,
				  CLU_BUFF_SIZE,
				  vsp2->clu->buff_v,
				  vsp2->clu->buff_h);

	if (vsp2->hgo != NULL && vsp2->hgo->buff_v != NULL)
		dma_free_coherent(vsp2->dev,
				  HGO_BUFF_SIZE,
				  vsp2->hgo->buff_v,
				  vsp2->hgo->buff_h);

	if (vsp2->hgt != NULL && vsp2->hgt->buff_v != NULL)
		dma_free_coherent(vsp2->dev,
				  HGT_BUFF_SIZE,
				  vsp2->hgt->buff_v,
				  vsp2->hgt->buff_h);
}
#endif

static void vsp2_destroy_entities(struct vsp2_device *vsp2)
{
	struct vsp2_entity *entity, *_entity;
	struct vsp2_video *video, *_video;

#ifdef USE_BUFFER /* TODO: delete USE_BUFFER */
	vsp2_free_buffers(vsp2);
#endif

	v4l2_device_unregister(&vsp2->v4l2_dev);

	list_for_each_entry_safe(entity, _entity, &vsp2->entities, list_dev) {
		list_del(&entity->list_dev);
		vsp2_entity_destroy(entity);
	}

	list_for_each_entry_safe(video, _video, &vsp2->videos, list) {
		list_del(&video->list);
		vsp2_video_cleanup(video);
	}

	media_device_unregister(&vsp2->media_dev);
	media_device_cleanup(&vsp2->media_dev);
}

static int vsp2_create_entities(struct vsp2_device *vsp2)
{
	struct media_device *mdev = &vsp2->media_dev;
	struct v4l2_device *vdev = &vsp2->v4l2_dev;
	struct vsp2_entity *entity;
	unsigned int i;
	int ret;

	mdev->dev = vsp2->dev;
	strlcpy(mdev->model, "VSP2", sizeof(mdev->model));
	snprintf(mdev->bus_info, sizeof(mdev->bus_info), "platform:%s",
		 dev_name(mdev->dev));
	media_device_init(mdev);

	vsp2->media_ops.link_setup = vsp2_entity_link_setup;
	/* Don't perform link validation when the userspace API is disabled as
	 * the pipeline is configured internally by the driver in that case, and
	 * its configuration can thus be trusted.
	 */
	vsp2->media_ops.link_validate = v4l2_subdev_link_validate;

	vdev->mdev = mdev;
	ret = v4l2_device_register(vsp2->dev, vdev);
	if (ret < 0) {
		dev_err(vsp2->dev, "V4L2 device registration failed (%d)\n",
			ret);
		goto done;
	}

	/* Instantiate all the entities. */

	/* - BRU */

	if (vsp2->pdata.features & VSP2_HAS_BRU) {
		vsp2->bru = vsp2_bru_create(vsp2);
		if (IS_ERR(vsp2->bru)) {
			ret = PTR_ERR(vsp2->bru);
			goto done;
		}
		list_add_tail(&vsp2->bru->entity.list_dev, &vsp2->entities);
	}

	/* - BRS */

	if (vsp2->pdata.features & VSP2_HAS_BRS) {
		vsp2->brs = vsp2_brs_create(vsp2);
		if (IS_ERR(vsp2->brs)) {
			ret = PTR_ERR(vsp2->brs);
			goto done;
		}
		list_add_tail(&vsp2->brs->entity.list_dev, &vsp2->entities);
	}

	/* - LUT */

	if (vsp2->pdata.features & VSP2_HAS_LUT) {
		vsp2->lut = vsp2_lut_create(vsp2);
		if (IS_ERR(vsp2->lut)) {
			ret = PTR_ERR(vsp2->lut);
			goto done;
		}
		list_add_tail(&vsp2->lut->entity.list_dev, &vsp2->entities);
	}

	/* - CLU */

	if (vsp2->pdata.features & VSP2_HAS_CLU) {
		vsp2->clu = vsp2_clu_create(vsp2);
		if (IS_ERR(vsp2->clu)) {
			ret = PTR_ERR(vsp2->clu);
			goto done;
		}
		list_add_tail(&vsp2->clu->entity.list_dev, &vsp2->entities);
	}

	/* - HGO */

	if (vsp2->pdata.features & VSP2_HAS_HGO) {
		vsp2->hgo = vsp2_hgo_create(vsp2);
		if (IS_ERR(vsp2->hgo)) {
			ret = PTR_ERR(vsp2->hgo);
			goto done;
		}
		list_add_tail(&vsp2->hgo->entity.list_dev, &vsp2->entities);
	}

	/* - HGT */

	if (vsp2->pdata.features & VSP2_HAS_HGT) {
		vsp2->hgt = vsp2_hgt_create(vsp2);
		if (IS_ERR(vsp2->hgt)) {
			ret = PTR_ERR(vsp2->hgt);
			goto done;
		}
		list_add_tail(&vsp2->hgt->entity.list_dev, &vsp2->entities);
	}

	/* - RPFs */

	for (i = 0; i < vsp2->pdata.rpf_count; ++i) {
		struct vsp2_video *video;
		struct vsp2_rwpf *rpf;

		rpf = vsp2_rpf_create(vsp2, i);
		if (IS_ERR(rpf)) {
			ret = PTR_ERR(rpf);
			goto done;
		}

		vsp2->rpf[i] = rpf;
		list_add_tail(&rpf->entity.list_dev, &vsp2->entities);

		video = vsp2_video_create(vsp2, rpf);
		if (IS_ERR(video)) {
			ret = PTR_ERR(video);
			goto done;
		}

		list_add_tail(&video->list, &vsp2->videos);
	}

	/* - UDSs */

	for (i = 0; i < vsp2->pdata.uds_count; ++i) {
		struct vsp2_uds *uds;

		uds = vsp2_uds_create(vsp2, i);
		if (IS_ERR(uds)) {
			ret = PTR_ERR(uds);
			goto done;
		}

		vsp2->uds[i] = uds;
		list_add_tail(&uds->entity.list_dev, &vsp2->entities);
	}

	/* - WPFs */

	for (i = 0; i < vsp2->pdata.wpf_count; ++i) {
		struct vsp2_video *video;
		struct vsp2_rwpf *wpf;

		wpf = vsp2_wpf_create(vsp2, i);
		if (IS_ERR(wpf)) {
			ret = PTR_ERR(wpf);
			goto done;
		}

		vsp2->wpf[i] = wpf;
		list_add_tail(&wpf->entity.list_dev, &vsp2->entities);

		video = vsp2_video_create(vsp2, wpf);
		if (IS_ERR(video)) {
			ret = PTR_ERR(video);
			goto done;
		}

		list_add_tail(&video->list, &vsp2->videos);
	}

	/* Register all subdevs. */
	list_for_each_entry(entity, &vsp2->entities, list_dev) {
		ret = v4l2_device_register_subdev(&vsp2->v4l2_dev,
						  &entity->subdev);
		if (ret < 0)
			goto done;
	}

	/* Create links. */
	ret = vsp2_create_links(vsp2);
	if (ret < 0)
		goto done;

	ret = v4l2_device_register_subdev_nodes(&vsp2->v4l2_dev);
	if (ret < 0)
		goto done;

	ret = media_device_register(mdev);

done:
	if (ret < 0)
		vsp2_destroy_entities(vsp2);

	return ret;
}

static int vsp2_device_init(struct vsp2_device *vsp2)
{
	long vspm_ret = R_VSPM_OK;

	/* Initialize the VSPM driver */
	vspm_ret = vsp2_vspm_drv_init(vsp2);
	if (vspm_ret != R_VSPM_OK) {
		dev_err(vsp2->dev,
			"failed to initialize the VSPM driver : %ld\n",
			vspm_ret);
		return -EFAULT;
	}

	return 0;
}

/*
 * vsp2_device_get - Acquire the VSP2 device
 *
 * Increment the VSP2 reference count and initialize the device if the first
 * reference is taken.
 *
 * Return 0 on success or a negative error code otherwise.
 */
int vsp2_device_get(struct vsp2_device *vsp2)
{
	int ret = 0;

	mutex_lock(&vsp2->lock);
	if (vsp2->ref_count > 0)
		goto done;

	ret = vsp2_device_init(vsp2);
	if (ret < 0)
		goto done;

done:
	if (!ret)
		vsp2->ref_count++;

	mutex_unlock(&vsp2->lock);
	return ret;
}

/*
 * vsp2_device_put - Release the VSP2 device
 *
 * Decrement the VSP2 reference count and cleanup the device if the last
 * reference is released.
 */
void vsp2_device_put(struct vsp2_device *vsp2)
{
	long vspm_ret = R_VSPM_OK;

	if (vsp2->ref_count == 0)
		return;

	mutex_lock(&vsp2->lock);

	if (--vsp2->ref_count == 0) {
		vspm_ret = vsp2_vspm_drv_quit(vsp2);
		if (vspm_ret != R_VSPM_OK)
			dev_err(vsp2->dev,
				"failed to exit the VSPM driver : %ld\n",
				vspm_ret);
	}

	mutex_unlock(&vsp2->lock);
}

/* -----------------------------------------------------------------------------
 * Power Management
 */

static int __maybe_unused vsp2_pm_suspend(struct device *dev)
{
	struct vsp2_device *vsp2 = dev_get_drvdata(dev);

	WARN_ON(mutex_is_locked(&vsp2->lock));

	if (vsp2->ref_count == 0)
		return 0;

	vsp2_pipelines_suspend(vsp2);

	return 0;
}

static int __maybe_unused vsp2_pm_resume(struct device *dev)
{
	struct vsp2_device *vsp2 = dev_get_drvdata(dev);

	WARN_ON(mutex_is_locked(&vsp2->lock));

	if (vsp2->ref_count == 0)
		return 0;

	vsp2_pipelines_resume(vsp2);

	return 0;
}

static const struct dev_pm_ops vsp2_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vsp2_pm_suspend, vsp2_pm_resume)
};

/* -----------------------------------------------------------------------------
 * Platform Driver
 */

static int vsp2_parse_dt(struct vsp2_device *vsp2)
{
	struct device_node			*np = vsp2->dev->of_node;
	struct vsp2_platform_data	*pdata = &vsp2->pdata;
	unsigned int ch;

	if (of_property_read_bool(np, "renesas,has-bru"))
		pdata->features |= VSP2_HAS_BRU;

	if (of_property_read_bool(np, "renesas,has-brs"))
		pdata->features |= VSP2_HAS_BRS;

	if (of_property_read_bool(np, "renesas,has-lut"))
		pdata->features |= VSP2_HAS_LUT;

	if (of_property_read_bool(np, "renesas,has-clu"))
		pdata->features |= VSP2_HAS_CLU;

	if (of_property_read_bool(np, "renesas,has-hgo"))
		pdata->features |= VSP2_HAS_HGO;

	if (of_property_read_bool(np, "renesas,has-hgt"))
		pdata->features |= VSP2_HAS_HGT;

	of_property_read_u32(np, "renesas,#rpf", &pdata->rpf_count);
	of_property_read_u32(np, "renesas,#uds", &pdata->uds_count);
	of_property_read_u32(np, "renesas,#wpf", &pdata->wpf_count);

	if (pdata->rpf_count <= 0 || pdata->rpf_count > VSP2_COUNT_RPF) {
		dev_err(vsp2->dev, "invalid number of RPF (%u)\n",
			pdata->rpf_count);
		return -EINVAL;
	}

	if (pdata->uds_count > VSP2_COUNT_UDS) {
		dev_err(vsp2->dev, "invalid number of UDS (%u)\n",
			pdata->uds_count);
		return -EINVAL;
	}

	if (pdata->wpf_count <= 0 || pdata->wpf_count > VSP2_COUNT_WPF) {
		dev_err(vsp2->dev, "invalid number of WPF (%u)\n",
			pdata->wpf_count);
		return -EINVAL;
	}

	if (of_property_read_u32(np, "renesas,#ch", &ch) == 0) {
		switch (ch) {
		case 0:
			pdata->use_ch = VSPM_USE_CH0;
			break;
		case 1:
			pdata->use_ch = VSPM_USE_CH1;
			break;
		case 2:
			pdata->use_ch = VSPM_USE_CH2;
			break;
		case 3:
			pdata->use_ch = VSPM_USE_CH3;
			break;
		case 4:
			pdata->use_ch = VSPM_USE_CH4;
			break;
		default:
			return -EINVAL;
		}
	} else {
		pdata->use_ch = VSPM_EMPTY_CH;
	}

	return 0;
}

static int vsp2_probe(struct platform_device *pdev)
{
	struct vsp2_device *vsp2;
	int ret;

	vsp2 = devm_kzalloc(&pdev->dev, sizeof(*vsp2), GFP_KERNEL);
	if (vsp2 == NULL)
		return -ENOMEM;

	vsp2->dev = &pdev->dev;
	mutex_init(&vsp2->lock);
	INIT_LIST_HEAD(&vsp2->entities);
	INIT_LIST_HEAD(&vsp2->videos);

	ret = vsp2_parse_dt(vsp2);
	if (ret < 0)
		return ret;

	ret = vsp2_vspm_init(vsp2, pdev->id);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize VSPM info\n");
		return ret;
	}

	/* Instanciate entities */
	ret = vsp2_create_entities(vsp2);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to create entities\n");
		return ret;
	}

	platform_set_drvdata(pdev, vsp2);

	return 0;
}

static int vsp2_remove(struct platform_device *pdev)
{
	struct vsp2_device *vsp2 = platform_get_drvdata(pdev);

	vsp2_device_put(vsp2);
	vsp2_destroy_entities(vsp2);

	/* Finalize VSPM */

	vsp2_vspm_exit(vsp2);

	return 0;
}

static const struct of_device_id vsp2_of_match[] = {
	{ .compatible = "renesas,vspm-vsp2" },
	{ },
};
MODULE_DEVICE_TABLE(of, vsp2_of_match);

static struct platform_driver vsp2_driver = {
	.probe		= vsp2_probe,
	.remove		= vsp2_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= DRVNAME,
		.of_match_table = vsp2_of_match,
		.pm	= &vsp2_pm_ops,
	},
};

static int __init vsp2_init(void)
{
	int ercd = 0;

	ercd = platform_driver_register(&vsp2_driver);
	if (ercd) {
		VSP2_PRINT_ALERT(
		  "failed to register a driver for platform-level devices.\n");
		goto err_exit;
	}

	return ercd;

err_exit:

	return ercd;
}

static void __exit vsp2_exit(void)
{
	platform_driver_unregister(&vsp2_driver);
}

module_init(vsp2_init);
module_exit(vsp2_exit);

MODULE_ALIAS("vsp2");
MODULE_AUTHOR("Renesas Electronics Corporation");
MODULE_DESCRIPTION("Renesas VSP2 Driver");
MODULE_LICENSE("Dual MIT/GPL");
