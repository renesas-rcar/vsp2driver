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

#ifdef VSP2_DEBUG

#include <linux/device.h>
#include <media/v4l2-subdev.h>

#include <linux/delay.h>
#include  <linux/dma-mapping.h>
#include "vsp2_entity.h"
#include "vsp2_debug.h"
#include "vspm_public.h"

/*-----------------------------------------------------------------------------
 * MACRO
 *-----------------------------------------------------------------------------
 */

#define DEBMSG	printk

/*-----------------------------------------------------------------------------
 * Get Entity Name
 *-----------------------------------------------------------------------------
 */
const char *vsp2_debug_getEntityName(struct vsp2_entity *pentity)
{
	/* table */

	struct entry {
		enum vsp2_entity_type   type;
		const char              *pname;
	};

	static struct entry table[] = {
		{ VSP2_ENTITY_BRU, "VSP2_ENTITY_BRU"},
		{ VSP2_ENTITY_LUT, "VSP2_ENTITY_LUT"},
		{ VSP2_ENTITY_CLU, "VSP2_ENTITY_CLU"},
		{ VSP2_ENTITY_RPF, "VSP2_ENTITY_RPF"},
		{ VSP2_ENTITY_UDS, "VSP2_ENTITY_UDS"},
		{ VSP2_ENTITY_HGO, "VSP2_ENTITY_HGO"},
		{ VSP2_ENTITY_HGT, "VSP2_ENTITY_HGT"},
		{ VSP2_ENTITY_WPF, "VSP2_ENTITY_WPF"},
	};

	/* search */

	const char      *pname  = "unknown";
	struct entry    *pentry = table;
	int     count = sizeof(table) / sizeof(struct entry);
	int     i;

	for (i = 0; i < count; i++) {

		if (pentry->type == pentity->type) {

			pname = pentry->pname;
			break;
		}
		pentry++;
	}

	return pname;
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static int s_tabCount;

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_tabs(void)
{
	const char	*ptabs = "";

	switch (s_tabCount) {
	case 0:
		ptabs = "";
		break;

	case 1:
		ptabs = "  ";
		break;

	case 2:
		ptabs = "    ";
		break;
	case 3:
		ptabs = "      ";
		break;

	case 4:
		ptabs = "        ";
		break;

	case 5:
		ptabs = "          ";
		break;

	case 6:
		ptabs = "            ";
		break;

	case 7:
		ptabs = "              ";
		break;

	case 8:
		ptabs = "                ";
		break;

	case 9:
		ptabs = "                  ";
		break;

	default:
		ptabs = "                    ";
		break;
	}
	DEBMSG("%s", ptabs);
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void inc_tab(const char *pname)
{
	s_tabCount++;
	if (pname != NULL) {

		print_tabs();
		DEBMSG("# %s\n", pname);
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void dec_tab(void)
{
	if (s_tabCount > 0)
		s_tabCount--;
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_uc(const char *pname, unsigned char value)
{
	print_tabs();
	DEBMSG("- %-20s = 0x%02x (%d)\n", pname, value, value);
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_us(const char *pname, unsigned short value)
{
	print_tabs();
	DEBMSG("- %-20s = 0x%04x (%d)\n", pname, value, value);
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
#if 0
static void print_ui(const char *pname, unsigned int value)
{
	print_tabs();
	DEBMSG("- %-20s = 0x%08x (%d)\n", pname, value, value);
}
#endif
/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_ul(const char *pname, unsigned long value)
{
	print_tabs();
	DEBMSG("- %-20s = 0x%08x (%d)\n", pname, (int)value, (int)value);
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
#if 0
static void print_ull(const char *pname, unsigned long long value)
{
	print_tabs();
	DEBMSG("- %-20s = 0x%08x%08x\n",
			pname,
			(int)((value>>0)&0x00000000ffffffff),
			(int)((value>>32)&0x00000000ffffffff));
}
#endif
/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
#if 0
static void print_null(const char *pname)
{
	print_tabs();
	DEBMSG("- %s is NULL !!\n", pname);
}
#endif
/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_addr(const char *pname, void *ptr)
{
	print_tabs();
	if (ptr == NULL) {
		DEBMSG("- %s is NULL !!\n", pname);
	} else {
#ifdef TYPE_GEN2
		DEBMSG("- %-20s = 0x%08x\n", pname, (int)ptr);
#else
		unsigned int ptr_h = (unsigned int)((unsigned long)ptr >> 32);
		unsigned int ptr_l = (unsigned int)((unsigned long)ptr >>  0);

		DEBMSG("- %-20s = 0x%08x%08x\n", pname, (int)ptr_h, (int)ptr_l);
#endif
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
#if 0
static void print_vsp_xxx_t(const char *pname, struct vsp_xxx_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_xxx_t");
		dec_tab();
	}
}
#endif

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_irop_unit_t(
	const char *pname, struct vsp_irop_unit_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_irop_unit_t");

		print_uc("op_mode", ptr->op_mode);
		print_uc("ref_sel", ptr->ref_sel);
		print_uc("bit_sel", ptr->bit_sel);

		print_ul("comp_color", ptr->comp_color);
		print_ul("irop_color0", ptr->irop_color0);
		print_ul("irop_color1", ptr->irop_color1);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_ckey_unit_t(
	const char *pname, struct vsp_ckey_unit_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_ckey_unit_t");

		print_uc("mode", ptr->mode);

		print_ul("color1", ptr->color1);
		print_ul("color2", ptr->color2);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_mult_unit_t(
	const char *pname, struct vsp_mult_unit_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_mult_unit_t");

		print_uc("a_mmd", ptr->a_mmd);
		print_uc("p_mmd", ptr->p_mmd);
		print_uc("ratio", ptr->ratio);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_dl_t(const char *pname, struct vsp_dl_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {

		inc_tab("struct vsp_dl_t");

		print_addr("hard_addr", ptr->hard_addr);
		print_addr("virt_addr", ptr->virt_addr);

		print_us("tbl_num", ptr->tbl_num);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_alpha_unit_t(
	const char *pname, struct vsp_alpha_unit_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_alpha_unit_t");

		print_addr("addr_a     ", ptr->addr_a);
		print_us("stride_a   ", ptr->stride_a);
		print_uc("swap       ", ptr->swap);
		print_uc("asel       ", ptr->asel);
		print_uc("aext       ", ptr->aext);
		print_uc("anum0      ", ptr->anum0);
		print_uc("anum1      ", ptr->anum1);
		print_uc("afix       ", ptr->afix);

		print_vsp_irop_unit_t("irop     ", ptr->irop);
		print_vsp_ckey_unit_t("ckey     ", ptr->ckey);
		print_vsp_mult_unit_t("mult     ", ptr->mult);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_src_t(const char *pname, struct vsp_src_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_src_t");

		print_addr("addr", ptr->addr);
		print_addr("addr_c0", ptr->addr_c0);
		print_addr("addr_c1", ptr->addr_c1);

		print_us("stride       ", ptr->stride);
		print_us("stride_c     ", ptr->stride_c);
		print_us("width        ", ptr->width);
		print_us("height       ", ptr->height);
		print_us("width_ex     ", ptr->width_ex);
		print_us("height_ex    ", ptr->height_ex);
		print_us("x_offset     ", ptr->x_offset);
		print_us("y_offset     ", ptr->y_offset);
		print_us("format       ", ptr->format);
		print_uc("swap         ", ptr->swap);
		print_us("x_position   ", ptr->x_position);
		print_us("y_position   ", ptr->y_position);
		print_uc("pwd          ", ptr->pwd);
		print_uc("cipm         ", ptr->cipm);
		print_uc("cext         ", ptr->cext);
		print_uc("csc          ", ptr->csc);
		print_uc("iturbt       ", ptr->iturbt);
		print_uc("clrcng       ", ptr->clrcng);
		print_uc("vir          ", ptr->vir);
		print_ul("vircolor     ", ptr->vircolor);

		print_vsp_dl_t("clut     ", ptr->clut);
		print_vsp_alpha_unit_t("alpha     ", ptr->alpha);
		print_ul("connect      ", ptr->connect);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_dst_t(const char *pname, struct vsp_dst_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_dst_t");

		print_addr("addr", ptr->addr);
		print_addr("addr_c0", ptr->addr_c0);
		print_addr("addr_c1", ptr->addr_c1);

		print_us("stride       ", ptr->stride);
		print_us("stride_c     ", ptr->stride_c);
		print_us("width        ", ptr->width);
		print_us("height       ", ptr->height);
		print_us("x_offset     ", ptr->x_offset);
		print_us("y_offset     ", ptr->y_offset);
		print_us("format       ", ptr->format);
		print_uc("swap         ", ptr->swap);
		print_uc("pxa          ", ptr->pxa);
		print_uc("pad          ", ptr->pad);
		print_us("x_coffset    ", ptr->x_coffset);
		print_us("y_coffset    ", ptr->y_coffset);
		print_uc("csc          ", ptr->csc);
		print_uc("iturbt       ", ptr->iturbt);
		print_uc("clrcng       ", ptr->clrcng);
		print_uc("cbrm          ", ptr->cbrm);
		print_uc("abrm          ", ptr->abrm);
		print_uc("athres        ", ptr->athres);
		print_uc("clmd          ", ptr->clmd);
		print_uc("dith          ", ptr->dith);
		print_uc("rotation      ", ptr->rotation);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_sru_t(const char *pname, struct vsp_sru_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_sru_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_uds_t(const char *pname, struct vsp_uds_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_uds_t");

		print_uc("amd        ", ptr->amd);
		print_uc("clip       ", ptr->clip);
		print_uc("alpha      ", ptr->alpha);
		print_uc("complement ", ptr->complement);
		print_uc("athres0    ", ptr->athres0);
		print_uc("athres1    ", ptr->athres1);
		print_uc("anum0      ", ptr->anum0);
		print_uc("anum1      ", ptr->anum1);
		print_uc("anum2      ", ptr->anum2);
		print_us("x_ratio    ", ptr->x_ratio);
		print_us("y_ratio    ", ptr->y_ratio);
		print_ul("connect    ", ptr->connect);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_lut_t(const char *pname, struct vsp_lut_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_lut_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_clu_t(const char *pname, struct vsp_clu_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_clu_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_hst_t(const char *pname, struct vsp_hst_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_hst_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_hsi_t(const char *pname, struct vsp_hsi_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_hsi_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_bld_dither_t(
	const char *pname, struct vsp_bld_dither_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_bld_dither_t");

		print_uc("mode", ptr->mode);
		print_uc("bpp", ptr->bpp);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_bld_vir_t(const char *pname, struct vsp_bld_vir_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_bld_vir_t");

		print_us("width         ", ptr->width);
		print_us("height        ", ptr->height);
		print_us("x_position    ", ptr->x_position);
		print_us("y_position    ", ptr->y_position);
		print_uc("pwd           ", ptr->pwd);
		print_ul("color         ", ptr->color);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_bld_ctrl_t(const char *pname, struct vsp_bld_ctrl_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_bld_ctrl_t");

		print_uc("rbc           ", ptr->rbc);
		print_uc("crop          ", ptr->crop);
		print_uc("arop          ", ptr->arop);
		print_uc("blend_formula ", ptr->blend_formula);
		print_uc("blend_coefx   ", ptr->blend_coefx);
		print_uc("blend_coefy   ", ptr->blend_coefy);
		print_uc("aformula      ", ptr->aformula);
		print_uc("acoefx        ", ptr->acoefx);
		print_uc("acoefy        ", ptr->acoefy);
		print_uc("acoefx_fix    ", ptr->acoefx_fix);
		print_uc("acoefy_fix    ", ptr->acoefy_fix);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_bld_rop_t(const char *pname, struct vsp_bld_rop_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_bld_rop_t");

		print_uc("crop", ptr->crop);
		print_uc("arop", ptr->arop);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_bru_t(const char *pname, struct vsp_bru_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_bru_t");

		print_ul("lay_order", ptr->lay_order);
		print_uc("adiv", ptr->adiv);

		print_vsp_bld_dither_t("dither_unit[0]", ptr->dither_unit[0]);
		print_vsp_bld_dither_t("dither_unit[1]", ptr->dither_unit[1]);
		print_vsp_bld_dither_t("dither_unit[2]", ptr->dither_unit[2]);
		print_vsp_bld_dither_t("dither_unit[3]", ptr->dither_unit[3]);
		print_vsp_bld_dither_t("dither_unit[4]", ptr->dither_unit[4]);
		print_vsp_bld_vir_t("blend_virtual", ptr->blend_virtual);
		print_vsp_bld_ctrl_t("blend_unit_a", ptr->blend_unit_a);
		print_vsp_bld_ctrl_t("blend_unit_b", ptr->blend_unit_b);
		print_vsp_bld_ctrl_t("blend_unit_c", ptr->blend_unit_c);
		print_vsp_bld_ctrl_t("blend_unit_d", ptr->blend_unit_d);
		print_vsp_bld_ctrl_t("blend_unit_e", ptr->blend_unit_e);
		print_vsp_bld_rop_t	("rop_unit", ptr->rop_unit);

		print_ul("connect", ptr->connect);

		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_hgo_t(const char *pname, struct vsp_hgo_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_hgo_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_hgt_t(const char *pname, struct vsp_hgt_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_hgt_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_shp_t(const char *pname, struct vsp_shp_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_shp_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_drc_t(const char *pname, struct vsp_drc_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_drc_t");
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
static void print_vsp_ctrl_t(const char *pname, struct vsp_ctrl_t *ptr)
{
	print_addr(pname, ptr);
	if (ptr != NULL) {
		inc_tab("struct vsp_ctrl_t");
		print_vsp_sru_t("sru", ptr->sru);/* super-resolution */
		print_vsp_uds_t("uds", ptr->uds);/* up down scaler */
		print_vsp_lut_t("lut", ptr->lut);/* look up table */
		print_vsp_clu_t("clu", ptr->clu);/* cubic look up table */
		print_vsp_hst_t("hst", ptr->hst);/* hue saturation val trans. */
		print_vsp_hsi_t("hsi", ptr->hsi);/* hue saturation val inverse*/
		print_vsp_bru_t("bru", ptr->bru);/* blend rop */
		print_vsp_hgo_t("hgo", ptr->hgo);/* histogram generator-one */
		print_vsp_hgt_t("hgt", ptr->hgt);/* histogram generator-two */
		print_vsp_shp_t("shp", ptr->shp);/* sharpness */
		print_vsp_drc_t("drc", ptr->drc);/* dynamic range compression */
		dec_tab();
	}
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
void print_vspm_entry(void *addr)
{
	struct vsp_start_t *ptr = addr;

	DEBMSG(">>>> vspm_entry() called !!\n");
	DEBMSG("--- vspm params ---\n");
	print_uc("rpf_num", ptr->rpf_num);
	print_ul("rpf_order", ptr->rpf_order);
	print_ul("use_module", ptr->use_module);

	print_vsp_src_t	("src_par[0]", ptr->src_par[0]);
	print_vsp_src_t	("src_par[1]", ptr->src_par[1]);
	print_vsp_src_t	("src_par[2]", ptr->src_par[2]);
	print_vsp_src_t	("src_par[3]", ptr->src_par[3]);
	print_vsp_src_t	("src_par[4]", ptr->src_par[4]);
	print_vsp_dst_t	("vsp_dst_t", ptr->dst_par);
	print_vsp_ctrl_t("ctrl_par", ptr->ctrl_par);
	print_vsp_dl_t	("dl_par", &ptr->dl_par);
	DEBMSG("-------------------\n");
}

/*-----------------------------------------------------------------------------
 *
 *-----------------------------------------------------------------------------
 */
void print_vspm_entry_cb(void)
{
	DEBMSG("<<<< vspm_entry_job() call !!\n");
}

/*-----------------------------------------------------------------------------
 * print version
 *-----------------------------------------------------------------------------
 */
static void print_version(struct vsp2_device *vsp2, struct vsp2_debug *pdeb)
{
#ifdef TYPE_GEN2
	DEBMSG("\n## vsp2driver for Gen2 ##\n\n");
#else
	DEBMSG("\n## vsp2driver for Gen3 ##\n\n");
#endif
}

/*-----------------------------------------------------------------------------
 * vspm debug flgag
 *-----------------------------------------------------------------------------
 */

static bool	s_vspmDebugFlag;

static void setVspmDebug(struct vsp2_device *vsp2, struct vsp2_debug *pdeb)
{
	if (pdeb->param1 == 0) {
		s_vspmDebugFlag = false;
		DEBMSG("## vspm debug : off\n");
	} else {
		s_vspmDebugFlag = true;
		DEBMSG("## vspm debug : on\n");
	}
}

bool vsp2_debug_vspmDebug(void)
{
	return s_vspmDebugFlag;
}

/*-----------------------------------------------------------------------------
 * debug
 *-----------------------------------------------------------------------------
 */

void vsp2_debug(struct vsp2_device *vsp2, void *args)
{
	struct vsp2_debug *pdeb = (struct vsp2_debug *)args;

	switch (pdeb->command) {
	case 0:
		/* print version intfo */
		print_version(vsp2, pdeb);
		break;

	case 1:
		/* set vspm debug mode */
		setVspmDebug(vsp2, pdeb);
		break;

	default:
		DEBMSG("## ERROR!! / %s() unknown command...\n", __func__);
		DEBMSG("comamnd = %d\n", pdeb->command);
		DEBMSG("param1  = %d\n", pdeb->param1);
		DEBMSG("param2  = %d\n", pdeb->param2);
		DEBMSG("param3  = %d\n", pdeb->param3);
		DEBMSG("param4  = %d\n", pdeb->param4);
		break;
	}
}

#endif /* VSP2_DEBUG */
