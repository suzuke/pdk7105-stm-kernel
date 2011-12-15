/*****************************************************************************
 *
 * File name   : clock-fli7610.c
 * Description : Low Level API - HW specific implementation
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
 * Original code from stx7111 platform
*/

/* Includes ----------------------------------------------------------------- */

#include <linux/stm/fli7610.h>
#include <linux/stm/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clock-stxtae.h"
#include "clock-regs-stxtae.h"

#include "clock-oslayer.h"
#include "clock-common.h"

static void *cga_base;

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgena_recalc(clk_t *clk_p);
static int clkgena_enable(clk_t *clk_p);
static int clkgena_disable(clk_t *clk_p);
static int clkgena_init(clk_t *clk_p);


static void *clksouth_base;
static int clksouth_set_rate(clk_t *clk_p, unsigned long freq);
static int clksouth_fsyn_recalc(clk_t *clk_p);
static int clksouth_enable(clk_t *clk_p);
static int clksouth_disable(clk_t *clk_p);
static int clksouth_init(clk_t *clk_p);

static void *clkaudio_base;
static int clkaudio_set_rate(clk_t *clk_p, unsigned long freq);
static int clkaudio_fsyn_recalc(clk_t *clk_p);
static int clkaudio_enable(clk_t *clk_p);
static int clkaudio_disable(clk_t *clk_p);
static int clkaudio_init(clk_t *clk_p);


#define SYSA_CLKIN			30	/* FE osc */

_CLK_OPS(clkgena,
	"Clockgen A",
	clkgena_init,
	clkgena_set_parent,
	clkgena_set_rate,
	clkgena_recalc,
	clkgena_enable,
	clkgena_disable,
	NULL,
	NULL,
	NULL
);
_CLK_OPS(clksouth,
	"Clockgen South",
	clksouth_init,
	NULL,
	clksouth_set_rate,
	clksouth_fsyn_recalc,
	clksouth_enable,
	clksouth_disable,
	NULL,
	NULL,
	NULL
);
_CLK_OPS(clkaudio,
	"Clockgen Audio",
	clkaudio_init,
	NULL,
	clkaudio_set_rate,
	clkaudio_fsyn_recalc,
	clkaudio_enable,
	clkaudio_disable,
	NULL,
	NULL,
	NULL
);

/* Physical clocks description */
clk_t clk_clocks[] = {

_CLK(CLKA_REF, &clkgena, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKA_PLL0HS, &clkgena, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA_REF]),
_CLK_P(CLKA_PLL0LS, &clkgena, 450000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA_PLL0HS]),
_CLK_P(CLKA_PLL1, &clkgena, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKA_REF]),


_CLK(CLKA_VTAC_0_PHY, 	&clkgena, 900000000, 0),
_CLK(CLKA_VTAC_1_PHY, 	&clkgena, 900000000, 0),
_CLK(CLKA_STAC_PHY, 	&clkgena, 800000000, 0),
_CLK(CLKA_DCE_PHY, 	&clkgena, 200000000, 0),
_CLK(CLKA_STAC_DIGITAL, &clkgena, 400000000, 0),
_CLK(CLKA_AIP, 		&clkgena, 225000000, 0),
_CLK(CLKA_RESV0, 	&clkgena, 0	   , 0),
_CLK(CLKA_FDMA, 	&clkgena, 400000000, CLK_ALWAYS_ENABLED),
_CLK(CLKA_RESV1, 	&clkgena, 0	   , 0),
_CLK(CLKA_AATV, 	&clkgena, 200000000, 0),
_CLK(CLKA_EMI, 		&clkgena, 133000000, 0),
_CLK(CLKA_GMAC_LPM,	&clkgena,  75000000, 0),
_CLK(CLKA_GMAC, 	&clkgena,  75000000, 0),
_CLK(CLKA_PCI,  	&clkgena, 100000000, CLK_ALWAYS_ENABLED),
_CLK(CLKA_IC_100, 	&clkgena, 100000000, CLK_ALWAYS_ENABLED),
_CLK(CLKA_IC_150, 	&clkgena, 150000000, 0),
_CLK(CLKA_ETHERNET, 	&clkgena,  25000000, CLK_ALWAYS_ENABLED),
_CLK(CLKA_IC_200, 	&clkgena, 200000000, CLK_ALWAYS_ENABLED),

};

