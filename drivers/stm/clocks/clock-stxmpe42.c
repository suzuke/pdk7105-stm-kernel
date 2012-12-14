/*****************************************************************************
 *
 * File name   : clock-stxmpe42.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* Compilation flags
   CLKLLA_NO_PLL     => Emulation/co-emulation case. PLL/FSYN are not present.
			The rest of the logic is there.
   CLKLLA_NO_MEASURE => disable HW measure capability. Always returns 0.
 */

/* ----- Modification history (most recent first)----
13/dec/12 fabrice.charpentier@st.com
	  Clockgen E & F FS set_rate fixes: wrong prog sequence
16/nov/12 fabrice.charpentier@st.com
	  clkgenf_set_parent() bug fix for M_HVA
12/nov/12 fabrice.charpentier@st.com
	  Clockgen E & F observation func fixes
30/oct/12 fabrice.charpentier@st.com
	  FS660 now using new 'clk-common' API.
26/oct/12 fabrice.charpentier@st.com
	  Clockgen F; added missing clocks.
24/oct/12 fabrice.charpentier@st.com
	  Clockgen VCC bug fixes.
23/oct/12 fabrice.charpentier@st.com
	  Several bug fixes in Linux codes. CLK_M_APB_PM_10 renamed to
	  CLK_M_A0_SPARE_0. Clock obs bugs fixes.
22/oct/12 fabrice.charpentier@st.com
	  clkgenf_identify_parent() bug fix. clkgenf_vcc_xable() added.
19/oct/12 francesco.virlinzi@st.com
	  general review and fix.
24/sep/12 francesco.virlinzi@st.com
	  Common code for PLL API change; now using 'clk-common'
10/sep/12 francesco.virlinzi@st.com
	  Linux cleanup
07/aug/12 fabrice.charpentier@st.com
	  clkgenax_enable() fixes.
13/jul/12 fabrice.charpentier@st.com
	  Some fixes with "parent_rate".
11/jul/12 fabrice.charpentier@st.com
	  Other updates for Orly2
24/may/12 fabrice.charpentier@st.com
	  Clockgen F revisited.
23/may/12 fabrice.charpentier@st.com
	  Sys conf, clockgen A9, DDR, & E revisited.
14/mar/12 fabrice.charpentier@st.com
	  Preliminary mpe42 version
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"

#define CLKLLA_SYSCONF_UNIQREGS			1

#else /* Linux */

#include <linux/stm/clk.h>
#include <linux/io.h>
#include <linux/delay.h>

#endif

#include "clock-stxmpe42.h"
#include "clock-regs-stxmpe42.h"
#include "clock-oslayer.h"
#include "clk-common.h"

#ifdef ST_OS21
static sysconf_base_t sysconf_base[] = {
	{ 5000, 5999, SYS_5000_BASE_ADDRESS },
	{ 6000, 6999, SYS_6000_BASE_ADDRESS },
	{ 7500, 7999, SYS_7500_BASE_ADDRESS },
	{ 8500, 8999, SYS_8500_BASE_ADDRESS },
	{ 9500, 9999, SYS_9500_BASE_ADDRESS },
	{ 0, 0, 0 }
};
#endif

static void *cga_base[3];
static void *mali_base;

/* Physical clocks description */
static struct clk *SataClock;
static struct clk *AltClock;

#ifndef ST_OS21
SYSCONF(0, 7502, 0, 0);
SYSCONF(0, 7502, 25, 27);
SYSCONF(0, 7504, 0, 7);
SYSCONF(0, 7504, 8, 13);
SYSCONF(0, 7504, 14, 19);
SYSCONF(0, 7555, 0, 0);
SYSCONF(0, 7555, 1, 1);
SYSCONF(0, 7556, 0, 0);
SYSCONF(0, 7556, 1, 5);
SYSCONF(0, 7556, 25, 27);
SYSCONF(0, 7558, 0, 7);
SYSCONF(0, 7558, 8, 13);
SYSCONF(0, 7583, 0, 0);
SYSCONF(0, 8539, 0, 15);
SYSCONF(0, 8539, 16, 16);
SYSCONF(0, 8539, 17, 17);
SYSCONF(0, 8540, 0, 31);
SYSCONF(0, 8541, 0, 31);
SYSCONF(0, 8580, 0, 0);
SYSCONF(0, 9505, 0, 0);
SYSCONF(0, 9505, 1, 1);
SYSCONF(0, 9505, 2, 2);
SYSCONF(0, 9505, 3, 3);
SYSCONF(0, 9538, 0, 0);

struct fsynth_sysconf {
	struct sysconf_field *nsb;
	struct sysconf_field *nsdiv;
	struct sysconf_field *pe;
	struct sysconf_field *md;
	struct sysconf_field *sdiv;
	struct sysconf_field *prog_en;
};

struct fsynth_pll {
	struct sysconf_field *npdpll;
	struct sysconf_field *ndiv;
};

enum {
	fsynth_e,
	fsynth_f,
	fsynth_max_value
};

static struct fsynth_pll	fsynth_plls[fsynth_max_value];
static struct fsynth_sysconf	fsynth_channels[fsynth_max_value][4];
#endif

/* Prototypes */
static struct clk clk_clocks[];
static int clkgenax_recalc(struct clk *clk_p);
static int clkgenax_identify_parent(struct clk *clk_p);

/******************************************************************************
CLOCKGEN Ax clocks groups. Common functions
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLK_M_A2_REF) ? 2 :
		 ((clk_id >= CLK_M_A1_REF) ? 1 : 0));
}

/* Returns corresponding clockgen Ax base address for 'clk_id' */
static inline void *clkgenax_get_base_addr(int clk_id)
{
	return cga_base[clkgenax_get_bank(clk_id)];
}

/* Returns corresponding CLK_M_Ax_REF */
static inline unsigned long clkgenax_get_base_id(struct clk *clk_p)
{
	return ((clk_p->id >= CLK_M_A2_REF) ? CLK_M_A2_REF :
		((clk_p->id >= CLK_M_A1_REF) ? CLK_M_A1_REF : CLK_M_A0_REF));
}

static int clkgenax_pll_phi_index(struct clk *clk, int pll_id)
{
	if (pll_id)
		return (clk->id >= CLK_M_A2_PLL1_PHI0 ?
			clk->id - CLK_M_A2_PLL1_PHI0 :
			(clk->id >= CLK_M_A1_PLL1_PHI0 ?
				clk->id - CLK_M_A1_PLL1_PHI0 :
				clk->id - CLK_M_A0_PLL1_PHI0));
	return (clk->id >= CLK_M_A2_PLL0_PHI0 ?
		clk->id - CLK_M_A2_PLL0_PHI0 :
			(clk->id >= CLK_M_A1_PLL0_PHI0 ?
				clk->id - CLK_M_A1_PLL0_PHI0 :
				clk->id - CLK_M_A0_PLL0_PHI0));
}

/* Returns divN_cfg register offset */
static inline unsigned long clkgenax_div_cfg(int clk_src, int clk_idx)
{
	static const unsigned short pll0_odf_table[] = {
		CKGA_PLL0_ODF0_DIV0_CFG,
		CKGA_PLL0_ODF1_DIV0_CFG,
		CKGA_PLL0_ODF2_DIV0_CFG,
		CKGA_PLL0_ODF3_DIV0_CFG
	};
	static const unsigned short pll1_odf_table[] = {
		CKGA_PLL1_ODF0_DIV0_CFG,
		CKGA_PLL1_ODF1_DIV0_CFG,
		CKGA_PLL1_ODF2_DIV0_CFG,
		CKGA_PLL1_ODF3_DIV0_CFG
	};
	unsigned long offset;

	switch (clk_src) {
	case 0: /* OSC */
		return CKGA_OSC_DIV0_CFG + (clk_idx * 4);
	case 1: /* PLL0 ODFx */
		offset = pll0_odf_table[clk_idx / 8];
		break;
	case 2: /* PLL1 ODFx */
		offset = pll1_odf_table[clk_idx / 8];
		break;
	default:
		return 0;	/* Warning: unexpected case */
	}
	offset += (clk_idx % 8) * 4;

	return offset;
}

