/*****************************************************************************
 *
 * File name   : clock-stxSASG1.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
07/nov/11 fabrice.charpentier@st.com
	  Clocks rename to match datasheet:
	  FDMAx=>FDMA_x, IC_REG=>ICN_REG_0, IC_IF_0=>ICN_IF_0, etc etc
02/nov/11 fabrice.charpentier@st.com
	  clk_pll1600_xxx() renamed to clk_pll1600c65_xxx().
	  A0 & A1 measure functions revisited.
04/May/11 Francesco Virlinzi
	  Inter-dies clock management
	  Linux-Arm (anticipation)
18/jun/10 fabrice.charpentier@st.com
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

struct fsynth_sysconf {
        struct sysconf_field *pe;
        struct sysconf_field *md;
        struct sysconf_field *sdiv;
        struct sysconf_field *prog_en;
};

/* Sysconf from 135 to 150 */
static struct fsynth_sysconf fsynth_vid_channel[4];
/* Sysconf from 313 to 328 */
static struct fsynth_sysconf fsynth_gp_channel[4];

#endif

#include "clock-stxsasg1.h"
#include "clock-regs-stxsasg1.h"
#include "clock-oslayer.h"
#include "clock-common.h"

static int clkgenax_identify_parent(clk_t *clk_p);
static int clkgenax_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenc_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenb_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenc_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenb_fsyn_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgend_fsyn_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenax_recalc(clk_t *clk_p);
static int clkgenb_recalc(clk_t *clk_p);
static int clkgenb_fsyn_recalc(clk_t *clk_p);
static int clkgenc_vcc_recalc(clk_t *clk_p);
static int clkgenc_recalc(clk_t *clk_p);
static int clkgend_recalc(clk_t *clk_p);
static int clkgend_fsyn_recalc(clk_t *clk_p);
static int clkgenax_enable(clk_t *clk_p);
static int clkgenb_enable(clk_t *clk_p);
static int clkgenc_enable(clk_t *clk_p);
static int clkgend_enable(clk_t *clk_p);
static int clkgenax_disable(clk_t *clk_p);
static int clkgenb_disable(clk_t *clk_p);
static int clkgenc_disable(clk_t *clk_p);
static int clkgend_disable(clk_t *clk_p);
static unsigned long clkgenax_get_measure(clk_t *clk_p);
static int clkgenax_init(clk_t *clk_p);
static int clkgenb_init(clk_t *clk_p);
static int clkgenc_init(clk_t *clk_p);
static int clkgend_init(clk_t *clk_p);


#ifdef ST_OS21
static sysconf_base_t sysconf_base[] = {
	{ 0, 99, SYS_SBC_BASE_ADDRESS },
	{ 100, 299, SYS_FRONT_BASE_ADDRESS },
	{ 300, 399, SYS_REAR_BASE_ADDRESS },
	{ 0, 0, 0 }
};
#endif

static void *cga0_base;
static void *cga1_base;
static void *cgb_base;
static void *cgc_base;
static void *cgd_base;

_CLK_OPS(clkgena0,
	"A0",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena0_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO12[0]"       /* Observation point */
);
_CLK_OPS(clkgena1,
	"A1",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena1_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO12[1]"       /* Observation point */
);
_CLK_OPS(clkgenb,
	"B/Audio",
	clkgenb_init,
	clkgenb_set_parent,
	clkgenb_set_rate,
	clkgenb_recalc,
	clkgenb_enable,
	clkgenb_disable,
	clkgenb_observe,
	NULL,	       /* No measure function */
	"PIO10[2]"	  /* Observation point */
);
_CLK_OPS(clkgenc,
	"C/Video",
	clkgenc_init,
	clkgenc_set_parent,
	clkgenc_set_rate,
	clkgenc_recalc,
	clkgenc_enable,
	clkgenc_disable,
	clkgenc_observe,
	NULL,	       /* No measure function */
	"PIO10[3]"	  /* Observation point */
);
_CLK_OPS(clkgend,
	"D/Transport",
	clkgend_init,
	NULL,
	clkgend_fsyn_set_rate,
	clkgend_recalc,
	clkgend_enable,
	clkgend_disable,
	NULL,
	NULL,	       /* No measure function */
	"?"		 /* Observation point */
);

/* Physical clocks description */
static clk_t clk_clocks[] = {
/* Clockgen A0 */
_CLK(CLKS_A0_REF, &clkgena0, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKS_A0_PLL0HS, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKS_A0_REF]),
_CLK_P(CLKS_A0_PLL0LS, &clkgena0, 500000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKS_A0_PLL0HS]),
_CLK_P(CLKS_A0_PLL1, &clkgena0, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKS_A0_REF]),

_CLK(CLKS_FDMA_0,	&clkgena0,    400000000,    0),
_CLK(CLKS_FDMA_1,	&clkgena0,    400000000,    0),
_CLK(CLKS_JIT_SENSE,	&clkgena0,    0,    0),
_CLK(CLKS_ICN_REG_0,	&clkgena0,    100000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_ICN_IF_0,	&clkgena0,    200000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_ICN_REG_LP_0,	&clkgena0,    100000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_EMISS,	&clkgena0,    100000000,    0),
_CLK(CLKS_ETH1_PHY,	&clkgena0,    50000000,    0),
_CLK(CLKS_MII1_REF_CLK_OUT,	&clkgena0,    200000000,    0),

/* Clockgen A1 */
_CLK(CLKS_A1_REF, &clkgena1, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKS_A1_PLL0HS, &clkgena1, 1800000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKS_A1_REF]),
_CLK_P(CLKS_A1_PLL0LS, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKS_A1_PLL0HS]),
_CLK_P(CLKS_A1_PLL1, &clkgena1, 800000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKS_A1_REF]),

_CLK(CLKS_ADP_WC_STAC,	&clkgena1,   0,    0),
_CLK(CLKS_ADP_WC_VTAC,	&clkgena1,   0,    0),
_CLK(CLKS_STAC_TX_CLK_PLL,	&clkgena1,   900000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_STAC,		&clkgena1,   400000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_ICN_IF_2,	&clkgena1,   200000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_CARD_MMC,	&clkgena1,   50000000,    0),
_CLK(CLKS_ICN_IF_1,	&clkgena1,   200000000, CLK_ALWAYS_ENABLED),
_CLK(CLKS_GMAC0_PHY,	&clkgena1,   50000000,    0),
_CLK(CLKS_NAND_CTRL,	&clkgena1,   200000000,    0),
_CLK(CLKS_DCEIMPD_CTRL,	&clkgena1,   25000000,    0),
_CLK(CLKS_MII0_PHY_REF_CLK,	&clkgena1,   129000000,    0),
_CLK(CLKS_TST_MVTAC_SYS,	&clkgena1,   0,    0),

