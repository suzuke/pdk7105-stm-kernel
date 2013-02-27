/*****************************************************************************
 *
 * File name   : clock-stig125.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2012 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
26/jun/12 ravinder-dlh.singh@st.com - francesco.virlinzi@st.com
	  extended VCC clock support
13/jun/12 ravinder-dlh.singh@st.com - francesco.virlinzi@st.com
	  extened ccm clocks support
	  added VCC observation
05/jun/12 ravinder-dlh.singh@st.com - francesco.virlinzi@st.com
	  A lot of fixes on ccm clocks
29/may/12 francesco.virlinzi@st.com
	  Added all the ccm clocks with related parent.
25/may/12 ravinder-dlh.singh@st.com
	  Some updates/fixes for clock configurations
22/may/12 francesco.virlinzi@st.com
	  General review for integration in stlinux
	  Merged code for clock_gen B/C/D/E/F which are managed as bank
	  Added dedicated clk_ops for the clock_gen_B_VCC
14/may/12 ravinder-dlh.singh@st.com
	  Preliminary version
*/

/* Co-emulation support:
   CLKLLA_NO_PLL  => RTL (emulation or co-emulation) where only PLL/FSYN are
		     not present. The rest of the logic is there.
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

#include "clock-stxsasc1.h"
#include "clock-regs-stxsasc1.h"
#include "clock-oslayer.h"
#include "clock-common.h"

#ifdef ST_OS21
static sysconf_base_t sysconf_base[] = {
	{ 0, 58, SYS_CFG_WEST },
	{ 200, 285, SYS_CFG_SOUTH },
	{ 400, 435, SYS_CFG_NORTH },
	{ 700, 771, SYS_CFG_CPU },
	{ 900, 946, SYS_CFG_HD }
};
#endif

static struct clk clk_clocks[];

static void *cga_base[2];
static void *cgb_vcc_base;
static void *cg_freq_synth_base[5];
static void *ife_phy_prog_base;

/* ClockgenB-VCC sysconf */
SYSCONF(0, 901, 0, 3);		/* video clock control and debug */
SYSCONF(0, 901, 4, 4);		/* video clock control and debug */
SYSCONF(0, 902, 0, 15);		/* video clock enable  */
SYSCONF(0, 903, 0, 31);		/* video clock selection */
SYSCONF(0, 904, 0, 31);		/* video clock divider */

/* A9 sysconf */
SYSCONF(0, 722, 0, 0);
SYSCONF(0, 722, 1, 1);
SYSCONF(0, 722, 2, 2);
SYSCONF(0, 722, 3, 8);
SYSCONF(0, 722, 9, 16);
SYSCONF(0, 722, 22, 24);
SYSCONF(0, 760, 0, 0);

static int clkgenax_recalc(struct clk *clk);