/* ========================================================================
Name:        clkgenax_get_index
Description: Returns index of given clockgenA clock and source reg infos
	     Source field values: 0=OSC, 1=PLL0-PHIx, 2=PLL1-PHIx, 3=STOP
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgenax_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	switch (clkid) {
	case CLK_M_A0_SPARE_0 ... CLK_M_A9_TRACE:
		idx = clkid - CLK_M_A0_SPARE_0;
		break;
	case CLK_M_A1_SPARE_0 ... CLK_M_GPU_ALT:
		idx = clkid - CLK_M_A1_SPARE_0;
		break;
	case CLK_M_VTAC_MAIN_PHY ... CLK_M_A2_SPARE_31:
		idx = clkid - CLK_M_VTAC_MAIN_PHY;
		break;
	default:
		return -1;
	}

	*srcreg = CKGA_CLKOPSRC_SWITCH_CFG + (idx / 16) * 0x4;
	*shift = (idx % 16) * 2;

	return idx;
}

/* ========================================================================
   Name:        clkgenax_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_parent(struct clk *clk_p, struct clk *src_p)
{
	unsigned long clk_src, val, base_id;
	int idx, shift;
	unsigned long srcreg;
	void *base_addr;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;
	switch (clk_p->id) {
	case CLK_M_A0_SPARE_0 ... CLK_M_A9_TRACE:
	case CLK_M_A1_SPARE_0 ... CLK_M_GPU_ALT:
	case CLK_M_VTAC_MAIN_PHY ... CLK_M_A2_SPARE_31:
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	/* Check if clock and parent are on the same bank */
	if (clkgenax_get_bank(clk_p->id) != clkgenax_get_bank(src_p->id))
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks from dividers */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;
	base_id = clkgenax_get_base_id(src_p);

	switch (src_p->id) {
	case CLK_M_A0_REF:
	case CLK_M_A1_REF:
	case CLK_M_A2_REF:
		clk_src = 0;
		break;
	case CLK_M_A0_PLL0:
	case CLK_M_A1_PLL0:
	case CLK_M_A2_PLL0:
		clk_src = 1;
		src_p = &clk_clocks[base_id + 3 + (idx / 8)];
		break;
	case CLK_M_A0_PLL1:
	case CLK_M_A1_PLL1:
	case CLK_M_A2_PLL1:
		clk_src = 2;
		src_p = &clk_clocks[base_id + 7 + (idx / 8)];
		break;
	case CLK_M_A0_PLL0_PHI0 ... CLK_M_A0_PLL0_PHI3:
	case CLK_M_A1_PLL0_PHI0 ... CLK_M_A1_PLL0_PHI3:
	case CLK_M_A2_PLL0_PHI0 ... CLK_M_A2_PLL0_PHI3:
	case CLK_M_A0_PLL1_PHI0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL1_PHI0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL1_PHI0 ... CLK_M_A2_PLL1_PHI3:
	/* Fall in the default (error) case */
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	base_addr = clkgenax_get_base_addr(clk_p->id);
	val = CLK_READ(base_addr + srcreg) & ~(0x3 << shift);
	val |= (clk_src << shift);
	CLK_WRITE(base_addr + srcreg, val);
	clk_p->parent = src_p;

#if defined(CLKLLA_NO_PLL)
	/* If NO PLL means emulation like platform. Then HW may be forced in
	   a specific position preventing SW change */
	clkgenax_identify_parent(clk_p);
#endif

	return clkgenax_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenax_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_identify_parent(struct clk *clk_p)
{
	int idx;
	unsigned long src_sel, srcreg, base_id;
	int shift;
	void *base_addr;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Statically initialized clocks */
	switch (clk_p->id) {
	case CLK_M_A0_REF ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_REF ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_REF ... CLK_M_A2_PLL1_PHI3:
	case CLK_M_A9_EXT2F_DIV2:
		return 0;
	}

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	base_id = clkgenax_get_base_id(clk_p);
	base_addr = clkgenax_get_base_addr(clk_p->id);
	src_sel = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	switch (src_sel) { /* 0=OSC, 1=PLL0-PHIx, 2=PLL1-PHIx, 3=STOP */
	case 0:
		/* CLKAx_REF */
		clk_p->parent = &clk_clocks[base_id + 0];
		break;
	case 1:
		/* PLL0 PHI0..3 */
		clk_p->parent = &clk_clocks[base_id + 3 + (idx / 8)];
		break;
	case 2:
		/* PLL1 PHI0..3 */
		clk_p->parent = &clk_clocks[base_id + 7 + (idx / 8)];
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenax_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_xable_pll(struct clk *clk_p, int enable)
{
	unsigned long val;
	void *base_addr;
	int bit, err = 0, pll_id = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_A0_PLL0:
	case CLK_M_A0_PLL1:
	case CLK_M_A1_PLL0:
	case CLK_M_A1_PLL1:
	case CLK_M_A2_PLL0:
	case CLK_M_A2_PLL1:
		if (clk_p->id == CLK_M_A0_PLL1 || clk_p->id == CLK_M_A1_PLL1 ||
		    clk_p->id == CLK_M_A2_PLL1)
			bit = 1;
		else
			bit = 0;
		base_addr = clkgenax_get_base_addr(clk_p->id);
		val = CLK_READ(base_addr + CKGA_POWER_CFG);
		if (enable)
			val &= ~(1 << bit);
		else
			val |= (1 << bit);
		CLK_WRITE(base_addr + CKGA_POWER_CFG, val);
		break;
	case CLK_M_A0_PLL1_PHI0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL1_PHI0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL1_PHI0 ... CLK_M_A2_PLL1_PHI3:
		pll_id = 1;
		/* no break here ! */
	case CLK_M_A0_PLL0_PHI0 ... CLK_M_A0_PLL0_PHI3:
	case CLK_M_A1_PLL0_PHI0 ... CLK_M_A1_PLL0_PHI3:
	case CLK_M_A2_PLL0_PHI0 ... CLK_M_A2_PLL0_PHI3:
		bit = clkgenax_pll_phi_index(clk_p, pll_id);
		base_addr = clkgenax_get_base_addr(clk_p->id);
		val = CLK_READ(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 3));
		if (enable)
			val &= ~(1 << bit);
		else
			val |= (1 << bit);
		CLK_WRITE(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 3), val);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (enable)
		err = clkgenax_recalc(clk_p);
	else
		clk_p->rate = 0;

	return err;
}

/* ========================================================================
   Name:        clkgenax_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_enable(struct clk *clk_p)
{
	int err;
	struct clk *parent_clk;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	switch (clk_p->id) {
	case CLK_M_A0_REF:
	case CLK_M_A1_REF:
	case CLK_M_A2_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_M_A0_PLL0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL0 ... CLK_M_A2_PLL1_PHI3:
		return clkgenax_xable_pll(clk_p, 1);
	}

	/* Enabling means there setting the parent clock instead of "off".
	   If parent is undefined, let's select oscillator as default */
	if (clk_p->parent)
		return 0; /* Already ON */
	parent_clk = (clk_p->id >= CLK_M_A2_REF) ? &clk_clocks[CLK_M_A2_REF]
		      : (clk_p->id >= CLK_M_A1_REF) ?
		      &clk_clocks[CLK_M_A1_REF] : &clk_clocks[CLK_M_A0_REF];
	err = clkgenax_set_parent(clk_p, parent_clk);
	/* clkgenax_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:        clkgenax_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_disable(struct clk *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;
	void *base_addr;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power down */
	switch (clk_p->id) {
	case CLK_M_A0_REF:
	case CLK_M_A1_REF:
	case CLK_M_A2_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_M_A0_PLL0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL0 ... CLK_M_A2_PLL1_PHI3:
		return clkgenax_xable_pll(clk_p, 0);
	}

	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	base_addr = clkgenax_get_base_addr(clk_p->id);
	val = CLK_READ(base_addr + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);     /* 3 = STOP clock */
	CLK_WRITE(base_addr + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

/* ========================================================================
   Name:        clkgenax_set_div
   Description: Set divider ratio for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_div(struct clk *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;
	unsigned long clk_src;
	void *base_addr;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = clkgenax_get_base_addr(clk_p->id);

	/* Clock source: 0=OSC, 1=PLL0-PHIx, 2=PLL1-PHIx, 3=STOP */
	clk_src = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	if (clk_src == 3)
		return 0; /* Clock stopped */

	/* Now according to parent, let's write divider ratio */
	offset = clkgenax_div_cfg(clk_src, idx);
	div_cfg = (*div_p - 1) & 0x1F;
	CLK_WRITE(base_addr + offset, div_cfg);

	return 0;
}

/* ========================================================================
   Name:        clkgenax_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_recalc(struct clk *clk_p)
{
	unsigned long data, ratio;
	struct stm_pll pll = {
		.type = stm_pll3200c32,
	};
	unsigned long srcreg, offset;
	void *base_addr;
	int shift, err, idx, pll_id = 0;
	unsigned long parent_rate;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* If no parent, assuming clock is stopped. Sometimes reset default. */
	if (!clk_p->parent) {
		clk_p->rate = 0;
		return 0;
	}
	parent_rate = clk_get_rate(clk_get_parent(clk_p));

	base_addr = clkgenax_get_base_addr(clk_p->id);

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLK_M_A0_REF:  /* Clockgen A0 reference clock */
	case CLK_M_A1_REF:  /* Clockgen A1 reference clock */
	case CLK_M_A2_REF:  /* Clockgen A2 reference clock */
		clk_p->rate = parent_rate;
		return 0;

	case CLK_M_A0_PLL1:
	case CLK_M_A1_PLL1:
	case CLK_M_A2_PLL1:
		pll_id = 1;
		/* No break here! */
	case CLK_M_A0_PLL0:
	case CLK_M_A1_PLL0:
	case CLK_M_A2_PLL0:
#if !defined(CLKLLA_NO_PLL)
		pll.ndiv = CLK_READ(base_addr +
			CKGA_PLLX_REGY_CFG(pll_id, 0)) & 0xff;
		pll.idf = CLK_READ(base_addr +
			CKGA_PLLX_REGY_CFG(pll_id, 1)) & 0x7;
		err = stm_clk_pll_get_rate(clk_p->parent->rate,
			&pll, &clk_p->rate);
#endif
		break;
	case CLK_M_A0_PLL1_PHI0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL1_PHI0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL1_PHI0 ... CLK_M_A2_PLL1_PHI3:
		pll_id = 1;
		/* No break here! */
	case CLK_M_A0_PLL0_PHI0 ... CLK_M_A0_PLL0_PHI3:
	case CLK_M_A1_PLL0_PHI0 ... CLK_M_A1_PLL0_PHI3:
	case CLK_M_A2_PLL0_PHI0 ... CLK_M_A2_PLL0_PHI3:
#if !defined(CLKLLA_NO_PLL)
		ratio = CLK_READ(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 3));
		idx = clkgenax_pll_phi_index(clk_p, pll_id);
		ratio = (ratio >> (4 + (6 * idx))) & 0x3f;
		if (ratio == 0)
			ratio = 1;
		clk_p->rate = parent_rate / ratio;
#endif
		break;

	case CLK_M_A9_EXT2F_DIV2:
		clk_p->rate = parent_rate / 2;
		break;

	default:
		idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Clock source: 0=OSC, 1=PLL0-PHIx, 2=PLL1-PHIx, 3=STOP */
		data = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
		if (data == 3) {
			clk_p->rate = 0;
			return 0;
		}

		offset = clkgenax_div_cfg(data, idx);
		data =  CLK_READ(base_addr + offset);
		ratio = (data & 0x1F) + 1;
		clk_p->rate = parent_rate / ratio;
		return 0;
	}

	#if defined(CLKLLA_NO_PLL)
	if (clk_p->nominal_rate)
		clk_p->rate = clk_p->nominal_rate;
	else
		clk_p->rate = 12121212;
	#endif

	return 0;
}

/* ========================================================================
   Name:        clkgenax_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenax_init(struct clk *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenax_identify_parent(clk_p);
	if (!err)
		err = clkgenax_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenax_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

#ifdef ST_OS21
static unsigned long clkgenax_get_measure(struct clk *clk_p)
{
#ifndef CLKLLA_NO_MEASURE
	unsigned long src;
	unsigned long data, measure;
	void *base;
	int i;

	if (!clk_p)
		return 0;

	switch (clk_p->id) {
	case CLK_M_A0_SPARE_0 ... CLK_M_A9_TRACE:
		src = clk_p->id - CLK_M_A0_SPARE_0;
		break;
	case CLK_M_A1_SPARE_0 ... CLK_M_GPU_ALT:
		src = clk_p->id - CLK_M_A1_SPARE_0;
		break;
	case  CLK_M_VTAC_MAIN_PHY ... CLK_M_A2_SPARE_31:
		src = clk_p->id - CLK_M_VTAC_MAIN_PHY;
		break;
	default:
		return 0;
	}

	measure = 0;
	base = clkgenax_get_base_addr(clk_p->id);

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(base + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(base + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(base + CKGA_CLKOBS_MUX0_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(base + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
		mdelay(10);
		data = CLK_READ(base + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break;	/* IT */
	}
	if (i == 10)
		return 0;

	/* Reading value */
	data = CLK_READ(base + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	CLK_WRITE(base + CKGA_CLKOBS_CMD, 3);

	return measure;
#else
# warning Clock HW measure not implemented
	return 0;
#endif
}
#else
#define clkgenax_get_measure	NULL
#endif

