/*****************************************************************************
 *
 * File name   : clock-stid127.c (Alicante C1)
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
02/may/13 fabrice.charpentier@st.com
	  CCM code revisited.
27/mar/13 fabrice.charpentier@st.com
	  Some bug fixes running on VSOC.
15/mar/13 carmelo.amoroso@st.com
	  Build fix for first Linux integration due to undefined reference to
	  'platform_sys_claim' function
12/mar/13 fabrice.charpentier@st.com
	  Initial release
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"

#define CLKLLA_SYSCONF_UNIQREGS			1

#else /* Linux */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/clk.h>
#include <linux/stm/stid127.h>
#include <linux/stm/sysconf.h>


struct sysconf_field *stid127_platform_sys_claim(int _nr, int _lsb, int _msb)
{
	return sysconf_claim(SYSCONFG_GROUP(_nr),
		SYSCONF_OFFSET(_nr), _lsb, _msb, "Clk lla");
}


#endif

#include "clock-stid127.h"
#include "clock-regs-stid127.h"
#include "clock-oslayer.h"
#include "clk-common.h"

#define SYS_IFE_REF	27 /* IFE ref clock=27Mhz */

#ifndef ST_OS21

#else /* OS21 specific */

static sysconf_base_t sysconf_base[] = {
	{ 40, 99, SYSCFG_WEST },
	{ 200, 299, SYSCFG_SOUTH },
	{ 600, 699, SYSCFG_DOCSIS },
	{ 700, 799, SYSCFG_CPU },
	{ 900, 999, SYSCFG_HD },
	{ 1000, 1099, SYSCFG_PWEST },
	{ 1200, 1299, SYSCFG_PSOUTH },
	{ 1400, 1499, SYSCFG_PEAST },
	{ 0, 0, 0 }
};

#endif

/* Base addresses */
static void *cga_base[2];	/* ClockgenA base: A0, A1 */
static void *qfs660_base[2];	/* QFS base: TEL, DOC */

/* A9 sysconf */
SYSCONF(0, 722, 0, 0);
SYSCONF(0, 722, 1, 1);
SYSCONF(0, 722, 2, 2);
SYSCONF(0, 722, 3, 8);
SYSCONF(0, 722, 9, 16);
SYSCONF(0, 722, 17, 21);
SYSCONF(0, 722, 22, 24);
SYSCONF(0, 760, 0, 0);

/* Prototypes */
static struct clk clk_clocks[];

/******************************************************************************
CLOCKGEN Ax clocks groups. Common functions
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLK_A1_REF) ? 1 : 0);
}

/* Returns corresponding clockgen Ax base_addraddress for 'clk_id' */
static inline void *clkgenax_get_base_addr(int clk_id)
{
	return cga_base[clkgenax_get_bank(clk_id)];
}

/* Returns divN_cfg register offset */
static inline unsigned long clkgenax_div_cfg(int clk_src, int clk_idx)
{
	const unsigned short parent_offset[] = {
		CKGA_OSC_DIV0_CFG,	CKGA_PLL0HS_DIV0_CFG,
		CKGA_PLL0LS_DIV0_CFG,	CKGA_PLL1HS_DIV0_CFG,
		CKGA_PLL1LS_DIV0_CFG
	};

	return parent_offset[clk_src] + clk_idx * 4;
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
	case CLK_CT_DIAG ... CLK_A0_SPARE_31:
		idx = clkid - CLK_CT_DIAG;
		break;
	case CLK_A1_SPARE_0 ... CLK_A1_SPARE_31:
		idx = clkid - CLK_A1_SPARE_0;
		break;
	default:
		return -1;
	}
	*srcreg = CKGA_CLKOPSRC_SWITCH_CFG + (idx / 16) * 0x4;
	*shift = (idx % 16) * 2;

	return idx;
}

/* ========================================================================
   Name:        clkgenax_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_recalc(struct clk *clk)
{
	unsigned long data, ratio;
	unsigned long srcreg, offset, parent_rate;
	void *base_addr;
	int shift, err, idx;
	struct stm_pll pll = {
		.type = stm_pll1600c45,
	};

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	/* Reading clock programmed value */
	base_addr = clkgenax_get_base_addr(clk->id);
	parent_rate = clk_get_rate(clk_get_parent(clk));

	switch (clk->id) {
	case CLK_A0_REF:  /* Clockgen A reference clock */
	case CLK_A1_REF:  /* Clockgen A reference clock */
		clk->rate = parent_rate;
		break;
	case CLK_A0_PLL0HS:
	case CLK_A1_PLL0HS:
		#if !defined(CLKLLA_NO_PLL)
		pll.ndiv = CLK_READ(base_addr + CKGA_PLL_CFG(0, 0)) & 0xff;
		pll.idf = CLK_READ(base_addr + CKGA_PLL_CFG(0, 1)) & 0x7;
		return stm_clk_pll_get_rate(parent_rate, &pll, &clk->rate);
		#else
		if (clk->nominal_rate)
			clk->rate = clk->nominal_rate;
		else
			clk->rate = 12121212;
		return 0;
		#endif
		return err;
	case CLK_A0_PLL1HS:
	case CLK_A1_PLL1HS:
		#if !defined(CLKLLA_NO_PLL)
		pll.ndiv = CLK_READ(base_addr + CKGA_PLL_CFG(1, 0)) & 0xff;
		pll.idf = CLK_READ(base_addr + CKGA_PLL_CFG(1, 1)) & 0x7;
		return stm_clk_pll_get_rate(parent_rate, &pll, &clk->rate);
		#else
		if (clk->nominal_rate)
			clk->rate = clk->nominal_rate;
		else
			clk->rate = 12121212;
		return 0;
		#endif
		return err;
	case CLK_A0_PLL0LS:
	case CLK_A1_PLL0LS:
	case CLK_A0_PLL1LS:
	case CLK_A1_PLL1LS:
	case CLK_A9_EXT2FS_DIV2:
		clk->rate = parent_rate / 2;
		return 0;
	default:
		idx = clkgenax_get_index(clk->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to parent, let's write divider ratio */
		if (clk->parent->id >= CLK_A1_REF)
			offset = clkgenax_div_cfg(clk->parent->id -
				CLK_A1_REF, idx);
		else
			offset = clkgenax_div_cfg(clk->parent->id -
				CLK_A0_REF, idx);

		data =  CLK_READ(base_addr + offset);
		ratio = (data & 0x1F) + 1;
		clk->rate = parent_rate / ratio;
		return 0;
	}

	#if defined(CLKLLA_NO_PLL)
	if (clk->nominal_rate)
		clk->rate = clk->nominal_rate;
	else
		clk->rate = 12121212;
	#endif

	return 0;
}