/******************************************************************************
CLOCKGEN Ax clocks groups
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLK_S_A1_REF) ? 1 : 0);
}

/* Returns corresponding clockgen Ax base address for 'clk_id' */
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
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgenax_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	switch (clkid) {
	case CLK_S_A0_CT_DIAG ... CLK_S_A0_A9_EXT2F:
		idx = clkid - CLK_S_A0_CT_DIAG;
		break;
	case CLK_S_A1_STAC_TX_PHY ... CLK_S_A1_IC_LP_ETH:
		idx = clkid - CLK_S_A1_STAC_TX_PHY;
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

static int clkgenax_set_parent(struct clk *clk, struct clk *src)
{
	unsigned long clk_src, val;
	int idx, shift;
	unsigned long srcreg;
	void *base;

	if (!clk || !src)
		return CLK_ERR_BAD_PARAMETER;

	if (clk->id < CLK_S_A0_CT_DIAG && clk->id > CLK_S_A0_A9_EXT2F &&
	    clk->id < CLK_S_A1_STAC_TX_PHY && clk->id > CLK_S_A1_IC_LP_ETH)
		return CLK_ERR_BAD_PARAMETER;

	/* check if they are on the same bank */
	if (clkgenax_get_bank(clk->id) != clkgenax_get_bank(src->id))
		return CLK_ERR_BAD_PARAMETER;

	switch (src->id) {
	case CLK_S_A0_REF:
	case CLK_S_A1_REF:
		clk_src = 0;
		break;
	case CLK_S_A0_PLL0LS:
	case CLK_S_A0_PLL0HS:
	case CLK_S_A1_PLL0LS:
	case CLK_S_A1_PLL0HS:
		clk_src = 1;
		break;
	case CLK_S_A0_PLL1LS:
	case CLK_S_A0_PLL1HS:
	case CLK_S_A1_PLL1LS:
	case CLK_S_A1_PLL1HS:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base = clkgenax_get_base_addr(clk->id);
	val = CLK_READ(base + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(base + srcreg, val);
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

	if ((clk->id >= CLK_S_A0_REF && clk->id <= CLK_S_A0_PLL1LS) ||
	    (clk->id >= CLK_S_A1_REF && clk->id <= CLK_S_A1_PLL1LS) ||
	     clk->id == CLK_S_A0_A9_EXT2F_DIV2)
		/* statically initialized */
		return 0;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	base_addr = clkgenax_get_base_addr(clk->id);
	base_id = ((clk->id >= CLK_S_A1_REF) ? CLK_S_A1_REF : CLK_S_A0_REF);
	src_sel = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	switch (src_sel) { /* 0=OSC, 1=PLL0 , 2=PLL1 , 3=STOP */
	case 0:
		clk->parent = &clk_clocks[base_id + 0];	/* CLK_S_Ax_REF */
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
   Name:        clkgenax_init
   Description: Read HW status to initialize 'struct clk' structure.
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
   Name:        clkgenax_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_xable_pll(struct clk *clk, int enable)
{
	unsigned long val;
	void *base_addr;
	int bit, err = 0;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_A0_PLL0LS:	/* all the PLL_LS return */
	case CLK_S_A0_PLL1LS:	/* CLK_ERR_FEATURE_NOT_SUPPORTED*/
	case CLK_S_A1_PLL0LS:
	case CLK_S_A1_PLL1LS:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_S_A0_PLL1HS:
	case CLK_S_A1_PLL1HS:
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

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	if (!clk->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_A0_REF:
	case CLK_S_A1_REF:
	case CLK_S_A0_A9_EXT2F_DIV2:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	/* all the PLLs */
	case CLK_S_A0_PLL0HS ... CLK_S_A0_PLL1LS:
	case CLK_S_A1_PLL0HS ... CLK_S_A1_PLL1LS:
		return clkgenax_xable_pll(clk, 1);
	default:
	err = clkgenax_set_parent(clk, clk->parent);
	}
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
	void *base_address;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power down */
	switch (clk->id) {
	case CLK_S_A0_REF:
	case CLK_S_A1_REF:
	case CLK_S_A0_A9_EXT2F_DIV2:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_S_A0_PLL0HS ... CLK_S_A0_PLL1LS:
	case CLK_S_A1_PLL0HS ... CLK_S_A1_PLL1LS:
		return clkgenax_xable_pll(clk, 0);
	}

	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	base_address = clkgenax_get_base_addr(clk->id);
	val = CLK_READ(base_address + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);     /* 3 = STOP clock */
	CLK_WRITE(base_address + srcreg, val);
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
	void *base_address;

	if (!clk || !clk->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_address = clkgenax_get_base_addr(clk->id);

	/* Now according to parent, let's write divider ratio */
	if (clk_get_parent(clk)->id >= CLK_S_A1_REF)
		clk_offset = clk_get_parent(clk)->id - CLK_S_A1_REF;
	else
		clk_offset = clk_get_parent(clk)->id - CLK_S_A0_REF;

	offset = clkgenax_div_cfg(clk_offset, idx);
	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;
	CLK_WRITE(base_address + offset, div_cfg);

	return 0;
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
#if !defined(CLKLLA_NO_PLL)
	unsigned long idf, ndiv;
#endif

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	/* Reading clock programmed value */
	base_addr = clkgenax_get_base_addr(clk->id);
	parent_rate = clk_get_rate(clk_get_parent(clk));

	switch (clk->id) {
	case CLK_S_A0_REF:  /* Clockgen A reference clock */
	case CLK_S_A1_REF:  /* Clockgen A reference clock */
		clk->rate = parent_rate;
		break;
	case CLK_S_A0_PLL0HS:
	case CLK_S_A1_PLL0HS:
		#if !defined(CLKLLA_NO_PLL)
		ndiv = CLK_READ(base_addr + CKGA_PLL_CFG(0, 0)) & 0xff;
		idf = CLK_READ(base_addr + CKGA_PLL_CFG(0, 1)) & 0x7;
		return clk_pll1600c45_get_rate(parent_rate,
				idf, ndiv, &clk->rate);
		#else
		if (clk->nominal_rate)
			clk->rate = clk->nominal_rate;
		else
			clk->rate = 12121212;
		return 0;
		#endif
		return err;
	case CLK_S_A0_PLL1HS:
	case CLK_S_A1_PLL1HS:
		#if !defined(CLKLLA_NO_PLL)
		ndiv = CLK_READ(base_addr + CKGA_PLL_CFG(1, 0)) & 0xff;
		idf = CLK_READ(base_addr + CKGA_PLL_CFG(1, 1)) & 0x7;
		return clk_pll1600c45_get_rate(parent_rate,
				idf, ndiv, &clk->rate);
		#else
		if (clk->nominal_rate)
			clk->rate = clk->nominal_rate;
		else
			clk->rate = 12121212;
		return 0;
		#endif
		return err;
	case CLK_S_A0_PLL0LS:
	case CLK_S_A1_PLL0LS:
	case CLK_S_A0_PLL1LS:
	case CLK_S_A1_PLL1LS:
	case CLK_S_A0_A9_EXT2F_DIV2:
		clk->rate = parent_rate / 2;
		return 0;
	default:
		idx = clkgenax_get_index(clk->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to parent, let's write divider ratio */
		if (clk->parent->id >= CLK_S_A1_REF)
			offset = clkgenax_div_cfg(clk->parent->id -
				CLK_S_A1_REF, idx);
		else
			offset = clkgenax_div_cfg(clk->parent->id -
				CLK_S_A0_REF, idx);

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
   Name:        clkgenax_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgenax_observe(struct clk *clk, unsigned long *div_p)
{
	unsigned long sel;
	void *base_addr;
	unsigned long divcfg;
	unsigned long srcreg;
	int shift;

	if (!clk || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_A0_CT_DIAG ... CLK_S_A0_A9_EXT2F:
	case CLK_S_A1_STAC_TX_PHY ... CLK_S_A1_IC_LP_ETH:
		sel = clkgenax_get_index(clk->id, &srcreg, &shift);
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
	base_addr = clkgenax_get_base_addr(clk->id);
	CLK_WRITE((base_addr + CKGA_CLKOBS_MUX0_CFG),
		(divcfg << 6) | (sel & 0x3f));

	/* Observation points:
	   A0 => CLKGENA0_CLK0: AltFunc3 of PIO2[5]
	   A1 => CLKGENA1_CLK0: AltFunc4 of PIO22[6]
	 */

	/* Configuring appropriate PIO */
	if (base_addr == cga_base[0]) {
		SYSCONF_WRITE(0, 2, 20, 22, 3);	 /* Selecting alternate 3 */
		SYSCONF_WRITE(0, 10, 21, 21, 1); /* Output Enable */
		SYSCONF_WRITE(0, 16, 21, 21, 0); /* Open drain */
		SYSCONF_WRITE(0, 13, 21, 21, 0); /* pull up */
	} else {
		SYSCONF_WRITE(0, 403, 24, 26, 4); /* Selecting alternate 4 */
		SYSCONF_WRITE(0, 404, 30, 30, 1); /* Output Enable */
		SYSCONF_WRITE(0, 406, 30, 30, 0); /* Open drain */
		SYSCONF_WRITE(0, 405, 30, 30, 0); /* pull up */
	}
	return 0;
}
#else
#define	clkgenax_observe	NULL
#endif

/* ========================================================================
   Name:        clkgenax_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_rate(struct clk *clk, unsigned long freq)
{
	unsigned long div, idf, ndiv, cp;
	int err = 0;
	long deviation, new_deviation;
	void *base_address;
	unsigned long parent_rate;
#if !defined(CLKLLA_NO_PLL)
	unsigned long data;
#endif

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (clk->id < CLK_S_A0_PLL0HS || clk->id > CLK_S_A1_IC_LP_ETH)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	base_address = clkgenax_get_base_addr(clk->id);

	switch (clk->id) {
	case CLK_S_A0_PLL0HS:
	case CLK_S_A1_PLL0HS:
		err = clk_pll1600c45_get_params(parent_rate,
			freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_address + CKGA_PLL_CFG(0, 0))
				& 0xffffff00;
		data |= ndiv;
		CLK_WRITE(base_address + CKGA_PLL_CFG(0, 0), data);
		data = CLK_READ(base_address + CKGA_PLL_CFG(0, 1))
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(base_address + CKGA_PLL_CFG(0, 1), data);
#endif
		break;
	case CLK_S_A0_PLL1HS:
	case CLK_S_A1_PLL1HS:
		err = clk_pll1600c45_get_params(parent_rate, freq,
			&idf, &ndiv, &cp);
		if (err != 0)
			break;
#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_address + CKGA_PLL_CFG(1, 0))
				& 0xffffff80;
		data |= ndiv;
		CLK_WRITE(base_address + CKGA_PLL_CFG(1, 0), data);
		data = CLK_READ(base_address + CKGA_PLL_CFG(1, 1))
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(base_address + CKGA_PLL_CFG(1, 1), data);
#endif
		break;
	case CLK_S_A0_PLL0LS:
	case CLK_S_A0_PLL1LS:
	case CLK_S_A1_PLL0LS:
	case CLK_S_A1_PLL1LS:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_S_A0_CT_DIAG ... CLK_S_A0_A9_EXT2F:
	case CLK_S_A1_STAC_TX_PHY ... CLK_S_A1_IC_LP_ETH:
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
	void *base;
	int i;

	if (!clk)
		return 0;

	switch (clk->id) {
	case CLK_S_A0_CT_DIAG ... CLK_S_A0_A9_EXT2F:
		src = clk->id - CLK_S_A0_CT_DIAG;
		break;
	case CLK_S_A1_STAC_TX_PHY ... CLK_S_A1_IC_LP_ETH:
		src = clk->id - CLK_S_A1_STAC_TX_PHY;
		break;
	default:
		return 0;
	}

	if (src == 0xff)
		return 0;

	measure = 0;
	base = clkgenax_get_base_addr(clk->id);

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(base + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(base + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(base + CKGA_CLKOBS_MUX0_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(base + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
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
}
#else
#define clkgenax_get_measure	NULL
#endif

/******************************************************************************
CLOCKGEN B - VCC (video/tango)
******************************************************************************/

/* ========================================================================
   Name:        clkgen_vcc_recalc
   Description: Update Video Clock Controller outputs value
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_vcc_recalc(struct clk *clk)
{
	int chan;
	unsigned long val;

	if (clk->id == CLK_S_B_TMDS) {
		clk->rate = clk_get_rate(clk_get_parent(clk));
		return 0;
	}

	chan = clk->id - CLK_S_B_PIX_MAIN;
	/* Is the channel stopped ? */
	val = (SYSCONF_READ(0, 902, 0, 15) >> chan) & 1;
	if (val) {
		clk->rate = 0;
		return 0;
	}

	/* What is the divider ratio ? */
	val = (SYSCONF_READ(0, 904, 0, 31) >> (chan * 2)) & 3;
	clk->rate = clk_get_rate(clk_get_parent(clk)) / (1 << val);

	return 0;
}


/* ========================================================================
   Name:        clkgen_vcc_set_div
   Description: Get Video Clocks Controller clocks divider function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_vcc_set_div(struct clk *clk, unsigned long *div_p)
{
	int chan;
	unsigned long set, val;
	static const unsigned char div_table[] = {
		/* 1  2     3  4     5     6     7  8 */
		   0, 1, 0xff, 2, 0xff, 0xff, 0xff, 3 };

	chan = clk->id - CLK_S_B_PIX_MAIN;
	if (*div_p < 1 || *div_p > 8)
		return CLK_ERR_BAD_PARAMETER;

	set = div_table[*div_p - 1];
	if (set == 0xff)
		return CLK_ERR_BAD_PARAMETER;

	/* Set SYSTEM_CONFIG904: div_mode, 2bits per channel */
	val = SYSCONF_READ(0, 904, 0, 31);
	val &= ~(3 << (chan * 2));
	val |= set << (chan * 2);
	SYSCONF_WRITE(0, 904, 0, 31, val);

	return 0;
}

/* ========================================================================
   Name:        clkgen_vcc_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static struct clk *vcc_parents[4] = {
	&clk_clocks[CLK_S_B_HD],
	&clk_clocks[CLK_S_B_SD],
	&clk_clocks[CLK_S_B_TMDS],
	&clk_clocks[CLK_S_CH34REF_DIV_2]
};

static int clkgen_vcc_identify_parent(struct clk *clk)
{
	unsigned long chan, val;

	if (clk->id == CLK_S_B_TMDS)
		return 0;

	chan = clk->id - CLK_S_B_PIX_MAIN;
	val = SYSCONF_READ(0, 903, 0, 31);
	val >>= (chan * 2);
	val &= 0x3;
	/* sel : 00 clk_hd,
	 *	 01 clk_sd,
	 *	 10 clk_tmds_hdms,
	 *	 11 clk_ch34ref_div2
	 */
	clk->parent = vcc_parents[val];

	return 0;
}

/* ========================================================================
   Name:        clkgen_vcc_set_parent
   Description: Set parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_vcc_set_parent(struct clk *clk, struct clk *parent_p)
{
	unsigned long chan, val, data;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks from "Video Clock Controller". */
	/* sel : 00 clk_hd,
	 *	 01 clk_sd,
	 *	 10 clk_tmds_hdms,
	 *	 11 clk_ch34ref_div2
	 */
	chan = clk->id - CLK_S_B_PIX_MAIN;

	for (val = 0; val < ARRAY_SIZE(vcc_parents); ++val)
		if (parent_p == vcc_parents[val])
			break;

	if (val == ARRAY_SIZE(vcc_parents))
		return CLK_ERR_BAD_PARAMETER;

	data = SYSCONF_READ(0, 903, 0, 31);
	data &= ~(0x3 << (chan * 2));
	data |= (val << (chan * 2));
	SYSCONF_WRITE(0, 903, 0, 31, data);
	clk->parent = parent_p;

	return clkgen_vcc_recalc(clk);
}

/* ========================================================================
   Name:        clkgenb_set_rate
   Description: Set CKGB clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_vcc_set_rate(struct clk *clk, unsigned long freq)
{
	int err = 0;
	unsigned long div;
	unsigned long parent_rate;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	/* Video Clock Controller clocks */
	div = clk_best_div(parent_rate, freq);
	err = clkgen_vcc_set_div(clk, &div);

	/* Recomputing freq from real HW status */
	return clkgen_vcc_recalc(clk);
}


/* ========================================================================
   Name:        clkgen_vcc_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgen_vcc_init(struct clk *clk)
{
	int err;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgen_vcc_identify_parent(clk);
	if (!err)
		err = clkgen_vcc_recalc(clk);

	return err;
}

/* ========================================================================
   Name:        clkgenb_xable_clock
   Description: Enable/disable clock (Clockgen B)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_vcc_xable_clock(struct clk *clk, unsigned long enable)
{
	unsigned long bit, data;
	int err = 0;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks from "Video Clock Controller".
	 * STOP clock controlled thru SYSTEM_CONFIG 903 of bank SYSCFG_HD
	 */
	data = SYSCONF_READ(0, 902, 0, 15);
	bit = clk->id - CLK_S_B_PIX_MAIN;
	if (enable)
		data &= ~(1 << bit);
	else
		data |= (1 << bit);
	SYSCONF_WRITE(0, 902, 0, 15, data);

	if (enable)
		err = clkgen_vcc_recalc(clk);
	else
		clk->rate = 0;

	return err;
}


/* ========================================================================
   Name:        clkgen_vcc_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgen_vcc_enable(struct clk *clk)
{
	return clkgen_vcc_xable_clock(clk, 1);
}

/* ========================================================================
   Name:        clkgen_vcc_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgen_vcc_disable(struct clk *clk)
{
	return clkgen_vcc_xable_clock(clk, 0);
}

/* ========================================================================
   Name:        clkgen_vcc_observe
   Description: Clockgen B clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgen_vcc_observe(struct clk *clk, unsigned long *div_p)
{
	unsigned long channel, je_ctrl;
	unsigned long base_addr;
	int div;	/* final_div = (1 << div) */

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation points:
	 * VCC channels => PIO22[5] alt 4
	 */
	base_addr = (unsigned long)cg_freq_synth_base[0];

	channel = clk->id - CLK_S_B_PIX_MAIN;
	SYSCONF_WRITE(0, 901, 0, 3, channel);

	if (*div_p < 1 || *div_p > 7)
		return CLK_ERR_BAD_PARAMETER;

	div = (*div_p - 1);

	je_ctrl = CLK_READ(base_addr + CKG_FS_JE_CTRL0);
	je_ctrl &= ~(0x1 << 27 | 0x7 << 24  | 0xf << 4 | 0x3 << 2);

	/* VCC outputs are on channel 7 of JE_CNTRL */
	je_ctrl |=  (0x1 << 27 | div << 24 | 7 << 4 | 0x2 << 2);

	CLK_WRITE(base_addr + CKG_FS_JE_CTRL0, je_ctrl);

	/* Configuring corresponding PIO (PIO22[5]) */
	SYSCONF_WRITE(0, 403, 20, 22, 4);/* Selecting alternate 4 */
	SYSCONF_WRITE(0, 404, 29, 29, 1);/* Output Enable */
	SYSCONF_WRITE(0, 406, 29, 29, 0);/* Open drain */
	SYSCONF_WRITE(0, 405, 29, 29, 0);/* pull up */

	return 0;
}
#else
#define clkgen_vcc_observe	NULL
#endif

/******************************************************************************
CLOCKGEN B (video/tango)	seen as bank 0
CLOCKGEN C (audio)		seen as bank 1
CLOCKGEN D (Telephony)		seen as bank 2
CLOCKGEN E (docsis)		seen as bank 3
CLOCKGEN F (misc)		seen as bank 4
******************************************************************************/
static inline int clkgen_freq_synth_get_bank(struct clk *clk)
{
	if (clk->id >= CLK_S_F_REF)
		return 4;
	if (clk->id >= CLK_S_E_REF)
		return 3;
	if (clk->id >= CLK_S_D_REF)
		return 2;
	if (clk->id >= CLK_S_C_REF)
		return 1;
	if (clk->id >= CLK_S_B_REF)
		return 0;
	if (clk->id >= CLK_S_CH34REF_DIV_1)
		return 1;
}

/* Returns corresponding clockgen Ax base address for 'clk_id' */
static inline void *clkgen_freq_synth_get_base_addr(struct clk *clk)
{
	return cg_freq_synth_base[clkgen_freq_synth_get_bank(clk)];
}

static inline int clkgen_freq_synth_get_channel(struct clk *clk)
{
	if (clk->id >= CLK_S_F_DSS)
		return clk->id - CLK_S_F_DSS;
	if (clk->id >= CLK_S_E_FP)
		return clk->id - CLK_S_E_FP;
	if (clk->id >= CLK_S_D_FDMA_TEL)
		return clk->id - CLK_S_D_FDMA_TEL;
	if (clk->id >= CLK_S_C_PCM0)
		return clk->id - CLK_S_C_PCM0;
	if (clk->id >= CLK_S_B_TP)
		return clk->id - CLK_S_B_TP;
	return -1;
}

/* ========================================================================
   Name:        clkgen_freq_synth_fsyn_recalc
   Description: Get CKG FSYN clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_freq_synth_fsyn_recalc(struct clk *clk)
{
	int err = 0;
	unsigned long setup, pwr, cfg;
	unsigned long pe, md, sdiv, ndiv, nsdiv;
	int channel = 0;
	unsigned long parent_rate;
	void *base_address;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

#if !defined(CLKLLA_NO_PLL)
	base_address = clkgen_freq_synth_get_base_addr(clk);

	/* Checking FSYN analog status */
	pwr = CLK_READ(base_address + CKG_FS_PWR);
	if ((pwr & 0x1) != 0) {
		/* FSx_PWR[0] = Analog power down : PLL power down*/
		clk->rate = 0;
		return 0;
	}
	/* At least analog part (PLL660) is running */

	setup = CLK_READ(base_address + CKG_FS_SETUP);
	ndiv = ((setup >> 1) & 0x7);

	parent_rate = clk_get_rate(clk_get_parent(clk));
	switch (clk->id) {
	case CLK_S_B_VCO:
	case CLK_S_C_VCO:
	case CLK_S_D_VCO:
	case CLK_S_E_VCO:
	case CLK_S_F_VCO:
		return clk_fs660c32_vco_get_rate(parent_rate, ndiv,
				&clk->rate);
	default:
		channel = clkgen_freq_synth_get_channel(clk);
		break;
	}

	/* Checking FSYN digital part */
	if ((pwr & (1 << (1 + channel))) != 0) {
		/* FSx_PWR[1+channel]  = digital part in standby */
		clk->rate = 0;
		return 0;
	}

	/* FSYN up & running. Computing frequency */
	cfg = CLK_READ(base_address + CKG_FS_CFG(channel));
	pe = (cfg & 0x7fff);		/* FSx_CGF[14:0] */
	md = ((cfg >> 15) & 0x1f);	/* FSx_CGF[19:15] */
	sdiv = ((cfg >> 21) & 0xf);	/* FSx_CGF[24:21] */
	nsdiv = ((cfg >> 26) & 0x1);	/* FSx_CGF[26] */
	err = clk_fs660c32_get_rate(parent_rate,
				nsdiv, md, pe, sdiv, &clk->rate);
#else
	if (clk->nominal_rate)
		clk->rate = clk->nominal_rate;
	else
		clk->rate = 12121212;
#endif

	return err;
}

/* ========================================================================
   Name:        clkgen_freq_synth_recalc
   Description: Get CKG_QFS clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_freq_synth_recalc(struct clk *clk)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_B_REF:
	case CLK_S_C_REF:
	case CLK_S_D_REF:
	case CLK_S_E_REF:
	case CLK_S_F_REF:
		clk->rate = clk_get_rate(clk_get_parent(clk));
		break;
	default:
		return clkgen_freq_synth_fsyn_recalc(clk);
	}
	return 0;
}

/* ========================================================================
   Name:        clkgen_freq_synth_observe
   Description: allows to observe a clock on a SYSACLK_OUT
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgen_freq_synth_observe(struct clk *clk, unsigned long *div_p)
{
	int channel;
	void *base_addr;
	unsigned long je_ctrl;
	int div;	/* final_div = (1 << div) */

	if (!clk || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = clkgen_freq_synth_get_base_addr(clk);
	channel = (clkgen_freq_synth_get_channel(clk) + 2);

	switch (clk->id) {
	case CLK_S_B_REF:
	case CLK_S_C_REF:
	case CLK_S_D_REF:
	case CLK_S_E_REF:
	case CLK_S_F_REF:
		channel = 0;
		break;
	case CLK_S_B_VCO:
	case CLK_S_C_VCO:
	case CLK_S_D_VCO:
	case CLK_S_E_VCO:
	case CLK_S_F_VCO:
		channel = 1;
		break;
	default:
		break;
	}

	if (*div_p < 1 || *div_p > 7)
		return CLK_ERR_BAD_PARAMETER;

	div = (*div_p - 1);

	je_ctrl = CLK_READ(base_addr + CKG_FS_JE_CTRL0);
	je_ctrl &= ~(0x1 << 27 | 0x7 << 24  | 0xf << 4 | 0x3 << 2);

	je_ctrl |=  (0x1 << 27 | div << 24 | channel << 4 | 0x2 << 2);

	CLK_WRITE((base_addr + CKG_FS_JE_CTRL0), je_ctrl);

	/* Observation points:
	   CLOCKGENB => QFS_VID_CLK: AltFunc4 of PIO22[5]
	   CLOCKGENC => QFS_AUD_CLK: AltFunc4 of PIO14[6]
	   CLOCKGEND => QFS_TEL_CLK: AltFunc3 of PIO14[7]
	   CLOCKGENE => QFS_DOC_CLK: AltFunc4 of PIO5[1]
	   CLOCKGENF => QFS_COM_CLK: AltFunc4 of PIO7[4]
	 */

	/* Configuring appropriate PIO */
	if (base_addr == cg_freq_synth_base[0]) {
		SYSCONF_WRITE(0, 403, 20, 22, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 404, 29, 29, 1);/* Output Enable */
		SYSCONF_WRITE(0, 406, 29, 29, 0);/* Open drain */
		SYSCONF_WRITE(0, 405, 29, 29, 0);/* pull up */
	} else if (base_addr == cg_freq_synth_base[1]) {
		SYSCONF_WRITE(0, 204, 24, 26, 3);/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 210, 6, 6, 1);	/* Output Enable */
		SYSCONF_WRITE(0, 216, 6, 6, 0);	/* Open drain */
		SYSCONF_WRITE(0, 213, 6, 6, 0); /* pull up */
	} else if (base_addr == cg_freq_synth_base[2]) {
		SYSCONF_WRITE(0, 204, 28, 30, 3);/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 210, 7, 7, 1);	/* Output Enable */
		SYSCONF_WRITE(0, 216, 7, 7, 0);	/* Open drain */
		SYSCONF_WRITE(0, 213, 7, 7, 0);	/* pull up */
	} else if (base_addr == cg_freq_synth_base[3]) {
		SYSCONF_WRITE(0, 5, 4, 6, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 11, 9, 9, 1);/* Output Enable */
		SYSCONF_WRITE(0, 17, 9, 9, 0);/* Open drain */
		SYSCONF_WRITE(0, 14, 9, 9, 0);/* pull up */
	} else {
		SYSCONF_WRITE(0, 7, 16, 18, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 11, 28, 28, 1);/* Output Enable */
		SYSCONF_WRITE(0, 17, 28, 28, 0);/* Open drain */
		SYSCONF_WRITE(0, 14, 28, 28, 0);/* pull up */
	}
	return 0;
}
#else
#define clkgen_freq_synth_observe	NULL
#endif