/* ========================================================================
   Name:        clkgenax_observe
   Description: Clockgen Ax clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgenax_observe(struct clk *clk_p, unsigned long *div_p)
{
	unsigned long sel;
	unsigned long divcfg;
	unsigned long srcreg;
	void *base_addr;
	int shift;

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_A0_SPARE_0 ... CLK_M_A9_TRACE:
	case CLK_M_A1_SPARE_0 ... CLK_M_GPU_ALT:
	case CLK_M_VTAC_MAIN_PHY ...CLK_M_A2_SPARE_31:
		sel = clkgenax_get_index(clk_p->id, &srcreg, &shift);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	switch (*div_p) {
	case 2:
		divcfg = 0;
		break;
	case 4:
		divcfg = 1;
		break;
	default:
		divcfg = 2;
		*div_p = 1;
		break;
	}
	base_addr = clkgenax_get_base_addr(clk_p->id);
	CLK_WRITE(base_addr + CKGA_CLKOBS_MUX0_CFG,
		(divcfg << 6) | (sel & 0x3f));

	/* Observation points:
	   A0 => PIO101[7] alt 6
	   A1 => PIO101[0] alt 3
	   A2 => PIO101[4] alt 3
	 */

	/* Configuring appropriate PIO */
	if (base_addr == cga_base[0]) {
		/* Selecting alternate 6 */
		SYSCONF_WRITE(0, 5001, 28, 30, 6);
		/* Enabling IO */
		SYSCONF_WRITE(0, 5040, 15, 15, 1);
	} else if (base_addr == cga_base[1]) {
		/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 5001, 0, 1, 3);
		/* Enabling IO */
		SYSCONF_WRITE(0, 5040, 8, 8, 1);
	} else {
		/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 5001, 16, 18, 3);
		/* Enabling IO */
		SYSCONF_WRITE(0, 5040, 12, 12, 1);
	}
	return 0;
}
#else
# define clkgenax_observe	NULL
#endif

/* ========================================================================
   Name:        clkgenax_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_rate(struct clk *clk_p, unsigned long freq)
{
	unsigned long div, data;
	struct stm_pll pll = {
		.type = stm_pll3200c32,
	};
	unsigned long parent_rate;
	int err = 0, idx, pll_id = 0;
	void *base_addr;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	parent_rate = clk_get_rate(clk_get_parent(clk_p));

	base_addr = clkgenax_get_base_addr(clk_p->id);
	switch (clk_p->id) {
	case CLK_M_A0_PLL1:
	case CLK_M_A1_PLL1:
	case CLK_M_A2_PLL1:
		pll_id = 1;
		/* no break here! */
	case CLK_M_A0_PLL0:
	case CLK_M_A1_PLL0:
	case CLK_M_A2_PLL0:
		err = stm_clk_pll_get_params(clk_p->parent->rate, freq, &pll);
		if (err != 0)
			break;
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 0))
				& 0xffffff00;
		data |= pll.ndiv;
		CLK_WRITE(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 0), data);
		data = CLK_READ(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 1))
				& 0xfffffff8;
		data |= pll.idf;
		CLK_WRITE(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 1), data);
#endif
		break;
	case CLK_M_A0_PLL1_PHI0 ... CLK_M_A0_PLL1_PHI3:
	case CLK_M_A1_PLL1_PHI0 ... CLK_M_A1_PLL1_PHI3:
	case CLK_M_A2_PLL1_PHI0 ... CLK_M_A2_PLL1_PHI3:
		pll_id = 1;
		/* no break here! */
	case CLK_M_A0_PLL0_PHI0 ... CLK_M_A0_PLL0_PHI3:
	case CLK_M_A1_PLL0_PHI0 ... CLK_M_A1_PLL0_PHI3:
	case CLK_M_A2_PLL0_PHI0 ... CLK_M_A2_PLL0_PHI3:
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 3));
		idx = clkgenax_pll_phi_index(clk_p, pll_id);
		idx = 4 + idx * 6;
		data &= ~(0x3f << idx);
		div = (freq / parent_rate) & 0x3f;
		data |= (div << idx);
		CLK_WRITE(base_addr + CKGA_PLLX_REGY_CFG(pll_id, 3), data);
		mdelay(10);
#endif
		break;
	case CLK_M_A0_SPARE_0 ... CLK_M_A9_TRACE:
	case CLK_M_A1_SPARE_0 ... CLK_M_GPU_ALT:
	case CLK_M_VTAC_MAIN_PHY ... CLK_M_DCEPHY_IMPCTRL:
		div = clk_best_div(parent_rate, freq);
		err = clkgenax_set_div(clk_p, &div);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (!err)
		err = clkgenax_recalc(clk_p);

	return err;
}

/******************************************************************************
CLOCKGEN E
******************************************************************************/

/* ========================================================================
   Name:        clkgene_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgene_fsyn_recalc(struct clk *clk_p)
{
#ifdef ST_OS21
	unsigned long val;
#endif
	unsigned long chan;
	unsigned long parent_rate;
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco
	};
	struct stm_fs fs = {
		.type = stm_fs660c32
	};

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	parent_rate = clk_get_rate(clk_get_parent(clk_p));

#ifdef ST_OS21
	/* Is FSYN analog part UP ? */
	if (SYSCONF_READ(0, 8559, 14, 14) == 0) {
		/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* At least analog part (PLL660) is running */
	fs_vco.ndiv = SYSCONF_READ(0, 8559, 15, 17);
	if (clk_p->id == CLK_M_E_FS_VCO)
		return stm_clk_fs_get_rate(parent_rate, &fs_vco,
			&clk_p->rate);

	chan = clk_p->id - CLK_M_PIX_MDTP_0;

	/* Is FSYN digital part UP ? */
	val = SYSCONF_READ(0, 8559, 10, 13);
	if ((val & (1 << chan)) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	fs.mdiv = SYSCONF_READ(0, 8560 + (4 * chan), 0, 4);
	fs.pe = SYSCONF_READ(0, 8561 + (4 * chan), 0, 14);
	fs.sdiv = SYSCONF_READ(0, 8562 + (4 * chan), 0, 3);
	fs.nsdiv = (SYSCONF_READ(0, 8559, 18, 21) >> chan) & 1;
#else
	if (sysconf_read(fsynth_plls[fsynth_e].npdpll) == 0) {
		/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}
	fs_vco.ndiv = sysconf_read(fsynth_plls[fsynth_e].ndiv);
	if (clk_p->id == CLK_M_E_FS_VCO)
		return stm_clk_fs_get_rate(parent_rate, &fs_vco,
				&clk_p->rate);

	chan = clk_p->id - CLK_M_PIX_MDTP_0;
	if (sysconf_read(fsynth_channels[fsynth_e][chan].nsb)) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}
	fs.mdiv = sysconf_read(fsynth_channels[fsynth_e][chan].md);
	fs.pe = sysconf_read(fsynth_channels[fsynth_e][chan].pe);
	fs.sdiv = sysconf_read(fsynth_channels[fsynth_e][chan].sdiv);
	fs.nsdiv = sysconf_read(fsynth_channels[fsynth_e][chan].nsdiv);
#endif

	return stm_clk_fs_get_rate(parent_rate, &fs, &clk_p->rate);
}

/* ========================================================================
   Name:        clkgene_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_fsyn_set_rate(struct clk *clk_p, unsigned long freq)
{
#ifdef ST_OS21
	unsigned long data;
#endif
	unsigned long chan;
	unsigned long parent_rate = clk_get_rate(clk_get_parent(clk_p));
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco
	};
	struct stm_fs fs = {
		.type = stm_fs660c32
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

#ifdef ST_OS21
	if (clk_p->id == CLK_M_E_FS_VCO) {
		if (stm_clk_fs_get_params(parent_rate, freq, &fs_vco))
			return CLK_ERR_BAD_PARAMETER;
		SYSCONF_WRITE(0, 8559, 15, 17, fs_vco.ndiv);
		SYSCONF_WRITE(0, 8559, 14, 14, 1); /* PLL power up */
		return 0;
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	chan = clk_p->id - CLK_M_PIX_MDTP_0;
	fs.nsdiv = 0xff;
	if (stm_clk_fs_get_params(parent_rate, freq, &fs))
		return CLK_ERR_BAD_PARAMETER;

	/* MD set */
	SYSCONF_WRITE(0, 8560 + (4 * chan), 0, 4, fs.mdiv);

	/* PE set */
	SYSCONF_WRITE(0, 8561 + (4 * chan), 0, 14, fs.pe);

	/* SDIV set */
	SYSCONF_WRITE(0, 8562 + (4 * chan), 0, 3, fs.sdiv);

	/* NSDIV set */
	data = SYSCONF_READ(0, 8559, 18, 21) & ~(1 << chan);
	data |= (fs.nsdiv << chan);
	SYSCONF_WRITE(0, 8559, 18, 21, data);

	/* Release "freeze" (NSBi) */
	data = SYSCONF_READ(0, 8559, 10, 13);
	SYSCONF_WRITE(0, 8559, 10, 13, data |  (1 << chan));

	/* PROG set/reset */
	SYSCONF_WRITE(0, 8563 + (4 * chan), 0, 0, 1);
	SYSCONF_WRITE(0, 8563 + (4 * chan), 0, 0, 0);
#else
	if (clk_p->id == CLK_M_E_FS_VCO) {
		if (stm_clk_fs_get_params(parent_rate, freq, &fs_vco))
			return CLK_ERR_BAD_PARAMETER;
		sysconf_write(fsynth_plls[fsynth_e].ndiv, fs_vco.ndiv);
		sysconf_write(fsynth_plls[fsynth_e].npdpll, 1);
		return 0;
	}

	chan = clk_p->id - CLK_M_PIX_MDTP_0;
	fs.nsdiv = 0xff;
	if (stm_clk_fs_get_params(parent_rate, freq, &fs))
		return CLK_ERR_BAD_PARAMETER;
	sysconf_write(fsynth_channels[fsynth_e][chan].md, fs.mdiv);
	sysconf_write(fsynth_channels[fsynth_e][chan].pe, fs.pe);
	sysconf_write(fsynth_channels[fsynth_e][chan].sdiv, fs.sdiv);
	sysconf_write(fsynth_channels[fsynth_e][chan].nsdiv, fs.nsdiv);

	sysconf_write(fsynth_channels[fsynth_e][chan].nsb, 1);
	sysconf_write(fsynth_channels[fsynth_e][chan].prog_en, 1);
	sysconf_write(fsynth_channels[fsynth_e][chan].prog_en, 0);
#endif

	return 0;
}

