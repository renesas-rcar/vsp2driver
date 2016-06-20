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

#include <linux/dma-mapping.h>	/* for dl_par */
#include "vsp2_device.h"
#include "vsp2_vspm.h"
#include "vsp2_debug.h"

void vsp2_vspm_param_init(struct vspm_job_t *par)
{
	struct vsp_start_t *vsp_par = par->par.vsp;
	void *temp_vp;
	int i;

	/* Initialize struct vspm_job_t. */
	par->type = VSPM_TYPE_VSP_AUTO;

	/* Initialize struct vsp_start_t. */
	vsp_par->rpf_num	= 0;
	vsp_par->rpf_order	= 0;
	vsp_par->use_module	= 0;

	for (i = 0; i < 5; i++) {
		/* Initialize struct vsp_src_t. */
		temp_vp = vsp_par->src_par[i]->alpha;
		memset(vsp_par->src_par[i], 0x00, sizeof(struct vsp_src_t));
		vsp_par->src_par[i]->alpha = temp_vp;

		/* Initialize struct vsp_alpha_unit_t. */
		temp_vp = vsp_par->src_par[i]->alpha->mult;
		memset(vsp_par->src_par[i]->alpha,
				0x00, sizeof(struct vsp_alpha_unit_t));
		vsp_par->src_par[i]->alpha->mult = temp_vp;

		/* Initialize struct vsp_mult_unit_t. */
		memset(vsp_par->src_par[i]->alpha->mult,
				0x00, sizeof(struct vsp_mult_unit_t));
	}

	/* Initialize struct vsp_dst_t. */
	temp_vp = vsp_par->dst_par->fcp;
	memset(vsp_par->dst_par, 0x00, sizeof(struct vsp_dst_t));
	vsp_par->dst_par->fcp = temp_vp;

	/* Initialize struct fcp_info_t. */
	memset(vsp_par->dst_par->fcp, 0x00, sizeof(struct fcp_info_t));

	/* Initialize struct vsp_uds_t. */
	memset(vsp_par->ctrl_par->uds, 0x00, sizeof(struct vsp_uds_t));

	/* Initialize struct vsp_bru_t. */
	vsp_par->ctrl_par->bru->lay_order	= 0;
	vsp_par->ctrl_par->bru->adiv		= 0;
	memset(vsp_par->ctrl_par->bru->dither_unit,
		0x00,
		sizeof(vsp_par->ctrl_par->bru->dither_unit));
	vsp_par->ctrl_par->bru->rop_unit	= NULL;
	vsp_par->ctrl_par->bru->connect		= 0;

	/* Initialize struct vsp_bld_vir_t. */
	memset(vsp_par->ctrl_par->bru->blend_virtual,
		0x00,
		sizeof(struct vsp_bld_vir_t));

	for (i = 0; i < 5; i++) {
		struct vsp_bld_ctrl_t *vsp_blend_ctrl;

		switch (i) {
		case 0:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_unit_a;
			break;
		case 1:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_unit_b;
			break;
		case 2:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_unit_c;
			break;
		case 3:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_unit_d;
			break;
		case 4:
			vsp_blend_ctrl =
				vsp_par->ctrl_par->bru->blend_unit_e;
			break;
		default:
			/* Invalid index. */
			break;
		}

		/* Initialize struct vsp_bld_ctrl_t. */
		memset(vsp_blend_ctrl, 0x00, sizeof(struct vsp_bld_ctrl_t));
	}
}

static int vsp2_vspm_alloc_vsp_in(struct device *dev, struct vsp_src_t **in)
{
	struct vsp_src_t *vsp_in = NULL;

	vsp_in = devm_kzalloc(dev, sizeof(*vsp_in), GFP_KERNEL);
	if (vsp_in == NULL) {
		*in = NULL;
		return -ENOMEM;
	}

	*in = vsp_in;

	vsp_in->alpha =
	  devm_kzalloc(dev, sizeof(*vsp_in->alpha), GFP_KERNEL);
	if (vsp_in->alpha == NULL)
		return -ENOMEM;

	vsp_in->alpha->mult =
	  devm_kzalloc(dev, sizeof(*vsp_in->alpha->mult), GFP_KERNEL);
	if (vsp_in->alpha->mult == NULL)
		return -ENOMEM;

	return 0;
}