clk_t clksouth_clocks[] = {
/* Clockgen A */
_CLK(CLKSOUTH_REF, &clksouth, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

_CLK_P(CLK27_RECOVERY_1, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_SMARTCARD, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_NOUSED_0, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_VDAC */
_CLK_P(CLK_DENC, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_NOTUSED_1, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_PIX_AUX */
_CLK_P(CLK_NOTUSED_2, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_PIX_MDTP_1 */
_CLK_P(CLK_NOTUSED_3, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK27_MCHI */
_CLK_P(CLK27_RECOVERY_2, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),

_CLK_P(CLK_CCSC, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_SECURE, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_SD_MS, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_NOTUSED_4, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_PIX_MDTP_2 */
_CLK_P(CLK_NOTUSED_5, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_GPADC */
_CLK_P(CLK_NOTUSED_6, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_RTC */

_CLK_P(CLK_USB1_48, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_USB2_48, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_USB1_60, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_NOTUSED_7, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_USB2_60 */
_CLK_P(CLK_NOTUSED_8, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* MCTI_VIP */
_CLK_P(CLK_MCTI_MCTI, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]),
_CLK_P(CLK_NOTUSED_9, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_EXT_USB_PHY  */
_CLK_P(CLK_NOTUSED_10, &clksouth,
	 0, 0, &clk_clocks[CLKSOUTH_REF]), /* CLK_GDP_PROC */
};

#define _CLKAUD_P(_id, _ops, _nominal, _flags, _parent, _divider)	\
[_id] = (clk_t){ .name = #_id,  \
		 .id = (_id),						\
		 .ops = (_ops), \
		 .flags = (_flags),					\
		 .parent = (_parent),					\
		 .private_data = (void *)(_divider)			\
}

clk_t clkaudio_clocks[] = {
/* Clockgen A */
_CLK(CLKAUDIO_REF, &clkaudio, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

_CLK_P(CLK_512FS_FREE_RUN, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_256FS_FREE_RUN, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_FS_FREE_RUN, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_256FS_DEC_1, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_FS_DEC_1, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_SPDIF_RX, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_256FS_DEC_2, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
_CLK_P(CLK_FS_DEC_2, &clkaudio,
	0, 0, &clk_clocks[CLKAUDIO_REF]),
};


/******************************************************************************
CLOCKGEN A (CPU+interco+comms) clocks group
******************************************************************************/

/* ========================================================================
Name:	     clkgena_get_index
Description: Returns index of given clockgenA clock and source reg infos
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgena_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	/* Warning: This function assumes clock IDs are perfectly
	   following real implementation order. Each "hole" has therefore
	   to be filled with "CLKx_NOT_USED" */
	if (clkid < CLKA_STAC_DIGITAL || clkid > CLK_NOT_USED_2)
		return -1;

	idx = (clkid - CLKA_STAC_DIGITAL) % 16;

	*srcreg = CKGA_CLKOPSRC_SWITCH_CFG + (0x10 * (idx / 16));
	*shift = 2 * (idx % 16);

	return idx;
}

/* ========================================================================
   Name:	clkgena_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     0=NO error
   ======================================================================== */

static int clkgena_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long clk_src, val;
	int idx, shift;
	unsigned long srcreg;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_STAC_DIGITAL || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKA_REF:
		clk_src = 0;
		break;
	case CLKA_PLL0LS:
	case CLKA_PLL0HS:
		clk_src = 1;
		break;
	case CLKA_PLL1:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	val = CLK_READ(cga_base + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(cga_base + srcreg, val);

	clk_p->parent = &clk_clocks[src_p->id];

	return clkgena_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgena_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_p->id >= CLKA_REF && clk_p->id <= CLKA_PLL1))
		return 0;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	src_sel = (CLK_READ(cga_base + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[CLKA_REF];
		break;
	case 1:
		if (idx <= 3)
			clk_p->parent = &clk_clocks[CLKA_PLL0HS];
		else
			clk_p->parent = &clk_clocks[CLKA_PLL0LS];
		break;
	case 2:
		clk_p->parent = &clk_clocks[CLKA_PLL1];
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_xable_pll(clk_t *clk_p, int enable)
{
	unsigned long val;
	int bit, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKA_PLL0HS && clk_p->id != CLKA_PLL1)
		return CLK_ERR_BAD_PARAMETER;

	bit = (clk_p->id == CLKA_PLL0HS ? 0 : 1);
	val = CLK_READ(cga_base + CKGA_POWER_CFG);
	if (enable)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	CLK_WRITE(cga_base + CKGA_POWER_CFG, val);

	if (enable)
		err = clkgena_recalc(clk_p);
	else
		clk_p->rate = 0;

	return err;
}

/* ========================================================================
   Name:	clkgena_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	if (clk_p->id >= CLKA_PLL0HS && clk_p->id <= CLKA_PLL1)
		return clkgena_xable_pll(clk_p, 1);

	err = clkgena_set_parent(clk_p, clk_p->parent);
	/* clkgena_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:	clkgena_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_PLL0HS || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	/* Can this clock be disabled ? */
	if (clk_p->flags & CLK_ALWAYS_ENABLED)
		return 0;

	/* PLL power down */
	if (clk_p->id >= CLKA_PLL0HS && clk_p->id <= CLKA_PLL1)
		return clkgena_xable_pll(clk_p, 0);

	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	val = CLK_READ(cga_base + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);	 /* 3 = STOP clock */
	CLK_WRITE(cga_base + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

static int clkgena_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA_REF);
	CLK_WRITE(cga_base + offset + (4 * idx), div_cfg);

	return 0;
}

static int clkgena_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div;
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKA_PLL0HS || clk_p->id > CLK_NOT_USED_2)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL set rate: to be completed */
	if ((clk_p->id >= CLKA_PLL0HS) && (clk_p->id <= CLKA_PLL1))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	div = clk_p->parent->rate / freq;
	err = clkgena_set_div(clk_p, &div);
	if (!err)
		clk_p->rate = clk_p->parent->rate / div;

	return err;
}

static int clkgena_recalc(clk_t *clk_p)
{
	unsigned long data, ratio;
	int idx;
	unsigned long srcreg, offset;
	int shift, err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLKA_REF:  /* Clockgen A reference clock */
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLKA_PLL0HS:
		data = CLK_READ(cga_base + CKGA_PLL0_CFG);
		err = clk_pll1600_get_rate(clk_p->parent->rate, data & 0x7,
				(data >> 8) & 0xff, &clk_p->rate);
		return err;
	case CLKA_PLL0LS:
		clk_p->rate = clk_p->parent->rate / 2;
		return 0;
	case CLKA_PLL1:
		data = CLK_READ(cga_base + CKGA_PLL1_CFG);
		return clk_pll800_get_rate(clk_p->parent->rate, data & 0xff,
			(data >> 8) & 0xff, (data >> 16) & 0x7, &clk_p->rate);

	default:
		idx = clkgena_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to source, let's get divider ratio */
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKA_REF);
		data = CLK_READ(cga_base + offset + (4 * idx));

		ratio = (data & 0x1F) + 1;

		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgena_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgena_identify_parent(clk_p);
	if (!err)
		err = clkgena_recalc(clk_p);

	return err;
}


/* ========================================================================
 * 				clkgen South
 * ======================================================================== */

struct clkgen_south_info {
	int id;
	int fsync;
	int fsync_op;
};
static struct clkgen_south_info clkgen_south_data[] = {
	{ CLK27_RECOVERY_1	, 1, 1},
	{ CLK_SMARTCARD		, 1, 2},
	{ CLK_NOUSED_0	 	, -1, -1}, /* CLK_VDAC */
	{ CLK_DENC		, 1, 3},
	{ CLK_NOTUSED_1		, -1, -1}, /* CLK_PIX_AUX */
	{ CLK_NOTUSED_2 	, -1, -1}, /* CLK_PIX_MDTP_1 */
	{ CLK_NOTUSED_3		, -1, -1}, /* CLK27_MCHI */
	{ CLK27_RECOVERY_2	, 1, 2},

	{ CLK_CCSC		, 2, 1},
	{ CLK_SECURE		, 2, 2},
	{ CLK_SD_MS		, 2, 3},
	{ CLK_NOTUSED_4 	, -1, -1}, /* CLK_PIX_MDTP_2 */
	{ CLK_NOTUSED_5		, -1, -1}, /* CLK_GPADC */
	{ CLK_NOTUSED_6		, -1, -1}, /* CLK_RTC */

	{ CLK_USB1_48		, 3, 1},
	{ CLK_USB2_48		, 3, 1},
	{ CLK_USB1_60		, 3, 2},
	{ CLK_NOTUSED_7		, -1, -1}, /* CLK_USB2_60 */
	{ CLK_NOTUSED_8		, -1, -1}, /* MCTI_VIP */
	{ CLK_MCTI_MCTI		, 3, 3},
	{ CLK_NOTUSED_9		, -1, -1}, /* CLK_EXT_USB_PHY  */
	{ CLK_NOTUSED_10	, -1, -1}, /* CLK_GDP_PROC */
};

/* ========================================================================
   Name:	clksouth_fsyn_recalc
   Description: calculate hw params for request clock output.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clksouth_fsyn_recalc(clk_t *clk_p)
{
	unsigned long pe, md, sdiv, nsdv3, val;
	int channel, enabled, err = 0;
	int fsync, fsync_op;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK27_RECOVERY_1 || clk_p->id > CLK_NOTUSED_10)
		return CLK_ERR_BAD_PARAMETER;


	/* FSYN up & running.
	   Computing frequency */
	channel = clk_p->id - CLK27_RECOVERY_1;
	enabled = CLK_READ(clksouth_base + CLGS_CTL_EN) & (1 << channel);

	if (!enabled) { /* Clk not enabled */
		clk_p->rate = 0;
		return 0;
	}
	fsync_op = clkgen_south_data[channel].fsync_op;
	fsync = clkgen_south_data[channel].fsync;

	if (fsync < 0 || fsync_op < 0) {
		clk_p->rate = 0;
		return 0;
	}

	val = CLK_READ(clksouth_base + CLGS_FS_CTL_REG(fsync, fsync_op));

	md = (val & 0x1F);
	sdiv = ((val >> 5) & 0x7);
	pe = ((val>>8) & 0xFFFF);
	nsdv3 = (val & 0x04000000) ? 1 : 0;

	if (clk_p->id > CLK_NOTUSED_6)
		err = clk_4fs432_get_rate(clk_p->parent->rate, pe, md, sdiv,
				nsdv3, &clk_p->rate);
	else
		err = clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv,
			&clk_p->rate);



	return err;
}

static int clksouth_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long val;
	int channel;
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK27_RECOVERY_1 || clk_p->id > CLK_NOTUSED_10)
		return CLK_ERR_BAD_PARAMETER;

	val = CLK_READ(clksouth_base + CLGS_CTL_EN);
	channel = clk_p->id - CLK27_RECOVERY_1;

	if (enable)
		val |= (1 << channel);
	else
		val &= ~(1 << channel);

	CLK_WRITE(clksouth_base + CLGS_CTL_EN, val);
	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clksouth_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:	clksouth_set_rate
   Description: set the request clock output rate
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clksouth_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv, sdiv3;
	int channel;
	unsigned long val;
	int fsync, fsync_op;
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK27_RECOVERY_1 || clk_p->id > CLK_NOTUSED_10)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_p->id > CLK_NOTUSED_6) {
		if (clk_4fs432_get_params(clk_p->parent->rate, freq, &md,
			&pe, &sdiv, &sdiv3))
			return CLK_ERR_BAD_PARAMETER;
	} else {
		if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md,
			&pe, &sdiv))
			return CLK_ERR_BAD_PARAMETER;
	}


	channel = clk_p->id - CLK27_RECOVERY_1;
	fsync_op = clkgen_south_data[channel].fsync_op;
	fsync = clkgen_south_data[channel].fsync;
	val = CLK_READ(clksouth_base + CLGS_FS_CTL_REG(fsync, fsync_op));
	val &= ~(0xFFFFFF);
	val = val | (md | (sdiv << 5) | (pe << 8));

	if (clk_p->id > CLK_NOTUSED_6)
		val  = val | (sdiv3 << 26);

	CLK_WRITE(clksouth_base + CLGS_FS_CTL_REG(fsync, fsync_op), val);

	return clksouth_fsyn_recalc(clk_p);
}