/* ========================================================================
   Name:        clkgene_fsyn_xable
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_fsyn_xable(struct clk *clk_p, unsigned long enable)
{
#ifdef ST_OS21
	unsigned long cfg8559_10_13;
#endif
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_E_FS_VCO || clk_p->id > CLK_M_MPELPC)
		return CLK_ERR_BAD_PARAMETER;

#ifdef ST_OS21
	switch (clk_p->id) {
	case CLK_M_E_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_M_E_FS_VCO:
		/* Powering down/up ANALOG part */
		if (enable)
			SYSCONF_WRITE(0, 8559, 14, 14, 1);
		else
			SYSCONF_WRITE(0, 8559, 14, 14, 0);
		break;
	default:
		/* Powering down/up DIGITAL part */
		cfg8559_10_13 = SYSCONF_READ(0, 8559, 10, 13);
		chan = clk_p->id - CLK_M_PIX_MDTP_0;
		if (enable) /* Powering up digital part */
			cfg8559_10_13 |= (1 << chan);
		else /* Powering down digital part */
			cfg8559_10_13 &= ~(1 << chan);
		SYSCONF_WRITE(0, 8559, 10, 13, cfg8559_10_13);
	}
#else
	switch (clk_p->id) {
	case CLK_M_E_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_M_E_FS_VCO:
		sysconf_write(fsynth_plls[fsynth_e].npdpll, enable);
		break;
	default:
		chan = clk_p->id - CLK_M_PIX_MDTP_0;
		sysconf_write(fsynth_channels[fsynth_e][chan].nsb, enable);
	}