/* Clockgen B */
_CLK(CLKS_B_REF, &clkgenb, 0,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),

_CLK_P(CLKS_B_USB48, &clkgenb, 48000000, 0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_DSS, &clkgenb, 36864000,	0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_DAA, &clkgenb, 32768000,	0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_THSENS_SCARD, &clkgenb, 27000000, 0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_PCM_FSYN0, &clkgenb, 0, 0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_PCM_FSYN1, &clkgenb, 0, 0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_PCM_FSYN2, &clkgenb, 0, 0, &clk_clocks[CLKS_B_REF]),
_CLK_P(CLKS_B_PCM_FSYN3, &clkgenb, 0, 0, &clk_clocks[CLKS_B_REF]),

/* Clockgen video FS+control (called "C") */
_CLK(CLKS_C_REF, &clkgenc, 0,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKS_C_PIX_HD_VCC, &clkgenc, 148500000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKS_C_REF]),
_CLK_P(CLKS_C_PIX_SD_VCC, &clkgenc, 108000000,
		CLK_RATE_PROPAGATES, &clk_clocks[CLKS_C_REF]),

_CLK(CLKS_C_PIX_HDMI, &clkgenc, 148500000, 0),
_CLK(CLKS_C_PIX_DVO, &clkgenc, 148500000, 0),
_CLK(CLKS_C_OUT_DVO, &clkgenc, 148500000, 0),
_CLK(CLKS_C_PIX_HDDAC, &clkgenc, 148500000, 0),
_CLK(CLKS_C_OUT_HDDAC, &clkgenc, 148500000, 0),
_CLK(CLKS_C_DENC, &clkgenc, 27000000, 0),
_CLK(CLKS_C_OUT_SDDAC, &clkgenc, 108000000, 0),
_CLK(CLKS_C_PIX_MAIN, &clkgenc, 148500000, CLK_RATE_PROPAGATES),		/* To MPE */
_CLK(CLKS_C_PIX_AUX, &clkgenc, 148500000, CLK_RATE_PROPAGATES),		/* To MPE */
_CLK(CLKS_C_PACE0, &clkgenc, 27000000, 0),
_CLK(CLKS_C_REF_MCRU, &clkgenc, 27000000, 0),
_CLK(CLKS_C_SLAVE_MCRU, &clkgenc, 27000000, 0),

/* Clockgen D: Generic quad FS (CCSC, MCHI, TSOUT, MMCRU ref clock) */
_CLK(CLKS_D_REF, &clkgend, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKS_D_CCSC, &clkgend, 0,
	0, &clk_clocks[CLKS_D_REF]),
_CLK_P(CLKS_D_PACE1, &clkgend, 0,
	0, &clk_clocks[CLKS_D_REF]),
_CLK_P(CLKS_D_TSOUT1_SRC, &clkgend, 0,
	0, &clk_clocks[CLKS_D_REF]),
_CLK_P(CLKS_D_MCHI, &clkgend, 0,
	0, &clk_clocks[CLKS_D_REF]),
};

/* ========================================================================
   Name:	sasg1_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

//SYSCONF(0, 107, 0, 1);
//SYSCONF(0, 107, 4, 5);
SYSCONF(0, 109, 24, 24);
SYSCONF(0, 109, 25, 25);

/*
 * FS_0:
 * 134: common config
 *	[0  :  0]: reset
 *	[10 : 13]: NSB (1 bit x channel
 *	[14 : 14]: NDPA
 *	[15 : 15]: NDIV
 *	[16 : 17]: BW
 */
SYSCONF(0, 134, 0, 31);

/*
 * FS_1:
 * 312: common config
 *      [0  :  0]: reset
 *      [10 : 13]: NSB (1 bit x channel
 *      [14 : 14]: NDPA
 *      [15 : 15]: NDIV
 *      [16 : 17]: BW
 */
SYSCONF(0, 312, 0, 31);

SYSCONF(0, 379, 0, 15);
SYSCONF(0, 380, 0, 31);
SYSCONF(0, 381, 0, 31);


int __init sasg1_clk_init(clk_t *_sys_clk_in)
{
	int ret;

//	call_platform_sys_claim(107, 0, 1);
//	call_platform_sys_claim(107, 4, 5);
	call_platform_sys_claim(109, 24, 24);
	call_platform_sys_claim(109, 25, 25);

	call_platform_sys_claim(134, 0, 31);
	call_platform_sys_claim(312, 0, 31);

#ifndef ST_OS21
	/* FS_0: ch_1 */
	fsynth_gp_channel[0].pe = platform_sys_claim(135, 0, 4);
	fsynth_gp_channel[0].md = platform_sys_claim(136, 0, 15);
	fsynth_gp_channel[0].sdiv = platform_sys_claim(137, 0, 2);
	fsynth_gp_channel[0].prog_en = platform_sys_claim(138, 0, 0);
	/* FS_0: ch_2 */
	fsynth_gp_channel[1].pe = platform_sys_claim(139, 0, 4);
	fsynth_gp_channel[1].md = platform_sys_claim(140, 0, 15);
	fsynth_gp_channel[1].sdiv = platform_sys_claim(141, 0, 2);
	fsynth_gp_channel[1].prog_en = platform_sys_claim(142, 0, 0);
	/* FS_0: ch_3 */
	fsynth_gp_channel[2].pe = platform_sys_claim(143, 0, 4);
	fsynth_gp_channel[2].md = platform_sys_claim(144, 0, 15);
	fsynth_gp_channel[2].sdiv = platform_sys_claim(145, 0, 2);
	fsynth_gp_channel[2].prog_en = platform_sys_claim(146, 0, 0);
	/*  FS_0: ch_4 */
	fsynth_gp_channel[3].pe = platform_sys_claim(147, 0, 4);
	fsynth_gp_channel[3].md = platform_sys_claim(148, 0, 15);
	fsynth_gp_channel[3].sdiv = platform_sys_claim(149, 0, 2);
	fsynth_gp_channel[3].prog_en = platform_sys_claim(150, 0, 0);

	/* FS_1: ch_1 */
	fsynth_vid_channel[0].pe = platform_sys_claim(313, 0, 4);
	fsynth_vid_channel[0].md = platform_sys_claim(314, 0, 15);
	fsynth_vid_channel[0].sdiv = platform_sys_claim(315, 0, 2);
	fsynth_vid_channel[0].prog_en = platform_sys_claim(316, 0, 0);
	/* FS_1: ch_2 */
	fsynth_vid_channel[1].pe = platform_sys_claim(317, 0, 4);
	fsynth_vid_channel[1].md = platform_sys_claim(318, 0, 15);
	fsynth_vid_channel[1].sdiv = platform_sys_claim(319, 0, 2);
	fsynth_vid_channel[1].prog_en = platform_sys_claim(320, 0, 0);
	/* FS_1: ch_3 */
	fsynth_vid_channel[2].pe = platform_sys_claim(321, 0, 4);
	fsynth_vid_channel[2].md = platform_sys_claim(322, 0, 15);
	fsynth_vid_channel[2].sdiv = platform_sys_claim(323, 0, 2);
	fsynth_vid_channel[2].prog_en = platform_sys_claim(324, 0, 0);
	/*  FS_1: ch_4 */
	fsynth_vid_channel[3].pe = platform_sys_claim(325, 0, 4);
	fsynth_vid_channel[3].md = platform_sys_claim(326, 0, 15);
	fsynth_vid_channel[3].sdiv = platform_sys_claim(327, 0, 2);
	fsynth_vid_channel[3].prog_en = platform_sys_claim(328, 0, 0);
#endif

	call_platform_sys_claim(379, 0, 15);
	call_platform_sys_claim(380, 0, 31);
	call_platform_sys_claim(381, 0, 31);

	clk_clocks[CLKS_A0_REF].parent = _sys_clk_in;
	clk_clocks[CLKS_A1_REF].parent = _sys_clk_in;
	clk_clocks[CLKS_B_REF].parent = _sys_clk_in;
	clk_clocks[CLKS_C_REF].parent = _sys_clk_in;
	clk_clocks[CLKS_D_REF].parent = _sys_clk_in;

	cga0_base = ioremap_nocache(CKGA0_BASE_ADDRESS , 0x1000);
	cga1_base = ioremap_nocache(CKGA1_BASE_ADDRESS , 0x1000);
	cgb_base = ioremap_nocache(CKGB_BASE_ADDRESS , 0x1000);
	cgc_base = ioremap_nocache(CKGC_BASE_ADDRESS , 0x1000);
	cgd_base = ioremap_nocache(CKGD_BASE_ADDRESS , 0x1000);

#ifdef ST_OS21
	printf("Registering SASG1 clocks\n");
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	printf(" => done\n");
#else
	ret = clk_register_table(clk_clocks, CLKS_B_REF, 1);

	ret |= clk_register_table(&clk_clocks[CLKS_B_REF],
				ARRAY_SIZE(clk_clocks) - CLKS_B_REF, 0);
#endif
	return ret;
}