/* ========================================================================
   Name:        clkgen_freq_synth_init
   Description: Read HW status to initialize 'struct clk' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */
static struct clk *clkgen_bc_ref_input[4] = {
	NULL,					/* CLK_SYSIN */
	&clk_clocks[CLK_S_CH34REF_DIV_X],
	NULL,					/* CLK_SYSALT */
	NULL					/* CLK_IFE_REF */
};

static struct clk *clkgen_e_ref_input[2] = {
	NULL,					/* CLK_IFE_REF */
	NULL,					/* CLK_SYSIN */
};

static int clkgen_freq_identify_parent(struct clk *clk)
{
	void *base = clkgen_freq_synth_get_base_addr(clk);
	unsigned long data;
	int bank = clkgen_freq_synth_get_bank(clk);

	data = CLK_READ(base + CKG_FS_REFCLKSEL);

	data &= 0x3;
	if (bank  < 2)
		clk->parent = clkgen_bc_ref_input[data];
	else
		clk->parent = clkgen_e_ref_input[data];

	clk->rate = clk_get_rate(clk_get_parent(clk));

	return 0;
}

static int clkgen_freq_synth_set_parent(struct clk *clk,
	struct clk *parent)
{
	int i;
	void *base = clkgen_freq_synth_get_base_addr(clk);
	struct clk **ref;
	int size;