/* ========================================================================
   Name:        clkgenax_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_parent(struct clk *clk, struct clk *src)
{
	unsigned long clk_src, val;
	int idx, shift;
	unsigned long srcreg;
	void *base_addr;

	if (!clk || !src)
		return CLK_ERR_BAD_PARAMETER;

	if (clk->id < CLK_CT_DIAG && clk->id > CLK_A0_SPARE_31 &&
	    clk->id < CLK_A1_SPARE_0 && clk->id > CLK_A1_SPARE_31)
		return CLK_ERR_BAD_PARAMETER;

	/* check if they are on the same bank */
	if (clkgenax_get_bank(clk->id) != clkgenax_get_bank(src->id))
		return CLK_ERR_BAD_PARAMETER;

	switch (src->id) {
	case CLK_A0_REF:
	case CLK_A1_REF:
		clk_src = 0;
		break;
	case CLK_A0_PLL0LS:
	case CLK_A0_PLL0HS:
	case CLK_A1_PLL0LS:
	case CLK_A1_PLL0HS:
		clk_src = 1;
		break;
	case CLK_A0_PLL1LS:
	case CLK_A0_PLL1HS:
	case CLK_A1_PLL1LS:
	case CLK_A1_PLL1HS:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = clkgenax_get_base_addr(clk->id);
	val = CLK_READ(base_addr + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(base_addr + srcreg, val);
	clk->parent = src;

	#if defined(CLKLLA_NO_PLL)
	/* If NO PLL means emulation like platform. Then HW may be forced in
	   a specific position preventing SW change */
	clkgenax_identify_parent(clk);
	#endif

	return clkgenax_recalc(clk);
}

/* ========================================================================
   Name:        clkgenax_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_identify_parent(struct clk *clk)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	unsigned long base_id;
	int shift;
	void *base_addr;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk->id >= CLK_A0_REF && clk->id <= CLK_A0_PLL1LS) ||
	    (clk->id >= CLK_A1_REF && clk->id <= CLK_A1_PLL1LS) ||
	     clk->id == CLK_A9_EXT2FS_DIV2)
		/* statically initialized */
		return 0;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	base_addr = clkgenax_get_base_addr(clk->id);
	base_id = ((clk->id >= CLK_A1_REF) ? CLK_A1_REF : CLK_A0_REF);
	src_sel = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	switch (src_sel) { /* 0=OSC, 1=PLL0 , 2=PLL1 , 3=STOP */
	case 0:
		clk->parent = &clk_clocks[base_id + 0];	/* CLK_Ax_REF */
		break;
	case 1:
		clk->parent = &clk_clocks[base_id +
			((idx <= 5) ? 1 : 2)];
		break;
	case 2:
		clk->parent = &clk_clocks[base_id +
			((idx <= 9) ? 3 : 4)];
		break;
	case 3:
		clk->parent = NULL;
		clk->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenax_pll_xable
   Description: Enable/disable PLL3200
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_pll_xable(struct clk *clk, int enable)
{
	unsigned long val;
	void *base_addr;
	int bit, err = 0;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_A0_PLL0LS:	/* all the PLL_LS return */
	case CLK_A0_PLL1LS:	/* CLK_ERR_FEATURE_NOT_SUPPORTED*/
	case CLK_A1_PLL0LS:
	case CLK_A1_PLL1LS:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_A0_PLL1HS:
	case CLK_A1_PLL1HS:
		bit = 1;
		break;
	default:
		bit = 0;
	}

	base_addr = clkgenax_get_base_addr(clk->id);
	val = CLK_READ(base_addr + CKGA_POWER_CFG);
	if (enable)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	CLK_WRITE(base_addr + CKGA_POWER_CFG, val);

	if (enable)
		err = clkgenax_recalc(clk);
	else
		clk->rate = 0;

	return err;
}

/* ========================================================================
   Name:        clkgenax_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_enable(struct clk *clk)
{
	int err;
	struct clk *parent_clk;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_A0_REF:
	case CLK_A1_REF:
	case CLK_A9_EXT2FS_DIV2:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_A0_PLL0HS ... CLK_A0_PLL1LS:
	case CLK_A1_PLL0HS ... CLK_A1_PLL1LS:
		return clkgenax_pll_xable(clk, 1);
	}

	/* Enabling means there setting the parent clock instead of "off".
	   If parent is undefined, let's select oscillator as default */
	if (clk->parent)
		return clkgenax_set_parent(clk, clk->parent);
	parent_clk = (clk->id >= CLK_A1_REF) ? &clk_clocks[CLK_A1_REF]
		      : &clk_clocks[CLK_A0_REF];
	err = clkgenax_set_parent(clk, parent_clk);
	/* clkgenax_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:        clkgenax_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_disable(struct clk *clk)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;
	void *base_addr;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power down */
	switch (clk->id) {
	case CLK_A0_REF:
	case CLK_A1_REF:
	case CLK_A9_EXT2FS_DIV2:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_A0_PLL0HS ... CLK_A0_PLL1LS:
	case CLK_A1_PLL0HS ... CLK_A1_PLL1LS:
		return clkgenax_pll_xable(clk, 0);
	}

	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	base_addr = clkgenax_get_base_addr(clk->id);
	val = CLK_READ(base_addr + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);     /* 3 = STOP clock */
	CLK_WRITE(base_addr + srcreg, val);
	clk->rate = 0;

	return 0;
}

/* ========================================================================
   Name:        clkgenax_set_div
   Description: Set divider ratio for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_div(struct clk *clk, unsigned long *div_p)
{
	int idx, clk_offset;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;
	void *base_addr;

	if (!clk || !clk->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = clkgenax_get_base_addr(clk->id);

	/* Now according to parent, let's write divider ratio */
	if (clk_get_parent(clk)->id >= CLK_A1_REF)
		clk_offset = clk_get_parent(clk)->id - CLK_A1_REF;
	else
		clk_offset = clk_get_parent(clk)->id - CLK_A0_REF;

	offset = clkgenax_div_cfg(clk_offset, idx);
	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;
	CLK_WRITE(base_addr + offset, div_cfg);

	return 0;
}