/******************************************************************************
CLOCKGEN Ax clocks groups
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLKS_A1_REF) ? 1 : 0);
}

/* Returns corresponding clockgen Ax base address for 'clk_id' */
static inline unsigned long clkgenax_get_base_address(int clk_id)
{
	static void **clkgenax_base[]={
			&cga0_base, &cga1_base };

	return (unsigned long)*clkgenax_base[clkgenax_get_bank(clk_id)];
}

/* ========================================================================
Name:	clkgenax_get_index
Description: Returns index of given clockgenA clock and source reg infos
Returns:     idx==-1 if error, else >=0
======================================================================== */

static int clkgenax_get_index(int clkid, unsigned long *srcreg, int *shift)
{
	int idx;

	if (clkid >= CLKS_FDMA_0 && clkid <= CLKS_A0_SPARE_17)
		idx = clkid - CLKS_FDMA_0;
	else if (clkid >= CLKS_ADP_WC_STAC && clkid <= CLKS_TST_MVTAC_SYS)
		idx = clkid - CLKS_ADP_WC_STAC;
	else
		return -1;

	*srcreg = CKGA_CLKOPSRC_SWITCH_CFG + (idx / 16) * 0x10;
	*shift = (idx % 16) * 2;
	
	return idx;
}

/* ========================================================================
   Name:	clkgenax_set_parent
   Description: Set clock source for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p)
{
    unsigned long clk_src, val;
	int idx, shift;
    unsigned long srcreg, base;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;

	/* check if they are on the same bank */
	if (clkgenax_get_bank(clk_p->id) != clkgenax_get_bank(src_p->id))
		return CLK_ERR_BAD_PARAMETER;

	switch (src_p->id) {
	case CLKS_A0_REF:
	case CLKS_A1_REF:
		clk_src = 0;
		break;
	case CLKS_A0_PLL0LS:
	case CLKS_A0_PLL0HS:
	case CLKS_A1_PLL0LS:
	case CLKS_A1_PLL0HS:
		clk_src = 1;
		break;
	case CLKS_A0_PLL1:
	case CLKS_A1_PLL1:
		clk_src = 2;
		break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base + srcreg) & ~(0x3 << shift);
	val = val | (clk_src << shift);
	CLK_WRITE(base + srcreg, val);
	clk_p->parent = &clk_clocks[src_p->id];

	return clkgenax_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgenax_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel;
	unsigned long srcreg;
	unsigned long base_addr, base_id;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Statically initialized clocks */
	if ((clk_p->id >= CLKS_A0_REF && clk_p->id <= CLKS_A0_PLL1) ||
	    (clk_p->id >= CLKS_A1_REF && clk_p->id <= CLKS_A1_PLL1))
		return 0;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Identifying source */
	base_addr = clkgenax_get_base_address(clk_p->id);
	base_id = ((clk_p->id >= CLKS_A1_REF) ? CLKS_A1_REF: CLKS_A0_REF);
	src_sel = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	switch (src_sel) {
	case 0:
		clk_p->parent = &clk_clocks[base_id + 0];	/* CLKAx_REF */
		break;
	case 1:
		if (idx <= 3)
			/* CLKAx_PLL0HS */
			clk_p->parent = &clk_clocks[base_id + 1];
		else	/* CLKAx_PLL0LS */
			clk_p->parent = &clk_clocks[base_id + 2];
		break;
	case 2:
		clk_p->parent = &clk_clocks[base_id + 3];	/* CLKAx_PLL1 */
		break;
	case 3:
		clk_p->parent = NULL;
		clk_p->rate = 0;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenax_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenax_init(clk_t *clk_p)
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
   Name:	clkgenax_xable_pll
   Description: Enable/disable PLL
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_xable_pll(clk_t *clk_p, int enable)
{
	unsigned long val, base_addr;
	int bit, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKS_A0_PLL0HS && clk_p->id != CLKS_A0_PLL1 &&
	    clk_p->id != CLKS_A1_PLL0HS && clk_p->id != CLKS_A1_PLL1)
		return CLK_ERR_BAD_PARAMETER;

	#if !defined(CLKLLA_NO_PLL)

	if (clk_p->id == CLKS_A0_PLL1 || clk_p->id == CLKS_A1_PLL1)
		bit = 1;
	else
		bit = 0;
	base_addr = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base_addr + CKGA_POWER_CFG);
	if (enable)
		val &= ~(1 << bit);
	else
		val |= (1 << bit);
	CLK_WRITE(base_addr + CKGA_POWER_CFG, val);

	#endif

	if (enable)
		err = clkgenax_recalc(clk_p);
	else
		clk_p->rate = 0;

	return err;
}