#endif

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgene_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgene_recalc
   Description: Get clocks frequencies (in Hz) from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_recalc(struct clk *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_E_REF:
		clk_p->rate = clk_p->parent->rate;
		return 0;
	case CLK_M_E_FS_VCO ... CLK_M_MPELPC:
		return clkgene_fsyn_recalc(clk_p);
	}

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:        clkgene_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_set_rate(struct clk *clk_p, unsigned long freq)
{
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_E_FS_VCO || clk_p->id > CLK_M_MPELPC)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgene_fsyn_set_rate(clk_p, freq);
	if (!err)
		err = clkgene_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgene_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_init(struct clk *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks have static parent */

	return clkgene_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgene_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_enable(struct clk *clk_p)
{
	if (clk_p->id == CLK_M_E_REF)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	return clkgene_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:        clkgene_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_disable(struct clk *clk_p)
{
	if (clk_p->id == CLK_M_E_REF)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	return clkgene_fsyn_xable(clk_p, 0);
}

/* ========================================================================
   Name:        clkgene_observe
   Description: Clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgene_observe(struct clk *clk_p, unsigned long *div_p)
{
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_E_FS_VCO || clk_p->id > CLK_M_MPELPC)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation point:
	 * PIO107[3] alt 2
	 */

	/* Configuring clock to observe */
	chan = clk_p->id - CLK_M_PIX_MDTP_0;
	SYSCONF_WRITE(0, 8576, 8, 9, chan);

	/* Configuring appropriate PIO */
	SYSCONF_WRITE(0, 6004, 12, 14, 2);	/* Alternate mode */
	SYSCONF_WRITE(0, 6041, 3, 3, 1);	/* Enabling IO */

	*div_p = 1; /* No divider available */
	return 0;
}
#else
#define clkgene_observe	NULL
#endif

/******************************************************************************
CLOCKGEN F
******************************************************************************/

/* ========================================================================
   Name:        clkgenf_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgenf_fsyn_recalc(struct clk *clk_p)
{
#ifdef ST_OS21
	unsigned long val;
#endif
	unsigned long chan;
	unsigned long parent_rate;
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco
	};
	struct stm_fs fs = {
		.type = stm_fs660c32
	};

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;
	parent_rate = clk_get_rate(clk_get_parent(clk_p));

#ifdef ST_OS21
	/* Is FSYN analog part UP ? */
	if (SYSCONF_READ(0, 8542, 14, 14) == 0) {
		/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* At least analog part (PLL660) is running */
	fs_vco.ndiv = SYSCONF_READ(0, 8542, 15, 17);
	if (clk_p->id == CLK_M_F_FS_VCO)
		return stm_clk_fs_get_rate(parent_rate, &fs_vco,
			&clk_p->rate);

	/* Is FSYN digital part UP ? */
	val = SYSCONF_READ(0, 8542, 10, 13);
	if ((val & (1 << chan)) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	fs.mdiv = SYSCONF_READ(0, 8543 + (4 * chan), 0, 4);
	fs.pe = SYSCONF_READ(0, 8544 + (4 * chan), 0, 14);
	fs.sdiv = SYSCONF_READ(0, 8545 + (4 * chan), 0, 3);
	fs.nsdiv = (SYSCONF_READ(0, 8542, 18, 21) >> chan) & 1;
#else
	if (sysconf_read(fsynth_plls[fsynth_f].npdpll) == 0) {
		/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	fs_vco.ndiv = sysconf_read(fsynth_plls[fsynth_f].ndiv);
	if (clk_p->id == CLK_M_F_FS_VCO)
		return stm_clk_fs_get_rate(parent_rate, &fs_vco,
						&clk_p->rate);

	if (sysconf_read(fsynth_channels[fsynth_f][chan].nsb) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	fs.mdiv = sysconf_read(fsynth_channels[fsynth_f][chan].md);
	fs.pe = sysconf_read(fsynth_channels[fsynth_f][chan].pe);
	fs.sdiv = sysconf_read(fsynth_channels[fsynth_f][chan].sdiv);
	fs.nsdiv = sysconf_read(fsynth_channels[fsynth_f][chan].nsdiv);
#endif

	return stm_clk_fs_get_rate(parent_rate, &fs, &clk_p->rate);
}

/* ========================================================================
   Name:        clkgenf_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_fsyn_set_rate(struct clk *clk_p, unsigned long freq)
{
#ifdef ST_OS21
	unsigned long data;
#endif
	unsigned long chan;
	unsigned long parent_rate = clk_get_rate(clk_get_parent(clk_p));
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco
	};
	struct stm_fs fs = {
		.type = stm_fs660c32
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

#ifdef ST_OS21
	if (clk_p->id == CLK_M_F_FS_VCO) {
		if (stm_clk_fs_get_params(parent_rate, freq, &fs_vco))
			return CLK_ERR_BAD_PARAMETER;
		SYSCONF_WRITE(0, 8542, 15, 17, fs_vco.ndiv);
		SYSCONF_WRITE(0, 8542, 14, 14, 1); /* PLL power up */
		return 0;
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	fs.nsdiv = 0xff;
	if (stm_clk_fs_get_params(parent_rate, freq, &fs))
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;

	/* MD set */
	SYSCONF_WRITE(0, 8543 + (4 * chan), 0, 4, fs.mdiv);
	/* PE set */
	SYSCONF_WRITE(0, 8544 + (4 * chan), 0, 14, fs.pe);
	/* SDIV set */
	SYSCONF_WRITE(0, 8545 + (4 * chan), 0, 3, fs.sdiv);
	/* NSDIV set */
	data = SYSCONF_READ(0, 8542, 18, 21) & ~(1 << chan);
	data |= (fs.nsdiv << chan);
	SYSCONF_WRITE(0, 8542, 18, 21, data);

	/* NSB set. Unfreeze clock */
	data = SYSCONF_READ(0, 8542, 10, 13);
	data |= (1 << chan);
	SYSCONF_WRITE(0, 8542, 10, 13, data);	/* Release "freeze" (NSBi) */

	/* Prog set/reset */
	SYSCONF_WRITE(0, 8546 + (4 * chan), 0, 0, 1);
	SYSCONF_WRITE(0, 8546 + (4 * chan), 0, 0, 0);
#else
	if (clk_p->id == CLK_M_F_FS_VCO) {
		if (stm_clk_fs_get_params(parent_rate, freq, &fs_vco))
			return CLK_ERR_BAD_PARAMETER;
		sysconf_write(fsynth_plls[fsynth_f].ndiv, fs_vco.ndiv);
		sysconf_write(fsynth_plls[fsynth_f].npdpll, 1);
		return 0;
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	fs.nsdiv = 0xff;
	if (stm_clk_fs_get_params(parent_rate, freq, &fs))
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;

	sysconf_write(fsynth_channels[fsynth_f][chan].md, fs.mdiv);
	sysconf_write(fsynth_channels[fsynth_f][chan].pe, fs.pe);
	sysconf_write(fsynth_channels[fsynth_f][chan].sdiv, fs.sdiv);
	sysconf_write(fsynth_channels[fsynth_f][chan].nsdiv, fs.nsdiv);

	sysconf_write(fsynth_channels[fsynth_f][chan].nsb, 1);
	sysconf_write(fsynth_channels[fsynth_f][chan].prog_en, 1);
	sysconf_write(fsynth_channels[fsynth_f][chan].prog_en, 0);
#endif
	return 0;
}

/* ========================================================================
   Name:        clkgenf_fsyn_xable
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_fsyn_xable(struct clk *clk_p, unsigned long enable)
{
#ifdef ST_OS21
	unsigned long cfg8542_10_13;
#endif
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_F_FS_VCO || clk_p->id > CLK_M_FVDP_PROC_FS)
		return CLK_ERR_BAD_PARAMETER;

#ifdef ST_OS21
	switch (clk_p->id) {
	case CLK_M_F_FS_VCO:
		/* Powering down/up ANALOG part */
		if (enable) /* Power up */
			SYSCONF_WRITE(0, 8542, 14, 14, 1);
		else
			SYSCONF_WRITE(0, 8542, 14, 14, 0);
		break;
	default:
		/* Powering down/up DIGITAL part */
		cfg8542_10_13 = SYSCONF_READ(0, 8542, 10, 13);
		chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;
		if (enable)
			cfg8542_10_13 |= (1 << chan);
		else
			cfg8542_10_13 &= ~(1 << chan);
		SYSCONF_WRITE(0, 8542, 10, 13, cfg8542_10_13);
	}
#else
	switch (clk_p->id) {
	case CLK_M_F_FS_VCO:
		sysconf_write(fsynth_plls[fsynth_f].npdpll, enable);
		break;
	default:
		chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;
		sysconf_write(fsynth_channels[fsynth_f][chan].nsb, enable);
	}
#endif

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenf_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgenf_vcc_set_div
   Description: Video Clocks Controller divider setup function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_vcc_set_div(struct clk *clk_p, unsigned long *div_p)
{
	int chan;
	unsigned long set, data;
	static const unsigned char div_table[] = {
		/* 1  2     3  4     5     6     7  8 */
		   0, 1, 0xff, 2, 0xff, 0xff, 0xff, 3 };

	if (clk_p->id < CLK_M_PIX_MAIN_PIPE || clk_p->id > CLK_M_PIX_HDMIRX_1)
		return CLK_ERR_BAD_PARAMETER;
	if (*div_p < 1 || *div_p > 8)
		return CLK_ERR_BAD_PARAMETER;

	set = div_table[*div_p - 1];
	if (set == 0xff)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;

	/* Set SYSTEM_CONFIG8541: div_mode, 2bits per channel */
	data = SYSCONF_READ(0, 8541, 0, 31);
	data &= ~(0x3 << (chan * 2));
	data |= (set << (chan * 2));
	SYSCONF_WRITE(0, 8541, 0, 31, data);

	return 0;
}

/* ========================================================================
   Name:	clkgenf_vcc_recalc
   Description: Update clk_p structure from HW for VCC
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_vcc_recalc(struct clk *clk_p)
{
	unsigned long chan, val;

	chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;
	/* Is the channel stopped ? */
	val = SYSCONF_READ(0, 8539, 0, 15);
	val &= (1 <<  chan);
	if (val)	/* 1=stopped */
		clk_p->rate = 0;
	else {
		/* What is the divider ratio ? */
		val = SYSCONF_READ(0, 8541, 0, 31);
		val >>= (chan * 2);
		val &= 0x3;
		clk_p->rate = clk_p->parent->rate >> val;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenf_vcc_xable
   Description: Enable/disable Video Clock Controller outputs
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_vcc_xable(struct clk *clk_p, int enable)
{
	int chan;
	unsigned long val;

	chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;
	val = SYSCONF_READ(0, 8539, 0, 15);
	if (enable)
		val &= ~(1 << chan);
	else
		val |= (1 << chan);
	SYSCONF_WRITE(0, 8539, 0, 15, val);

	if (enable)
		return clkgenf_vcc_recalc(clk_p);
	clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgenf_recalc
   Description: Get clocks frequencies (in Hz) from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_recalc(struct clk *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	/* Single source clocks */
	case CLK_M_F_REF:
	case CLK_M_PIX_MAIN_SAS ... CLK_M_PIX_HDMIRX_SAS:
	case CLK_M_HVA ... CLK_M_F_VCC_SD:
		clk_p->rate = clk_p->parent->rate;
		break;
	/* FS clocks */
	case CLK_M_F_FS_VCO ... CLK_M_FVDP_PROC_FS:
		return clkgenf_fsyn_recalc(clk_p);
	/* Video Clock Controller clocks */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		return clkgenf_vcc_recalc(clk_p);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenf_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_identify_parent(struct clk *clk_p)
{
	unsigned long chan, val;
	static const struct clk *vcc_parent_clocks[] = {
		&clk_clocks[CLK_M_F_VCC_HD],		/* clk_hd */
		&clk_clocks[CLK_M_F_VCC_SD],		/* clk_sd */
		&clk_clocks[CLK_M_PIX_MAIN_VIDFS],	/* clk_hd_ext */
		&clk_clocks[CLK_M_PIX_HDMIRX_SAS]	/* clk_sd_ext */
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_PIX_MAIN_PIPE ...CLK_M_PIX_HDMIRX_1:
		/* Clocks from "Video Clock Controller". */
		chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;
		/* sel : 00 clk_hd, 01 clk_sd, 10 clk_hd_ext, 11 clk_sd_ext */
		val = SYSCONF_READ(0, 8540, 0, 31);
		val >>= (chan * 2);
		val &= 0x3;
		clk_p->parent = (struct clk *)vcc_parent_clocks[val];
		break;
	case CLK_M_HVA:
		val = SYSCONF_READ(0, 9538, 0, 0);
		clk_p->parent =
			&clk_clocks[val ? CLK_M_HVA_ALT : CLK_M_HVA_FS];
		break;
	case CLK_M_FVDP_PROC:
		val = SYSCONF_READ(0, 8580, 0, 0);
		clk_p->parent = &clk_clocks[val ? CLK_M_FVDP_PROC_FS :
				CLK_M_FVDP_PROC_ALT];
		break;
	case CLK_M_F_VCC_HD:
		val = SYSCONF_READ(0, 8539, 16, 16);
		clk_p->parent = &clk_clocks[val ? CLK_M_PIX_MAIN_VIDFS :
				CLK_M_PIX_MAIN_SAS];
		break;
	case CLK_M_F_VCC_SD:
		val = SYSCONF_READ(0, 8539, 17, 17);
		clk_p->parent = &clk_clocks[val ? CLK_M_HVA_FS :
				CLK_M_PIX_AUX_SAS];
		break;
	}

	/* Other clocks are statically initialized
	   thanks to _CLK_P() macro */

	return 0;
}

/* ========================================================================
   Name:        clkgenf_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_init(struct clk *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenf_identify_parent(clk_p);
	if (!err)
		err = clkgenf_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenf_set_parent
   Description: Set clock source when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_set_parent(struct clk *clk_p, struct clk *src_p)
{
	unsigned long chan, val, data;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	/* Video clock controller sources */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		switch (src_p->id) {
		case CLK_M_F_VCC_HD:
			val = 0;	/* clk_hd */
			break;
		case CLK_M_F_VCC_SD:
			val = 1;	/* clk_sd */
			break;
		case CLK_M_PIX_MAIN_VIDFS:
			val = 2;	/* clk_hd_ext */
			break;
		case CLK_M_PIX_HDMIRX_SAS:
			val = 3;
			break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;
		data = SYSCONF_READ(0, 8540, 0, 31);
		data &= ~(0x3 << (chan * 2));
		data |= (val << (chan * 2));
		SYSCONF_WRITE(0, 8540, 0, 31, data);
		clk_p->parent = src_p;
		break;
	/* Muxes sources */
	case CLK_M_HVA:
		val = (src_p->id == CLK_M_HVA_ALT ? 1 : 0);
		SYSCONF_WRITE(0, 9538, 0, 0, val);
		clk_p->parent = src_p;
		break;
	case CLK_M_FVDP_PROC:
		if (src_p->id == CLK_M_FVDP_PROC_ALT)	/* A1 div 16 */
			SYSCONF_WRITE(0, 8580, 0, 0, 0);
		else	/* Fsyn = CLK_M_FVDP_PROC_FS */
			SYSCONF_WRITE(0, 8580, 0, 0, 1);
		clk_p->parent = src_p;
		break;
	case CLK_M_F_VCC_HD:
		if (src_p->id == CLK_M_PIX_MAIN_SAS)
			SYSCONF_WRITE(0, 8539, 16, 16, 0);
		else
			SYSCONF_WRITE(0, 8539, 16, 16, 1);
		clk_p->parent = src_p;
		break;
	case CLK_M_F_VCC_SD:
		if (src_p->id == CLK_M_PIX_AUX_SAS)
			SYSCONF_WRITE(0, 8539, 17, 17, 0);
		else
			SYSCONF_WRITE(0, 8539, 17, 17, 1);
		clk_p->parent = src_p;
		break;
	}

	#if defined(CLKLLA_NO_PLL)
	/* If NO PLL means emulation like platform. Then HW may be forced in
	   a specific position preventing SW change */
	clkgenf_identify_parent(clk_p);
	#endif

	return clkgenf_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenf_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_set_rate(struct clk *clk_p, unsigned long freq)
{
	int err = 0;
	unsigned long div;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	/* FS clocks */
	case CLK_M_F_FS_VCO ... CLK_M_FVDP_PROC_FS:
		err = clkgenf_fsyn_set_rate(clk_p, freq);
		break;
	/* VCC clocks */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		div = clk_best_div(clk_p->parent->rate, freq);
		err = clkgenf_vcc_set_div(clk_p, &div);
		break;
	/* Mux output clocks: updating parent rate */
	case CLK_M_HVA ... CLK_M_F_VCC_SD:
		/* Special cases: _ALT clocks do not belong to F */
		if (clk_p->parent->id == CLK_M_HVA_ALT ||
			clk_p->parent->id == CLK_M_FVDP_PROC_ALT)
			err = clkgenax_set_rate(clk_p->parent, freq);
		else
			err = clkgenf_set_rate(clk_p->parent, freq);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (!err)
		err = clkgenf_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenf_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_enable(struct clk *clk_p)
{
	switch (clk_p->id) {
	case CLK_M_F_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	/* FS outputs */
	case CLK_M_F_FS_VCO ... CLK_M_FVDP_PROC_FS:
		return clkgenf_fsyn_xable(clk_p, 1);
	/* Muxes outputs */
	case CLK_M_HVA ... CLK_M_F_VCC_SD:
		return clkgenf_enable(clk_p->parent);
	/* Video Clock Controller outputs */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		return clkgenf_vcc_xable(clk_p, 1);
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenf_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_disable(struct clk *clk_p)
{
	switch (clk_p->id) {
	case CLK_M_F_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	/* FS outputs */
	case CLK_M_F_FS_VCO ... CLK_M_FVDP_PROC_FS:
		return clkgenf_fsyn_xable(clk_p, 0);
	/* Muxes outputs */
	case CLK_M_HVA ... CLK_M_F_VCC_SD:
		return clkgenf_disable(clk_p->parent);
	/* Video Clock Controller outputs */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		return clkgenf_vcc_xable(clk_p, 0);
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenf_observe
   Description: Clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgenf_observe(struct clk *clk_p, unsigned long *div_p)
{
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_F_REF || clk_p->id > CLK_M_PIX_HDMIRX_1)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation points:
	 * VCC channels => PIO107[0] alt 3
	 * FS channels  => PIO107[2] alt 2
	 */

	/* Configuring appropriate PIO */
	switch (clk_p->id) {
	case CLK_M_PIX_MAIN_VIDFS ... CLK_M_FVDP_PROC_FS:
		chan = clk_p->id - CLK_M_PIX_MAIN_VIDFS;
		SYSCONF_WRITE(0, 8621, 0, 1, chan);
		/*FS channels => PIO107[2] alt 2 */
		SYSCONF_WRITE(0, 6004, 8, 10, 2);	/* Alternate mode */
		SYSCONF_WRITE(0, 6041, 2, 2, 1);	/* Enabling IO */
		break;
	/* Clocks from VCC */
	case CLK_M_PIX_MAIN_PIPE ... CLK_M_PIX_HDMIRX_1:
		chan = clk_p->id - CLK_M_PIX_MAIN_PIPE;
		SYSCONF_WRITE(0, 8576, 3, 6, chan);
		/*VCC channels => PIO107[0] alt 3 */
		SYSCONF_WRITE(0, 6004, 0, 1, 3);	/* Alternate mode */
		SYSCONF_WRITE(0, 6041, 0, 0, 1);	/* Enabling IO */
	}
	*div_p = 1; /* No divider available */

	return 0;
}
#else
#define clkgenf_observe	NULL
#endif

/******************************************************************************
CLOCKGEN D (DDR sub-systems)
******************************************************************************/

/* ========================================================================
   Name:        clkgenddr_recalc
   Description: Get CKGD (LMI) clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenddr_recalc(struct clk *clk_p)
{
	unsigned long vcoby2_rate, odf;
	struct stm_pll pll = {
		.type = stm_pll3200c32,
	};
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_DDR_REF:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLK_M_DDR_IC_LMI0:
	case CLK_M_DDR_IC_LMI1:
#if !defined(CLKLLA_NO_PLL)

		pll.idf = SYSCONF_READ(0, 7502, 25, 27);
		pll.ndiv = SYSCONF_READ(0, 7504, 0, 7);
		err = stm_clk_pll_get_rate
			(clk_p->parent->rate, &pll, &vcoby2_rate);
		if (clk_p->id == CLK_M_DDR_IC_LMI0)
			odf = SYSCONF_READ(0, 7504, 8, 13);
		else
			odf = SYSCONF_READ(0, 7504, 14, 19);
		if (odf == 0)
			odf = 1;
		clk_p->rate = vcoby2_rate / odf;

#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
#endif
		break;
	case CLK_M_DDR_DDR0:
	case CLK_M_DDR_DDR1:
		clk_p->rate = clk_p->parent->rate * 4;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */
	}
	return 0;
}

/* ========================================================================
   Name:        clkgenddr_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenddr_init(struct clk *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Parents are static. No idenfication required */
	return clkgenddr_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenddr_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgenddr_set_rate(struct clk *clk_p, unsigned long freq)
{
	unsigned long odf, vcoby2_rate;
	int err = 0;
	struct stm_pll pll = {
		.type = stm_pll3200c32,
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_DDR_IC_LMI0 || clk_p->id > CLK_M_DDR_IC_LMI1)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	if (freq < 800000000) {
		odf = 800000000 / freq;
		if (800000000 % freq)
			odf = odf + 1;
	} else
		odf = 1;
	vcoby2_rate = freq * odf;

	err = stm_clk_pll_get_params(clk_p->parent->rate, vcoby2_rate, &pll);
	if (err != 0)
		return err;

	/* WARNING: How to make it safe when code executed from DDR ? */

	SYSCONF_WRITE(0, 7502, 0, 0, 1);	/* Power down */
	SYSCONF_WRITE(0, 7502, 25, 27, pll.idf);	/* idf */
	SYSCONF_WRITE(0, 7504, 0, 7, pll.ndiv);	/* ndiv */
	SYSCONF_WRITE(0, 7502, 0, 0, 0);	/* Power up */

	/* Now should wait for PLL lock
	   TO BE COMPLETED !!! */

	if (clk_p->id == CLK_M_DDR_IC_LMI0)
		SYSCONF_WRITE(0, 7504, 8, 13, odf);
	else
		SYSCONF_WRITE(0, 7504, 14, 19, odf);

	return clkgenddr_recalc(clk_p);
}
#else
/*
 * In Linux clkgenddr_set_rate defined as NULL
 * because it can not be done in standard C-Code
 */
#define clkgenddr_set_rate	NULL
#endif

/******************************************************************************
CA9 PLL
******************************************************************************/

/* ========================================================================
   Name:        clkgena9_recalc
   Description: Get clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_recalc(struct clk *clk_p)
{
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_A9_REF:
	case CLK_M_A9:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLK_M_A9_PERIPHS:
		clk_p->rate = clk_p->parent->rate / 2;
		break;
	case CLK_M_A9_PHI0:
		{
		unsigned long vcoby2_rate, odf;
		struct stm_pll pll = {
			.type = stm_pll3200c32,
		};
		pll.idf = SYSCONF_READ(0, 7556, 25, 27);
		pll.ndiv = SYSCONF_READ(0, 7558, 0, 7);
		if (SYSCONF_READ(0, 7556, 0, 0))
			/* A9_PLL_PD=1 => PLL disabled */
			clk_p->rate = 0;
		else
			err = stm_clk_pll_get_rate
				(clk_p->parent->rate, &pll, &vcoby2_rate);
			if (err)
				return CLK_ERR_BAD_PARAMETER;
		odf = SYSCONF_READ(0, 7558, 8, 13);
		if (odf == 0)
			odf = 1;
		clk_p->rate = vcoby2_rate / odf;
		}
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */
	}

	return 0;
}

/* ========================================================================
   Name:        clkgena9_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_identify_parent(struct clk *clk_p)
{
	if (clk_p->id != CLK_M_A9) /* Other clocks have static parent */
		return 0;

	/* Is CA9 clock sourced from PLL or A10-10 ? */
	if (SYSCONF_READ(0, 7555, 1, 1))
		if (SYSCONF_READ(0, 7555, 0, 0))
			clk_p->parent = &clk_clocks[CLK_M_A9_EXT2F];
		else
			clk_p->parent = &clk_clocks[CLK_M_A9_EXT2F_DIV2];
	else
		clk_p->parent = &clk_clocks[CLK_M_A9_PHI0];

	return 0;
}

/* ========================================================================
   Name:        clkgena9_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena9_init(struct clk *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgena9_identify_parent(clk_p);
	if (!err)
		err = clkgena9_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgena9_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_set_rate(struct clk *clk_p, unsigned long freq)
{
	unsigned long odf, vcoby2_rate;
	struct stm_pll pll = {
		.type = stm_pll3200c32,
	};
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	if (clk_p->id == CLK_M_A9_PERIPHS)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id == CLK_M_A9)
		return clkgena9_set_rate(clk_p->parent, freq);
	if (clk_p->id != CLK_M_A9_PHI0)
		return CLK_ERR_BAD_PARAMETER;

	if (freq < 800000000) {
		odf = 800000000 / freq;
		if (800000000 % freq)
			odf = odf + 1;
	} else
		odf = 1;
	vcoby2_rate = freq * odf;
	err = stm_clk_pll_get_params(clk_p->parent->rate, vcoby2_rate, &pll);
	if (err != 0)
		return err;

	SYSCONF_WRITE(0, 7555, 1, 1, 1);	/* Bypassing PLL */

	SYSCONF_WRITE(0, 7556, 0, 0, 1);	/* Disabling PLL */
	SYSCONF_WRITE(0, 7556, 25, 27, pll.idf);	/* IDF */
	SYSCONF_WRITE(0, 7558, 0, 7, pll.ndiv);	/* NDIV */
	SYSCONF_WRITE(0, 7558, 8, 13, odf);	/* ODF */
	SYSCONF_WRITE(0, 7556, 1, 5, pll.cp);	/* Charge Pump */

	SYSCONF_WRITE(0, 7556, 0, 0, 0);	/* Reenabling PLL */
	/* Now wait for lock */
	while (!SYSCONF_READ(0, 7583, 0, 0))
		;
	/* Can't put any delay because may rely on a clock that is currently
	   changing (running from CA9 case). */

	SYSCONF_WRITE(0, 7555, 1, 1, 0);	/* Selecting internal PLL */

	return clkgena9_recalc(clk_p);
}

/******************************************************************************
MALI400/GPU clockgen (PLL1200)
******************************************************************************/

/* ========================================================================
   Name:        clkgengpu_recalc
   Description: Get clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgengpu_recalc(struct clk *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_M_GPU_REF:
	case CLK_M_GPU:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLK_M_GPU_PHI: {
		/* This clock is FVCO/ODF output */
#if !defined(CLKLLA_NO_PLL)
		unsigned long data;
		struct stm_pll pll = {
			.type = stm_pll1200c32,
		};

		/* Is the PLL enabled ? */
		if (!SYSCONF_READ(0, 9505, 3, 3) ||
		    !(CLK_READ(mali_base + 4) & 1)) {
			clk_p->rate = 0; /* PLL is disabled */
			return 0;
		}

		/* PLL is ON */
		data = CLK_READ(mali_base + 0);
		pll.idf = data & 0x7; /* rdiv */
		pll.ldf = (data >> 3) & 0x7f; /* ddiv */
		pll.odf = (data >> 10) & 0x3f; /* odf */
		return stm_clk_pll_get_rate(clk_p->parent->rate,
			&pll, &clk_p->rate);
#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
#endif
		}
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */
	}
	return 0;
}