/* ========================================================================
   Name:        clkgenax_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenax_init(struct clk *clk)
{
	int err;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenax_identify_parent(clk);
	if (!err)
		err = clkgenax_recalc(clk);

	return err;
}

/* ========================================================================
   Name:        clkgenax_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_rate(struct clk *clk, unsigned long freq)
{
	unsigned long div;
	int err = 0;
	long deviation, new_deviation;
	void *base_addr;
	unsigned long parent_rate, data;
	struct stm_pll pll = {
		.type = stm_pll1600c45,
	};

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (clk->id < CLK_A0_PLL0HS || clk->id > CLK_A1_SPARE_31)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	base_addr = clkgenax_get_base_addr(clk->id);

	switch (clk->id) {
	case CLK_A0_PLL0HS:
	case CLK_A1_PLL0HS:
		err = stm_clk_pll_get_params(parent_rate, freq, &pll);
		if (err != 0)
			break;
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_addr + CKGA_PLL_CFG(0, 0))
				& 0xffffff00;
		data |= pll.ndiv;
		CLK_WRITE(base_addr + CKGA_PLL_CFG(0, 0), data);
		data = CLK_READ(base_addr + CKGA_PLL_CFG(0, 1))
				& 0xfffffff8;
		data |= pll.idf;
		CLK_WRITE(base_addr + CKGA_PLL_CFG(0, 1), data);
#endif
		break;
	case CLK_A0_PLL1HS:
	case CLK_A1_PLL1HS:
		err = stm_clk_pll_get_params(parent_rate, freq, &pll);
		if (err != 0)
			break;
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_addr + CKGA_PLL_CFG(1, 0))
				& 0xffffff80;
		data |= pll.ndiv;
		CLK_WRITE(base_addr + CKGA_PLL_CFG(1, 0), data);
		data = CLK_READ(base_addr + CKGA_PLL_CFG(1, 1))
				& 0xfffffff8;
		data |= pll.idf;
		CLK_WRITE(base_addr + CKGA_PLL_CFG(1, 1), data);
#endif
		break;
	case CLK_A0_PLL0LS:
	case CLK_A0_PLL1LS:
	case CLK_A1_PLL0LS:
	case CLK_A1_PLL1LS:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_CT_DIAG ... CLK_A0_SPARE_31:
	case CLK_A1_SPARE_0 ... CLK_A1_SPARE_31:
		div = parent_rate / freq;
		deviation = (parent_rate / div) - freq;
		new_deviation = (parent_rate / (div + 1)) - freq;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (new_deviation < deviation)
			div++;
		err = clkgenax_set_div(clk, &div);
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	if (!err)
		err = clkgenax_recalc(clk);
	return err;
}

/* ========================================================================
   Name:        clkgenax_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

#ifdef ST_OS21
static unsigned long clkgenax_get_measure(struct clk *clk)
{
	unsigned long src, data;
	unsigned long measure;
	void *base_addr;
	int i;

	if (!clk)
		return 0;

	switch (clk->id) {
	case CLK_CT_DIAG ... CLK_A9_EXT2FS_DIV2:
		src = clk->id - CLK_CT_DIAG;
		break;
	case CLK_A1_SPARE_0 ... CLK_A1_SPARE_31:
		src = clk->id - CLK_A1_SPARE_0;
		break;
	default:
		return 0;
	}

	if (src == 0xff)
		return 0;

	measure = 0;
	base_addr = clkgenax_get_base_addr(clk->id);

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(base_addr + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(base_addr + CKGA_CLKOBS_MUX0_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
		data = CLK_READ(base_addr + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break;	/* IT */
	}
	if (i == 10)
		return 0;

	/* Reading value */
	data = CLK_READ(base_addr + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = SYS_IFE_REF * data * 1000;

	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 3);

	return measure;
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
static int clkgenax_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long sel;
	unsigned long divcfg;
	unsigned long srcreg;
	int shift;
	void *base_addr;

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLK_CT_DIAG ... CLK_A9_EXT2FS_DIV2:
	case CLK_A1_SPARE_0 ... CLK_A1_SPARE_31:
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
	CLK_WRITE((base_addr + CKGA_CLKOBS_MUX0_CFG), (divcfg << 6) | (sel & 0x3f));

	/* Observation points:
	   A0 => PIO0[2] alt 3 (or PIO0[3])
	   A1 => PIO1[3] alt 3
	 */

	/* Configuring appropriate PIO */
	if (base_addr == cga_base[0]) {
		SYSCONF_WRITE(0, 1000, 8, 10, 3); /* Selecting alternate 3 */
		SYSCONF_WRITE(0, 1008, 2, 2, 1); /* Enabling IO */
	} else {
		SYSCONF_WRITE(0, 1001, 12, 14, 3);	/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 1008, 11, 11, 1); /* Enabling IO */
	}

	return 0;
}
#else
#define clkgenax_observe	NULL
#endif

/******************************************************************************
FS660 common functions
******************************************************************************/

/* Returns corresponding clockgen base address
CLOCKGEN TEL: QFS_TEL + CCM_TEL + CCM_USB + CCM_LPC
CLOCKGEN DOC: QFS_DOC + CCM_IFE + CSM
*/
static inline void *qfs660_get_base_addr(struct clk *clk)
{
	switch(clk->id) {
	case CLK_USB_SRC ... CLK_USB_REF:
	case CLK_THERMAL_SENSE ... CLK_LPC_COMMS:
		break;	/* Clockgen TEL */
	case CLK_DOC_VCO ... CLK_IFE_216_RC:
		return qfs660_base[1];
	}

	return qfs660_base[0];
}

/* Returns channel number */
static inline int qfs660_get_channel(struct clk *clk)
{
	if (clk->id >= CLK_DOC_REF)
		return clk->id - CLK_FP;
	if (clk->id >= CLK_TEL_REF)
		return clk->id - CLK_FDMA_TEL;

	return -1;
}

/* ========================================================================
   Name:        qfs660_recalc()
   Description: Update "struct clk" from HW status for QuadFS-FS660
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int qfs660_recalc(struct clk *clk)
{
	int err = 0;
	unsigned long setup, pwr, cfg;
	int channel = 0;
	unsigned long parent_rate;
	void *base_addr;
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco,
	};
	struct stm_fs fs = {
		.type = stm_fs660c32,
	};
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

#if !defined(CLKLLA_NO_PLL)
	base_addr = qfs660_get_base_addr(clk);

	/* Checking FSYN analog status */
	pwr = CLK_READ(base_addr + QFS_PWR);
	if ((pwr & 0x1) != 0) {
		/* FSx_PWR[0] = Analog power down : PLL power down*/
		clk->rate = 0;
		return 0;
	}
	/* At least analog part (PLL660) is running */

	parent_rate = clk_get_rate(clk_get_parent(clk));
	/* If VCO clocks, let's compute and return */
	if ((clk->id == CLK_USB_SRC) || (clk->id == CLK_DOC_VCO)) {
		setup = CLK_READ(base_addr + QFS_SETUP);
		fs_vco.ndiv = ((setup >> 1) & 0x7);
		return stm_clk_fs_get_rate(parent_rate, &fs_vco,
				&clk->rate);
	}

	channel = qfs660_get_channel(clk);

	/* Checking FSYN digital part */
	if ((pwr & (1 << (1 + channel))) != 0) {
		/* FSx_PWR[1+channel]  = digital part in standby */
		clk->rate = 0;
		return 0;
	}

	/* FSYN up & running. Computing frequency */
	cfg = CLK_READ(base_addr + QFS_FSX_CFG(channel));
	fs.mdiv = ((cfg >> 15) & 0x1f);	/* FSx_CFG[19:15] */
	fs.pe = (cfg & 0x7fff);		/* FSx_CFG[14:0] */
	fs.sdiv = ((cfg >> 21) & 0xf);	/* FSx_CFG[24:21] */
	fs.nsdiv = ((cfg >> 26) & 0x1);	/* FSx_CFG[26] */
	err = stm_clk_fs_get_rate(parent_rate, &fs, &clk->rate);
#else
	if (clk->nominal_rate)
		clk->rate = clk->nominal_rate;
	else
		clk->rate = 12121212;
#endif

	return err;
}