/* ========================================================================
   Name:	clkgenax_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	if ((clk_p->id >= CLKS_A0_PLL0HS && clk_p->id <= CLKS_A0_PLL1) ||
	    (clk_p->id >= CLKS_A1_PLL0HS && clk_p->id <= CLKS_A1_PLL1))
		return clkgenax_xable_pll(clk_p, 1);

	err = clkgenax_set_parent(clk_p, clk_p->parent);
	/* clkgenax_set_parent() is performing also a recalc() */

	return err;
}

/* ========================================================================
   Name:	clkgenax_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;
	unsigned long base_address;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Can this clock be disabled ? */
	if (clk_p->flags & CLK_ALWAYS_ENABLED)
		return 0;

	/* PLL power down */
	if ((clk_p->id >= CLKS_A0_PLL0HS && clk_p->id <= CLKS_A0_PLL1) ||
	    (clk_p->id >= CLKS_A1_PLL0HS && clk_p->id <= CLKS_A1_PLL1))
		return clkgenax_xable_pll(clk_p, 0);

	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Disabling clock */
	base_address = clkgenax_get_base_address(clk_p->id);
	val = CLK_READ(base_address + srcreg) & ~(0x3 << shift);
	val = val | (3 << shift);     /* 3 = STOP clock */
	CLK_WRITE(base_address + srcreg, val);
	clk_p->rate = 0;

	return 0;
}

/* ========================================================================
   Name:	clkgenax_set_div
   Description: Set divider ratio for clockgenA when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;
	unsigned long base_address;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing divider config */
	div_cfg = (*div_p - 1) & 0x1F;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	/* Now according to parent, let's write divider ratio */
	if (clk_p->parent->id >= CLKS_A1_REF)
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKS_A1_REF);
	else
		offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKS_A0_REF);
	base_address = clkgenax_get_base_address(clk_p->id);
	CLK_WRITE(base_address + offset + (4 * idx), div_cfg);

	return 0;
}

/* ========================================================================
   Name:	clkgenax_recalc
   Description: Get CKGA programmed clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_recalc(clk_t *clk_p)
{
	unsigned long data, ratio;
	unsigned long srcreg, offset, base_address;
	int shift, idx;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* If no parent, assuming clock is stopped. Sometimes reset default. */
	if (!clk_p->parent) {
		clk_p->rate = 0;
		return 0;
	}

	/* Reading clock programmed value */
	base_address = clkgenax_get_base_address(clk_p->id);
	switch (clk_p->id) {
	case CLKS_A0_REF:  /* Clockgen A reference clock */
	case CLKS_A1_REF:  /* Clockgen A reference clock */
		clk_p->rate = clk_p->parent->rate;
		break;

	case CLKS_A0_PLL0HS:
	case CLKS_A1_PLL0HS:
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_address + CKGA_PLL0_CFG);
		return clk_pll1600c65_get_rate(clk_p->parent->rate, data & 0x7,
					(data >> 8) & 0xff, &clk_p->rate);
		#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
		return 0;
		#endif
	case CLKS_A0_PLL0LS:
	case CLKS_A1_PLL0LS:
		clk_p->rate = clk_p->parent->rate / 2;
		return 0;
	case CLKS_A0_PLL1:
	case CLKS_A1_PLL1:
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(base_address + CKGA_PLL1_CFG);
		return clk_pll800c65_get_rate(clk_p->parent->rate, data & 0xff,
					   (data >> 8) & 0xff,
					   (data >> 16) & 0x7, &clk_p->rate);
		#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
		return 0;
		#endif

	default:
		idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
		if (idx == -1)
			return CLK_ERR_BAD_PARAMETER;

		/* Now according to source, let's get divider ratio */
		if (clk_p->parent->id >= CLKS_A1_REF)
			offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKS_A1_REF);
		else
			offset = CKGA_SOURCE_CFG(clk_p->parent->id - CLKS_A0_REF);
		data =  CLK_READ(base_address + offset + (4 * idx));
		ratio = (data & 0x1F) + 1;
		clk_p->rate = clk_p->parent->rate / ratio;
		break;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenax_observe
   Description: Clockgen Ax signals observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_observe(clk_t *clk_p, unsigned long *div_p)
{
#ifdef ST_OS21 
	unsigned long src, base_addr;
	unsigned long divcfg;

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id >= CLKS_FDMA_0 && clk_p->id <= CLKS_A0_SPARE_17)
		src = 0x8 + clk_p->id - CLKS_FDMA_0;
	else if (clk_p->id >= CLKS_ADP_WC_STAC &&
		clk_p->id <= CLKS_TST_MVTAC_SYS)
		src = 0x8 + clk_p->id - CLKS_ADP_WC_STAC;
	else if (clk_p->id == CLKS_A0_PLL0HS && clk_p->id == CLKS_A1_PLL0HS) {
		src = 0x1;
		*div_p = 4;	/* Predivided by 4 */
	} else if (clk_p->id == CLKS_A0_PLL1 || clk_p->id == CLKS_A1_PLL1) {
		src = 0x4;
		*div_p = 4;	/* Predivided by 4 */
	} else
		return CLK_ERR_BAD_PARAMETER;

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
	base_addr = clkgenax_get_base_address(clk_p->id);
	CLK_WRITE((base_addr + CKGA_CLKOBS_MUX1_CFG), (divcfg << 6) | src);

	/* Observation points:
	   A0 => PIO12[0]
	   A1 => PIO12[1] */

	/* Configuring PIO for clock output */
	if (base_addr == (unsigned long)cga0_base) {
		SYSCONF_WRITE(0, 107, 0, 1, 3);	/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 109, 24, 24, 1);	/* Enabling output */
	} else {
		SYSCONF_WRITE(0, 107, 4, 5, 3);	/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 109, 25, 25, 1);	/* Enabling output */
	}
#else
#warning "clk_observe disabled"
#endif
	return 0;
}