/* ========================================================================
   Name:        clkgengpu_identify_parent
   Description: Identify parent clock for clockgen GPU clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgengpu_identify_parent(struct clk *clk_p)
{
	if (clk_p->id <  CLK_M_GPU_REF || clk_p->id > CLK_M_GPU)
		return 0;

	if (clk_p->id == CLK_M_GPU_REF) {
		if (SYSCONF_READ(0, 9505, 0, 0))
			clk_p->parent = SataClock; /* SATA osc */
		else
			clk_p->parent = AltClock; /* Local osc */
	} else if (clk_p->id == CLK_M_GPU) {
		if (SYSCONF_READ(0, 9505, 2, 2))
			clk_p->parent = &clk_clocks[CLK_M_GPU_PHI];
		else if (SYSCONF_READ(0, 9505, 1, 1))
			clk_p->parent = &clk_clocks[CLK_M_GPU_ALT];
		else
			clk_p->parent = &clk_clocks[CLK_M_GPU_REF];
	}

	return 0;
}

/* ========================================================================
   Name:        clkgengpu_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgengpu_init(struct clk *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgengpu_identify_parent(clk_p);
	if (!err)
		err = clkgengpu_recalc(clk_p);

	return err;
}

static int clkgengpu_xxable(struct clk *clk_p, int enable)
{
	switch (clk_p->id) {
	case CLK_M_GPU_REF:
	case CLK_M_GPU:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_M_GPU_PHI:
		SYSCONF_WRITE(0, 9505, 2, 2, 0);
		SYSCONF_WRITE(0, 9505, 3, 3, 0);
		if (enable) {
			unsigned long data;
			SYSCONF_WRITE(0, 9505, 3, 3, 1);
			SYSCONF_WRITE(0, 9505, 2, 2, 1);
			data = CLK_READ(mali_base + 4);
			data &= ~1;
			/* Strobe... */
			CLK_WRITE(mali_base + 0x4, data | (1 << 4));
			CLK_WRITE(mali_base + 0x4, data);
			data |= 1; /* set the pll enabled */;
			/* Strobe... */
			CLK_WRITE(mali_base + 0x4, data | (1 << 4));
			CLK_WRITE(mali_base + 0x4, data);
			clkgengpu_recalc(clk_p);
		} else
			clk_p->rate = 0;
		break;
	}
	return 0;
}

static int clkgengpu_enable(struct clk *clk_p)
{
	return clkgengpu_xxable(clk_p, 1);
}

static int clkgengpu_disable(struct clk *clk_p)
{
	return clkgengpu_xxable(clk_p, 0);
}