	switch (clk->id) {
	case CLK_S_B_REF:
	case CLK_S_C_REF:
		ref = clkgen_bc_ref_input;
		size = 4;
	case CLK_S_E_REF:
		ref = clkgen_e_ref_input;
		size = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	for (i = 0; i < size; ++i)
		if (parent == ref[i])
			break;

	if (i == size)
		/* parent NOT valid */
		return CLK_ERR_BAD_PARAMETER;

	clk->parent = ref[i];
	clk->rate = clk_get_rate(clk_get_parent(clk));
	CLK_WRITE(base + CKG_FS_REFCLKSEL, i);
	return 0;
}

static int clkgen_freq_synth_init(struct clk *clk)
{
	int err;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_B_REF:
	case CLK_S_C_REF:
	case CLK_S_E_REF:
		/*
		 * This block can select it's parent source
		 */
		clkgen_freq_identify_parent(clk);
	default:
		break;
	}
	err = clkgen_freq_synth_recalc(clk);

	return err;
}

/* ========================================================================
   Name:        clkgen_freq_synth_set_rate
   Description: Set CKG_QFS clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_freq_synth_set_rate(struct clk *clk, unsigned long freq)
{
	unsigned long cfg, setup, pwr, val = 0;
	unsigned long pe, md, sdiv, ndiv, nsdiv = -1;
	int channel = 0;
	unsigned long parent_rate, opclk, refclk;
	void *base_address;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

#if !defined(CLKLLA_NO_PLL)
	base_address = clkgen_freq_synth_get_base_addr(clk);
	opclk = CLK_READ(base_address + CKG_FS_OPCLKCMN);
	setup = CLK_READ(base_address + CKG_FS_SETUP);
	pwr = CLK_READ(base_address + CKG_FS_PWR);
	refclk = CLK_READ(base_address + CKG_FS_REFCLKSEL);

	parent_rate = clk_get_rate(clk_get_parent(clk));

	if (clk->id == CLK_S_B_VCO || clk->id == CLK_S_C_VCO ||
	    clk->id == CLK_S_D_VCO ||
	    clk->id == CLK_S_E_VCO || clk->id == CLK_S_F_VCO) {
		if (clk_fs660c32_vco_get_params(parent_rate, freq, &ndiv))
			return CLK_ERR_BAD_PARAMETER;
		/* PLL power down */
		CLK_WRITE(base_address + CKG_FS_PWR, (pwr | 0x1));
		CLK_WRITE(base_address + CKG_FS_SETUP,
			  (((setup & ~(0xe))|(ndiv << 1))));
		/* PLL power up */
		CLK_WRITE(base_address + CKG_FS_PWR, (pwr & ~(0x1)));
		/* enable output clock driven by FS */
		CLK_WRITE(base_address + CKG_FS_OPCLKCMN, opclk & ~(0x1));
		return clkgen_freq_synth_recalc(clk);
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fs660c32_dig_get_params(parent_rate,
		freq, &nsdiv, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	channel = clkgen_freq_synth_get_channel(clk);

	/* Removing digital reset, digital standby */
	/* FS_SETUP[4 + channel] = digital reset remove  */
	CLK_WRITE(base_address + CKG_FS_SETUP, setup | (0x10 << channel));
	/* FSx_PWR[1+channel] = digital standby remove */
	CLK_WRITE(base_address + CKG_FS_PWR, (pwr & ~(0x2 << channel)));

	cfg = CLK_READ(base_address + CKG_FS_CFG(channel));
	cfg &= ~(0x1 << 26 | 0x1 << 20 | 0xf << 21 | 0x1f << 15 | 0x7fff);
	val = (nsdiv << 26 | sdiv << 21 | md << 15 | pe);
	/* enable FS programming */
	val |= 0x1 << 20;
	cfg |= val;

	CLK_WRITE(base_address + CKG_FS_CFG(channel), cfg);
	/* Toggle EN_PRG */
	CLK_WRITE(base_address + CKG_FS_CFG(channel), cfg & ~(0x1 << 20));
	/* enable output clock driven by FS */
	CLK_WRITE(base_address + CKG_FS_OPCLKCMN, (opclk & ~(0x2 << channel)));

#endif
	return clkgen_freq_synth_recalc(clk);
}