/* ========================================================================
   Name:	clkgenax_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgenax_get_measure(clk_t *clk_p)
{
	unsigned long src, base_addr;
	unsigned long data, measure;
	int i;

	if (!clk_p)
		return 0;
	if (clk_p->id >= CLKS_FDMA_0 && clk_p->id <= CLKS_A0_SPARE_17)
		src = 0x8 + clk_p->id - CLKS_FDMA_0;
	else if (clk_p->id >= CLKS_ADP_WC_STAC &&
		clk_p->id <= CLKS_TST_MVTAC_SYS)
		src = 0x8 + clk_p->id - CLKS_ADP_WC_STAC;
	else
		return 0;

	measure = 0;
	base_addr = clkgenax_get_base_address(clk_p->id);

	/* Loading the MAX Count 1000 in 30MHz Oscillator Counter */
	CLK_WRITE(base_addr + CKGA_CLKOBS_MASTER_MAXCOUNT, 0x3E8);
	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 3);

	/* Selecting clock to observe */
	CLK_WRITE(base_addr + CKGA_CLKOBS_MUX1_CFG, (1 << 7) | src);

	/* Start counting */
	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 0);

	for (i = 0; i < 10; i++) {
		__mdelay(10);

		data = CLK_READ(base_addr + CKGA_CLKOBS_STATUS);
		if (data & 1)
			break; /* IT */
	}
	if (i == 10)
		return 0;

	/* Reading value */
	data = CLK_READ(base_addr + CKGA_CLKOBS_SLAVE0_COUNT);
	measure = 30 * data * 1000;

	CLK_WRITE(base_addr + CKGA_CLKOBS_CMD, 3);

	return measure;
}

/******************************************************************************
CLOCKGEN A0 clocks groups
******************************************************************************/

/* ========================================================================
   Name:	clkgena0_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, mdiv, ndiv, pdiv, data;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_A0_PLL0HS || clk_p->id > CLKS_MII1_REF_CLK_OUT)
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKS_A0_PLL0HS:
	case CLKS_A0_PLL0LS:
		if (clk_p->id == CLKS_A0_PLL0LS)
			freq = freq * 2;
		err = clk_pll1600c65_get_params(clk_clocks[CLKS_A0_REF].rate,
					     freq, &mdiv, &ndiv);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga0_base + CKGA_PLL0_CFG) &
			~(0xff << 8) & ~0x7;
		data = data | ((ndiv & 0xff) << 8) | (mdiv & 0x7);
		CLK_WRITE(cga0_base + CKGA_PLL0_CFG, data);
		if (clk_p->id == CLKS_A0_PLL0LS)
			err = clkgenax_recalc(&clk_clocks[CLKS_A0_PLL0HS]);
		#endif
		break;
	case CLKS_A0_PLL1:
		err = clk_pll800c65_get_params(clk_clocks[CLKS_A0_REF].rate,
					     freq, &mdiv, &ndiv, &pdiv);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga0_base + CKGA_PLL1_CFG)
			& 0xfff80000;
		data |= (pdiv<<16 | ndiv<<8 | mdiv);
		CLK_WRITE(cga0_base + CKGA_PLL1_CFG, data);
		#endif
		break;
	case CLKS_FDMA_0 ... CLKS_MII1_REF_CLK_OUT:
		div = clk_best_div(clk_p->parent->rate, freq);
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
CLOCKGEN A1 (A right) clocks group
******************************************************************************/

/* ========================================================================
   Name:	clkgena1_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, mdiv, ndiv, pdiv, data;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKS_A1_PLL0HS) || (clk_p->id > CLKS_TST_MVTAC_SYS))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKS_A1_PLL0HS:
	case CLKS_A1_PLL0LS:
		if (clk_p->id == CLKS_A1_PLL0LS)
			freq = freq * 2;
		err = clk_pll1600c65_get_params(clk_clocks[CLKS_A1_REF].rate,
					     freq, &mdiv, &ndiv);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga1_base + CKGA_PLL0_CFG) &
			~(0xff << 8) & ~0x7;
		data = data | ((ndiv & 0xff) << 8) | (mdiv & 0x7);
		CLK_WRITE(cga1_base + CKGA_PLL0_CFG, data);
		if (clk_p->id == CLKS_A1_PLL0LS)
			err = clkgenax_recalc(&clk_clocks[CLKS_A1_PLL0HS]);
		#endif
		break;
	case CLKS_A1_PLL1:
		err = clk_pll800c65_get_params(clk_clocks[CLKS_A1_REF].rate,
					     freq, &mdiv, &ndiv, &pdiv);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga1_base + CKGA_PLL1_CFG)
				& 0xfff80000;
		data |= (pdiv<<16 | ndiv<<8 | mdiv);
		CLK_WRITE(cga1_base + CKGA_PLL1_CFG, data);
		#endif
		break;
	case CLKS_ADP_WC_STAC ... CLKS_MII0_PHY_REF_CLK:
	case CLKS_TST_MVTAC_SYS:
		div = clk_best_div(clk_p->parent->rate, freq);
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
CLOCKGEN B/USB+DSS+Audio
******************************************************************************/

static void clkgenb_unlock(void)
{
	CLK_WRITE(cgb_base + CKGB_LOCK, 0xc0de);
}

static void clkgenb_lock(void)
{
	CLK_WRITE(cgb_base + CKGB_LOCK, 0xc1a0);
}

/* ========================================================================
   Name:	clkgenb_fsyn_xable
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long val, clkout, ctrl, bit, ctrlval;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id < CLKS_B_PCM_FSYN0) {
		clkout = CKGB_FS0_CLKOUT_CTRL;
		ctrl = CKGB_FS0_CTRL;
	} else {
		clkout = CKGB_FS1_CLKOUT_CTRL;
		ctrl = CKGB_FS1_CTRL;
	}

	#if !defined(CLKLLA_NO_PLL)

	clkgenb_unlock();

	/* Powering down/up digital part */
	val = CLK_READ(cgb_base + clkout);
	bit = (clk_p->id - CLKS_B_USB48) % 4;
	if (enable)
		val |= (1 << bit);
	else
		val &= ~(1 << bit);
	CLK_WRITE(cgb_base + clkout, val);

	/* Powering down/up analog part */
	ctrlval = CLK_READ(cgb_base + ctrl);
	if (enable)
		ctrlval |= (1 << 4);
	else {
		/* If all channels are off then power down FSx */
		if ((val & 0xf) == 0)
			ctrlval &= ~(1 << 4);
	}
	CLK_WRITE(cgb_base + ctrl, ctrlval);

	clkgenb_lock();

	#endif

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenb_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:	clkgenb_enable
   Description: Enable clock or FSYN (clockgen B)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_enable(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	return clkgenb_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clkgenb_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_disable(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	return clkgenb_fsyn_xable(clk_p, 0);
}

/* ========================================================================
   Name:	clkgenb_set_parent
   Description: Set clock source for clockgenB when possible
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	if (!clk_p || !parent_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Is there any useable mux in this clockgen ? */

	return 0;
}