/* ========================================================================
   Name:        qfs660_set_rate
   Description: Set FS660 clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int qfs660_set_rate(struct clk *clk, unsigned long freq)
{
	unsigned long cfg, setup, pwr, val = 0;
	int channel = 0;
	unsigned long parent_rate, opclk, refclk;
	void *base_addr;
	struct stm_fs fs_vco = {
		.type = stm_fs660c32vco,
	};
	struct stm_fs fs = {
		.type = stm_fs660c32,
		.nsdiv = 0xff,
	};

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

#if !defined(CLKLLA_NO_PLL)
	base_addr = qfs660_get_base_addr(clk);
	opclk = CLK_READ(base_addr + QFS_OPCLKCMN);
	setup = CLK_READ(base_addr + QFS_SETUP);
	pwr = CLK_READ(base_addr + QFS_PWR);
	refclk = CLK_READ(base_addr + QFS_REFCLKSEL);

	parent_rate = clk_get_rate(clk_get_parent(clk));

	if (clk->id == CLK_USB_SRC || clk->id == CLK_DOC_VCO) {
		if (stm_clk_fs_get_params(parent_rate, freq, &fs_vco))
			return CLK_ERR_BAD_PARAMETER;
		/* Power down PLL */
		CLK_WRITE(base_addr + QFS_PWR, (pwr | 0x1));
		CLK_WRITE(base_addr + QFS_SETUP,
			  ((setup & ~(0xe)) | (fs_vco.ndiv << 1)));
		/* Power up PLL */
		CLK_WRITE(base_addr + QFS_PWR, (pwr & ~(0x1)));
		/* Enable output clock driven by FS */
		CLK_WRITE(base_addr + QFS_OPCLKCMN, opclk & ~(0x1));
		return qfs660_recalc(clk);
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	if (stm_clk_fs_get_params(parent_rate, freq, &fs))
		return CLK_ERR_BAD_PARAMETER;

	channel = qfs660_get_channel(clk);

	/* Removing digital reset, digital standby */
	/* FS_SETUP[4 + channel] = digital reset remove  */
	CLK_WRITE(base_addr + QFS_SETUP, setup | (0x10 << channel));
	/* FSx_PWR[1+channel] = digital standby remove */
	CLK_WRITE(base_addr + QFS_PWR, (pwr & ~(0x2 << channel)));

	cfg = CLK_READ(base_addr + QFS_FSX_CFG(channel));
	cfg &= ~(0x1 << 26 | 0x1 << 20 | 0xf << 21 | 0x1f << 15 | 0x7fff);
	val = (fs.nsdiv << 26 | fs.sdiv << 21 | fs.mdiv << 15 | fs.pe);
	/* Enable FS programming */
	val |= 0x1 << 20;
	cfg |= val;

	CLK_WRITE(base_addr + QFS_FSX_CFG(channel), cfg);
	/* Toggle EN_PRG */
	CLK_WRITE(base_addr + QFS_FSX_CFG(channel), cfg & ~(0x1 << 20));
	/* Enable output clock driven by FS */
	CLK_WRITE(base_addr + QFS_OPCLKCMN, (opclk & ~(0x2 << channel)));

#endif
	return qfs660_recalc(clk);
}

/* ========================================================================
   Name:        qfs660_xable
   Description: Enable/Disable FSYN. If all channels OFF, FSYN is powered
		down.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int qfs660_xable(struct clk *clk, unsigned long enable)
{
	unsigned long pwr, setup;
	int channel = 0;
	void *base_addr;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_TEL_REF:
	case CLK_DOC_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_USB_SRC ... CLK_PAD_OUT:
	case CLK_DOC_VCO ... CLK_TSOUT_SRC:
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	base_addr = qfs660_get_base_addr(clk);

	channel = qfs660_get_channel(clk);

	pwr = CLK_READ(base_addr + QFS_PWR);
	setup = CLK_READ(base_addr + QFS_SETUP);

	if (clk->id == CLK_USB_SRC || clk->id == CLK_DOC_VCO) {
		/* ANALOG part */
		if (enable)
			pwr &= ~(0x1);	/* PLL power up */
		else
			pwr |= 0x1;	/* PLL power down */
		CLK_WRITE(base_addr + QFS_PWR, pwr);
	} else {
		/* DIGITAL part */
		if (enable) {
			/* digital part of FS power up */
			pwr &= ~(1 << (1 + channel));
			/* digital part of FS active */
			setup |= (1 << (4 + channel));
		} else {
			/* digital part of FS reset */
			setup &= ~(1 << (4 + channel));
			/* digital part of FS power down */
			pwr |= (1 << (1 + channel));
		}
		/* FS_SETUP[4 + channel] = digital reset */
		CLK_WRITE(base_addr + QFS_SETUP, setup);
		/* FSx_PWR[1 + channel] = digital standby */
		CLK_WRITE(base_addr + QFS_PWR, pwr);
	}

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return qfs660_recalc(clk);
	else
		clk->rate = 0;
	return 0;
}

/******************************************************************************
CCM (Clock Control Module) common functions
******************************************************************************/

/* Returns CCM output channel for given clock.
   0=div1, 1=div2, 2=div4, 3=divx, or CLK_ERR_BAD_PARAMETER */
static inline int ccm_get_channel(struct clk *clk)
{
	switch(clk->id) {
	case CLK_ZSI_TEL:
	case CLK_IFE_216:
		return 0;	/* div1 */
	case CLK_IFE_108:
		return 1;	/* div2 */
	case CLK_IFE_54:
	case CLK_THERMAL_SENSE:
		return 2;	/* div4 */
	case CLK_ZSI_APPL:
	case CLK_USB_REF:
	case CLK_LPC_COMMS:
		return 3;	/* divx */
	default:
		break;
	}

	return -1;
}

/* Returns CCM GPOUT ID
   TEL: CCM_TEL=0/gpoutA, CCM_USB=1/gpoutB, CCM_LPC=3/gpoutD
   DOC: CCM_IFE=0/gpoutA
 */
static inline int ccm_get_gpout_id(struct clk *clk)
{
	switch(clk->id) {
	case CLK_ZSI_TEL ... CLK_ZSI_APPL:
	case CLK_IFE_216 ... CLK_IFE_54:
		return 0;	/* gpoutA */
	case CLK_USB_REF:
		return 1;	/* gpoutB */
	case CLK_THERMAL_SENSE ... CLK_LPC_COMMS:
		return 3;	/* gpoutD */
	default:
		break;
	}

	return CLK_ERR_BAD_PARAMETER;
}