/* ========================================================================
   Name:        clkgen_freq_synth_xable_fsyn
   Description: Enable/Disable FSYN. If all channels OFF, FSYN is powered
		down.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgen_freq_synth_xable_fsyn(struct clk *clk, unsigned long enable)
{
	unsigned long pwr, setup;
	int channel = 0;
	void *base_address;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk->id) {
	case CLK_S_B_REF:
	case CLK_S_C_REF:
	case CLK_S_D_REF:
	case CLK_S_E_REF:
	case CLK_S_F_REF:
		return CLK_ERR_FEATURE_NOT_SUPPORTED;
	case CLK_S_B_VCO ... CLK_S_B_SECURE:
	case CLK_S_C_VCO ... CLK_S_C_PCM3:
	case CLK_S_D_VCO ... CLK_S_D_SPARE:
	case CLK_S_E_VCO ... CLK_S_E_IFE_WB:
	case CLK_S_F_VCO ... CLK_S_F_TSOUT1_SRC:
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	base_address = clkgen_freq_synth_get_base_addr(clk);

	channel = clkgen_freq_synth_get_channel(clk);

	pwr = CLK_READ(base_address + CKG_FS_PWR);
	setup = CLK_READ(base_address + CKG_FS_SETUP);

	if (clk->id == CLK_S_C_VCO || clk->id == CLK_S_D_VCO ||
	    clk->id == CLK_S_E_VCO || clk->id == CLK_S_F_VCO) {
		/* ANALOG part */
		if (enable)
			pwr &= ~(0x1);	/* PLL power up */
		else
			pwr |= 0x1;	/* PLL power down */
		CLK_WRITE(base_address + CKG_FS_PWR, pwr);
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
		CLK_WRITE(base_address + CKG_FS_SETUP, setup);
		/* FSx_PWR[1 + channel] = digital standby */
		CLK_WRITE(base_address + CKG_FS_PWR, pwr);
	}

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgen_freq_synth_fsyn_recalc(clk);
	else
		clk->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgen_freq_synth_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgen_freq_synth_enable(struct clk *clk)
{
	return clkgen_freq_synth_xable_fsyn(clk, 1);
}

/* ========================================================================
   Name:        clkgen_freq_synth_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgen_freq_synth_disable(struct clk *clk)
{
	return clkgen_freq_synth_xable_fsyn(clk, 0);
}

/*
 * CMM channel
 */
static inline int clkgen_ccm_divx_channel(struct clk *clk)
{
	const long ccm_divx_range[] = {
		CLK_S_G_DIV_0,
		CLK_S_E_WB_DIV_1,
		CLK_S_E_IFE_216,
		CLK_S_D_ISIS_ETH_250,
		CLK_S_D_USB_DIV_1,
		CLK_S_D_TEL_ZSI_TEL,
		CLK_S_CH34REF_DIV_1,
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(ccm_divx_range); ++i)
		if (clk->id >= ccm_divx_range[i])
			return clk->id - ccm_divx_range[i];

	return -1;
}

static inline int clkgen_ccm_gpout_channel(struct clk *clk)
{
	int i;
	struct ccm_gpout_range {
		long begin, end;
	};
	const struct ccm_gpout_range ccm_gpout_range[] = {
		{
			CLK_S_D_TEL_ZSI_TEL,
			CLK_S_D_ISIS_DIV_X
		}, {
			CLK_S_E_IFE_216,
			CLK_S_E_WB_1
		}, {
			CLK_S_G_DIV_0,
			CLK_S_G_LPC
		}, {
			CLK_S_CH34REF_DIV_1,
			CLK_S_CH34REF_DIV_X
		}
	};

	for (i = 0; i < ARRAY_SIZE(ccm_gpout_range); ++i)
		if (clk->id >= ccm_gpout_range[i].begin &&
		    clk->id <= ccm_gpout_range[i].end)
			return (clk->id - ccm_gpout_range[i].begin) / 4;
	return -1;
}

static int clkgen_ccm_set_divx(struct clk *clk, int gpout_id, int div)
{
	unsigned long gpout;
	void *base_address;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	base_address = clkgen_freq_synth_get_base_addr(clk);

	gpout = div & 0xff;
	gpout |= (1 << 8); /* input 1:1 */
	gpout |= (1 << 17); /* select src_clk */
	/*
	 * USB PLL ref clk input's 12/24/48Mhz selection via fsx_gpout[19:18]
	 */
	if (clk->id == CLK_S_D_USB_REF)
		switch (div) {
		case 10:
			gpout = ((gpout & ~(0x3 << 18)) | (0x1 << 19));
			break;
		case 20:
		case 25:
			gpout = ((gpout & ~(0x3 << 18)) | (0x1 << 18));
			break;
		default:
			break;
		}

	CLK_WRITE(base_address + CKG_FS_GPOUT(gpout_id), gpout);
	CLK_WRITE(base_address + CKG_FS_GPOUT_CTRL, (0x3 << (gpout_id * 2)));

	return 0;
}

static int clkgen_ccm_set_rate(struct clk *clk, unsigned long freq)
{
	int channel, divx;
	unsigned long parent_rate;
	int err, gpout_id;

	channel = clkgen_ccm_divx_channel(clk);
	if (channel != 3)
		return CLK_ERR_BAD_PARAMETER;

	gpout_id = clkgen_ccm_gpout_channel(clk);
	parent_rate = clk_get_rate(clk_get_parent(clk));
	divx = parent_rate / freq ;
	err = clkgen_ccm_set_divx(clk, gpout_id, divx);
	if (err)
		return err;

	clk->rate = freq;
	return 0;
}