/* ========================================================================
   Name:	clksouth_enable
   Description: enable the requested clock.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clksouth_enable(clk_t *clk_p)
{
	return clksouth_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clksouth_disable
   Description: disable the requested clock.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clksouth_disable(clk_t *clk_p)
{
	return clksouth_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clksouth_init
   Description: Initialize clockgen south clocks.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clksouth_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	/* Fixed Parent  */
	err = clksouth_fsyn_recalc(clk_p);

	return err;
}


/* ========================================================================
 * 				clkgen Audio
 * ======================================================================== */

struct clkgen_audio_info {
	int id;
	int output;
	int divider;
};

/* We assume the audio devices use 128 times oversampling */
static struct clkgen_audio_info clkgen_audio_data[] = {
	{ CLK_512FS_FREE_RUN, 1, 0},
	{ CLK_256FS_FREE_RUN, 1, 2},
	{ CLK_FS_FREE_RUN,    1, 0},
	{ CLK_256FS_DEC_1,    2, 2},
	{ CLK_FS_DEC_1,       2, 0},
	{ CLK_SPDIF_RX,       3, 0},
	{ CLK_256FS_DEC_2,    4, 2},
	{ CLK_FS_DEC_2,       4, 0},
};

/* ========================================================================
   Name:	clkaudio_fsyn_recalc
   Description: calculate hw params for request clock output.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clkaudio_fsyn_recalc(clk_t *clk_p)
{
	unsigned long pe, md, sdiv, val;
	int channel, enabled, err = 0;
	int output;
	int divider;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_512FS_FREE_RUN || clk_p->id > CLK_FS_DEC_2)
		return CLK_ERR_BAD_PARAMETER;

	/* Check if the clock output is enabled */
	channel = clk_p->id - CLK_512FS_FREE_RUN;
	enabled = CLK_READ(clkaudio_base + CLGA_CTL_EN) & (1 << channel);

	if (!enabled) { /* Clk not enabled */
		clk_p->rate = 0;
		return 0;
	}

	/* Get the synth4x output */
	output = clkgen_audio_data[channel].output;

	if (output < 0) {
		clk_p->rate = 0;
		return 0;
	}

	/* Decode md, sdiv and pe */
	val = CLK_READ(clkaudio_base + CLGA_CTL_SYNTH4X_AUD_n(output));

	md = (val & MD__MASK) >> MD;
	sdiv = (val & SDIV__MASK) >> SDIV;
	pe = (val & PE__MASK) >> PE;

	err = clk_fsyn_get_rate(clk_p->parent->rate, pe, md, sdiv,
			&clk_p->rate);

	if (!err) {
		divider = clkgen_audio_data[channel].divider;

		if (divider)
			clk_p->rate /= divider;
	}

	return err;
}