/* Update "struct clk" rate from HW status */
static int ccm_recalc(struct clk *clk)
{
	unsigned long parent_rate, data;
	int channel, gpout_id;
	void *base_addr;

	channel = ccm_get_channel(clk);
	if (channel == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = qfs660_get_base_addr(clk);
	gpout_id = ccm_get_gpout_id(clk);
	data = CLK_READ(base_addr + QFS_GPOUTX(gpout_id));
	parent_rate = clk_get_rate(clk_get_parent(clk));

	/* DIV1K_EN */
	if (data & (1 << 16)) {
		parent_rate /= 1000;
	} else {
		/* GATE_CTL[7:0] */
		switch ((data >> 8) & 0xff) {
		case 0:
			parent_rate = 0; /* Stopped */
			break;
		case 1 ... 255:
			parent_rate /= ((data >> 8) & 0xff);
			break;
		}
	}

	/* 'parent_rate' is now gated divider output */

	/* Channel: 0=div1, 1=div2, 2=div4, 3=divx */
	if (channel != 3) {
		/*
		 * fixed ratio...
		 */
		clk->rate = parent_rate >> channel;
		return 0;
	}

	/*
	 * channel == 3 is a programmable channel (divx)
	 */

	/* DIVX_CTL[7:0] */
	switch (data & 0xff) {
	case 0:
		clk->rate = parent_rate / 256;
		break;
	case 1:
		clk->rate = 0;
		break;
	case 2 ... 255:
		clk->rate = parent_rate / (data & 0xff);
		break;
	}

	return 0;
}

/* Set CCM internal div to get requested output freq */
static int ccm_set_rate(struct clk *clk, unsigned long freq)
{
	int channel, gpout_id;
	unsigned long parent_rate;
	unsigned long gate, divx;
	void *base_addr;

	channel = ccm_get_channel(clk);
	if (channel == -1)
		return CLK_ERR_BAD_PARAMETER;
	if (channel != 3) /* 0=div1, 1=div2, 2=div4, 3=divx */
		return CLK_ERR_BAD_PARAMETER;

	gpout_id = ccm_get_gpout_id(clk);
	parent_rate = clk_get_rate(clk_get_parent(clk));
	base_addr = qfs660_get_base_addr(clk);

	if (clk->id == CLK_ZSI_APPL) {
		gate = 1;
		divx = parent_rate / freq;
	} else if (clk->id == CLK_USB_REF) {
		/*
		 * USB PLL ref clk input's 12/24/48Mhz selection
		48Mhz => div by 11 => 49.09Mhz
		24Mhz => div by 22 => 24.54Mhz
		12Mhz => div by 45
		 */
		gate = 1;
		divx = parent_rate / freq;
	} else if (clk->id == CLK_LPC_COMMS) {
		gate = 50;
		parent_rate /= 50;
		divx = parent_rate / freq;
	} else
		return CLK_ERR_BAD_PARAMETER;

	/* divx=1 would mean STOP not bypass => ERROR */
	if (divx == 1)
		return CLK_ERR_BAD_PARAMETER;

	CLK_WRITE(base_addr + QFS_GPOUTX(gpout_id), (1 << 17) | (gate << 8) | divx);
	CLK_WRITE(base_addr + QFS_GPOUTT, (0x3 << (gpout_id * 2)));

	return ccm_recalc(clk);
}

/* Enable/disable CCM clock.
   2 levels of control:
     at 'gate' but common to all channels,
     or at 'divx' and specific per channel. */
static int ccm_xable(struct clk *clk, int enable)
{
	unsigned long data;
	int channel, gpout_id;
	void *base_addr;

	/* Channel: 0=div1, 1=div2, 2=div4, 3=divx */
	channel = ccm_get_channel(clk);
	if (channel == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = qfs660_get_base_addr(clk);
	gpout_id = ccm_get_gpout_id(clk);
	data = CLK_READ(base_addr + QFS_GPOUTX(gpout_id));

	/* DIVX_CTL[7:0] */
	if (enable) {
		unsigned long gate, divx;

		/* Do not touch programmed ratios. */
		divx = data & 0xff;
		gate = (data >> 8) & 0xff;
		/* Gate: STOP replaced by BYPASS (div by 1) */
		if (gate == 0)
			gate = 1;
		/* DivX: STOP replaced by div by 2 */
		if (divx == 1)
			divx = 2;
		data &= ~0xffff;
		data |= ((gate << 8) | divx);
	} else {
		/* Setting divx=STOP (1) */
		data &= ~0xff;
		data |= 1;
	}
	CLK_WRITE(base_addr + QFS_GPOUTX(gpout_id), data);

	return ccm_recalc(clk);
}

/******************************************************************************
CSM (Clock Stop Module) common functions
******************************************************************************/

/* Returns GDPOUT id and corresponding bit to control STOP and SLOW sates.
   ID: 0=GPOUTA, 1=GPOUTB, 2=GPOUTC, 3=GPOUTD, 4=GPOUTE */
static int csm_get_gpout(struct clk *clk, int *stopid, int *slowid, int *bit)
{
	switch(clk->id) {
	case CLK_IFE_54_DQAM0 ... CLK_IFE_54_DQAM15:
		*stopid = 3; /* fsx_gpoutd */
		*slowid = 4; /* fsx_gpoute */
		*bit = clk->id - CLK_IFE_54_DQAM0;
		break;
	case CLK_IFE_54_QPSK ... CLK_IFE_54_RC:
		*stopid = 0; /* fsx_gpouta */
		*slowid = 1; /* fsx_gpoutb */
		*bit = (clk->id - CLK_IFE_54_QPSK) + 20;
		break;
	case CLK_IFE_54_DOCSIS ... CLK_IFE_54_D3HS:
		*stopid = 0; /* fsx_gpouta */
		*slowid = 1; /* fsx_gpoutb */
		*bit = (clk->id - CLK_IFE_54_DOCSIS) + 29;
		break;
	case CLK_IFE_108_DQAM0 ... CLK_IFE_108_DQAM15:
		*stopid = 3; /* fsx_gpoutd */
		*slowid = 4; /* fsx_gpoute */
		*bit = (clk->id - CLK_IFE_108_DQAM0) + 16;
		break;
	case CLK_IFE_216_DOCSIS ... CLK_IFE_216_FP:
		*stopid = 0; /* fsx_gpouta */
		*slowid = 1; /* fsx_gpoutb */
		*bit = (clk->id - CLK_IFE_216_DOCSIS) + 23;
		break;
	case CLK_IFE_216_RC:
		*stopid = 0; /* fsx_gpouta */
		*slowid = 1; /* fsx_gpoutb */
		*bit = 27;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* Update "struct clk" rate from CSM HW status */
static int csm_recalc(struct clk *clk)
{
	int stopid, slowid, bit;
	unsigned long parent_rate, status;
	void *base_addr;

	if (csm_get_gpout(clk, &stopid, &slowid, &bit) != 0)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = qfs660_get_base_addr(clk);
	status = ((CLK_READ(base_addr + QFS_GPOUTX(stopid)) >> bit) << 1) & 2;
	status |= ((CLK_READ(base_addr + QFS_GPOUTX(slowid)) >> bit) & 1);
	parent_rate = clk_get_rate(clk_get_parent(clk));

	/* CSM bits config
	stop_clock = 0, slow_clock = 0 ==> bypass
	stop_clock = 0, slow_clock = 1 ==> div by 1000 mode
	stop_clock = 1, slow_clock = 0 ==> clock is stopped
	stop_clock = 1, slow_clock = 1 ==> div by 2 mode
	*/

	switch(status) {
	case 0:	/* Bypass */
		clk->rate = parent_rate;
		break;
	case 1:	/* Div by 1000 */
		clk->rate = parent_rate / 1000;
		break;
	case 2:	/* Stopped */
		clk->rate = 0;
		break;
	case 3:	/* Div by 2 */
		clk->rate = parent_rate / 2;
		break;
	}

	return 0;
}

/* Change CSM HW config to change rate */
static int csm_set_rate(struct clk *clk, unsigned long freq)
{
	int stopid, slowid, bit;
	unsigned long parent_rate, div, stop_clock, slow_clock, val;
	void *base_addr;

	if (csm_get_gpout(clk, &stopid, &slowid, &bit) != 0)
		return CLK_ERR_BAD_PARAMETER;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	div = clk_best_div(parent_rate, freq);
	base_addr = qfs660_get_base_addr(clk);

	/* CSM bits config
	stop_clock = 0, slow_clock = 0 ==> bypass
	stop_clock = 0, slow_clock = 1 ==> div by 1000 mode
	stop_clock = 1, slow_clock = 0 ==> clock is stopped
	stop_clock = 1, slow_clock = 1 ==> div by 2 mode
	*/

	switch(div) {
	case 1:	/* Bypass */
		stop_clock = slow_clock = 0;
		break;
	case 2:	/* Div by 2 */
		stop_clock = slow_clock = 1;
		break;
	case 1000:	/* Div by 1000 */
		stop_clock = 0;
		slow_clock = 1;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	val = CLK_READ(base_addr + QFS_GPOUTX(stopid)) & ~(1 << bit);
	val |= stop_clock << bit;
	CLK_WRITE(base_addr + QFS_GPOUTX(stopid), val);
	val = CLK_READ(base_addr + QFS_GPOUTX(slowid)) & ~(1 << bit);
	val |= slow_clock << bit;
	CLK_WRITE(base_addr + QFS_GPOUTX(slowid), val);

	return csm_recalc(clk);
}

/* Enable/disable CSM clock */
static int csm_xable(struct clk *clk, int enable)
{
	int stopid, slowid, bit;
	unsigned long stop_clock, slow_clock, val;
	void *base_addr;

	if (csm_get_gpout(clk, &stopid, &slowid, &bit) != 0)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = qfs660_get_base_addr(clk);

	/* CSM bits config
	stop_clock = 0, slow_clock = 0 ==> bypass
	stop_clock = 0, slow_clock = 1 ==> div by 1000 mode
	stop_clock = 1, slow_clock = 0 ==> clock is stopped
	stop_clock = 1, slow_clock = 1 ==> div by 2 mode
	*/

	slow_clock = 0;
	if (enable)
		/* Defaulting to bypass mode */
		stop_clock = 0;
	else
		stop_clock = 1;

	val = CLK_READ(base_addr + QFS_GPOUTX(stopid)) & ~(1 << bit);
	val |= stop_clock << bit;
	CLK_WRITE(base_addr + QFS_GPOUTX(stopid), val);
	val = CLK_READ(base_addr + QFS_GPOUTX(slowid)) & ~(1 << bit);
	val |= slow_clock << bit;
	CLK_WRITE(base_addr + QFS_GPOUTX(slowid), val);

	return csm_recalc(clk);
}

/******************************************************************************
CLOCKGEN TEL
******************************************************************************/

/* ========================================================================
   Name:        clkgentel_recalc
   Description: Update "struct clk" from HW status
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgentel_recalc(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_TEL_REF:
		clk->rate = clk_get_rate(clk_get_parent(clk));
		break;
	case CLK_USB_SRC ... CLK_PAD_OUT:
		return qfs660_recalc(clk);
	case CLK_ZSI_TEL ... CLK_USB_REF:
		return ccm_recalc(clk);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgentel_init
   Description: Identify clock source and update struct from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgentel_init(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Only 1 possible source clock.
           No parent to identify */

	return clkgentel_recalc(clk);
}

/* ========================================================================
   Name:        clkgentel_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgentel_enable(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_TEL_REF:
	case CLK_ZSI_TEL: /* CCM div 1, can't xable */
		break;
	case CLK_USB_SRC ... CLK_PAD_OUT:
		return qfs660_xable(clk, 1);
	case CLK_ZSI_APPL ... CLK_USB_REF:
		return ccm_xable(clk, 1);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return CLK_ERR_FEATURE_NOT_SUPPORTED;
}

/* ========================================================================
   Name:        clkgentel_disable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgentel_disable(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_TEL_REF:
	case CLK_ZSI_TEL: /* CCM div 1, can't xable */
		break;
	case CLK_USB_SRC ... CLK_PAD_OUT:
		return qfs660_xable(clk, 0);
	case CLK_ZSI_APPL ... CLK_USB_REF:
		return ccm_xable(clk, 0);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return CLK_ERR_FEATURE_NOT_SUPPORTED;
}

/* ========================================================================
   Name:        clkgentel_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgentel_set_rate(struct clk *clk, unsigned long freq)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	switch(clk->id) {
	case CLK_USB_SRC ... CLK_PAD_OUT:
		return qfs660_set_rate(clk, freq);
	case CLK_ZSI_TEL: /* CCM div1 output, can't configure */
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_ZSI_APPL ... CLK_USB_REF:
		return ccm_set_rate(clk, freq);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	/* No recalc required there since done by qfs660_set_rate() or
	   ccm_set_rate() */

	return 0;
}

/* ========================================================================
   Name:        clkgentel_observe
   Description: Clocks observation function for TEL
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgentel_observe(clk_t *clk_p, unsigned long *div_p)
{

	return 0;
}
#else
#define clkgentel_observe	NULL
#endif

/******************************************************************************
CLOCKGEN DOC
******************************************************************************/

/* ========================================================================
   Name:        clkgendoc_recalc
   Description: Update "struct clk" from HW status
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgendoc_recalc(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_DOC_REF:
		clk->rate = clk_get_rate(clk_get_parent(clk));
		break;
	case CLK_DOC_VCO ... CLK_TSOUT_SRC:
		return qfs660_recalc(clk);
	case CLK_IFE_216 ... CLK_IFE_54:
		return ccm_recalc(clk);
	case CLK_IFE_54_DQAM0 ... CLK_IFE_216_RC:
		return csm_recalc(clk);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgendoc_init
   Description: Identify clock source and update struct from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgendoc_init(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Only 1 possible source clock.
           No parent to identify */

	return clkgendoc_recalc(clk);
}