static int clkgen_ccm_recalc(struct clk *clk)
{
	unsigned long parent_rate, data;
	int channel, gpout_id;
	void *base_address;
	/*
	 * channel dev_1, div_2, div_4 are fixed
	 */
	parent_rate = clk_get_rate(clk_get_parent(clk));
	channel = clkgen_ccm_divx_channel(clk);
	if (channel != 3) {
		/*
		 * fixed ratio...
		 */
		clk->rate = parent_rate >> channel;
		return 0;
	}
	/*
	 * channel == 3 is a programmable channel
	 */
	base_address = clkgen_freq_synth_get_base_addr(clk);
	gpout_id = clkgen_ccm_gpout_channel(clk);
	data = CLK_READ(base_address + CKG_FS_GPOUT(gpout_id));

	if (data & (1 << 16)) {
		clk->rate = parent_rate / 1000;
		return 0;
	}

	switch (data & 0xff) {
	case 0:
	case 255:
		clk->rate = parent_rate / 255;
		break;
	case 2 ... 254:
		clk->rate = parent_rate / (data & 0xff);
		break;
	case 1:
		clk->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:        clkgen_ccm_observe
   Description: CCM clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgen_ccm_observe(struct clk *clk, unsigned long *div_p)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation points:
	 * CCM_ETH_CLK => PIO13[6] alt 7
	 * CCM_TEL_CLK => PIO13[7] alt 7
	 * CCM_USB_CLK => PIO14[0] alt 7
	 * CCM_WB1_CLK => PIO6[6] alt 4
	 * CCM_WB2_CLK => PIO6[7] alt 4
	 * CCM_IFE_CLK => PIO7[3] alt 4
	 */

	switch (clk->id) {
	case CLK_S_D_ISIS_ETH_250:
		SYSCONF_WRITE(0, 203, 24, 26, 7);/* Selecting alternate 7 */
		SYSCONF_WRITE(0, 209, 30, 30, 1);/* Output Enable */
		SYSCONF_WRITE(0, 215, 30, 30, 0);/* Open drain */
		SYSCONF_WRITE(0, 212, 30, 30, 0);/* pull up */
		break;
	case CLK_S_D_TEL_ZSI_APPL:
		SYSCONF_WRITE(0, 203, 28, 30, 7);/* Selecting alternate 7 */
		SYSCONF_WRITE(0, 209, 31, 31, 1);/* Output Enable */
		SYSCONF_WRITE(0, 215, 31, 31, 0);/* Open drain */
		SYSCONF_WRITE(0, 212, 31, 31, 0);/* pull up */
		break;
	case CLK_S_D_USB_REF:
		SYSCONF_WRITE(0, 204, 0, 2, 7);/* Selecting alternate 7 */
		SYSCONF_WRITE(0, 210, 0, 0, 1);	/* Output Enable */
		SYSCONF_WRITE(0, 216, 0, 0, 0);	/* Open drain */
		SYSCONF_WRITE(0, 213, 0, 0, 0);	/* pull up */
		break;
	case CLK_S_E_WB_1:
		SYSCONF_WRITE(0, 6, 24, 26, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 11, 22, 22, 1);/* Output Enable */
		SYSCONF_WRITE(0, 17, 22, 22, 0);/* Open drain */
		SYSCONF_WRITE(0, 14, 22, 22, 0);/* pull up */
		break;
	case CLK_S_E_WB_2:
		SYSCONF_WRITE(0, 6, 28, 30, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 11, 23, 23, 1);/* Output Enable */
		SYSCONF_WRITE(0, 17, 23, 23, 0);/* Open drain */
		SYSCONF_WRITE(0, 14, 23, 23, 0);/* pull up */
		break;
	case CLK_S_E_MCHI:
		SYSCONF_WRITE(0, 7, 12, 14, 4);/* Selecting alternate 4 */
		SYSCONF_WRITE(0, 11, 27, 27, 1);/* Output Enable */
		SYSCONF_WRITE(0, 17, 27, 27, 0);/* Open drain */
		SYSCONF_WRITE(0, 14, 27, 27, 0);/* pull up */
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}
#else
#define clkgen_ccm_observe	NULL
#endif

static int clkgen_ccm_init(struct clk *clk)
{
	int channel;
	unsigned int gpout_id, data;
	void *base_address;

	base_address = clkgen_freq_synth_get_base_addr(clk);

	/* guarantee the CCM block has the correct clk source */
	gpout_id = clkgen_ccm_gpout_channel(clk);
	data = CLK_READ(base_address + CKG_FS_GPOUT(gpout_id));
	data |= (1 << 17);
	CLK_WRITE(base_address + CKG_FS_GPOUT(gpout_id), data);

	channel = clkgen_ccm_divx_channel(clk);
	if (channel != 3)
		return clkgen_ccm_recalc(clk);

	data = CLK_READ(base_address + CKG_FS_GPOUT(gpout_id));
	data &= 0xff;
	data |= (1 << 17);	/* power sel_clk */
	data &= ~(1 << 16);	/* No div_1000 */
	data |= (1 << 8);	/* input 1_to_1 */
	CLK_WRITE(base_address + CKG_FS_GPOUT(gpout_id), data);
	CLK_WRITE(base_address + CKG_FS_GPOUT_CTRL, 0x3 << (gpout_id * 2));

	/* pr_info("%s - %s - %x\n", __func__, clk->name, data); */
	return clkgen_ccm_recalc(clk);
}

static int clkgen_g_recalc(struct clk *clk)
{
	unsigned long parent_rate;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	if (clk->id == CLK_S_G_REF)
		clk->rate = parent_rate;
	else
		clk->rate = clk_get_rate(clk_get_parent(clk)) / 50;

	return 0;
}

/******************************************************************************
CA9 PLL
******************************************************************************/

/* ========================================================================
   Name:        clkgena9_recalc
   Description: Get clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_recalc(struct clk *clk)
{
	unsigned long parent_rate;
	unsigned long idf, ndiv, odf, vco_by2;
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	parent_rate = clk_get_rate(clk_get_parent(clk));
	switch (clk->id) {
	case CLK_S_A9_REF:
	case CLK_S_A9:
		clk->rate = parent_rate;
		break;
	case CLK_S_A9_PERIPH:
		clk->rate = parent_rate / 2;
		break;
	case CLK_S_A9_PHI0:
		#if !defined(CLKLLA_NO_PLL)
		idf = SYSCONF_READ(0, 722, 22, 24);
		ndiv = SYSCONF_READ(0, 722, 9, 16);
		odf = SYSCONF_READ(0, 722, 3, 8);
		if (!odf)
			odf = 1;
		if (SYSCONF_READ(0, 722, 0, 0))
			clk->rate = 0;	/* PLL disabled */
		else {
			clk_pll1600c45_get_phi_rate(parent_rate,
				idf, ndiv, odf, &vco_by2);
			clk->rate = vco_by2 / odf;
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
	if (clk->id != CLK_S_A9) /* Other clocks have static parent */
		return 0;
	/* Is CA9 clock sourced from PLL or A0-25 ? */
	if (SYSCONF_READ(0, 722, 2, 2))
		clk->parent = &clk_clocks[
			(SYSCONF_READ(0, 722, 1, 1) ?
				CLK_S_A0_A9_EXT2F : CLK_S_A0_A9_EXT2F_DIV2)];
	else
		clk->parent = &clk_clocks[CLK_S_A9_PHI0];

	return 0;
}

/* ========================================================================
   Name:        clkgena9_observe
   Description: CA9 clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

#ifdef ST_OS21
static int clkgena9_observe(struct clk *clk, unsigned long *div_p)
{
	if (!clk)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation points:
	 * A9_PLL_OBS_CLK => PIO2[7] alt 4
	 */
	SYSCONF_WRITE(0, 2, 28, 30, 4);		/* Selecting alternate 4 */
	SYSCONF_WRITE(0, 10, 23, 23, 1);	/* Output Enable */
	SYSCONF_WRITE(0, 16, 23, 23, 0);	/* Open drain */
	SYSCONF_WRITE(0, 13, 23, 23, 0);	/* pull up */

	return 0;
}
#else
#define clkgena9_observe	NULL
#endif

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
	unsigned long idf, ndiv, cp, odf;
	int err = 0;

	if (!clk)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk->parent)
		return CLK_ERR_INTERNAL;

	if (clk->id == CLK_S_A9)
		return clkgena9_set_rate(clk->parent, freq);
	if (clk->id != CLK_S_A9_PHI0)
		return CLK_ERR_BAD_PARAMETER;

	err = clk_pll1600c45_get_phi_params(clk->parent->rate,
			freq, &idf, &ndiv, &odf, &cp);
	if (err != 0)
		return err;

	#if !defined(CLKLLA_NO_PLL)
	SYSCONF_WRITE(0, 722, 2, 2, 1);	/* Bypassing PLL */

	SYSCONF_WRITE(0, 722, 0, 0, 1); /* Disabling PLL */
	SYSCONF_WRITE(0, 722, 22, 24, idf); /* IDF: syscfg654, bits 22 to 24 */
	SYSCONF_WRITE(0, 722, 9, 16, ndiv); /* NDIV: syscfg654, bits 9 to 16 */
	SYSCONF_WRITE(0, 722, 3, 8, odf); /* ODF : set to default value "1" */
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
	"PIO2[5]"	/* Observation point */
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
	"PIO22[6]"	/* Observation point */
);

_CLK_OPS(clkgen_freq_synth,
	"FreqSynth",
	clkgen_freq_synth_init,
	clkgen_freq_synth_set_parent,
	clkgen_freq_synth_set_rate,
	clkgen_freq_synth_recalc,
	clkgen_freq_synth_enable,
	clkgen_freq_synth_disable,
	clkgen_freq_synth_observe,
	NULL,		/* No measure function */
	"?"		/* Observation point */
);

_CLK_OPS(clkgen_ccm,
	"CMM",
	clkgen_ccm_init,	/* init */
	NULL,	/* set parent */
	clkgen_ccm_set_rate,
	clkgen_ccm_recalc,
	NULL,	/* enable */
	NULL,	/* disable */
	clkgen_ccm_observe,
	NULL,
	"?"
);


_CLK_OPS(clkgen_vcc,
	"B/Video-Vcc",
	clkgen_vcc_init,
	clkgen_vcc_set_parent,
	clkgen_vcc_set_rate,
	clkgen_vcc_recalc,
	clkgen_vcc_enable,
	clkgen_vcc_disable,
	clkgen_vcc_observe,
	NULL,	       /* No measure function */
	"PIO22[5]"		/* Observation point */
);

_CLK_OPS(clkgen_g,
	"clockgen-G",
	clkgen_g_recalc,
	NULL,
	NULL,
	clkgen_g_recalc,
	NULL,
	NULL,
	NULL,
	NULL,
	"?"
);