static int clkaudio_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long value;
	int channel;
	int output;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_512FS_FREE_RUN || clk_p->id > CLK_FS_DEC_2)
		return CLK_ERR_BAD_PARAMETER;

	value = CLK_READ(clkaudio_base + CLGA_CTL_EN);
	channel = clk_p->id - CLK_512FS_FREE_RUN;

	if (enable)
		value |= (1 << channel);
	else
		value &= ~(1 << channel);

	CLK_WRITE(clkaudio_base + CLGA_CTL_EN, value);

	output = clkgen_audio_data[channel].output;

	value = CLK_READ(clkaudio_base + CLGA_CTL_SYNTH4X_AUD_n(output));
	value &= ~NSB__MASK;

	if (enable)
		value |= NSB__ACTIVE;
	else
		value |= NSB__STANDBY;

	CLK_WRITE(clkaudio_base + CLGA_CTL_SYNTH4X_AUD_n(output), value);

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkaudio_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;

	return 0;
}

/* ========================================================================
   Name:	clkaudio_set_rate
   Description: set the request clock output rate
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clkaudio_set_rate(clk_t *clk_p, unsigned long freq)
{
	int divider;
	unsigned long md, pe, sdiv;
	int channel;
	int output;
	unsigned long val;


	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLK_512FS_FREE_RUN || clk_p->id > CLK_FS_DEC_2)
		return CLK_ERR_BAD_PARAMETER;

	/* Identify synth4x output */
	channel = clk_p->id - CLK_512FS_FREE_RUN;
	output = clkgen_audio_data[channel].output;
	divider = clkgen_audio_data[channel].divider;

	if (divider)
		freq *= divider;

	/* Compute FSyn params */
	if (clk_fsyn_get_params(clk_p->parent->rate, freq, &md,
			&pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	/* Set the new synth4x value */
	val = CLK_READ(clkaudio_base + CLGA_CTL_SYNTH4X_AUD_n(output));

	val &= ~MD__MASK;
	val |= MD__(md);

	val &= ~SDIV__MASK;
	val |= SDIV__(sdiv);

	val &= ~PE__MASK;
	val |= PE__(pe);

	CLK_WRITE(clkaudio_base + CLGA_CTL_SYNTH4X_AUD_n(output), val);

	return clkaudio_fsyn_recalc(clk_p);
}

/* ========================================================================
   Name:	clkaudio_enable
   Description: enable the requested clock.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clkaudio_enable(clk_t *clk_p)
{
	return clkaudio_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clkaudio_disable
   Description: disable the requested clock.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clkaudio_disable(clk_t *clk_p)
{
	return clkaudio_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clkaudio_init
   Description: Initialize clockgen audio clocks.
   Returns:     0 on success and error code for any errors.
   ======================================================================== */
static int clkaudio_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* CLKAUD has a fixed audio clock parent, so no need to identify */

	err = clkaudio_fsyn_recalc(clk_p);

	return err;
}