/* ========================================================================
   Name:	clkgenb_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_set_rate(clk_t *clk_p, unsigned long freq)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	/* No clockgen B internal divider used. Hence all clocks can be considered
	   as FS output */
	if (clk_p->id == CLKS_B_PCM_FSYN3) {
		/* Disable power down reset state */
		clkgenb_unlock();
		CLK_WRITE(cgb_base + CKGB_POWER_DOWN, 0);
		clkgenb_lock();
	}
	err = clkgenb_fsyn_set_rate(clk_p, freq);
	if (!err)
		err = clkgenb_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:	clkgenb_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_fsyn_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	int bank, channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	#if !defined(CLKLLA_NO_PLL)

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fs216c65_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	bank = (clk_p->id - CLKS_B_USB48) / 4;
	channel = (clk_p->id - CLKS_B_USB48) % 4;

	clkgenb_unlock();
	CLK_WRITE(cgb_base + CKGB_FS_MD(bank, channel), md);
	CLK_WRITE(cgb_base + CKGB_FS_PE(bank, channel), pe);
	CLK_WRITE(cgb_base + CKGB_FS_SDIV(bank, channel), sdiv);
	CLK_WRITE(cgb_base + CKGB_FS_EN_PRG(bank, channel), 0x1);
	CLK_WRITE(cgb_base + CKGB_FS_EN_PRG(bank, channel), 0x0);
	clkgenb_lock();

	#endif

	return 0;
}

/* ========================================================================
   Name:	clkgenb_observe
   Description: Allows to observe a clock on a PIO5_2
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_observe(clk_t *clk_p, unsigned long *div_p)
{
#ifdef ST_OS21
	unsigned long out0, out1 = 0;
	static const unsigned long observe_table[] = { 3, 9, 10, 12, 8, 11, 11, 13 };

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	out0 = observe_table[clk_p->id - CLKS_B_USB48];
	if (out0 == 0xff)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;

	if (clk_p->id == CLKS_B_PCM_FSYN2 || clk_p->id == CLKS_B_THSENS_SCARD) {
		out1 = out0;
		out0 = 12;
	}
	clkgenb_unlock();
	CLK_WRITE((cgb_base + CKGB_OUT_CTRL), (out1 << 4) | out0);
	clkgenb_lock();

	/* Configuring corresponding PIO */
	SYSCONF_WRITE(0, 105, 8, 10, 5);	/* Selecting alt mode 5 */
	SYSCONF_WRITE(0, 109, 10, 10, 1);	/* Enabling IO */

	/* No possible predivider on clockgen B */
	*div_p = 1;
#else
#warning "clk_observe disabled"
#endif
	return 0;
}

/* ========================================================================
   Name:	clkgenb_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgenb_fsyn_recalc(clk_t *clk_p)
{
	unsigned long val, clkout, ctrl;
	unsigned long pe, md, sdiv;
	int bank, channel;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_USB48 || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	#if !defined(CLKLLA_NO_PLL)

	/* Which FSYN control registers to use ? */
	clkout = (clk_p->id < CLKS_B_PCM_FSYN0 ? CKGB_FS0_CLKOUT_CTRL : CKGB_FS1_CLKOUT_CTRL);
	ctrl = (clk_p->id < CLKS_B_PCM_FSYN0 ? CKGB_FS0_CTRL : CKGB_FS1_CTRL);
	
	/* Is FSYN analog part UP ? */
	val = CLK_READ(cgb_base + ctrl);
	if ((val & (1 << 4)) == 0) {	/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	channel = (clk_p->id - CLKS_B_USB48) % 4;

	/* Is FSYN digital part UP ? */
	val = CLK_READ(cgb_base + clkout);
	if ((val & (1 << channel)) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	bank = (clk_p->id - CLKS_B_USB48) / 4;
	pe = CLK_READ(cgb_base + CKGB_FS_PE(bank, channel));
	md = CLK_READ(cgb_base + CKGB_FS_MD(bank, channel));
	sdiv = CLK_READ(cgb_base + CKGB_FS_SDIV(bank, channel));
	return clk_fs216c65_get_rate(clk_p->parent->rate,
				pe, md, sdiv, &clk_p->rate);

	#else

	if (clk_p->nominal_rate)
		clk_p->rate = clk_p->nominal_rate;
	else
		clk_p->rate = 12121212;
	return 0;

	#endif
}

/* ========================================================================
   Name:	clkgenb_recalc
   Description: Get CKGB clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenb_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_REF || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKS_B_REF) {
		clk_p->rate = clk_p->parent->rate;
		return 0;
	}

	return clkgenb_fsyn_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgenb_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenb_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_B_REF || clk_p->id > CLKS_B_PCM_FSYN3)
		return CLK_ERR_BAD_PARAMETER;

	/* All clocks have static parent */
	return clkgenb_recalc(clk_p);
}

/******************************************************************************
CLOCKGEN C (video & transport)
Quad FSYN + Video Clock Controller
******************************************************************************/

/* ========================================================================
   Name:	clkgenc_fsyn_recalc
   Description: Get CKGC FSYN clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_fsyn_recalc(clk_t *clk_p)
{
	unsigned long cfg, dig_bit;
	unsigned long pe, md, sdiv;
	int channel, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_C_PIX_HD_VCC || clk_p->id > CLKS_C_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	#if !defined(CLKLLA_NO_PLL)

	/* Checking FSYN analog & reset status */
	cfg = SYSCONF_READ(0, 312, 0, 31);
	if ((cfg  & ((1 << 14) | (1 << 0))) != ((1 << 14) | (1 << 0))) {
		/* Analog power down or reset */
		clk_p->rate = 0;
		return 0;
	}

	channel = clk_p->id - CLKS_C_PIX_HD_VCC;

	/* Checking FSYN digital part */
	dig_bit = 10 + channel;
	if ((cfg & (1 << dig_bit)) == 0) {	/* digital part in standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN up & running.
	   Computing frequency */
#ifdef ST_OS21
	pe = SYSCONF_READ(0, 314 + (4 * channel), 0, 15);
	md = SYSCONF_READ(0, 313 + (4 * channel), 0, 4);
	sdiv = SYSCONF_READ(0, 315 + (4 * channel), 0, 2);
#else
	pe = sysconf_read(fsynth_vid_channel[channel].pe);
	md = sysconf_read(fsynth_vid_channel[channel].md);
	sdiv = sysconf_read(fsynth_vid_channel[channel].sdiv);
#endif
	err = clk_fs216c65_get_rate(clk_p->parent->rate, pe, md,
				sdiv, &clk_p->rate);

	#else

	if (clk_p->nominal_rate)
		clk_p->rate = clk_p->nominal_rate;
	else
		clk_p->rate = 12121212;

	#endif

	return err;
}