/* ========================================================================
   Name:        clkgengpu_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgengpu_set_rate(struct clk *clk_p, unsigned long freq)
{
	struct stm_pll pll = {
		.type = stm_pll1200c32,
	};
	int err = 0;
	unsigned long val;
	unsigned long parent_rate;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_M_GPU_PHI || clk_p->id > CLK_M_GPU)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	parent_rate = clk_get_rate(clk_get_parent(clk_p));

#if !defined(CLKLLA_NO_PLL)
	switch (clk_p->id) {
	case CLK_M_GPU_PHI:
		err = stm_clk_pll_get_params(parent_rate, freq, &pll);
		if (err)
			break;

/* WARNING: there is probably something to check/do before changing PLL freq.
   Shouldn't we bypass it first ????
 */

		CLK_WRITE(mali_base + 0,
			pll.odf << 10 | pll.ldf << 3 | pll.idf);
		val = CLK_READ(mali_base + 0x4);
		CLK_WRITE(mali_base + 0x4, val | (1 << 4)); /* Strobe UP */
		CLK_WRITE(mali_base + 0x4, val); /* Strobe DOWN */
		break;
	case CLK_M_GPU:
		if (clk_p->parent->id == CLK_M_GPU_PHI)
			err = clkgengpu_set_rate(clk_p->parent, freq);
		else
			err = CLK_ERR_BAD_PARAMETER;
		break;
	}
#endif

	if (!err)
		err = clkgengpu_recalc(clk_p);
	return err;
}

_CLK_OPS(clkgena0,
	"A10",
	clkgenax_init,
	clkgenax_set_parent,
	clkgenax_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[2]"       /* Observation point */
);
_CLK_OPS(clkgena1,
	"A11",
	clkgenax_init,
	clkgenax_set_parent,
	clkgenax_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[0]"       /* Observation point */
);
_CLK_OPS(clkgena2,
	"A12",
	clkgenax_init,
	clkgenax_set_parent,
	clkgenax_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[4]"	/* Observation point */
);
_CLK_OPS(clkgene,
	"E",
	clkgene_init,
	NULL,
	clkgene_set_rate,
	clkgene_recalc,
	clkgene_enable,
	clkgene_disable,
	clkgene_observe,
	NULL,		/* No measure function */
	"PIO107[3]"	/* Observation point */
);
_CLK_OPS(clkgenf,
	"F",
	clkgenf_init,
	clkgenf_set_parent,
	clkgenf_set_rate,
	clkgenf_recalc,
	clkgenf_enable,
	clkgenf_disable,
	clkgenf_observe,
	NULL,		/* No measure function */
	"PIO107[0 & 2]"	/* Observation point */
);
_CLK_OPS(clkgenddr,
	"DDRSS",
	clkgenddr_init,
	NULL,
	clkgenddr_set_rate,
	clkgenddr_recalc,
	NULL,
	NULL,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgena9,
	"CA9",
	clkgena9_init,
	NULL,
	clkgena9_set_rate,
	clkgena9_recalc,
	NULL,
	NULL,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgengpu,
	"GPU",
	clkgengpu_init,
	NULL,
	clkgengpu_set_rate,
	clkgengpu_recalc,
	clkgengpu_enable,
	clkgengpu_disable,
	NULL,
	NULL,
	NULL		/* No observation point */
);

static struct clk clk_clocks[] = {
/* Clockgen A10 */
_CLK(CLK_M_A0_REF, &clkgena0, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_A0_PLL0, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_REF]),
_CLK_P(CLK_M_A0_PLL0_PHI0, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL0]),
_CLK_P(CLK_M_A0_PLL0_PHI1, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL0]),
_CLK_P(CLK_M_A0_PLL0_PHI2, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL0]),
_CLK_P(CLK_M_A0_PLL0_PHI3, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL0]),
_CLK_P(CLK_M_A0_PLL1, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_REF]),
_CLK_P(CLK_M_A0_PLL1_PHI0, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL1]),
_CLK_P(CLK_M_A0_PLL1_PHI1, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL1]),
_CLK_P(CLK_M_A0_PLL1_PHI2, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL1]),
_CLK_P(CLK_M_A0_PLL1_PHI3, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A0_PLL1]),

_CLK(CLK_M_A0_SPARE_0,	&clkgena0,    50000000,    0),
_CLK(CLK_M_FDMA_12,	&clkgena0,    400000000,    0),
_CLK(CLK_M_PP_DMU_0,	&clkgena0,    200000000,    0),
_CLK(CLK_M_PP_DMU_1,	&clkgena0,    200000000,    0),
_CLK(CLK_M_ICN_LMI,	&clkgena0,    0, 0),
_CLK(CLK_M_VID_DMU_0,	&clkgena0,    0, 0),
_CLK(CLK_M_VID_DMU_1,	&clkgena0,    0, 0),
_CLK(CLK_M_A9_EXT2F,	&clkgena0,    200000000,    0),
_CLK_P(CLK_M_A9_EXT2F_DIV2,	&clkgena0,    30000000, CLK_ALWAYS_ENABLED, &clk_clocks[CLK_M_A9_EXT2F]),
_CLK(CLK_M_ST40RT,	&clkgena0,    500000000,    0),
_CLK(CLK_M_ST231_DMU_0,	&clkgena0,    500000000,    0),
_CLK(CLK_M_ST231_DMU_1,	&clkgena0,    500000000,    0),
_CLK(CLK_M_ST231_AUD,	&clkgena0,    600000000,    0),
_CLK(CLK_M_ST231_GP_0,	&clkgena0,    600000000,    0),
_CLK(CLK_M_ST231_GP_1,	&clkgena0,     600000000,    0),
_CLK(CLK_M_ICN_CPU,	&clkgena0,    500000000,    CLK_ALWAYS_ENABLED),
_CLK(CLK_M_ICN_STAC,	&clkgena0,     300000000,   CLK_ALWAYS_ENABLED),
_CLK(CLK_M_TX_ICN_DMU_0,	&clkgena0,     333333333,    0),
_CLK(CLK_M_TX_ICN_DMU_1,	&clkgena0,    333333333,    0),
_CLK(CLK_M_TX_ICN_TS,	&clkgena0,    333333333,    0),
_CLK(CLK_M_TX_ICN_VDP_0,	&clkgena0,    333333333, 0),
_CLK(CLK_M_TX_ICN_VDP_1,	&clkgena0,    333333333, 0),
_CLK(CLK_M_ICN_VP8,	&clkgena0,    333333333, 0),
_CLK(CLK_M_ICN_REG_11,	&clkgena0,    200000000, CLK_ALWAYS_ENABLED),
_CLK(CLK_M_A9_TRACE,	&clkgena0,    200000000,    0),

/* Clockgen A11 */
_CLK(CLK_M_A1_REF, &clkgena1, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_A1_PLL0, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLK_M_A1_REF]),
_CLK_P(CLK_M_A1_PLL0_PHI0, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL0]),
_CLK_P(CLK_M_A1_PLL0_PHI1, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL0]),
_CLK_P(CLK_M_A1_PLL0_PHI2, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL0]),
_CLK_P(CLK_M_A1_PLL0_PHI3, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL0]),
_CLK_P(CLK_M_A1_PLL1, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_REF]),
_CLK_P(CLK_M_A1_PLL1_PHI0, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL1]),
_CLK_P(CLK_M_A1_PLL1_PHI1, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL1]),
_CLK_P(CLK_M_A1_PLL1_PHI2, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL1]),
_CLK_P(CLK_M_A1_PLL1_PHI3, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A1_PLL1]),

_CLK(CLK_M_FDMA_10,    &clkgena1,   450000000,    0),
_CLK(CLK_M_FDMA_11,    &clkgena1,   450000000,    0),
_CLK(CLK_M_HVA_ALT,    &clkgena1,   400000000,    0),
_CLK(CLK_M_PROC_SC,    &clkgena1,   450000000,    0),
_CLK(CLK_M_TP,    &clkgena1,   400000000,    0),
_CLK(CLK_M_RX_ICN_DMU_0,    &clkgena1,   450000000,    0),
_CLK(CLK_M_RX_ICN_DMU_1,    &clkgena1,   450000000,    0),
_CLK(CLK_M_RX_ICN_TS,  &clkgena1,   450000000,    0),
_CLK(CLK_M_RX_ICN_VDP_0,	  &clkgena1,   450000000,    0),
_CLK(CLK_M_PRV_T1_BUS,      &clkgena1,   50000000,    0),
_CLK(CLK_M_ICN_REG_12,      &clkgena1,    200000000,    0),
_CLK(CLK_M_ICN_REG_10,      &clkgena1,   200000000,    0),
_CLK(CLK_M_ICN_ST231,      &clkgena1,   0,    0),
_CLK(CLK_M_FVDP_PROC_ALT,      &clkgena1,   0,    CLK_RATE_PROPAGATES),
_CLK(CLK_M_ICN_REG_13,      &clkgena1,   0,    0),
_CLK(CLK_M_TX_ICN_GPU,      &clkgena1,   0,    0),
_CLK(CLK_M_RX_ICN_GPU,      &clkgena1,   0,    0),
_CLK(CLK_M_APB_PM_12,      &clkgena1,   0,    0),
_CLK(CLK_M_GPU_ALT,      &clkgena1,   0,    0),

/* Clockgen A12 */
_CLK(CLK_M_A2_REF, &clkgena2, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_A2_PLL0, &clkgena2, 900000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLK_M_A2_REF]),
_CLK_P(CLK_M_A2_PLL0_PHI0, &clkgena2, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL0]),
_CLK_P(CLK_M_A2_PLL0_PHI1, &clkgena2, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL0]),
_CLK_P(CLK_M_A2_PLL0_PHI2, &clkgena2, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL0]),
_CLK_P(CLK_M_A2_PLL0_PHI3, &clkgena2, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL0]),
_CLK_P(CLK_M_A2_PLL1, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_REF]),
_CLK_P(CLK_M_A2_PLL1_PHI0, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL1]),
_CLK_P(CLK_M_A2_PLL1_PHI1, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL1]),
_CLK_P(CLK_M_A2_PLL1_PHI2, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL1]),
_CLK_P(CLK_M_A2_PLL1_PHI3, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A2_PLL1]),