static int vsp2_vspm_alloc_vsp_bru(struct device *dev, struct vsp_bru_t **bru)
{
	struct vsp_bru_t *vsp_bru = NULL;

	vsp_bru = devm_kzalloc(dev, sizeof(*vsp_bru), GFP_KERNEL);
	if (vsp_bru == NULL) {
		*bru = NULL;
		return -ENOMEM;
	}

	*bru = vsp_bru;

	vsp_bru->blend_virtual =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_virtual), GFP_KERNEL);
	if (vsp_bru->blend_virtual == NULL)
		return -ENOMEM;

	vsp_bru->blend_unit_a =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_unit_a), GFP_KERNEL);
	if (vsp_bru->blend_unit_a == NULL)
		return -ENOMEM;
	vsp_bru->blend_unit_b =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_unit_b), GFP_KERNEL);
	if (vsp_bru->blend_unit_b == NULL)
		return -ENOMEM;
	vsp_bru->blend_unit_c =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_unit_c), GFP_KERNEL);
	if (vsp_bru->blend_unit_c == NULL)
		return -ENOMEM;
	vsp_bru->blend_unit_d =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_unit_d), GFP_KERNEL);
	if (vsp_bru->blend_unit_d == NULL)
		return -ENOMEM;
	vsp_bru->blend_unit_e =
	  devm_kzalloc(dev, sizeof(*vsp_bru->blend_unit_e), GFP_KERNEL);
	if (vsp_bru->blend_unit_e == NULL)
		return -ENOMEM;

	return 0;
}

static int vsp2_vspm_alloc(struct vsp2_device *vsp2)
{
	struct vsp_start_t *vsp_par = NULL;
	int ret = 0;
	int i;
	void		*virt_addr;		/* for dl_par */
	dma_addr_t	hard_addr;		/* for dl_par */

	vsp2->vspm = devm_kzalloc(vsp2->dev, sizeof(*vsp2->vspm), GFP_KERNEL);
	if (vsp2->vspm == NULL)
		return -ENOMEM;

	vsp2->vspm->ip_par.par.vsp =
		devm_kzalloc(vsp2->dev,
		sizeof(*vsp2->vspm->ip_par.par.vsp),
		GFP_KERNEL);
	if (vsp2->vspm->ip_par.par.vsp == NULL)
		return -ENOMEM;

	vsp_par = vsp2->vspm->ip_par.par.vsp;

	for (i = 0; i < 5; i++) {
		ret = vsp2_vspm_alloc_vsp_in(vsp2->dev, &vsp_par->src_par[i]);
		if (ret != 0)
			return -ENOMEM;
	}

	vsp_par->dst_par =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->dst_par), GFP_KERNEL);
	if (vsp_par->dst_par == NULL)
		return -ENOMEM;

	vsp_par->dst_par->fcp =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->dst_par->fcp), GFP_KERNEL);
	if (vsp_par->dst_par->fcp == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par), GFP_KERNEL);
	if (vsp_par->ctrl_par == NULL)
		return -ENOMEM;

	ret = vsp2_vspm_alloc_vsp_bru(vsp2->dev, &vsp_par->ctrl_par->bru);
	if (ret != 0)
		return -ENOMEM;

	vsp_par->ctrl_par->uds =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->uds), GFP_KERNEL);
	if (vsp_par->ctrl_par->uds == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par->lut =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->lut), GFP_KERNEL);
	if (vsp_par->ctrl_par->lut == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par->clu =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->clu), GFP_KERNEL);
	if (vsp_par->ctrl_par->clu == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par->hgo =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->hgo), GFP_KERNEL);
	if (vsp_par->ctrl_par->hgo == NULL)
		return -ENOMEM;

	vsp_par->ctrl_par->hgt =
	  devm_kzalloc(vsp2->dev, sizeof(*vsp_par->ctrl_par->hgt), GFP_KERNEL);
	if (vsp_par->ctrl_par->hgt == NULL)
		return -ENOMEM;

	virt_addr = dma_alloc_coherent(vsp2->dev, (128+2048)*8,
						&hard_addr, GFP_KERNEL|GFP_DMA);
	if (virt_addr == NULL)
		return -ENOMEM;

	vsp_par->dl_par.hard_addr = (void *)(hard_addr);
	vsp_par->dl_par.virt_addr = virt_addr;
	vsp_par->dl_par.tbl_num = 128+2048;

	return 0;
}