/* ========================================================================
   Name:	clkgenc_vcc_recalc
   Description: Update Video Clock Controller outputs value
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_vcc_recalc(clk_t *clk_p)
{
	int chan;
	unsigned long val;
	static unsigned char tab1248[] = { 1, 2, 4, 8 };

	if (clk_p->id < CLKS_C_PIX_HDMI || clk_p->id > CLKS_C_SLAVE_MCRU)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKS_C_PIX_HDMI;
	/* Is the channel stopped ? */
	val = (SYSCONF_READ(0, 379, 0, 15) >> chan) & 1;
	if (val)	/* 1=stopped */
		clk_p->rate = 0;
	else {
		/* What is the divider ratio ? */
		val = (SYSCONF_READ(0, 381, 0, 31)
				>> (chan * 2)) & 3;
		clk_p->rate = clk_p->parent->rate / tab1248[val];
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenc_recalc
   Description: Get CKGC clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKS_C_REF:
		clk_p->rate = clk_p->parent->rate;
		break;
	case CLKS_C_PIX_HD_VCC ... CLKS_C_FS0_CH4:	/* FS0 clocks */
		return clkgenc_fsyn_recalc(clk_p);
	case CLKS_C_PIX_HDMI ... CLKS_C_SLAVE_MCRU:	/* VCC clocks */
		return clkgenc_vcc_recalc(clk_p);
	default:
		return CLK_ERR_BAD_PARAMETER;
	}

	return 0;
}

/* ========================================================================
   Name:	clkgenc_vcc_set_div
   Description: Video Clocks Controller divider setup function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_vcc_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int chan;
	unsigned long set, val;
	static const unsigned char div_table[] = {
		/* 1  2     3  4     5     6     7  8 */
		0, 1, 0xff, 2, 0xff, 0xff, 0xff, 3 };

	if (clk_p->id < CLKS_C_PIX_HDMI || clk_p->id > CLKS_C_SLAVE_MCRU)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKS_C_PIX_HDMI;
	if (*div_p < 1 || *div_p > 8)
		return CLK_ERR_BAD_PARAMETER;

	set = div_table[*div_p - 1];
	if (set == 0xff)
		return CLK_ERR_BAD_PARAMETER;

	/* Set SYSTEM_CONFIG381: div_mode, 2bits per channel */
	val = SYSCONF_READ(0, 381, 0, 31);
	val &= ~(3 << (chan * 2));
	val |= set << (chan * 2);
	SYSCONF_WRITE(0, 381, 0, 31, val);

	return 0;
}

/* ========================================================================
   Name:	clkgenc_set_rate
   Description: Set CKGC clocks frequencies
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	unsigned long val;
	int channel, err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKS_C_PIX_HD_VCC) || (clk_p->id > CLKS_C_SLAVE_MCRU))
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_p->id >= CLKS_C_PIX_HD_VCC) && (clk_p->id <= CLKS_C_FS0_CH4)) {
		if (clk_fs216c65_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
			return CLK_ERR_BAD_PARAMETER;

		channel = clk_p->id - CLKS_C_PIX_HD_VCC;

		val = SYSCONF_READ(0, 312, 0, 31);
		/* Removing reset, digit standby and analog standby */
		val |= (1 << 14) | (1 << (10 + channel)) | (1 << 0);
		SYSCONF_WRITE(0, 312, 0, 31, val);

#ifdef ST_OS21
		SYSCONF_WRITE(0, 314 + (4 * channel), 0, 15, pe);
		SYSCONF_WRITE(0, 313 + (4 * channel), 0, 4, md);
		SYSCONF_WRITE(0, 315 + (4 * channel), 0, 2, sdiv);
		SYSCONF_WRITE(0, 316 + (4 * channel), 0, 0, 0x01);
		SYSCONF_WRITE(0, 316 + (4 * channel), 0, 0, 0x00);
#else
		sysconf_write(fsynth_vid_channel[channel].pe, pe);
		sysconf_write(fsynth_vid_channel[channel].md, md);
		sysconf_write(fsynth_vid_channel[channel].sdiv, sdiv);
		sysconf_write(fsynth_vid_channel[channel].prog_en, 1);
		sysconf_write(fsynth_vid_channel[channel].prog_en, 0);
#endif
	} else { /* Video Clock Controller clocks */
		unsigned long div;

		div = clk_best_div(clk_p->parent->rate, freq);
		err = clkgenc_vcc_set_div(clk_p, &div);
	}

	/* Recomputing freq from real HW status */
	return clkgenc_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgenc_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_identify_parent(clk_t *clk_p)
{
	unsigned long chan, val;

	if (clk_p->id >= CLKS_C_REF && clk_p->id <= CLKS_C_FS0_CH4)
		return 0; /* These clocks have static parent */

	/* Clocks from "Video Clock Controller". */
	if ((clk_p->id >=CLKS_C_PIX_HDMI) && (clk_p->id <= CLKS_C_SLAVE_MCRU)) {
		chan = clk_p->id - CLKS_C_PIX_HDMI;
		val = SYSCONF_READ(0, 380, 0, 31);
		val >>= (chan * 2);
		val &= 0x3;
		/* sel : 00 clk_hd, 01 clk_sd, 10 clk_hd_ext, 11 clk_sd_ext
			 clk_hd_ext & clk_sd_ext are unused */
		switch (val) {
		case 0: clk_p->parent = &clk_clocks[CLKS_C_PIX_HD_VCC];
			break;
		case 1: clk_p->parent = &clk_clocks[CLKS_C_PIX_SD_VCC];
			break;
		default:
			break;
		}
	}

	/* Other clockgen C clocks are statically initialized
	   thanks to _CLK_P() macro */

	return 0;
}