_CLK(CLK_M_VTAC_MAIN_PHY,    &clkgena2,   744000000,    0),
_CLK(CLK_M_VTAC_AUX_PHY,    &clkgena2,   248000000,    0),
_CLK(CLK_M_STAC_PHY,    &clkgena2,   800000000,    0),
_CLK(CLK_M_STAC_SYS,    &clkgena2,   400000000,    0),
_CLK(CLK_M_MPESTAC_PG,    &clkgena2,   0,    0),
_CLK(CLK_M_MPESTAC_WC,    &clkgena2,   0,    0),
_CLK(CLK_M_MPEVTACAUX_PG,    &clkgena2,   0,    0),
_CLK(CLK_M_MPEVTACMAIN_PG,    &clkgena2,   0,    0),
_CLK(CLK_M_MPEVTACRX0_WC,    &clkgena2,   0,    0),
_CLK(CLK_M_MPEVTACRX1_WC,    &clkgena2,   0,    0),
_CLK(CLK_M_COMPO_MAIN,    &clkgena2,   400000000,    0),
_CLK(CLK_M_COMPO_AUX,    &clkgena2,   200000000,    0),
_CLK(CLK_M_BDISP_0,    &clkgena2,   400000000,    0),
_CLK(CLK_M_BDISP_1,    &clkgena2,   400000000,    0),
_CLK(CLK_M_ICN_BDISP,    &clkgena2,   320000000,    0),
_CLK(CLK_M_ICN_COMPO,    &clkgena2,   320000000,    0),
_CLK(CLK_M_ICN_VDP_2,    &clkgena2,   320000000,    0),
_CLK(CLK_M_ICN_REG_14,    &clkgena2,   200000000,    0),
_CLK(CLK_M_MDTP,    &clkgena2,   266666666,    0),
_CLK(CLK_M_JPEGDEC,    &clkgena2,   320000000,    0),
_CLK(CLK_M_DCEPHY_IMPCTRL,    &clkgena2,   30000000,    0),
_CLK(CLK_M_APB_PM_11,    &clkgena2,   200000000,    0),

/* Clockgen E */
_CLK(CLK_M_E_REF, &clkgene, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_E_FS_VCO, &clkgene, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_E_REF]),
_CLK_P(CLK_M_PIX_MDTP_0, &clkgene, 148500000,
	0, &clk_clocks[CLK_M_E_FS_VCO]),
_CLK_P(CLK_M_PIX_MDTP_1, &clkgene, 148500000,
	0, &clk_clocks[CLK_M_E_FS_VCO]),
_CLK_P(CLK_M_PIX_MDTP_2, &clkgene, 148500000,
	0, &clk_clocks[CLK_M_E_FS_VCO]),
_CLK_P(CLK_M_MPELPC, &clkgene, 50000000,
	0, &clk_clocks[CLK_M_E_FS_VCO]),

/* Clockgen F: Frequency Synthesizer */
_CLK(CLK_M_F_REF, &clkgenf, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_F_FS_VCO, &clkgenf, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_F_REF]),
_CLK_P(CLK_M_PIX_MAIN_VIDFS, &clkgenf, 297000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_F_FS_VCO]),
_CLK_P(CLK_M_HVA_FS, &clkgenf, 13500000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_F_FS_VCO]),
_CLK_P(CLK_M_FVDP_VCPU, &clkgenf, 350000000,
	0, &clk_clocks[CLK_M_F_FS_VCO]),
_CLK_P(CLK_M_FVDP_PROC_FS, &clkgenf, 333000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_F_FS_VCO]),

/* Clockgen F: Clocks from SAS */
_CLK(CLK_M_PIX_MAIN_SAS, &clkgenf, 0, CLK_RATE_PROPAGATES),
_CLK(CLK_M_PIX_AUX_SAS, &clkgenf, 0, CLK_RATE_PROPAGATES),
_CLK(CLK_M_PIX_HDMIRX_SAS, &clkgenf, 0, CLK_RATE_PROPAGATES),

/* Clockgen F: Clocks output from muxes */
_CLK(CLK_M_HVA, &clkgenf, 0, 0),
_CLK(CLK_M_FVDP_PROC, &clkgenf, 333000000, 0),
_CLK(CLK_M_F_VCC_HD, &clkgenf, 0, CLK_RATE_PROPAGATES),
_CLK(CLK_M_F_VCC_SD, &clkgenf, 0, CLK_RATE_PROPAGATES),

/* Clockgen F: Video Clock Controller */
_CLK(CLK_M_PIX_MAIN_PIPE, &clkgenf, 0, 0),
_CLK(CLK_M_PIX_AUX_PIPE, &clkgenf, 0, 0),
_CLK(CLK_M_PIX_MAIN_CRU, &clkgenf, 0, 0),
_CLK(CLK_M_PIX_AUX_CRU, &clkgenf, 0, 0),
_CLK(CLK_M_XFER_BE_COMPO, &clkgenf, 0, 0),
_CLK(CLK_M_XFER_PIP_COMPO, &clkgenf, 0, 0),
_CLK(CLK_M_XFER_AUX_COMPO, &clkgenf, 0, 0),
_CLK(CLK_M_VSENS, &clkgenf, 0, 0),
_CLK(CLK_M_PIX_HDMIRX_0, &clkgenf, 0, 0),
_CLK(CLK_M_PIX_HDMIRX_1, &clkgenf, 0, 0),

/* Clockgen DDR-subsystem */
_CLK(CLK_M_DDR_REF, &clkgenddr, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_DDR_IC_LMI0, &clkgenddr, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_DDR_REF]),
_CLK_P(CLK_M_DDR_IC_LMI1, &clkgenddr, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_DDR_REF]),
_CLK_P(CLK_M_DDR_DDR0, &clkgenddr, 1600000000,
		0, &clk_clocks[CLK_M_DDR_IC_LMI0]),
_CLK_P(CLK_M_DDR_DDR1, &clkgenddr, 1600000000,
		0, &clk_clocks[CLK_M_DDR_IC_LMI1]),

/* CA9 PLL */
_CLK(CLK_M_A9_REF, &clkgena9, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_A9_PHI0, &clkgena9, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_A9_REF]),
_CLK(CLK_M_A9, &clkgena9, 0, 0),
_CLK_P(CLK_M_A9_PERIPHS, &clkgena9, 0, 0, &clk_clocks[CLK_M_A9]),

/* MALI400/GPU PLL1200 */
_CLK(CLK_M_GPU_REF, &clkgengpu, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLK_M_GPU_PHI, &clkgengpu, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLK_M_GPU_REF]),
_CLK_P(CLK_M_GPU, &clkgengpu, 400000000,
	0, &clk_clocks[CLK_M_GPU_PHI]),
};

/* ========================================================================
   Name:        mpe42_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int __init mpe42_clk_init(struct clk *_sys_clk_in, struct clk *_sys_clkalt_in,
		struct clk *_pix_main_clk, struct clk *_pix_aux_clk,
		struct clk *_pix_hdmirx_clk)
{
	int ret, i;

	SataClock = _sys_clk_in;
	AltClock = _sys_clkalt_in;
#ifndef ST_OS21
	for (i = 0; i < 4; ++i) {
		fsynth_channels[fsynth_e][i].md =
			platform_sys_claim(8560 + i * 4, 0, 4);
		fsynth_channels[fsynth_e][i].pe =
			platform_sys_claim(8561 + i * 4, 0, 14);
		fsynth_channels[fsynth_e][i].sdiv =
			platform_sys_claim(8562 + i * 4, 0, 3);
		fsynth_channels[fsynth_e][i].nsdiv =
			platform_sys_claim(8559, 18 + i, 18 + i);
		fsynth_channels[fsynth_e][i].prog_en =
			platform_sys_claim(8563 + i * 4, 0, 0);
		fsynth_channels[fsynth_e][i].nsb =
			platform_sys_claim(8559, 10 + i, 10 + i);
	}
	fsynth_plls[fsynth_e].ndiv =
			platform_sys_claim(8559, 15, 17);
	fsynth_plls[fsynth_e].npdpll =
			platform_sys_claim(8559, 14, 14);

	for (i = 0; i < 4; ++i) {
		fsynth_channels[fsynth_f][i].md =
			platform_sys_claim(8543 + i * 4, 0, 4);
		fsynth_channels[fsynth_f][i].pe =
			platform_sys_claim(8544 + i * 4, 0, 14);
		fsynth_channels[fsynth_f][i].sdiv =
			platform_sys_claim(8545 + i * 4, 0, 3);
		fsynth_channels[fsynth_f][i].nsdiv =
			platform_sys_claim(8542, 18 + i, 18 + i);
		fsynth_channels[fsynth_f][i].prog_en =
			platform_sys_claim(8546 + i * 4, 0, 0);
		fsynth_channels[fsynth_f][i].nsb =
			platform_sys_claim(8542, 10 + i, 10 + i);
	}
	fsynth_plls[fsynth_f].ndiv =
			platform_sys_claim(8542, 15, 17);
	fsynth_plls[fsynth_f].npdpll =
			platform_sys_claim(8542, 14, 14);

	call_platform_sys_claim(7502, 0, 0);
	call_platform_sys_claim(7502, 25, 27);
	call_platform_sys_claim(7504, 0, 7);
	call_platform_sys_claim(7504, 8, 13);
	call_platform_sys_claim(7504, 14, 19);
	call_platform_sys_claim(7555, 0, 0);
	call_platform_sys_claim(7555, 1, 1);
	call_platform_sys_claim(7556, 0, 0);
	call_platform_sys_claim(7556, 1, 5);
	call_platform_sys_claim(7556, 25, 27);
	call_platform_sys_claim(7558, 0, 7);
	call_platform_sys_claim(7558, 8, 13);
	call_platform_sys_claim(7583, 0, 0);
	call_platform_sys_claim(8539, 0, 15);
	call_platform_sys_claim(8539, 16, 16);
	call_platform_sys_claim(8539, 17, 17);
	call_platform_sys_claim(8540, 0, 31);
	call_platform_sys_claim(8541, 0, 31);
	call_platform_sys_claim(8580, 0, 0);
	call_platform_sys_claim(9505, 0, 0);
	call_platform_sys_claim(9505, 1, 1);
	call_platform_sys_claim(9505, 2, 2);
	call_platform_sys_claim(9505, 3, 3);
	call_platform_sys_claim(9538, 0, 0);
#endif
	clk_clocks[CLK_M_A0_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_A1_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_A2_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_E_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_F_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_DDR_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_A9_REF].parent = _sys_clk_in;
	clk_clocks[CLK_M_GPU_REF].parent = _sys_clk_in;

	clk_clocks[CLK_M_PIX_MAIN_SAS].parent = _pix_main_clk;
	clk_clocks[CLK_M_PIX_AUX_SAS].parent = _pix_aux_clk;
	clk_clocks[CLK_M_PIX_HDMIRX_SAS].parent = _pix_hdmirx_clk;

	cga_base[0] = ioremap_nocache(CKGA0_BASE_ADDRESS, 0x1000);
	cga_base[1] = ioremap_nocache(CKGA1_BASE_ADDRESS, 0x1000);
	cga_base[2] = ioremap_nocache(CKGA2_BASE_ADDRESS, 0x1000);
	mali_base =  ioremap_nocache(SYS_MALI_BASE_ADDRESS, 0x1000);

#ifdef ST_OS21
	printf("Registering MPE42 clocks\n");
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	printf(" => done\n");
#else
	ret = clk_register_table(clk_clocks, CLK_M_E_REF, 1);

	ret |= clk_register_table(&clk_clocks[CLK_M_E_REF],
		ARRAY_SIZE(clk_clocks) - CLK_M_E_REF, 0);
#endif

	return ret;
}