/* ========================================================================
   Name:        clkgendoc_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgendoc_enable(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_DOC_REF:
		break;
	case CLK_DOC_VCO ... CLK_TSOUT_SRC:
		return qfs660_xable(clk, 1);
	case CLK_IFE_216 ... CLK_IFE_54:
		return ccm_xable(clk, 1);
	case CLK_IFE_54_DQAM0 ... CLK_IFE_216_RC:
		return csm_xable(clk, 1);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgendoc_disable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgendoc_disable(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_DOC_REF:
		break;
	case CLK_DOC_VCO ... CLK_TSOUT_SRC:
		return qfs660_xable(clk, 0);
	case CLK_IFE_216 ... CLK_IFE_54:
		/* Can't disable CCM div1..4 outputs independantly */
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_IFE_54_DQAM0 ... CLK_IFE_216_RC:
		return csm_xable(clk, 0);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgendoc_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgendoc_set_rate(struct clk *clk, unsigned long freq)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	switch(clk->id) {
	case CLK_DOC_VCO ... CLK_TSOUT_SRC:
		return qfs660_set_rate(clk, freq);
	case CLK_IFE_216 ... CLK_IFE_54:
		return ccm_set_rate(clk, freq);
	case CLK_IFE_54_DQAM0 ... CLK_IFE_216_RC:
		return csm_set_rate(clk, freq);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	/* No recalc required there since done by qfs660_set_rate(),
	   ccm_set_rate(), or csm_set_rate() */

	return 0;
}

/* ========================================================================
   Name:        clkgendoc_observe
   Description: Clocks observation function for DOC
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgendoc_observe(clk_t *clk_p, unsigned long *div_p)
{

	return 0;
}
#else
#define clkgendoc_observe	NULL
#endif

/******************************************************************************
CLOCKGEN LPC
******************************************************************************/

/* ========================================================================
   Name:        clkgenlpc_recalc
   Description: Update "struct clk" from HW status
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenlpc_recalc(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_LPC_REF:
		clk->rate = clk_get_rate(clk_get_parent(clk));
		break;
	case CLK_THERMAL_SENSE ... CLK_LPC_COMMS:
		return ccm_recalc(clk);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenlpc_init
   Description: Identify clock source and update struct from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenlpc_init(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Only 1 possible source clock.
           No parent to identify */

	return clkgenlpc_recalc(clk);
}

/* ========================================================================
   Name:        clkgenlpc_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenlpc_set_rate(struct clk *clk, unsigned long freq)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	switch(clk->id) {
	case CLK_LPC_COMMS:
		return ccm_set_rate(clk, freq);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	/* No recalc required there since done by qfs660_set_rate() or
	   ccm_set_rate() */

	return 0;
}

/******************************************************************************
CLOCKGEN A9
******************************************************************************/