/* ========================================================================
   Name:	clkgenc_set_parent
   Description: Set parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_set_parent(clk_t *clk_p, clk_t *parent_p)
{
	unsigned long chan, val, data;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_C_PIX_HDMI || clk_p->id > CLKS_C_SLAVE_MCRU)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks from "Video Clock Controller". */
	/* sel : 00 clk_hd, 01 clk_sd, 10 clk_hd_ext, 11 clk_sd_ext
			 clk_hd_ext & clk_sd_ext are unused */
	chan = clk_p->id - CLKS_C_PIX_HDMI;
	switch (parent_p->id) {
	case CLKS_C_PIX_HD_VCC: val = 0; break;
	case CLKS_C_PIX_SD_VCC: val = 1; break;
	default:
		return CLK_ERR_BAD_PARAMETER;
	}
	data = SYSCONF_READ(0, 380, 0, 31);
	data &= ~(0x3 << (chan * 2));
	data |= (val << (chan * 2));
	SYSCONF_WRITE(0, 380, 0, 31, data);
	clk_p->parent = parent_p;

	return clkgenc_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgenc_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_C_REF || clk_p->id > CLKS_C_SLAVE_MCRU)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgenc_identify_parent(clk_p);
	if (!err)
		err = clkgenc_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:	clkgenc_fsyn_xable
   Description: Enable/Disable FSYN. If all channels OFF, FSYN is powered
		down.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long val, chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_C_PIX_HD_VCC || clk_p->id > CLKS_C_FS0_CH4)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKS_C_PIX_HD_VCC;

	val = SYSCONF_READ(0, 312, 0, 31);
	/* Powering down/up digital part */
	if (enable) {
		/* Powering up digital part */
		val |= (1 << (10 + chan));
		/* Powering up analog part */
		val |= (1 << 14);
	} else {
		/* Powering down digital part */
		val &= ~(1 << (10 + chan));
		/* If all channels are off then power down FS0 */
		if ((val & 0x3c00) == 0)
			val &= ~(1 << 14);
	}
	SYSCONF_WRITE(0, 312, 0, 31, val);

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenc_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:	clkgenc_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_enable(clk_t *clk_p)
{
	return clkgenc_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clkgenc_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenc_disable(clk_t *clk_p)
{
	return clkgenc_fsyn_xable(clk_p, 0);
}

/* ========================================================================
   Name:	clkgenc_observe
   Description: Clockgen C clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenc_observe(clk_t *clk_p, unsigned long *div_p)
{
#ifdef ST_OS21
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_C_PIX_HDMI || clk_p->id > CLKS_C_SLAVE_MCRU)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKS_C_PIX_HDMI;
	SYSCONF_WRITE(0, 378, 0, 3, chan);

	/* Configuring corresponding PIO (PIO10[3]) */
	SYSCONF_WRITE(0, 105, 12, 14, 5);	/* Selecting alt mode 5 */
	SYSCONF_WRITE(0, 109, 11, 11, 1);	/* Enabling IO */

	/* No possible predivider on this clockgen */
	*div_p = 1;
#else
#warning "clk_observe disabled"
#endif

	return 0;
}

/******************************************************************************
CLOCKGEN D (CCSC, MCHI, TSout src, ref clock for MMCRU)
******************************************************************************/

/* ========================================================================
   Name:	clkgend_fsyn_recalc
   Description: Get CKGD FSYN clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgend_fsyn_recalc(clk_t *clk_p)
{
	unsigned long cfg, dig_bit;
	unsigned long pe, md, sdiv;
	int channel, err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_D_CCSC || clk_p->id > CLKS_D_MCHI)
		return CLK_ERR_BAD_PARAMETER;

	/* Checking FSYN analog status */
	cfg = SYSCONF_READ(0, 134, 0, 31);
	if ((cfg & (1 << 14)) == 0) {   /* Analog power down */
		clk_p->rate = 0;
		return 0;
	}

	/* Checking FSYN digital part */
	dig_bit = 10 + clk_p->id - CLKS_D_CCSC;
	if ((cfg & (1 << dig_bit)) == 0) { /* digital part in standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN up & running.
	   Computing frequency */
	channel = clk_p->id - CLKS_D_CCSC;
#ifdef ST_OS21
	pe = SYSCONF_READ(0, 136 + (4 * channel), 0, 15);
	md = SYSCONF_READ(0, 135 + (4 * channel), 0, 4);
	sdiv = SYSCONF_READ(0, 137 + (4 * channel), 0, 2);
#else
	pe = sysconf_read(fsynth_gp_channel[channel].pe);
	md = sysconf_read(fsynth_gp_channel[channel].md);
	sdiv = sysconf_read(fsynth_gp_channel[channel].sdiv);
#endif
	err = clk_fs216c65_get_rate(clk_p->parent->rate, pe, md, sdiv,
		&clk_p->rate);

	return err;
}

/* ========================================================================
   Name:	clkgend_recalc
   Description: Get CKGD clocks frequencies function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgend_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKS_D_REF)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id >= CLKS_D_CCSC && clk_p->id <= CLKS_D_MCHI)
		return clkgend_fsyn_recalc(clk_p);
	else
		return CLK_ERR_BAD_PARAMETER;

	return 0;
}

/* ========================================================================
   Name:	clkgend_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Parents are static. No idenfication required */
	return clkgend_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgend_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgend_fsyn_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long md, pe, sdiv;
	int channel;
	unsigned long val;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;
	if (clk_p->id < CLKS_D_CCSC || clk_p->id > CLKS_D_MCHI)
		return CLK_ERR_BAD_PARAMETER;

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fs216c65_get_params(clk_p->parent->rate, freq, &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	channel = clk_p->id - CLKS_D_CCSC;
	val = SYSCONF_READ(0, 134, 0, 31);
	/* Power up, release digit reset & FS reset */
	val |= (1 << 14) | (1 << (10 + channel)) | (1 << 0);
	SYSCONF_WRITE(0, 134, 0, 31, val);

#ifdef ST_OS21
	SYSCONF_WRITE(0, 136 + (4 * channel), 0, 15, pe);
	SYSCONF_WRITE(0, 135 + (4 * channel), 0, 4, md);
	SYSCONF_WRITE(0, 137 + (4 * channel), 0, 2, sdiv);
	SYSCONF_WRITE(0, 138 + (4 * channel), 0, 0, 1);
	SYSCONF_WRITE(0, 138 + (4 * channel), 0, 0, 0);
#else
	sysconf_write(fsynth_gp_channel[channel].pe, pe);
	sysconf_write(fsynth_gp_channel[channel].md, md);
	sysconf_write(fsynth_gp_channel[channel].sdiv, sdiv);
	sysconf_write(fsynth_gp_channel[channel].prog_en, 1);
	sysconf_write(fsynth_gp_channel[channel].prog_en, 0);
#endif

	return clkgend_recalc(clk_p);
}

/* ========================================================================
   Name:	clkgend_fsyn_xable
   Description: Enable/Disable FSYN. If all channels OFF, FSYN is powered
		down.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgend_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long val;
	int channel;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKS_D_CCSC || clk_p->id > CLKS_D_MCHI)
		return CLK_ERR_BAD_PARAMETER;

	val = SYSCONF_READ(0, 134, 0, 31);
	channel = clk_p->id - CLKS_D_CCSC;

	/* Powering down/up digital part */
	if (enable) {
		val |= (1 << (10 + channel));
		val |= (1 << 14) | (1 << 0);
	} else {
		val &= ~(1 << (10 + channel));
		if ((val & 0x3c00) == 0)
			val &= ~(1 << 14);
	}
	SYSCONF_WRITE(0, 134, 0, 31, val);

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgend_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:	clkgend_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_enable(clk_t *clk_p)
{
	return clkgend_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:	clkgend_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgend_disable(clk_t *clk_p)
{
	return clkgend_fsyn_xable(clk_p, 0);
}