static bool vsp2_vspm_is_infmt_yvup(unsigned short format)
{
	if (((format & VI6_FMT_Y_U_V_420) == VI6_FMT_Y_U_V_420) ||
	    ((format & VI6_FMT_Y_U_V_422) == VI6_FMT_Y_U_V_422) ||
	    ((format & VI6_FMT_Y_U_V_444) == VI6_FMT_Y_U_V_444))
		if (format & VI6_RPF_INFMT_SPUVS)
			return true;

	return false;
}

static bool vsp2_vspm_is_outfmt_yvup(unsigned short format)
{
	if (((format & VI6_FMT_Y_U_V_420) == VI6_FMT_Y_U_V_420) ||
	    ((format & VI6_FMT_Y_U_V_422) == VI6_FMT_Y_U_V_422) ||
	    ((format & VI6_FMT_Y_U_V_444) == VI6_FMT_Y_U_V_444))
		if (format & VI6_WPF_OUTFMT_SPUVS)
			return true;

	return false;
}

static void vsp2_vspm_yvup_swap(struct vsp_start_t *vsp_par)
{
	int i;
	void *tmp;

	/* If format is YVU planar, change Cb Cr address. */

	for (i = 0; i < vsp_par->rpf_num; i++) {
		if (vsp2_vspm_is_infmt_yvup(vsp_par->src_par[i]->format)) {
			tmp = vsp_par->src_par[i]->addr_c0;
			vsp_par->src_par[i]->addr_c0 =
				vsp_par->src_par[i]->addr_c1;
			vsp_par->src_par[i]->addr_c1 = tmp;
			vsp_par->src_par[i]->format ^= VI6_RPF_INFMT_SPUVS;
		}
	}

	if (vsp2_vspm_is_outfmt_yvup(vsp_par->dst_par->format)) {
		tmp = vsp_par->dst_par->addr_c0;
		vsp_par->dst_par->addr_c0 = vsp_par->dst_par->addr_c1;
		vsp_par->dst_par->addr_c1 = tmp;
		vsp_par->dst_par->format ^= VI6_WPF_OUTFMT_SPUVS;
	}
}

long vsp2_vspm_drv_init(struct vsp2_device *vsp2)
{
	long ret = R_VSPM_OK;
	struct vspm_init_t init_par;

#ifdef TYPE_GEN2 /* TODO: delete TYPE_GEN2 */
	init_par.use_ch = VSPM_USE_CH1;
#else /*TYPE_GEN3 */
	init_par.use_ch = vsp2->pdata.use_ch;
#endif
	if (init_par.use_ch == (unsigned int)VSPM_EMPTY_CH)
		init_par.mode = VSPM_MODE_MUTUAL;
	else
		init_par.mode = VSPM_MODE_OCCUPY;
	init_par.type = VSPM_TYPE_VSP_AUTO;
	ret = vspm_init_driver(&vsp2->vspm->hdl, &init_par);
	if (ret != R_VSPM_OK) {
		dev_dbg(vsp2->dev,
			"failed to vspm_init_driver : %ld\n",
			ret);
		return ret;
	}

	vsp2_vspm_param_init(&vsp2->vspm->ip_par);

	return ret;
}

long vsp2_vspm_drv_quit(struct vsp2_device *vsp2)
{
	long ret = R_VSPM_OK;

	ret = vspm_quit_driver(vsp2->vspm->hdl);
	if (ret != R_VSPM_OK) {
		dev_dbg(vsp2->dev,
			"failed to vspm_quit_driver : %ld\n",
			ret);
	}

	return ret;
}

static void vsp2_vspm_drv_entry_cb(unsigned long job_id, long result,
				   unsigned long user_data)
{
	struct vsp2_device *vsp2;

#ifdef VSP2_DEBUG
	if (vsp2_debug_vspmDebug() == true)
		print_vspm_entry_cb();
#endif

	vsp2 = (struct vsp2_device *)user_data;

	/* check job_id when mode is VSPM_MODE_MUTUAL */
	if (vsp2->pdata.use_ch == (unsigned int)VSPM_EMPTY_CH)
		if (job_id != vsp2->vspm->job_id)
			dev_err(vsp2->dev,
				"vspm_entry_job: unexpected job id %lu (exp=%lu)\n",
				job_id, vsp2->vspm->job_id);

	if (result != R_VSPM_OK)
		dev_err(vsp2->dev, "vspm_entry_job: result=%ld\n", result);

	vsp2_frame_end(vsp2);
}