_CLK_OPS(clkgena9,
	"CA9",
	clkgena9_init,
	NULL,
	clkgena9_set_rate,
	clkgena9_recalc,
	NULL,
	NULL,
	clkgena9_observe,
	NULL,		/* No measure function */
	"PIO2[7]"	/* No observation point */
);

/* Physical clocks description */
static struct clk clk_clocks[] = {
/* Clockgen A0 */
_CLK(CLK_S_A0_REF, &clkgena0, 0, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_A0_PLL0HS, &clkgena0, 1200000000, 0,
	&clk_clocks[CLK_S_A0_REF]),
_CLK_P(CLK_S_A0_PLL0LS, &clkgena0, 600000000, 0,
	&clk_clocks[CLK_S_A0_PLL0HS]),
_CLK_P(CLK_S_A0_PLL1HS, &clkgena0, 1000000000, 0,
	&clk_clocks[CLK_S_A0_REF]),
_CLK_P(CLK_S_A0_PLL1LS, &clkgena0, 500000000, 0,
	&clk_clocks[CLK_S_A0_PLL1HS]),

_CLK(CLK_S_A0_CT_DIAG, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_FDMA_0, &clkgena0,    400000000,    0),
_CLK(CLK_S_A0_FDMA_1, &clkgena0,    400000000,    0),
_CLK(CLK_S_A0_IC_DVA_ST231, &clkgena0,    600000000,    0),
_CLK(CLK_S_A0_IC_SEC_ST231, &clkgena0,    600000000,    0),
_CLK(CLK_S_A0_IC_CM_ST40, &clkgena0,    500000000,    0),
_CLK(CLK_S_A0_IC_DVA_ST40, &clkgena0,    500000000,    0),
_CLK(CLK_S_A0_IC_CPU, &clkgena0,    500000000,    0),
_CLK(CLK_S_A0_IC_MAIN, &clkgena0,    500000000,    0),
_CLK(CLK_S_A0_IC_ROUTER, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_IC_PCIE_SATA, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_IC_FHASH, &clkgena0,    300000000,    0),
_CLK(CLK_S_A0_IC_STFE, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_GLOBAL_ROUTER, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_GLOBAL_SATAPCI, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_GLOBAL_PCI_TARG, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_GLOBAL_NETWORK, &clkgena0,    200000000,    0),
_CLK(CLK_S_A0_A9_TRACE_INT, &clkgena0, 100000000, 0),
_CLK(CLK_S_A0_A9_EXT2F, &clkgena0, 100000000, 0),
_CLK_P(CLK_S_A0_A9_EXT2F_DIV2, &clkgena0, 50000000, 0,
	&clk_clocks[CLK_S_A0_A9_EXT2F]),

/* Clockgen A1 */
_CLK(CLK_S_A1_REF, &clkgena1, 0, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_A1_PLL0HS, &clkgena1, 1332000000, 0,  &clk_clocks[CLK_S_A1_REF]),
_CLK_P(CLK_S_A1_PLL0LS, &clkgena1, 666000000, 0,  &clk_clocks[CLK_S_A1_PLL0HS]),
_CLK_P(CLK_S_A1_PLL1HS, &clkgena1, 800000000, 0, &clk_clocks[CLK_S_A1_REF]),
_CLK_P(CLK_S_A1_PLL1LS, &clkgena1, 400000000, 0, &clk_clocks[CLK_S_A1_PLL1HS]),

_CLK(CLK_S_A1_STAC_TX_PHY, &clkgena1,   300000000, 0),
_CLK(CLK_S_A1_STAC_BIST, &clkgena1,   300000000, 0),
_CLK(CLK_S_A1_VTAC_BIST, &clkgena1,   450000000, 0),
_CLK(CLK_S_A1_IC_DDR,	&clkgena1,   333000000, 0),
_CLK(CLK_S_A1_BLIT_PROC,	&clkgena1,   266000000,    0),
_CLK(CLK_S_A1_SYS_MMC_SS, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_CARD_MMC_SS, &clkgena1,   50000000,    0),
_CLK(CLK_S_A1_IC_EMI, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_BCH_NAND, &clkgena1,   200000000,    0),
_CLK(CLK_S_A1_IC_STAC, &clkgena1,   400000000,    0),
_CLK(CLK_S_A1_IC_BDISP, &clkgena1,   200000000,    0),
_CLK(CLK_S_A1_IC_TANGO, &clkgena1,   200000000,    0),
_CLK(CLK_S_A1_IC_GLOBAL_STFE_STAC, &clkgena1,   200000000,    0),
_CLK(CLK_S_A1_IC_LP, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_IC_LP_CPU, &clkgena1,   100000000, CLK_ALWAYS_ENABLED),
_CLK(CLK_S_A1_IC_LP_HD, &clkgena1,   100000000,  CLK_ALWAYS_ENABLED),
_CLK(CLK_S_A1_IC_DMA, &clkgena1,   200000000,    0),
_CLK(CLK_S_A1_IC_SECURE, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_IC_LP_D3, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_IC_LP_DQAM, &clkgena1,   100000000,    0),
_CLK(CLK_S_A1_IC_LP_ETH, &clkgena1,   100000000,    0),

_CLK(CLK_S_CH34REF_DIV_1, &clkgen_ccm, 0, 0),
_CLK(CLK_S_CH34REF_DIV_2, &clkgen_ccm, 0, 0),
_CLK(CLK_S_CH34REF_DIV_4, &clkgen_ccm, 0, 0),
_CLK(CLK_S_CH34REF_DIV_X, &clkgen_ccm, 0, 0),

/* Clockgen B (Video)*/
_CLK(CLK_S_B_REF, &clkgen_freq_synth, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_B_VCO, &clkgen_freq_synth, 600000000, 0,
		&clk_clocks[CLK_S_B_REF]),
_CLK_P(CLK_S_B_TP, &clkgen_freq_synth, 360000000, 0,
		&clk_clocks[CLK_S_B_VCO]),
_CLK_P(CLK_S_B_HD, &clkgen_freq_synth, 297000000, 0,
		&clk_clocks[CLK_S_B_VCO]),
_CLK_P(CLK_S_B_SD, &clkgen_freq_synth, 108000000, 0,
		&clk_clocks[CLK_S_B_VCO]),
_CLK_P(CLK_S_B_SECURE, &clkgen_freq_synth, 450000000, 0,
		&clk_clocks[CLK_S_B_VCO]),
_CLK(CLK_S_B_TMDS, &clkgen_vcc, 297000000, 0),

_CLK(CLK_S_B_PIX_MAIN, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_PIX_AUX, &clkgen_vcc, 13500000, 0),
_CLK(CLK_S_B_PIX_HDMI, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_PIX_DVO, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_PIX_HDDAC, &clkgen_vcc, 148500000, 0),
_CLK(CLK_S_B_DENC, &clkgen_vcc, 27000000, 0),
_CLK(CLK_S_B_OUT_HDDAC, &clkgen_vcc, 148500000, 0),
_CLK(CLK_S_B_OUT_SDDAC, &clkgen_vcc, 108000000, 0),
_CLK(CLK_S_B_OUT_DVO, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_HDMI_PLL, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_HD_MCRU, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_SD_MCRU, &clkgen_vcc, 108000000, 0),
_CLK(CLK_S_B_XD_MCRU, &clkgen_vcc, 297000000, 0),
_CLK(CLK_S_B_PACE0,	&clkgen_vcc, 27000000, 0),
_CLK(CLK_S_B_TMDS_HDMI, &clkgen_vcc, 297000000, 0),

/* Clockgen C (Audio) */
_CLK(CLK_S_C_REF, &clkgen_freq_synth, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_C_VCO, &clkgen_freq_synth, 600000000, 0,
		&clk_clocks[CLK_S_C_REF]),
_CLK_P(CLK_S_C_PCM0, &clkgen_freq_synth, 50000000, 0,
		&clk_clocks[CLK_S_C_VCO]),
_CLK_P(CLK_S_C_PCM1, &clkgen_freq_synth, 50000000, 0,
		&clk_clocks[CLK_S_C_VCO]),
_CLK_P(CLK_S_C_PCM2, &clkgen_freq_synth, 50000000, 0,
		&clk_clocks[CLK_S_C_VCO]),
_CLK_P(CLK_S_C_PCM3, &clkgen_freq_synth, 50000000, 0,
		&clk_clocks[CLK_S_C_VCO]),

/* Clockgen D (Telephony)*/
_CLK(CLK_S_D_REF, &clkgen_freq_synth, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_D_VCO, &clkgen_freq_synth, 600000000, 0,
		&clk_clocks[CLK_S_D_REF]),
_CLK_P(CLK_S_D_FDMA_TEL, &clkgen_freq_synth, 400000000, 0,
		&clk_clocks[CLK_S_D_VCO]),
_CLK_P(CLK_S_D_ZSI, &clkgen_freq_synth, 49152000, 0,
		&clk_clocks[CLK_S_D_VCO]),
_CLK_P(CLK_S_D_ISIS_ETH, &clkgen_freq_synth, 250000000, 0,
		&clk_clocks[CLK_S_D_VCO]),

/* Clockgen D: CMM-A */
_CLK_P(CLK_S_D_TEL_ZSI_TEL, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_D_ZSI]),
_CLK_P(CLK_S_D_TEL_ZSI_APPL, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_D_ZSI]),
/* Clockgen D-CMM-B*/
_CLK_P(CLK_S_D_USB_REF, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_D_VCO]),
/* Clockgen D: CMM-C */
_CLK_P(CLK_S_D_ISIS_ETH_250, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_D_ISIS_ETH]),
_CLK_P(CLK_S_D_ISIS_ETH_125, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_D_ISIS_ETH]),