/* ========================================================================
   Name:        clkgena9_recalc
   Description: Get clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_recalc(struct clk *clk)
{
	unsigned long parent_rate;
	unsigned long vco_by2;
	struct stm_pll pll = {
		.type = stm_pll1600c45phi,
	};

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	switch (clk->id) {
	case CLK_A9_REF:
	case CLK_A9:
		clk->rate = parent_rate;
		break;
	case CLK_A9_PERIPHS:
		clk->rate = parent_rate / 2;
		break;
	case CLK_A9_PHI0:
		#if !defined(CLKLLA_NO_PLL)
		pll.idf = SYSCONF_READ(0, 722, 22, 24);
		pll.ndiv = SYSCONF_READ(0, 722, 9, 16);
		pll.odf = SYSCONF_READ(0, 722, 3, 8);
		if (!pll.odf)
			pll.odf = 1;
		if (SYSCONF_READ(0, 722, 0, 0))
			clk->rate = 0;	/* PLL disabled */
		else {
			stm_clk_pll_get_rate(parent_rate, &pll,  &vco_by2);
			clk->rate = vco_by2 / pll.odf;
		}
		#endif
		break;
	}

	#if defined(CLKLLA_NO_PLL)
	if (clk->nominal_rate)
		clk->rate = clk->nominal_rate;
	else
		clk->rate = 12121212;
	#endif

	return 0;
}

/* ========================================================================
   Name:        clkgena9_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_identify_parent(struct clk *clk)
{
	if (clk->id != CLK_A9) /* Other clocks have static parent */
		return 0;

	/* Is CA9 clock sourced from PLL or A0-25 ? */
	if (SYSCONF_READ(0, 722, 2, 2))
		clk->parent = &clk_clocks[
			(SYSCONF_READ(0, 722, 1, 1) ?
				CLK_A9_EXT2FS : CLK_A9_EXT2FS_DIV2)];
	else
		clk->parent = &clk_clocks[CLK_A9_PHI0];

	return 0;
}

/* ========================================================================
   Name:        clkgena9_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena9_init(struct clk *clk)
{
	int err;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgena9_identify_parent(clk);
	if (!err)
		err = clkgena9_recalc(clk);

	return err;
}

/* ========================================================================
   Name:        clkgena9_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_set_rate(struct clk *clk, unsigned long freq)
{
	int err = 0;
	struct stm_pll pll = {
		.type = stm_pll1600c45phi,
	};

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	if (clk->id == CLK_A9)
		return clkgena9_set_rate(clk->parent, freq);
	if (clk->id != CLK_A9_PHI0)
		return CLK_ERR_BAD_PARAMETER;

	err = stm_clk_pll_get_params(clk->parent->rate, freq, &pll);
	if (err != 0)
		return err;

	#if !defined(CLKLLA_NO_PLL)
	SYSCONF_WRITE(0, 722, 2, 2, 1);	/* Bypassing PLL */

	SYSCONF_WRITE(0, 722, 0, 0, 1); /* Disabling PLL */
	SYSCONF_WRITE(0, 722, 22, 24, pll.idf); /* IDF */
	SYSCONF_WRITE(0, 722, 17, 21, pll.cp); /* Charge Pump */
	SYSCONF_WRITE(0, 722, 9, 16, pll.ndiv); /* NDIV */
	SYSCONF_WRITE(0, 722, 3, 8, pll.odf); /* ODF: set to default value "1" */
	SYSCONF_WRITE(0, 722, 0, 0, 0); /* Reenabling PLL */
	/* Now wait for lock */
	while (!SYSCONF_READ(0, 760, 0, 0));
	/*
	 * Can't put any delay because may rely on a clock that is currently
	 * changing (running from CA9 case).
	 */

	SYSCONF_WRITE(0, 722, 2, 2, 0);		/* Selecting internal PLL */
	#endif
	return clkgena9_recalc(clk);
}

/* ========================================================================
   Clocks groups declaration
   ======================================================================== */

_CLK_OPS(clkgena0,
	"A0",
	clkgenax_init,
	clkgenax_set_parent,
	clkgenax_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"?"		/* Observation point */
);
_CLK_OPS(clkgena1,
	"A1",
	clkgenax_init,
	clkgenax_set_parent,
	clkgenax_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"?"		/* Observation point */
);
_CLK_OPS(clkgentel,
	"TEL",
	clkgentel_init,
	NULL,		/* set parent */
	clkgentel_set_rate,
	clkgentel_recalc,
	clkgentel_enable,
	clkgentel_disable,
	clkgentel_observe,
	NULL,
	"?"		/* Observation point */
);
_CLK_OPS(clkgendoc,
	"DOC",
	clkgendoc_init,
	NULL,		/* set parent */
	clkgendoc_set_rate,
	clkgendoc_recalc,
	clkgendoc_enable,
	clkgendoc_disable,
	clkgendoc_observe,
	NULL,
	"?"		/* Observation point */
);
_CLK_OPS(clkgenlpc,
	"LPC",
	clkgenlpc_init,
	NULL,		/* set parent */
	clkgenlpc_set_rate,
	clkgenlpc_recalc,
	NULL,		/* enable */
	NULL,		/* disable */
	NULL,
	NULL,
	"?"		/* Observation point */
);
_CLK_OPS(clkgena9,
	"CA9",
	clkgena9_init,
	NULL,
	clkgena9_set_rate,
	clkgena9_recalc,
	NULL,
	NULL,
	NULL,		/* Observation */
	NULL,		/* No measure function */
	"?"		/* No observation point */
);