void vsp2_vspm_drv_entry_work(struct work_struct *work)
{
	long ret = R_VSPM_OK;

	struct vsp2_vspm_entry_work *entry_work;
	struct vsp2_device *vsp2;
	struct vsp_start_t *vsp_par;

	entry_work = (struct vsp2_vspm_entry_work *)work;
	vsp2 = entry_work->vsp2;
	vsp_par = vsp2->vspm->ip_par.par.vsp;

	if (vsp_par->use_module & VSP_BRU_USE) {
		/* Set lay_order of BRU. */
		vsp_par->ctrl_par->bru->lay_order = VSP_LAY_VIRTUAL;

		if (vsp_par->rpf_num >= 1)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_1 << 4);

		if (vsp_par->rpf_num >= 2)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_2 << 8);

		if (vsp_par->rpf_num >= 3)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_3 << 12);

		if (vsp_par->rpf_num >= 4)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_4 << 16);

#ifdef TYPE_GEN2 /* TODO: delete TYPE_GEN2 */
#else
		if (vsp_par->rpf_num >= 5)
			vsp_par->ctrl_par->bru->lay_order |= (VSP_LAY_5 << 20);
#endif

	} else {
		/* Not use BRU. Set RPF0 to parent layer. */
		vsp_par->src_par[0]->pwd = VSP_LAYER_PARENT;
	}

	vsp2_vspm_yvup_swap(vsp_par);

#ifdef VSP2_DEBUG
	if (vsp2_debug_vspmDebug() == true)
		print_vspm_entry(vsp2->vspm->ip_par.par.vsp);
#endif

	ret = vspm_entry_job(vsp2->vspm->hdl, &vsp2->vspm->job_id,
			     vsp2->vspm->job_pri, &vsp2->vspm->ip_par,
			     (unsigned long)vsp2, vsp2_vspm_drv_entry_cb);
	if (ret != R_VSPM_OK) {
		dev_err(vsp2->dev, "failed to vspm_entry_job : %ld\n", ret);

		vsp2_frame_end(vsp2);
	}
}

void vsp2_vspm_drv_entry(struct vsp2_device *vsp2)
{
	vsp2->vspm->entry_work.vsp2 = vsp2;

	schedule_work((struct work_struct *)&vsp2->vspm->entry_work);
}

static void vsp2_vspm_work_queue_init(struct vsp2_device *vsp2)
{
	/* Initialize the work queue
	 * for the entry of job to the VSPM driver. */
	INIT_WORK((struct work_struct *)&vsp2->vspm->entry_work,
		  vsp2_vspm_drv_entry_work);
}

int vsp2_vspm_init(struct vsp2_device *vsp2, int dev_id)
{
	int ret = 0;

	ret = vsp2_vspm_alloc(vsp2);
	if (ret != 0)
		return -ENOMEM;

	/* Initialize the work queue. */
	vsp2_vspm_work_queue_init(vsp2);

	/* Initialize the parameters to VSPM driver. */
	vsp2_vspm_param_init(&vsp2->vspm->ip_par);

	/* Set the VSPM job priority. */
	vsp2->vspm->job_pri = (dev_id == DEVID_1) ? VSP2_VSPM_JOB_PRI_1
						  : VSP2_VSPM_JOB_PRI_0;

	return 0;
}

static void vsp2_vspm_free(struct vsp2_device *vsp2)
{
	struct vsp_start_t *vsp_par = vsp2->vspm->ip_par.par.vsp;

	/* dl_par */
	dma_free_coherent(
		vsp2->dev,
		(128+2048)*8,
		vsp_par->dl_par.virt_addr,
		(dma_addr_t)(vsp_par->dl_par.hard_addr));
}

void vsp2_vspm_exit(struct vsp2_device *vsp2)
{
	vsp2_vspm_free(vsp2);
}