int tae_clk_init(clk_t *_sys_clk_in)
{
	int ret = 0;
	unsigned long value;

	clk_clocks[CLKA_REF].parent = _sys_clk_in;
	cga_base = ioremap_nocache(CKGA_BASE_ADDRESS , 0x1000);

	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	if (ret)
		return ret;

	clksouth_base = ioremap_nocache(CLGS_BASE_ADDRESS, 0x60);

	ret = clk_register_table(clksouth_clocks,
					ARRAY_SIZE(clksouth_clocks), 0);
	if (ret)
		return ret;

	clkaudio_base = ioremap_nocache(CLGA_BASE_ADDRESS, 0x30);

	value =  SYNTH4X_AUD_NDIV__30_MHZ;
	value |= SYNTH4X_AUD_SELCLKIN__CLKIN1V2;
	value |= SYNTH4X_AUD_SELBW__VERY_GOOD_REFERENCE;
	value |= SYNTH4X_AUD_NPDA__ACTIVE;
	value |= SYNTH4X_AUD_NRST__NORMAL;
	CLK_WRITE(clkaudio_base + CLGA_CTL_SYNTH4X_AUD, value);

	ret = clk_register_table(clkaudio_clocks,
					ARRAY_SIZE(clkaudio_clocks), 0);

	return ret;
}