/* Physical clocks description */
static struct clk clk_clocks[] = {
/* Top level clocks */
_CLK_FIXED(CLK_IFE_REF, SYS_IFE_REF * 1000000, CLK_ALWAYS_ENABLED),

/* Clockgen A0 */
_CLK_P(CLK_A0_REF, &clkgena0, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_A0_PLL0HS, &clkgena0, 0, 0, &clk_clocks[CLK_A0_REF]),
_CLK_P(CLK_A0_PLL0LS, &clkgena0, 0, 0, &clk_clocks[CLK_A0_PLL0HS]),
_CLK_P(CLK_A0_PLL1HS, &clkgena0, 0, 0, &clk_clocks[CLK_A0_REF]),
_CLK_P(CLK_A0_PLL1LS, &clkgena0, 0, 0, &clk_clocks[CLK_A0_PLL1HS]),

_CLK(CLK_CT_DIAG, &clkgena0,    0,    0),
_CLK(CLK_FDMA_0, &clkgena0,    0,    0),
_CLK(CLK_FDMA_1, &clkgena0,    0,    0),
_CLK(CLK_IC_CM_ST40, &clkgena0,    0,    0),
_CLK(CLK_IC_SPI, &clkgena0,    0,    0),
_CLK(CLK_IC_CPU, &clkgena0,    0,    0),
_CLK(CLK_IC_MAIN, &clkgena0,    0,    0),
_CLK(CLK_IC_ROUTER, &clkgena0,    0,    0),
_CLK(CLK_IC_PCIE, &clkgena0,    0,    0),
_CLK(CLK_IC_LP, &clkgena0,    0,    0),
_CLK(CLK_IC_LP_CPU, &clkgena0,    0,    0),
_CLK(CLK_IC_STFE, &clkgena0,    0,    0),
_CLK(CLK_IC_DMA, &clkgena0,    0,    0),
_CLK(CLK_IC_GLOBAL_ROUTER, &clkgena0,    0,    0),
_CLK(CLK_IC_GLOBAL_PCI, &clkgena0,    0,    0),
_CLK(CLK_IC_GLOBAL_PCI_TARG, &clkgena0,    0,    0),
_CLK(CLK_IC_GLOBAL_NETWORK, &clkgena0,    0,    0),
_CLK(CLK_A9_TRACE_INT, &clkgena0,    0,    0),
_CLK(CLK_A9_EXT2FS, &clkgena0,    0,    0),
_CLK(CLK_IC_LP_D3, &clkgena0,    0,    0),
_CLK(CLK_IC_LP_DQAM, &clkgena0,    0,    0),
_CLK(CLK_IC_LP_ETH, &clkgena0,    0,    0),
_CLK(CLK_IC_LP_HD, &clkgena0,    0,    0),
_CLK(CLK_IC_SECURE, &clkgena0,    0,    0),

_CLK_P(CLK_A9_EXT2FS_DIV2, &clkgena0, 0, 0,
	&clk_clocks[CLK_A9_EXT2FS]),

/* Clockgen A1 */
_CLK_P(CLK_A1_REF, &clkgena1, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_A1_PLL0HS, &clkgena1, 0, 0, &clk_clocks[CLK_A1_REF]),
_CLK_P(CLK_A1_PLL0LS, &clkgena1, 0, 0, &clk_clocks[CLK_A1_PLL0HS]),
_CLK_P(CLK_A1_PLL1HS, &clkgena1, 0, 0, &clk_clocks[CLK_A1_REF]),
_CLK_P(CLK_A1_PLL1LS, &clkgena1, 0, 0, &clk_clocks[CLK_A1_PLL1HS]),

_CLK(CLK_IC_DDR,	&clkgena1,   0,    0),

/* Clockgen TEL: Quad FS */
_CLK_P(CLK_TEL_REF, &clkgentel, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_USB_SRC, &clkgentel, 540000000, 0,
		&clk_clocks[CLK_TEL_REF]),
_CLK_P(CLK_FDMA_TEL, &clkgentel, 0, 0,
		&clk_clocks[CLK_USB_SRC]),
_CLK_P(CLK_ZSI, &clkgentel, 49152000, 0,
		&clk_clocks[CLK_USB_SRC]),
_CLK_P(CLK_ETH0, &clkgentel, 0, 0,
		&clk_clocks[CLK_USB_SRC]),
_CLK_P(CLK_PAD_OUT, &clkgentel, 0, 0,
		&clk_clocks[CLK_USB_SRC]),

/* Clockgen TEL: CCM */
_CLK_P(CLK_ZSI_TEL, &clkgentel, 49152000, 0,
		&clk_clocks[CLK_ZSI]),
_CLK_P(CLK_ZSI_APPL, &clkgentel, 0, 0,
		&clk_clocks[CLK_ZSI]),
_CLK_P(CLK_USB_REF, &clkgentel, 0, 0,
		&clk_clocks[CLK_USB_SRC]),

/* Clockgen DOC: Quad FS */
_CLK_P(CLK_DOC_REF, &clkgendoc, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_DOC_VCO, &clkgendoc, 0, 0,
		&clk_clocks[CLK_DOC_REF]),
_CLK_P(CLK_FP, &clkgendoc, 0, 0,
		&clk_clocks[CLK_DOC_VCO]),
_CLK_P(CLK_D3_XP70, &clkgendoc, 0, 0,
		&clk_clocks[CLK_DOC_VCO]),
_CLK_P(CLK_IFE, &clkgendoc, 216000000, 0,
		&clk_clocks[CLK_DOC_VCO]),
_CLK_P(CLK_TSOUT_SRC, &clkgendoc, 0, 0,
		&clk_clocks[CLK_DOC_VCO]),

/* Clockgen DOC: CCM */
_CLK_P(CLK_IFE_216, &clkgendoc, 216000000, 0, &clk_clocks[CLK_IFE]),
_CLK_P(CLK_IFE_108, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE]),
_CLK_P(CLK_IFE_54, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE]),

/* Clockgen DOC: CSM */
_CLK_P(CLK_IFE_54_DQAM0, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM1, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM2, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM3, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM4, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM5, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM6, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM7, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM8, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM9, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM10, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM11, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM12, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM13, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM14, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DQAM15, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_QPSK, &clkgendoc,54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_RC, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_DOCSIS, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_54_D3HS, &clkgendoc, 54000000, 0, &clk_clocks[CLK_IFE_54]),
_CLK_P(CLK_IFE_108_DQAM0, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM1, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM2, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM3, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM4, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM5, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM6, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM7, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM8, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM9, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM10, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM11, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM12, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM13, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM14, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_108_DQAM15, &clkgendoc, 108000000, 0, &clk_clocks[CLK_IFE_108]),
_CLK_P(CLK_IFE_216_D3HS, &clkgendoc, 216000000, 0, &clk_clocks[CLK_IFE_216]),
_CLK_P(CLK_IFE_216_DOCSIS, &clkgendoc, 216000000, 0, &clk_clocks[CLK_IFE_216]),
_CLK_P(CLK_IFE_216_FP, &clkgendoc, 216000000, 0, &clk_clocks[CLK_IFE_216]),
_CLK_P(CLK_IFE_216_RC, &clkgendoc, 216000000, 0, &clk_clocks[CLK_IFE_216]),

/* LPC */
_CLK_P(CLK_LPC_REF, &clkgenlpc, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_THERMAL_SENSE, &clkgenlpc, 135000, 0, &clk_clocks[CLK_LPC_REF]),
_CLK_P(CLK_LPC_COMMS, &clkgenlpc, 0, 0, &clk_clocks[CLK_LPC_REF]),

/* CA9 PLL */
_CLK_P(CLK_A9_REF, &clkgena9, 0, CLK_ALWAYS_ENABLED,
	&clk_clocks[CLK_IFE_REF]),
_CLK_P(CLK_A9_PHI0, &clkgena9, 0, 0, &clk_clocks[CLK_A9_REF]),
_CLK(CLK_A9, &clkgena9, 0, 0),
_CLK_P(CLK_A9_PERIPHS, &clkgena9, 0, 0, &clk_clocks[CLK_A9]),
};

/* ========================================================================
   Name:	plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int __init stid127_plat_clk_init()
{
	int ret;
	platform_sys_claim = stid127_platform_sys_claim;

	/* Base addresses */
	cga_base[0] = ioremap_nocache(CKGA0_BASE_ADDRESS, 0x1000);
	cga_base[1] = ioremap_nocache(CKGA1_BASE_ADDRESS, 0x1000);
	qfs660_base[0] = ioremap_nocache(QFS_TEL_ADDRESS, 0x1000);
	qfs660_base[1] = ioremap_nocache(QFS_DOC_ADDRESS, 0x1000);

	/* A9 sysconf */
	call_platform_sys_claim(722, 0, 0);
	call_platform_sys_claim(722, 1, 1);
	call_platform_sys_claim(722, 2, 2);
	call_platform_sys_claim(722, 3, 8);
	call_platform_sys_claim(722, 9, 16);
	call_platform_sys_claim(722, 22, 24);
	call_platform_sys_claim(760, 0, 0);

#ifdef ST_OS21
	printf("Registering STiD127 clocks\n");
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	printf(" => done\n");
#else
	ret = clk_register_table(clk_clocks, CLK_TEL_REF, 1);
	ret |= clk_register_table(&clk_clocks[CLK_TEL_REF],
				ARRAY_SIZE(clk_clocks) - CLK_TEL_REF, 0);
#endif

	return ret;
}