/* Clockgen E (Docsis)*/
_CLK(CLK_S_E_REF, &clkgen_freq_synth, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_E_VCO, &clkgen_freq_synth, 600000000, 0,
		&clk_clocks[CLK_S_E_REF]),
_CLK_P(CLK_S_E_FP, &clkgen_freq_synth, 250000000, 0,
		&clk_clocks[CLK_S_E_VCO]),
_CLK_P(CLK_S_E_D3_XP70, &clkgen_freq_synth, 324000000, 0,
		&clk_clocks[CLK_S_E_VCO]),
_CLK_P(CLK_S_E_IFE, &clkgen_freq_synth, 216000000, 0,
		&clk_clocks[CLK_S_E_VCO]),
_CLK_P(CLK_S_E_IFE_WB, &clkgen_freq_synth, 324000000, 0,
		&clk_clocks[CLK_S_E_VCO]),

/* Clockgen E CMM - A */
_CLK_P(CLK_S_E_IFE_216, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE]),
_CLK_P(CLK_S_E_IFE_108, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE]),
_CLK_P(CLK_S_E_IFE_54, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE]),
_CLK_P(CLK_S_E_MCHI, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE]),
/* Clockgen E CMM - B */
_CLK_P(CLK_S_E_WB_2, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE_WB]),
_CLK_P(CLK_S_E_WB_1, &clkgen_ccm, 0, 0,
		&clk_clocks[CLK_S_E_IFE_WB]),

/* Clockgen F (Misc)*/
_CLK(CLK_S_F_REF, &clkgen_freq_synth, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_F_VCO, &clkgen_freq_synth, 600000000, 0,
		&clk_clocks[CLK_S_F_REF]),
_CLK_P(CLK_S_F_DSS, &clkgen_freq_synth, 36864000, 0,
		&clk_clocks[CLK_S_F_VCO]),
_CLK_P(CLK_S_F_PACE1, &clkgen_freq_synth, 27000000, 0,
		&clk_clocks[CLK_S_F_VCO]),
_CLK_P(CLK_S_F_PAD_OUT, &clkgen_freq_synth, 50000000, 0,
		&clk_clocks[CLK_S_F_VCO]),
_CLK_P(CLK_S_F_TSOUT1_SRC, &clkgen_freq_synth, 150000000, 0,
		&clk_clocks[CLK_S_F_VCO]),

/* Clockgen G Cmm-LPC */
_CLK(CLK_S_G_REF, &clkgen_g, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_G_REF_DIV50, &clkgen_g, 0, 0,
		&clk_clocks[CLK_S_G_REF]),
_CLK_P(CLK_S_G_TMP_SENS, &clkgen_ccm, 0, 0, &clk_clocks[CLK_S_G_REF_DIV50]),
_CLK_P(CLK_S_G_LPC, &clkgen_ccm, 0, 0, &clk_clocks[CLK_S_G_REF_DIV50]),

/* CA9 PLL */
_CLK(CLK_S_A9_REF, &clkgena9, 30000000, CLK_ALWAYS_ENABLED),
_CLK_P(CLK_S_A9_PHI0, &clkgena9, 1000000000,
	0, &clk_clocks[CLK_S_A9_REF]),
_CLK(CLK_S_A9, &clkgena9, 0, 0),
_CLK_P(CLK_S_A9_PERIPH, &clkgena9, 0, 0, &clk_clocks[CLK_S_A9]),
};


/* ========================================================================
   Name:	sasc1_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int __init sasc1_clk_init(struct clk *_sys_clk_in, struct clk *_sys_clkalt_in,
		struct clk *_ch34_ref_clk, struct clk *_ife_ref_clk,
		struct clk *_clk_tmds_hdms)
{
	int ret;
	long data;

	call_platform_sys_claim(901, 0, 3);
	call_platform_sys_claim(901, 4, 4);

	call_platform_sys_claim(902, 0, 15);
	call_platform_sys_claim(903, 0, 31);
	call_platform_sys_claim(904, 0, 31);

#ifdef ST_OS21
	call_platform_sys_claim(2, 20, 22);
	call_platform_sys_claim(10, 21, 21);
	call_platform_sys_claim(16, 21, 21);
	call_platform_sys_claim(13, 21, 21);
	call_platform_sys_claim(403, 24, 26);
	call_platform_sys_claim(404, 30, 30)
	call_platform_sys_claim(406, 30, 30);
	call_platform_sys_claim(405, 30, 30);
#endif

	call_platform_sys_claim(722, 0, 0);
	call_platform_sys_claim(722, 1, 1);
	call_platform_sys_claim(722, 2, 2);
	call_platform_sys_claim(722, 3, 8);
	call_platform_sys_claim(722, 9, 16);
	call_platform_sys_claim(722, 22, 24);
	call_platform_sys_claim(760, 0, 0);

	cga_base[0] = ioremap_nocache(0xFEE48000, 0x1000);
	cga_base[1] = ioremap_nocache(0xFEA20000, 0x1000);
	cgb_vcc_base = ioremap_nocache(0xFE950000, 0x1000);
	cg_freq_synth_base[0] = cgb_vcc_base;
	cg_freq_synth_base[1] = ioremap_nocache(0xFEF61000, 0x1000);
	cg_freq_synth_base[2] = ioremap_nocache(0xFE910000, 0x1000);
	cg_freq_synth_base[3] = ioremap_nocache(0xFEF62000, 0x1000);
	cg_freq_synth_base[4] = ioremap_nocache(0xFEE74000, 0x1000);
	ife_phy_prog_base = ioremap_nocache(0xFEE7A000, 0x1000);

	clk_clocks[CLK_S_CH34REF_DIV_1].parent =
		clk_clocks[CLK_S_CH34REF_DIV_2].parent =
		clk_clocks[CLK_S_CH34REF_DIV_4].parent =
		clk_clocks[CLK_S_CH34REF_DIV_X].parent = _ch34_ref_clk;
	/*
	 * CLK_S_CH34REF_DIV_X has to be programmed with a div_8 divisor
	 */
	data = CLK_READ(cg_freq_synth_base[1] + CKG_FS_GPOUT(0));
	CLK_WRITE(cg_freq_synth_base[1] + CKG_FS_GPOUT(0),
		(data & 0xfffc0000) | 0x20108);
	CLK_WRITE(cg_freq_synth_base[1] + CKG_FS_GPOUT_CTRL, 0x3);

	/*
	 * Patch to ensure 27Mhz CLK_IFE_REF is driven by default
	 */
	CLK_WRITE(ife_phy_prog_base + 0x54, 0x0);
	CLK_WRITE(ife_phy_prog_base + 0x4c, 0x0);

	clk_clocks[CLK_S_A0_REF].parent =
		clk_clocks[CLK_S_A1_REF].parent = _sys_clk_in;
	/* CLK_S_B_REF, CLK_S_C_REF, CLK_S_E_REF clock
	 * parent are evaluated during the boot
	 */
	clkgen_bc_ref_input[0] = _sys_clk_in;
	clkgen_bc_ref_input[2] = _sys_clkalt_in;
	clkgen_bc_ref_input[3] = _ife_ref_clk;

	clkgen_e_ref_input[0] = _ife_ref_clk;
	clkgen_e_ref_input[1] = _sys_clk_in;

	clk_clocks[CLK_S_D_REF].parent =
		clk_clocks[CLK_S_F_REF].parent =
		clk_clocks[CLK_S_G_REF].parent =
		clk_clocks[CLK_S_A9_REF].parent = _sys_clk_in;

	clk_clocks[CLK_S_B_TMDS].parent = _clk_tmds_hdms;

#ifdef ST_OS21
	printf("Registering SASC1 clocks\n");
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	printf(" => done\n");
#else
	ret = clk_register_table(clk_clocks, CLK_S_B_REF, 1);

	ret |= clk_register_table(&clk_clocks[CLK_S_B_REF],
				ARRAY_SIZE(clk_clocks) - CLK_S_B_REF, 0);
#endif
	return ret;
}
