/*****************************************************************************
 *
 * File name   : clock-stxMPE41.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
18/nov/11 Fabrice Charpentier
	  clkgene_fsyn_set_rate() bug fix for SDIV setup.
	  FS660 new API with nsdiv.
14/nov/11 Fabrice Charpentier
	  Added clockgen E enable/disable functions + clkgen F xable changes.
03/nov/11 Fabrice Charpentier
	  Clocks rename to match datasheet.
	  A10: PP_DMUx=>PP_DMU_x, NOC_DISP=>ICN_DISP, EXT2F_A9=>A9_EXT2FS,
	    ST231DMUx=>ST231_DMU_x, ST231AUD=>ST231_AUD, ST231GPx=>ST231_GP_x,
	    NOC_CPU=>ICN_CPU, IC_DMUx=>ICN_DMU_x, IC_RAM=>ICN_ERAM
	  A11: FDMAx=>FDMA_x, NOC_LMI=>ICN_LMI, PROC_TP=>TP, NOC_GPU=>ICN_GPU,
	    NOC_TV=>ICN_VDP_0, IC_TV_MCTI=>ICN_VDP_1, IC_TV=>ICN_VDP_2,
	    IC_TV_SC_STR=>ICN_VDP_3, IC_REG=ICN_REG_10
	  A12: PROC_COMPO_x=>COMPO_x, PROC_BDISPx=>BDISP_x,
	    IC_BDISPx=>ICN_BDISP_x, IC_COMPO=>ICN_COMPO,
	    IC_TS=>ICN_TS, IC_REG_LP=>ICN_REG_LP_10.
27/oct/11 Fabrice Charpentier
	  Revisited GPU functions & identifiers.
12/oct/11 Fabrice Charpentier
	  clockgenf_set_rate(), clkgena9_set_rate() &
	  clkgenddr_set_rate() fixes.
04/May/11 Francesco Virlinzi
	  Inter-dies clock management
	  Linux-Arm (anticipation)
07/mar/11 fabrice.charpentier@st.com
	  Updates & testing on emulator.
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

#endif

#include "clock-stxmpe41.h"
#include "clock-regs-stxmpe41.h"
#include "clock-oslayer.h"
#include "clock-common.h"


static int clkgenax_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenax_observe2(clk_t *clk_p, unsigned long *div_p);
static int clkgenf_observe(clk_t *clk_p, unsigned long *div_p);
static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgenf_set_parent(clk_t *clk_p, clk_t *src_p);
static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena2_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgene_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenf_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenddr_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgena9_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgengpu_set_rate(clk_t *clk_p, unsigned long freq);
static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p);
static int clkgenax_recalc(clk_t *clk_p);
static int clkgene_recalc(clk_t *clk_p);
static int clkgenf_recalc(clk_t *clk_p);
static int clkgenddr_recalc(clk_t *clk_p);
static int clkgena9_recalc(clk_t *clk_p);
static int clkgengpu_recalc(clk_t *clk_p);
static int clkgenax_enable(clk_t *clk_p);
static int clkgene_enable(clk_t *clk_p);
static int clkgenf_enable(clk_t *clk_p);
static int clkgenax_disable(clk_t *clk_p);
static int clkgene_disable(clk_t *clk_p);
static int clkgenf_disable(clk_t *clk_p);
static unsigned long clkgenax_get_measure(clk_t *clk_p);
static int clkgenax_init(clk_t *clk_p);
static int clkgene_init(clk_t *clk_p);
static int clkgenf_init(clk_t *clk_p);
static int clkgenddr_init(clk_t *clk_p);
static int clkgena9_init(clk_t *clk_p);
static int clkgengpu_init(clk_t *clk_p);
static int clkgenax_identify_parent(clk_t *clk_p);


#ifdef ST_OS21
static sysconf_base_t sysconf_base[] = {
	{ 400, 499, SYS_LEFT_BASE_ADDRESS },
	{ 500, 599, SYS_RIGHT_BASE_ADDRESS },
	{ 600, 699, SYS_SYSTEM_BASE_ADDRESS },
	{ 0, 0, 0 }
};
#endif

static void *cga0_base;
static void *cga1_base;
static void *cga2_base;
static void *cgb_base;
static void *cgd_base;
static void *mali_base;

_CLK_OPS2(clkgena0,
	"A10",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena0_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[2]",      /* Observation point 1 */
	"PIO101[3]",      /* Observation point 2 */
	clkgenax_observe2
);
_CLK_OPS2(clkgena1,
	"A11",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena1_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[0]",      /* Observation point 1 */
	"PIO101[1]",      /* Observation point 2 */
	clkgenax_observe2
);
_CLK_OPS2(clkgena2,
	"A12",
	clkgenax_init,
	clkgenax_set_parent,
	clkgena2_set_rate,
	clkgenax_recalc,
	clkgenax_enable,
	clkgenax_disable,
	clkgenax_observe,
	clkgenax_get_measure,
	"PIO101[4]",      /* Observation point 1 */
	"PIO101[5]",      /* Observation point 2 */
	clkgenax_observe2
);
_CLK_OPS(clkgene,
	"E/MDTP",
	clkgene_init,
	NULL,
	clkgene_set_rate,
	clkgene_recalc,
	clkgene_enable,
	clkgene_disable,
	NULL,
	NULL,		/* No measure function */
	NULL		/* No observation point */
);
_CLK_OPS(clkgenf,
	"F/TVPipe",
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
	"DDR-SS",
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
	NULL,
	NULL,
	NULL,
	NULL,
	NULL		/* No observation point */
);

/* Physical clocks description */
static clk_t clk_clocks[] = {
/* Clockgen A10 */
_CLK(CLKM_A0_REF, &clkgena0, 0,
	  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_A0_PLL0, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_REF]),
_CLK_P(CLKM_A0_PLL0_PHI0, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL0]),
_CLK_P(CLKM_A0_PLL0_PHI1, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL0]),
_CLK_P(CLKM_A0_PLL0_PHI2, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL0]),
_CLK_P(CLKM_A0_PLL0_PHI3, &clkgena0, 1200000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL0]),
_CLK_P(CLKM_A0_PLL1, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_REF]),
_CLK_P(CLKM_A0_PLL1_PHI0, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL1]),
_CLK_P(CLKM_A0_PLL1_PHI1, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL1]),
_CLK_P(CLKM_A0_PLL1_PHI2, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL1]),
_CLK_P(CLKM_A0_PLL1_PHI3, &clkgena0, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A0_PLL1]),

_CLK(CLKM_APB_PM,	&clkgena0,    50000000,    0),
_CLK(CLKM_PP_DMU_0,	&clkgena0,    200000000,    0),
_CLK(CLKM_PP_DMU_1,	&clkgena0,    200000000,    0),
_CLK(CLKM_ICN_DISP,	&clkgena0,    0, 0),
_CLK(CLKM_A9_EXT2F,	&clkgena0,    200000000,    0),
_CLK_P(CLKM_A9_EXT2F,	&clkgena0,    30000000, 0, &clk_clocks[CLKM_A9_EXT2F]),
_CLK(CLKM_ST40RT,	&clkgena0,    500000000,    0),
_CLK(CLKM_ST231_DMU_0,	&clkgena0,    500000000,    0),
_CLK(CLKM_ST231_DMU_1,	&clkgena0,    500000000,    0),
_CLK(CLKM_ST231_AUD,	&clkgena0,    600000000,    0),
_CLK(CLKM_ST231_GP_0,	&clkgena0,    600000000,    0),
_CLK(CLKM_ST231_GP_1,	&clkgena0,     600000000,    0),
_CLK(CLKM_ICN_CPU,	&clkgena0,    600000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_IC_STAC,	&clkgena0,     200000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_ICN_DMU_0,	&clkgena0,     250000000,    0),
_CLK(CLKM_ICN_DMU_1,	&clkgena0,    250000000,    0),
_CLK(CLKM_ICN_ERAM,	&clkgena0,    200000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_A9_TRACE,	&clkgena0,    200000000,    0),

/* Clockgen A11 */
_CLK(CLKM_A1_REF, &clkgena1, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_A1_PLL0, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKM_A1_REF]),
_CLK_P(CLKM_A1_PLL0_PHI0, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL0]),
_CLK_P(CLKM_A1_PLL0_PHI1, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL0]),
_CLK_P(CLKM_A1_PLL0_PHI2, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL0]),
_CLK_P(CLKM_A1_PLL0_PHI3, &clkgena1, 900000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL0]),
_CLK_P(CLKM_A1_PLL1, &clkgena1, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_REF]),
_CLK_P(CLKM_A1_PLL1_PHI0, &clkgena1, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL1]),
_CLK_P(CLKM_A1_PLL1_PHI1, &clkgena1, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL1]),
_CLK_P(CLKM_A1_PLL1_PHI2, &clkgena1, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL1]),
_CLK_P(CLKM_A1_PLL1_PHI3, &clkgena1, 1000000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A1_PLL1]),

_CLK(CLKM_FDMA_12,    &clkgena1,   450000000,    0),
_CLK(CLKM_FDMA_10,    &clkgena1,   450000000,    0),
_CLK(CLKM_FDMA_11,    &clkgena1,   450000000,    0),
_CLK(CLKM_ICN_LMI,    &clkgena1,   450000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_PROC_SC,    &clkgena1,   225000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_TP,    &clkgena1,   333333333,    0),
_CLK(CLKM_ICN_GPU,    &clkgena1,   333333333,    0),
_CLK(CLKM_ICN_VDP_0,    &clkgena1,   333333333,    0),
_CLK(CLKM_ICN_VDP_1,  &clkgena1,   250000000,    0),
_CLK(CLKM_ICN_VDP_2,	  &clkgena1,   250000000,    0),
_CLK(CLKM_ICN_VDP_3,	  &clkgena1,   333333333,    0),
_CLK(CLKM_PRV_T1_BUS,      &clkgena1,   50000000,    0),
_CLK(CLKM_ICN_VDP_4,      &clkgena1,    200000000,    0),
_CLK(CLKM_ICN_REG_10,      &clkgena1,   100000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_FVDP_PROC_ALT,  &clkgena1, 0, CLK_RATE_PROPAGATES),

/* Clockgen A12 */
_CLK(CLKM_A2_REF, &clkgena2, 0,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_A2_PLL0, &clkgena2, 1488000000,
	CLK_RATE_PROPAGATES,  &clk_clocks[CLKM_A2_REF]),
_CLK_P(CLKM_A2_PLL0_PHI0, &clkgena2, 1488000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL0]),
_CLK_P(CLKM_A2_PLL0_PHI1, &clkgena2, 1488000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL0]),
_CLK_P(CLKM_A2_PLL0_PHI2, &clkgena2, 1488000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL0]),
_CLK_P(CLKM_A2_PLL0_PHI3, &clkgena2, 1488000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL0]),
_CLK_P(CLKM_A2_PLL1, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_REF]),
_CLK_P(CLKM_A2_PLL1_PHI0, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL1]),
_CLK_P(CLKM_A2_PLL1_PHI1, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL1]),
_CLK_P(CLKM_A2_PLL1_PHI2, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL1]),
_CLK_P(CLKM_A2_PLL1_PHI3, &clkgena2, 1600000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A2_PLL1]),

_CLK(CLKM_VTAC_MAIN_PHY,    &clkgena2,   744000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_VTAC_AUX_PHY,    &clkgena2,   248000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_STAC_PHY,    &clkgena2,   800000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_STAC_SYS,    &clkgena2,   400000000, CLK_ALWAYS_ENABLED),
_CLK(CLKM_MPESTAC_PG,    &clkgena2,   0,    0),
_CLK(CLKM_MPESTAC_WC,    &clkgena2,   0,    0),
_CLK(CLKM_MPEVTACAUX_PG,    &clkgena2,   0,    0),
_CLK(CLKM_MPEVTACMAIN_PG,    &clkgena2,   0,    0),
_CLK(CLKM_MPEVTACRX0_WC,    &clkgena2,   0,    0),
_CLK(CLKM_MPEVTACRX1_WC,    &clkgena2,   0,    0),
_CLK(CLKM_COMPO_MAIN,    &clkgena2,   400000000,    0),
_CLK(CLKM_COMPO_AUX,    &clkgena2,   200000000,    0),
_CLK(CLKM_BDISP_0,    &clkgena2,   320000000,    0),
_CLK(CLKM_BDISP_1,    &clkgena2,   320000000,    0),
_CLK(CLKM_ICN_BDISP_0,    &clkgena2,   200000000,    0),
_CLK(CLKM_ICN_BDISP_1,    &clkgena2,   200000000,    0),
_CLK(CLKM_ICN_COMPO,    &clkgena2,   200000000,    0),
_CLK(CLKM_IC_VDPAUX,    &clkgena2,   0,    0),
_CLK(CLKM_ICN_TS,    &clkgena2,   200000000,    0),
/* Nominal set to 200Mhz for Orly1/H415 as VTG programmation WA */
_CLK(CLKM_ICN_REG_LP_10,    &clkgena2,   200000000,    0),
_CLK(CLKM_DCEPHY_IMPCTRL,    &clkgena2,   30000000,    0),

/* Clockgen E */
_CLK(CLKM_E_REF, &clkgene, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_E_FS_VCO, &clkgene, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_E_REF]),
_CLK_P(CLKM_PIX_MDTP_0, &clkgene, 148500000,
	0, &clk_clocks[CLKM_E_FS_VCO]),
_CLK_P(CLKM_PIX_MDTP_1, &clkgene, 148500000,
	0, &clk_clocks[CLKM_E_FS_VCO]),
_CLK_P(CLKM_MPELPC, &clkgene, 50000000,
	0, &clk_clocks[CLKM_E_FS_VCO]),

/* Clockgen F */
_CLK(CLKM_F_REF, &clkgenf, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_F_FS_VCO, &clkgenf, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_F_REF]),
_CLK_P(CLKM_PIX_MAIN_VIDFS, &clkgenf, 297000000,
	0, &clk_clocks[CLKM_F_FS_VCO]),
_CLK_P(CLKM_PIX_AUX_VIDFS, &clkgenf, 13500000,
	0, &clk_clocks[CLKM_F_FS_VCO]),
_CLK_P(CLKM_FVDP_VCPU, &clkgenf, 350000000,
	0, &clk_clocks[CLKM_F_FS_VCO]),
_CLK_P(CLKM_FVDP_PROC_FS, &clkgenf, 333000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_F_FS_VCO]),
_CLK(CLKM_PIX_MAIN_PIPE, &clkgenf, 0, 0),
_CLK(CLKM_PIX_AUX_PIPE, &clkgenf, 0, 0),
_CLK(CLKM_PIX_MAIN_SAS, &clkgenf, 0, CLK_RATE_PROPAGATES),	/* PIX_HD from SAS = clk_hd_ext. To VCC */
_CLK(CLKM_PIX_AUX_SAS, &clkgenf, 0, CLK_RATE_PROPAGATES),	/* PIX_SD from SAS = clk_sd_ext. To VCC */
_CLK(CLKM_FVDP_PROC, &clkgenf, 333000000, 0),			/* Mux output */

/* Clockgen DDR-subsystem */
_CLK(CLKM_DDR_REF, &clkgenddr, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_DDR_IC_LMI0, &clkgenddr, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_DDR_REF]),
_CLK_P(CLKM_DDR_IC_LMI1, &clkgenddr, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_DDR_REF]),
_CLK_P(CLKM_DDR_DDR0, &clkgenddr, 1600000000,
		0, &clk_clocks[CLKM_DDR_IC_LMI0]),
_CLK_P(CLKM_DDR_DDR1, &clkgenddr, 1600000000,
		0, &clk_clocks[CLKM_DDR_IC_LMI1]),

/* CA9 PLL */
_CLK(CLKM_A9_REF, &clkgena9, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_A9_PHI0, &clkgena9, 0,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_A9_REF]),
_CLK(CLKM_A9, &clkgena9, 0, 0),

/* MALI400/GPU PLL1200 */
_CLK(CLKM_GPU_REF, &clkgengpu, 30000000,
	CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
_CLK_P(CLKM_GPU_PHI, &clkgengpu, 400000000,
	CLK_RATE_PROPAGATES, &clk_clocks[CLKM_GPU_REF]),
_CLK_P(CLKM_GPU, &clkgengpu, 400000000,
	0, &clk_clocks[CLKM_GPU_PHI]),
};

/* ========================================================================
   Name:        mpe41_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

SYSCONF(0, 401, 0, 1);
SYSCONF(0, 401, 4, 5);
SYSCONF(0, 401, 8, 10);
SYSCONF(0, 401, 12, 14);
SYSCONF(0, 401, 16, 18);
SYSCONF(0, 401, 20, 22);
SYSCONF(0, 403, 8, 8);
SYSCONF(0, 403, 9, 9);
SYSCONF(0, 403, 10, 10);
SYSCONF(0, 403, 11, 11);
SYSCONF(0, 403, 12, 12);
SYSCONF(0, 403, 13, 13);
SYSCONF(0, 417, 2, 2);
SYSCONF(0, 417, 3, 3);
SYSCONF(0, 504, 0, 1);
SYSCONF(0, 504, 8, 10);

SYSCONF(0, 506, 0, 0);
SYSCONF(0, 506, 2, 2);

SYSCONF(0, 555, 0, 3);
/*
 * 556: [0:7] clock source (2 bits x channel)
 */
SYSCONF(0, 556, 0, 7);

SYSCONF(0, 557, 0, 7); /*div_mode (2 bits x channel)*/
/*
 * FS_0: Channel 1-3
 * 558: [0:3]: Enable programmation of FS channels
 *	[8 : 23] MD (5 bits x channel)
 *	[24: 26] NDIV
 *	[27: 27] Pwd PLL
 * 559: [0:3]: NBS (1 bit x channel)
 *	[4: 18]: Pe.0
 * 560: [0: 14]: Pe.1
 *	[15:29]: Pe.2
 * 561: [0 :14]: pe.3
 *	[16:31]: NDIV (4 bits x channel)
 */
SYSCONF(0, 558, 0, 3);
SYSCONF(0, 558, 4, 23);
SYSCONF(0, 558, 24, 26);
SYSCONF(0, 558, 27, 27);
SYSCONF(0, 559, 0, 3);
SYSCONF(0, 559, 4, 18);
SYSCONF(0, 560, 0, 14);
SYSCONF(0, 560, 15, 29);
SYSCONF(0, 561, 0, 14);
SYSCONF(0, 561, 16, 31);
/*
 * FS_1: Channel 1-3
 * 562: [0:3]: Enable programmation of FS channels
 *	[8 : 23] MD (5 bits x channel)
 *	[24: 26] NDIV
 *	[27: 27] Pwd PLL
 * 563: [0:3]: NBS (1 bit x channel)
 *	[4: 18]: Pe.0
 * 564: [0: 14]: Pe.1
 *	[15:29]: Pe.2
 * 565: [0 :14]: pe.3
 *	[16:31]: NDIV (4 bits x channel)
 */
SYSCONF(0, 562, 0, 3);
SYSCONF(0, 562, 4, 23);
SYSCONF(0, 562, 24, 26);
SYSCONF(0, 562, 27, 27);
SYSCONF(0, 563, 0, 3);
SYSCONF(0, 563, 4, 18);
SYSCONF(0, 564, 0, 14);
SYSCONF(0, 564, 15, 29);
SYSCONF(0, 565, 0, 14);
SYSCONF(0, 565, 16, 31);

SYSCONF(0, 566, 3, 6);

SYSCONF(0, 573, 0, 0);
SYSCONF(0, 601, 0, 0);
SYSCONF(0, 601, 25, 27);
/*
 * 603: Clock Gen D
 */
SYSCONF(0, 603, 0, 7);
SYSCONF(0, 603, 8, 13);
SYSCONF(0, 603, 14, 19);
/*
 * 654: A9 pll configuration
 */
SYSCONF(0, 654, 0, 0);
SYSCONF(0, 654, 1, 1);
SYSCONF(0, 654, 2, 2);
SYSCONF(0, 654, 3, 8);
SYSCONF(0, 654, 22, 24);
SYSCONF(0, 654, 9, 16);
SYSCONF(0, 681, 0, 0);

int __init mpe41_clk_init(clk_t *_sys_clk_in, clk_t *_sys_clkalt_in,
		clk_t *_pix_main_clk, clk_t *_pix_aux_clk)
{
	int ret;

	call_platform_sys_claim(401, 0, 1);
	call_platform_sys_claim(401, 4, 5);
	call_platform_sys_claim(401, 8, 10);
	call_platform_sys_claim(401, 12, 14);
	call_platform_sys_claim(401, 16, 18);
	call_platform_sys_claim(401, 20, 22);
	call_platform_sys_claim(403, 8, 8);
	call_platform_sys_claim(403, 9, 9);
	call_platform_sys_claim(403, 11, 11);
	call_platform_sys_claim(403, 12, 12);
	call_platform_sys_claim(403, 13, 13);
	call_platform_sys_claim(417, 2, 2);
	call_platform_sys_claim(417, 3, 3);

	call_platform_sys_claim(504, 0, 1);
	call_platform_sys_claim(504, 8, 10);

	call_platform_sys_claim(506, 0, 0);
	call_platform_sys_claim(506, 2, 2);

	call_platform_sys_claim(555, 0, 3);
	call_platform_sys_claim(556, 0, 7);
	call_platform_sys_claim(557, 0, 7);

	/*
	 * 558: [0:3]: Enable programmation of FS channels
	 *	[8 : 23] MD (5 bits x channel)
	 *	[24: 26] NDIV
	 *	[27: 27] Pwd PLL
	 * 559: [0:3]: NBS (1 bit x channel)
	 *	[4: 18]: Pe.0
	 * 560: [0: 14]: Pe.1
	 *	[15:29]: Pe.2
	 * 561: [0 :14]: pe.3
	 *	[16:31]: NDIV (4 bits x channel)
	 */
	call_platform_sys_claim(558, 0, 3);
	call_platform_sys_claim(558, 4, 23);
	call_platform_sys_claim(558, 24, 26);
	call_platform_sys_claim(558, 27, 27);
	call_platform_sys_claim(559, 0, 3);
	call_platform_sys_claim(559, 4, 18);
	call_platform_sys_claim(560, 0, 14);
	call_platform_sys_claim(560, 15, 29);
	call_platform_sys_claim(561, 0, 14);
	call_platform_sys_claim(561, 16, 31);

	/*
	 * 562: [0:3]: Enable programmation of FS channels
	 *	[8 : 23] MD (5 bits x channel)
	 *	[24: 26] NDIV
	 *	[27: 27] Pwd PLL
	 * 563: [0:3]: NBS (1 bit x channel)
	 *	[4: 18]: Pe.0
	 * 564: [0: 14]: Pe.1
	 *	[15:29]: Pe.2
	 * 565: [0 :14]: pe.3
	 *	[16:31]: NDIV (4 bits x channel)
	 */
	call_platform_sys_claim(562, 0, 3);
	call_platform_sys_claim(562, 4, 23);
	call_platform_sys_claim(562, 24, 26);
	call_platform_sys_claim(562, 27, 27);
	call_platform_sys_claim(563, 0, 3);
	call_platform_sys_claim(563, 4, 18);
	call_platform_sys_claim(564, 0, 14);
	call_platform_sys_claim(564, 15, 29);
	call_platform_sys_claim(565, 0, 14);
	call_platform_sys_claim(565, 16, 31);

	call_platform_sys_claim(566, 3, 6);

	call_platform_sys_claim(573, 0, 0);
	call_platform_sys_claim(601, 0, 0);
	call_platform_sys_claim(601, 25, 27);
	/*
	 * 603: Clock Gen D
	 */
	call_platform_sys_claim(603, 0, 7);
	call_platform_sys_claim(603, 8, 13);
	call_platform_sys_claim(603, 14, 19);
	/*
	 * 654: A9 pll configuration
	 */
	call_platform_sys_claim(654, 0, 0);
	call_platform_sys_claim(654, 1, 1);
	call_platform_sys_claim(654, 2, 2);
	call_platform_sys_claim(654, 3, 8);
	call_platform_sys_claim(654, 22, 24);
	call_platform_sys_claim(654, 9, 16);
	call_platform_sys_claim(681, 0, 0);


	clk_clocks[CLKM_A0_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_A1_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_A2_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_E_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_F_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_DDR_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_A9_REF].parent = _sys_clk_in;
	clk_clocks[CLKM_GPU_REF].parent = _sys_clk_in;

	clk_clocks[CLKM_PIX_MAIN_SAS].parent = _pix_main_clk;
	clk_clocks[CLKM_PIX_AUX_SAS].parent = _pix_aux_clk;


	cga0_base = ioremap_nocache(CKGA0_BASE_ADDRESS, 0x1000);
	cga1_base = ioremap_nocache(CKGA1_BASE_ADDRESS, 0x1000);
	cga2_base = ioremap_nocache(CKGA2_BASE_ADDRESS, 0x1000);
	cgb_base = ioremap_nocache(CKGB_BASE_ADDRESS, 0x1000);
	cgd_base =  ioremap_nocache(CKGD_BASE_ADDRESS, 0x1000);
	mali_base =  ioremap_nocache(SYS_MALI_BASE_ADDRESS, 0x1000);

#ifdef ST_OS21
	printf("Registering MPE41 clocks\n");
	ret = clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
	printf(" => done\n");
#else
	ret = clk_register_table(clk_clocks, CLKM_E_REF, 1);

	ret |= clk_register_table(&clk_clocks[CLKM_E_REF],
		ARRAY_SIZE(clk_clocks) - CLKM_E_REF, 0);
#endif
	return ret;
}


/******************************************************************************
CLOCKGEN Ax clocks groups. Common functions
******************************************************************************/

static inline int clkgenax_get_bank(int clk_id)
{
	return ((clk_id >= CLKM_A2_REF) ? 2 : ((clk_id >= CLKM_A1_REF) ? 1 : 0));
}

/* Returns corresponding clockgen Ax base address for 'clk_id' */
static inline unsigned long clkgenax_get_base_addr(int clk_id)
{
	static void **clkgenax_base[]={
			&cga0_base, &cga1_base, &cga2_base};

	return (unsigned long)*clkgenax_base[clkgenax_get_bank(clk_id)];
}

/* Returns corresponding CLKM_Ax_REF */
static inline unsigned long clkgenax_get_base_id(clk_t *clk_p)
{
	return ((clk_p->id >= CLKM_A2_REF) ? CLKM_A2_REF :
		((clk_p->id >= CLKM_A1_REF) ? CLKM_A1_REF: CLKM_A0_REF));
}

/* Returns divN_cfg register offset */
static inline unsigned long clkgenax_div_cfg(int clk_src, int clk_idx)
{
	static const unsigned short pll0_odf_table[] = {
		CKGA_PLL0_ODF0_DIV0_CFG,	CKGA_PLL0_ODF1_DIV0_CFG,
		CKGA_PLL0_ODF2_DIV0_CFG,	CKGA_PLL0_ODF3_DIV0_CFG
	};
	static const unsigned short pll1_odf_table[] = {
		CKGA_PLL1_ODF0_DIV0_CFG,	CKGA_PLL1_ODF1_DIV0_CFG,
		CKGA_PLL1_ODF2_DIV0_CFG,	CKGA_PLL1_ODF3_DIV0_CFG
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
	case CLKM_APB_PM ... CLKM_A9_TRACE:
		idx = clkid - CLKM_APB_PM;
		break;
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
		idx = clkid - CLKM_FDMA_12;
		break;
	case CLKM_VTAC_MAIN_PHY ... CLKM_DCEPHY_IMPCTRL:
		idx = clkid - CLKM_VTAC_MAIN_PHY;
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

static int clkgenax_set_parent(clk_t *clk_p, clk_t *src_p)
{
	unsigned long clk_src, val, base_id;
	int idx, shift;
	unsigned long srcreg, base_addr;

	if (!clk_p || !src_p)
		return CLK_ERR_BAD_PARAMETER;
	switch (clk_p->id) {
	case CLKM_APB_PM ... CLKM_A9_TRACE:
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
	case CLKM_VTAC_MAIN_PHY ... CLKM_DCEPHY_IMPCTRL:
		break; /* to continue */
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
	case CLKM_A0_REF:
	case CLKM_A1_REF:
	case CLKM_A2_REF:
		clk_src = 0;
		break;
	case CLKM_A0_PLL0:
	case CLKM_A0_PLL0_PHI0 ... CLKM_A0_PLL0_PHI3:
	case CLKM_A1_PLL0:
	case CLKM_A1_PLL0_PHI0 ... CLKM_A1_PLL0_PHI3:
	case CLKM_A2_PLL0:
	case CLKM_A2_PLL0_PHI0 ... CLKM_A2_PLL0_PHI3:
		clk_src = 1;
		src_p = &clk_clocks[base_id + 3 + (idx / 8)];
		break;
	case CLKM_A0_PLL1:
	case CLKM_A0_PLL1_PHI0 ... CLKM_A0_PLL1_PHI3:
	case CLKM_A1_PLL1:
	case CLKM_A1_PLL1_PHI0 ... CLKM_A1_PLL1_PHI3:
	case CLKM_A2_PLL1:
	case CLKM_A2_PLL1_PHI0 ... CLKM_A2_PLL1_PHI3:
		clk_src = 2;
		src_p = &clk_clocks[base_id + 7 + (idx / 8)];
		break;
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

static int clkgenax_identify_parent(clk_t *clk_p)
{
	int idx;
	unsigned long src_sel, srcreg, base_addr, base_id;
	int shift;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Statically initialized clocks */
	switch (clk_p->id) {
	case CLKM_A0_REF ... CLKM_A0_PLL1_PHI3:
	case CLKM_A1_REF ... CLKM_A1_PLL1_PHI3:
	case CLKM_A2_REF ... CLKM_A2_PLL1_PHI3:
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
		clk_p->parent = &clk_clocks[base_id + 0];	/* CLKAx_REF */
		break;
	case 1:
		clk_p->parent = &clk_clocks[base_id + 3 + (idx / 8)];	/* PLL0 PHI0..3 */
		break;
	case 2:
		clk_p->parent = &clk_clocks[base_id + 7 + (idx / 8)];	/* PLL1 PHI0..3 */
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

static int clkgenax_xable_pll(clk_t *clk_p, int enable)
{
	unsigned long val, base_addr;
	int bit, err = 0;

	if (clk_p->id != CLKM_A0_PLL0 && clk_p->id != CLKM_A0_PLL1 &&
	    clk_p->id != CLKM_A1_PLL0 && clk_p->id != CLKM_A1_PLL1 &&
	    clk_p->id != CLKM_A2_PLL0 && clk_p->id != CLKM_A2_PLL1)
		return CLK_ERR_FEATURE_NOT_SUPPORTED;

	if (clk_p->id == CLKM_A0_PLL1 || clk_p->id == CLKM_A1_PLL1 ||
	    clk_p->id == CLKM_A2_PLL1)
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

static int clkgenax_enable(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		/* Unsupported. Init must be called first. */
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power up */
	switch (clk_p->id) {
	case CLKM_A0_PLL0 ... CLKM_A0_PLL1_PHI3:
	case CLKM_A1_PLL0 ... CLKM_A1_PLL1_PHI3:
	case CLKM_A2_PLL0 ... CLKM_A2_PLL1_PHI3:
		return clkgenax_xable_pll(clk_p, 1);
	}

	err = clkgenax_set_parent(clk_p, clk_p->parent);
	/* Note: clkgenax_set_parent() is performing recalc() */

	return err;
}

/* ========================================================================
   Name:        clkgenax_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_disable(clk_t *clk_p)
{
	unsigned long val;
	int idx, shift;
	unsigned long srcreg;
	unsigned long base_addr;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* PLL power down */
	switch (clk_p->id) {
	case CLKM_A0_PLL0 ... CLKM_A0_PLL1_PHI3:
	case CLKM_A1_PLL0 ... CLKM_A1_PLL1_PHI3:
	case CLKM_A2_PLL0 ... CLKM_A2_PLL1_PHI3:
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

static int clkgenax_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int idx;
	unsigned long div_cfg = 0;
	unsigned long srcreg, offset;
	int shift;
	unsigned long base_addr, clk_src;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Which divider to setup ? */
	idx = clkgenax_get_index(clk_p->id, &srcreg, &shift);
	if (idx == -1)
		return CLK_ERR_BAD_PARAMETER;

	base_addr = clkgenax_get_base_addr(clk_p->id);

	/* Clock source: 0=OSC, 1=PLL0-PHIx, 2=PLL1-PHIx, 3=STOP */
	clk_src = (CLK_READ(base_addr + srcreg) >> shift) & 0x3;
	if (clk_src == 3) return 0; /* Clock stopped */

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

static int clkgenax_recalc(clk_t *clk_p)
{
	unsigned long data, ratio, idf, ndiv;
	unsigned long srcreg, offset, base_addr;
	int shift, err, idx;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* If no parent, assuming clock is stopped. Sometimes reset default. */
	if (!clk_p->parent) {
		clk_p->rate = 0;
		return 0;
	}

	base_addr = clkgenax_get_base_addr(clk_p->id);

	/* Reading clock programmed value */
	switch (clk_p->id) {
	case CLKM_A0_REF:  /* Clockgen A0 reference clock */
	case CLKM_A1_REF:  /* Clockgen A1 reference clock */
	case CLKM_A2_REF:  /* Clockgen A2 reference clock */
		clk_p->rate = clk_p->parent->rate;
		return 0;

	case CLKM_A0_PLL0:
	case CLKM_A1_PLL0:
	case CLKM_A2_PLL0:
		#if !defined(CLKLLA_NO_PLL)
		ndiv = CLK_READ(base_addr + CKGA_PLL0_REG0_CFG) & 0xff;
		idf = CLK_READ(base_addr + CKGA_PLL0_REG1_CFG) & 0x7;
		err = clk_pll3200c32_get_rate(clk_p->parent->rate, idf, ndiv, &clk_p->rate);
		#endif
		break;
	case CLKM_A0_PLL1:
	case CLKM_A1_PLL1:
	case CLKM_A2_PLL1:
		#if !defined(CLKLLA_NO_PLL)
		ndiv = CLK_READ(base_addr + CKGA_PLL1_REG0_CFG) & 0xff;
		idf = CLK_READ(base_addr + CKGA_PLL1_REG1_CFG) & 0x7;
		err = clk_pll3200c32_get_rate(clk_p->parent->rate, idf, ndiv, &clk_p->rate);
		#endif
		break;
	case CLKM_A0_PLL0_PHI0 ... CLKM_A0_PLL0_PHI3:
	case CLKM_A1_PLL0_PHI0 ... CLKM_A1_PLL0_PHI3:
	case CLKM_A2_PLL0_PHI0 ... CLKM_A2_PLL0_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		ratio = CLK_READ(base_addr + CKGA_PLL0_REG3_CFG);
		if (clk_p->id >= CLKM_A2_PLL0_PHI0)
			idx = clk_p->id - CLKM_A2_PLL0_PHI0;
		else if (clk_p->id >= CLKM_A1_PLL0_PHI0)
			idx = clk_p->id - CLKM_A1_PLL0_PHI0;
		else
			idx = clk_p->id - CLKM_A0_PLL0_PHI0;
		ratio = (ratio >> (4 + (6 * idx))) & 0x3f;
		if (ratio == 0)
			ratio = 1;
		clk_p->rate = clk_p->parent->rate / ratio;
		#endif
		break;
	case CLKM_A0_PLL1_PHI0 ... CLKM_A0_PLL1_PHI3:
	case CLKM_A1_PLL1_PHI0 ... CLKM_A1_PLL1_PHI3:
	case CLKM_A2_PLL1_PHI0 ... CLKM_A2_PLL1_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		ratio = CLK_READ(base_addr + CKGA_PLL1_REG3_CFG);
		if (clk_p->id >= CLKM_A2_PLL1_PHI0)
			idx = clk_p->id - CLKM_A2_PLL1_PHI0;
		else if (clk_p->id >= CLKM_A1_PLL1_PHI0)
			idx = clk_p->id - CLKM_A1_PLL1_PHI0;
		else
			idx = clk_p->id - CLKM_A0_PLL1_PHI0;
		ratio = (ratio >> (4 + (6 * idx))) & 0x3f;
		if (ratio == 0)
			ratio = 1;
		clk_p->rate = clk_p->parent->rate / ratio;
		#endif
		break;

	case CLKM_A9_EXT2F_DIV2:
		clk_p->rate = clk_p->parent->rate / 2;
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
		clk_p->rate = clk_p->parent->rate / ratio;
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
   Name:        clkgenax_get_measure
   Description: Use internal HW feature (when avail.) to measure clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static unsigned long clkgenax_get_measure(clk_t *clk_p)
{
	unsigned long src;
	unsigned long data, measure;
	void *base;
	int i;

	if (!clk_p)
		return 0;

	switch (clk_p->id) {
	case CLKM_APB_PM ... CLKM_A9_TRACE:
		src = clk_p->id - CLKM_APB_PM;
		break;
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
		src = clk_p->id - CLKM_FDMA_12;
		break;
	case  CLKM_VTAC_MAIN_PHY ...CLKM_DCEPHY_IMPCTRL:
		src = clk_p->id - CLKM_VTAC_MAIN_PHY;
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
		CLK_DELAYMS(10);
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

/* ========================================================================
   Name:        clkgenax_observe
   Description: Clockgen Ax clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long sel, base_addr;
	unsigned long divcfg;
	unsigned long srcreg;
	int shift;

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKM_APB_PM ... CLKM_A9_TRACE:
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
	case CLKM_VTAC_MAIN_PHY ...CLKM_DCEPHY_IMPCTRL:
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
	   A0 => PIO101[2] alt 3
	   A1 => PIO101[0] alt 3
	   A2 => PIO101[4] alt 3
	 */

	/* Configuring appropriate PIO */
	if (base_addr == (unsigned long)cga0_base) {
		SYSCONF_WRITE(0, 401, 8, 10, 3); /* Selecting alternate 3 */
		SYSCONF_WRITE(0, 403, 10, 10, 1);/* Enabling IO */
	} else if (base_addr == (unsigned long)cga1_base) {
		SYSCONF_WRITE(0, 401, 0, 1, 3);	/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 403, 8, 8, 1);/* Enabling IO */
	} else {
		SYSCONF_WRITE(0, 401, 16, 18, 3);	/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 403, 12, 12, 1);/* Enabling IO */
	}

	return 0;
}

/* ========================================================================
   Name:        clkgenax_observe2
   Description: Clockgen Ax clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenax_observe2(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long sel, base_addr;
	unsigned long divcfg;
	unsigned long srcreg;
	int shift;

	if (!clk_p || !div_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKM_APB_PM ... CLKM_A9_TRACE:
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
	case CLKM_VTAC_MAIN_PHY ...CLKM_DCEPHY_IMPCTRL:
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
	CLK_WRITE((base_addr + CKGA_CLKOBS_MUX1_CFG),
		(divcfg << 6) | (sel & 0x3f));

	/* 2nd observation points:
	   A0 => PIO101[3] alt 3
	   A1 => PIO101[1] alt 3
	   A2 => PIO101[5] alt 3
	 */

	/* Configuring appropriate PIO */
	if (base_addr == (unsigned long)cga0_base) {
		/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 401, 12, 14, 3);
		/* Enabling IO */
		SYSCONF_WRITE(0, 403, 11, 11, 1);
	} else if (base_addr == (unsigned long)cga1_base) {
		/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 401, 4, 5, 3);
		/* Enabling IO */
		SYSCONF_WRITE(0, 403, 9, 9, 1);
	} else {
		/* Selecting alternate 3 */
		SYSCONF_WRITE(0, 401, 20, 22, 3);
		/* Enabling IO */
		SYSCONF_WRITE(0, 403, 13, 13, 1);
	}

	return 0;
}

/******************************************************************************
CLOCKGEN A0 clocks group
******************************************************************************/

/* ========================================================================
   Name:        clkgena0_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */
#if !defined(CLKLLA_NO_PLL)
static void clkgenax_pll_phi_set_div(void *reg, unsigned clk_id,
	unsigned clk_base_id, unsigned long div)
{
	unsigned long shift = 4 + (clk_id - clk_base_id) * 6;
	unsigned long data;

	data = CLK_READ(reg);
	data &= ~(0x3f << shift);
	data |= (div << shift);
	CLK_WRITE(reg, data);
}
#endif

static int clkgena0_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, idf, ndiv, cp, data;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKM_A0_PLL0) || (clk_p->id > CLKM_A9_TRACE))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKM_A0_PLL0:
		err = clk_pll3200c32_get_params(clk_p->parent->rate, freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga0_base + CKGA_PLL0_REG0_CFG)
				& 0xffffff00;
		data |= ndiv;
		CLK_WRITE(cga0_base + CKGA_PLL0_REG0_CFG, data);
		data = CLK_READ(cga0_base + CKGA_PLL0_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga0_base + CKGA_PLL0_REG1_CFG, data);
		#endif
		break;
	case CLKM_A0_PLL1:
		err = clk_pll3200c32_get_params(clk_p->parent->rate, freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga0_base + CKGA_PLL1_REG0_CFG)
				& 0xffffff80;
		data |= ndiv;
		CLK_WRITE(cga0_base + CKGA_PLL1_REG0_CFG, data);
		data = CLK_READ(cga0_base + CKGA_PLL1_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga0_base + CKGA_PLL1_REG1_CFG, data);
		#endif
		break;
	case CLKM_A0_PLL0_PHI0 ... CLKM_A0_PLL0_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga0_base + CKGA_PLL0_REG3_CFG,
			clk_p->id, CLKM_A0_PLL0_PHI0, div);
		#endif
		break;
	case CLKM_A0_PLL1_PHI0 ... CLKM_A0_PLL1_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga0_base + CKGA_PLL1_REG3_CFG,
			clk_p->id, CLKM_A0_PLL1_PHI0, div);
		#endif
		break;
	case CLKM_APB_PM ... CLKM_A9_TRACE:
		if (clk_p->id >= CLKM_A0_SPARE_1 && clk_p->id <= CLKM_A0_SPARE_3)
			return CLK_ERR_BAD_PARAMETER;
		if (clk_p->id >= CLKM_A0_SPARE_7 && clk_p->id <= CLKM_A0_SPARE_9)
			return CLK_ERR_BAD_PARAMETER;
		if (clk_p->id >= CLKM_A0_SPARE_21 && clk_p->id <= CLKM_A0_SPARE_29)
			return CLK_ERR_BAD_PARAMETER;

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
CLOCKGEN A1 clocks group
******************************************************************************/

/* ========================================================================
   Name:        clkgena1_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena1_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, idf, ndiv, cp, data;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKM_A1_PLL0) || (clk_p->id > CLKM_FVDP_PROC_ALT))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKM_A1_PLL0:
		err = clk_pll3200c32_get_params(clk_p->parent->rate, freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga1_base + CKGA_PLL0_REG0_CFG)
				& 0xffffff00;
		data |= ndiv;
		CLK_WRITE(cga1_base + CKGA_PLL0_REG0_CFG, data);
		data = CLK_READ(cga1_base + CKGA_PLL0_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga1_base + CKGA_PLL0_REG1_CFG, data);
		#endif
		break;
	case CLKM_A1_PLL1:
		err = clk_pll3200c32_get_params(clk_p->parent->rate, freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga1_base + CKGA_PLL1_REG0_CFG)
				& 0xffffff80;
		data |= ndiv;
		CLK_WRITE(cga1_base + CKGA_PLL1_REG0_CFG, data);
		data = CLK_READ(cga1_base + CKGA_PLL1_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga1_base + CKGA_PLL1_REG1_CFG, data);
		#endif
		break;
	case CLKM_A1_PLL0_PHI0 ... CLKM_A1_PLL0_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga1_base + CKGA_PLL0_REG3_CFG,
			clk_p->id, CLKM_A1_PLL0_PHI0, div);
		#endif
		break;
	case CLKM_A1_PLL1_PHI0 ... CLKM_A1_PLL1_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga1_base + CKGA_PLL1_REG3_CFG,
			clk_p->id, CLKM_A1_PLL1_PHI0, div);
		#endif
		break;
	case CLKM_FDMA_12 ... CLKM_FVDP_PROC_ALT:
		if (clk_p->id == CLKM_A1_SPARE_14 || clk_p->id == CLKM_A1_SPARE_15)
			return CLK_ERR_BAD_PARAMETER;

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
CLOCKGEN A2 clocks group
******************************************************************************/

/* ========================================================================
   Name:        clkgena2_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena2_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long div, idf, ndiv, cp, data;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKM_A2_PLL0) || (clk_p->id > CLKM_DCEPHY_IMPCTRL))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	switch (clk_p->id) {
	case CLKM_A2_PLL0:
		err = clk_pll3200c32_get_params(clk_p->parent->rate, freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga2_base + CKGA_PLL0_REG0_CFG)
				& 0xffffff00;
		data |= ndiv;
		CLK_WRITE(cga2_base + CKGA_PLL0_REG0_CFG, data);
		data = CLK_READ(cga2_base + CKGA_PLL0_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga2_base + CKGA_PLL0_REG1_CFG, data);
		#endif
		break;
	case CLKM_A2_PLL1:
		err = clk_pll3200c32_get_params(clk_p->parent->rate,
			freq, &idf, &ndiv, &cp);
		if (err != 0)
			break;
		#if !defined(CLKLLA_NO_PLL)
		data = CLK_READ(cga2_base + CKGA_PLL1_REG0_CFG)
				& 0xffffff80;
		data |= ndiv;
		CLK_WRITE(cga2_base + CKGA_PLL1_REG0_CFG, data);
		data = CLK_READ(cga2_base + CKGA_PLL1_REG1_CFG)
				& 0xfffffff8;
		data |= idf;
		CLK_WRITE(cga2_base + CKGA_PLL1_REG1_CFG, data);
		#endif
		break;
	case CLKM_A2_PLL0_PHI0 ... CLKM_A2_PLL0_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga2_base + CKGA_PLL0_REG3_CFG,
			clk_p->id, CLKM_A2_PLL0_PHI0, div);
		#endif
		break;
	case CLKM_A2_PLL1_PHI0 ... CLKM_A2_PLL1_PHI3:
		#if !defined(CLKLLA_NO_PLL)
		div = freq / clk_p->parent->rate;
		clkgenax_pll_phi_set_div(cga2_base + CKGA_PLL1_REG3_CFG,
			clk_p->id, CLKM_A2_PLL1_PHI0, div);
		#endif
		break;
	case CLKM_VTAC_MAIN_PHY ... CLKM_DCEPHY_IMPCTRL:
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
CLOCKGEN E
******************************************************************************/

/* ========================================================================
   Name:        clkgene_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgene_fsyn_recalc(clk_t *clk_p)
{
	unsigned long chan, val;
	unsigned long pe, md, sdiv, ndiv, nsdiv;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	/* Is FSYN analog part UP ? */
	if (SYSCONF_READ(0, 562, 27, 27) == 0) {	/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* At least analog part (PLL660) is running */
	ndiv = SYSCONF_READ(0, 562, 24, 26);
	if (clk_p->id == CLKM_E_FS_VCO)
		return clk_fs660c32_vco_get_rate(clk_p->parent->rate, ndiv,
					      &clk_p->rate);

	chan = clk_p->id - CLKM_PIX_MDTP_0;

	/* Is FSYN digital part UP ? */
	val = SYSCONF_READ(0, 563, 0, 3);
	if ((val & (1 << chan)) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	switch (chan) {
	case 0: pe = SYSCONF_READ(0, 563, 4, 18); break;
	case 1:	pe = SYSCONF_READ(0, 564, 0, 14); break;
	case 2:	pe = SYSCONF_READ(0, 564, 15, 29); break;
	default:
		pe = SYSCONF_READ(0, 565, 0, 14);
	}
	md = SYSCONF_READ(0, 562, 4, 23);
	md >>= (5 * chan);
	md &= 0x1f;

	sdiv = SYSCONF_READ(0, 565, 16, 31);
	sdiv >>= (4 * chan);
	sdiv &= 0xf;

	nsdiv = (chan == 3 ? 0 : 1);

	return clk_fs660c32_get_rate(clk_p->parent->rate, nsdiv,
				  md, pe, sdiv, &clk_p->rate);
}

/* ========================================================================
   Name:        clkgene_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_fsyn_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long chan, data;
	unsigned long md, pe, sdiv, ndiv, nsdiv;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_E_FS_VCO) {
		if (clk_fs660c32_vco_get_params(clk_p->parent->rate, freq, &ndiv))
			return CLK_ERR_BAD_PARAMETER;
		SYSCONF_WRITE(0, 562, 24, 26, ndiv);
		SYSCONF_WRITE(0, 562, 27, 27, 1); /* PLL power up */
		return 0;
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	chan = clk_p->id - CLKM_PIX_MDTP_0;
	nsdiv = (chan == 3 ? 0 : 1);
	if (clk_fs660c32_dig_get_params(clk_p->parent->rate, freq, nsdiv,
				     &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	data = SYSCONF_READ(0, 562, 4, 23);
	data &= ~(0x1f << (5 * chan));
	data |= (md << (5 * chan));
	SYSCONF_WRITE(0, 562, 4, 23, data);

	/* PE set */
	switch (chan) {
	case 0:
		SYSCONF_WRITE(0, 563,  4, 18, pe);
		break;
	case 1:
		SYSCONF_WRITE(0, 564, 0, 14, pe);
		break;
	case 2:
		SYSCONF_WRITE(0, 564, 15, 29, pe);
		break;
	default:
		SYSCONF_WRITE(0, 565, 0, 14, pe);
	}

	/* SDIV set */
	data = SYSCONF_READ(0, 565, 16, 31);
	data &= ~(0xf << (4 * chan));
	data |= (sdiv << (4 * chan));
	SYSCONF_WRITE(0, 565, 16, 31, data);

	/* PROG set/reset */
	data = SYSCONF_READ(0, 562, 0, 3);
	SYSCONF_WRITE(0, 562, 0, 3, data | (1 << chan));
	SYSCONF_WRITE(0, 562, 0, 3, data & ~(1 << chan));

	/* NSB set */
	data = SYSCONF_READ(0, 563, 0, 3);
	SYSCONF_WRITE(0, 563, 0, 3, data |  (1 << chan));/* Release "freeze" (NSBi) */

	return 0;
}

/* ========================================================================
   Name:        clkgene_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_set_rate(clk_t *clk_p, unsigned long freq)
{
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKM_E_FS_VCO || clk_p->id > CLKM_MPELPC)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgene_fsyn_set_rate(clk_p, freq);
	if (!err)
		err = clkgene_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgene_recalc
   Description: Get clocks frequencies (in Hz) from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	switch (clk_p->id) {
	case CLKM_E_REF:
		clk_p->rate = clk_p->parent->rate;
		return 0;
	case CLKM_E_FS_VCO ... CLKM_MPELPC:
		return clkgene_fsyn_recalc(clk_p);
	}

	return CLK_ERR_BAD_PARAMETER;
}

/* ========================================================================
   Name:        clkgene_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_init(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks have static parent */

	return clkgene_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgene_fsyn_xable
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgene_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long cfg563_0_3, chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKM_E_FS_VCO || clk_p->id > CLKM_MPELPC)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_E_FS_VCO)
		/* Powering down/up ANALOG part */
		if (enable)
			SYSCONF_WRITE(0, 562, 27, 27, 1);
		else
			SYSCONF_WRITE(0, 562, 27, 27, 0);
	else {
		/* Powering down/up DIGITAL part */
		cfg563_0_3 = SYSCONF_READ(0, 563, 0, 3);
		chan = clk_p->id - CLKM_PIX_MDTP_0;
		if (enable) /* Powering up digital part */
			cfg563_0_3 |= (1 << chan);
		else /* Powering down digital part */
			cfg563_0_3 &= ~(1 << chan);
		SYSCONF_WRITE(0, 563, 0, 3, cfg563_0_3);
	}

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgene_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgene_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_enable(clk_t *clk_p)
{
	return clkgene_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:        clkgene_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgene_disable(clk_t *clk_p)
{
	return clkgene_fsyn_xable(clk_p, 0);
}

/******************************************************************************
CLOCKGEN F
******************************************************************************/

/* ========================================================================
   Name:        clkgenf_fsyn_recalc
   Description: Check FSYN & channels status... active, disabled, standbye
		'clk_p->rate' is updated accordingly.
   Returns:     Error code.
   ======================================================================== */

static int clkgenf_fsyn_recalc(clk_t *clk_p)
{
	unsigned long chan, val;
	unsigned long pe, md, sdiv, ndiv;

	if (!clk_p || !clk_p->parent)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKM_PIX_MAIN_VIDFS;

	/* Is FSYN analog part UP ? */
	if (SYSCONF_READ(0, 558, 27, 27) == 0) {
		/* NO. Analog part is powered down */
		clk_p->rate = 0;
		return 0;
	}

	/* At least analog part (PLL660) is running */
	ndiv = SYSCONF_READ(0, 558, 24, 26);
	if (clk_p->id == CLKM_F_FS_VCO)
		return clk_fs660c32_vco_get_rate(clk_p->parent->rate, ndiv,
					      &clk_p->rate);

	/* Is FSYN digital part UP ? */
	val = SYSCONF_READ(0, 559, 0, 3);
	if ((val & (1 << chan)) == 0) {
		/* Digital standbye */
		clk_p->rate = 0;
		return 0;
	}

	/* FSYN is up and running.
	   Now computing frequency */
	switch (chan) {
	case 0:	pe = SYSCONF_READ(0, 559, 4, 18); break;
	case 1:	pe = SYSCONF_READ(0, 560, 0, 14); break;
	case 2:	pe = SYSCONF_READ(0, 560, 15, 29); break;
	default:
		pe = SYSCONF_READ(0, 561, 0, 14);
	}
	md = SYSCONF_READ(0, 558, 4, 23);
	md = (md >> (5 * chan)) & 0x1f;
	sdiv = SYSCONF_READ(0, 561, 16, 31);
	sdiv = (sdiv >> (4 * chan)) & 0xf;
	return clk_fs660c32_get_rate(clk_p->parent->rate, 1,
				md, pe, sdiv, &clk_p->rate);
}

/* ========================================================================
   Name:        clkgenf_recalc
   Description: Get clocks frequencies (in Hz) from HW
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKM_F_REF || clk_p->id > CLKM_FVDP_PROC)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_F_REF)
		clk_p->rate = clk_p->parent->rate;
	else if ((clk_p->id >= CLKM_F_FS_VCO) &&
		(clk_p->id <= CLKM_FVDP_PROC_FS)) {
		return clkgenf_fsyn_recalc(clk_p);
	} else if ((clk_p->id >= CLKM_PIX_MAIN_SAS) &&
		(clk_p->id <= CLKM_PIX_AUX_SAS)) /* Clocks from SAS */
		clk_p->rate = clk_p->parent->rate;
	else if ((clk_p->id >= CLKM_PIX_MAIN_PIPE) &&
		(clk_p->id <= CLKM_PIX_AUX_PIPE)) {
		/* Video Clock Controller clocks */
		unsigned long chan, val;
		static const unsigned char tab1248[] = { 1, 2, 4, 8 };

		chan = clk_p->id - CLKM_PIX_MAIN_PIPE;
		/* Is the channel stopped ? */
		val = SYSCONF_READ(0, 555, 0, 3);
		val &= (1 <<  chan);
		if (val)	/* 1=stopped */
			clk_p->rate = 0;
		else {
			/* What is the divider ratio ? */
			val = SYSCONF_READ(0, 557, 0, 7);
			val >>= (chan * 2);
			val &= 0x3;
			clk_p->rate = clk_p->parent->rate / tab1248[val];
		}
	} else if (clk_p->id == CLKM_FVDP_PROC)
		clk_p->rate = clk_p->parent->rate;

	return 0;
}

/* ========================================================================
   Name:        clkgenf_identify_parent
   Description: Identify parent clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_identify_parent(clk_t *clk_p)
{
	unsigned long chan, val;
	static const clk_t *fs_parent_clocks[] = {
		&clk_clocks[CLKM_PIX_MAIN_VIDFS],
		&clk_clocks[CLKM_PIX_AUX_VIDFS],
		&clk_clocks[CLKM_PIX_MAIN_SAS],
		&clk_clocks[CLKM_PIX_AUX_SAS]
	};

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	/* Clocks from "Video Clock Controller". */
	if ((clk_p->id >= CLKM_PIX_MAIN_PIPE) &&
	    (clk_p->id <= CLKM_PIX_AUX_PIPE)) {
		chan = clk_p->id - CLKM_PIX_MAIN_PIPE;
		/* sel : 00 clk_hd, 01 clk_sd, 10 clk_hd_ext, 11 clk_sd_ext */
		val = SYSCONF_READ(0, 556, 0, 7);
		val >>= (chan * 2);
		val &= 0x3;
		clk_p->parent = (struct clk *)fs_parent_clocks[val];
	} else if (clk_p->id == CLKM_FVDP_PROC) {
		val = SYSCONF_READ(0, 573, 0, 0);
		if (val)
			clk_p->parent = &clk_clocks[CLKM_FVDP_PROC_FS];
		else
			clk_p->parent = &clk_clocks[CLKM_FVDP_PROC_ALT];
	}

	/* Other clocks are statically initialized
	   thanks to _CLK_P() macro */

	return 0;
}

/* ========================================================================
   Name:        clkgenf_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_init(clk_t *clk_p)
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

static int clkgenf_set_parent(clk_t *clk_p, clk_t *src_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id >= CLKM_PIX_MAIN_PIPE && clk_p->id <= CLKM_PIX_AUX_PIPE) {
		unsigned long chan, val, data;
		switch (src_p->id) {
		case CLKM_PIX_MAIN_VIDFS: val = 0; break;
		case CLKM_PIX_AUX_VIDFS: val = 1; break;
		case CLKM_PIX_MAIN_SAS: val = 2; break;
		case CLKM_PIX_AUX_SAS: val = 3; break;
		default:
			return CLK_ERR_BAD_PARAMETER;
		}
		chan = clk_p->id - CLKM_PIX_MAIN_PIPE;
		data = SYSCONF_READ(0, 556, 0, 7);
		data &= ~(0x3 << (chan * 2));
		data |= (val << (chan * 2));
		SYSCONF_WRITE(0, 556, 0, 7, data);
		clk_p->parent = src_p;
	} else if (clk_p->id == CLKM_FVDP_PROC) {
		if (src_p->id == CLKM_FVDP_PROC_ALT)	/* A1 div 16 */
			SYSCONF_WRITE(0, 573, 0, 0, 0);
		else	/* Fsyn = CLKM_FVDP_PROC_FS */
			SYSCONF_WRITE(0, 573, 0, 0, 1);
		clk_p->parent = src_p;
	}

	#if defined(CLKLLA_NO_PLL)
	/* If NO PLL means emulation like platform. Then HW may be forced in
	   a specific position preventing SW change */
	clkgenf_identify_parent(clk_p);
	#endif

	return clkgenf_recalc(clk_p);
}

/* ========================================================================
   Name:        clkgenf_vcc_set_div
   Description: Video Clocks Controller divider setup function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_vcc_set_div(clk_t *clk_p, unsigned long *div_p)
{
	int chan;
	unsigned long set, data;
	static const unsigned char div_table[] = {
		/* 1  2     3  4     5     6     7  8 */
		   0, 1, 0xff, 2, 0xff, 0xff, 0xff, 3 };

	if (clk_p->id < CLKM_PIX_MAIN_PIPE || clk_p->id > CLKM_PIX_AUX_PIPE)
		return CLK_ERR_BAD_PARAMETER;
	if (*div_p < 1 || *div_p > 8)
		return CLK_ERR_BAD_PARAMETER;

	set = div_table[*div_p - 1];
	if (set == 0xff)
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKM_PIX_MAIN_PIPE;

	/* Set SYSTEM_CONFIG557: div_mode, 2bits per channel */
	data = SYSCONF_READ(0, 557, 0, 7);
	data &= ~(0x3 << (chan * 2));
	data |= (set << (chan * 2));
	SYSCONF_WRITE(0, 557, 0, 7, data);

	return 0;
}

/* ========================================================================
   Name:        clkgenf_fsyn_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_fsyn_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long data, chan;
	unsigned long md, pe, sdiv, ndiv;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_F_FS_VCO) {
		if (clk_fs660c32_vco_get_params(clk_p->parent->rate, freq, &ndiv))
			return CLK_ERR_BAD_PARAMETER;
		SYSCONF_WRITE(0, 558, 24, 26, ndiv);
		SYSCONF_WRITE(0, 558, 27, 27, 1); /* PLL power up */
		return 0;
	}

	/* Computing FSyn params. Should be common function with FSyn type */
	if (clk_fs660c32_dig_get_params(clk_p->parent->rate, freq, 1,
				     &md, &pe, &sdiv))
		return CLK_ERR_BAD_PARAMETER;

	chan = clk_p->id - CLKM_PIX_MAIN_VIDFS;

	/* MD set */
	data = SYSCONF_READ(0, 558, 4, 23);
	data &= ~(0x1f << (chan * 5));
	data |= (md << (chan * 5));
	SYSCONF_WRITE(0, 558, 4, 23, data);

	/* PE set */
	switch (chan) {
	case 0:
		SYSCONF_WRITE(0, 559, 4, 18, pe);
		break;
	case 1:
		SYSCONF_WRITE(0, 560, 0, 14, pe);
		break;
	case 2:
		SYSCONF_WRITE(0, 560, 15, 29, pe);
		break;
	default:
		SYSCONF_WRITE(0, 561, 0, 14, pe);
		break;
	}

	/* SDIV set */
	data = SYSCONF_READ(0, 561, 16, 31);
	data &= ~(0xf << (4 * chan));
	data |= (sdiv << (4 * chan));
	SYSCONF_WRITE(0, 561, 16, 31, data);

	/* Prog set/reset */
	data = SYSCONF_READ(0, 558, 0, 3);
	data |= (1 << chan);
	SYSCONF_WRITE(0, 558, 0, 3, data);	/* Enable prog = consider MD & PE */
	data &= ~(1 << chan);
	SYSCONF_WRITE(0, 558, 0, 3, data);	/* Enable prog release */

	/* NSB set */
	data = SYSCONF_READ(0, 559, 0, 3);
	data |= (1 << chan);
	SYSCONF_WRITE(0, 559, 0, 3, data);	/* Release "freeze" (NSBi) */

	return 0;
}

/* ========================================================================
   Name:        clkgenf_set_rate
   Description: Set FS clock
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_set_rate(clk_t *clk_p, unsigned long freq)
{
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if ((clk_p->id >= CLKM_F_FS_VCO) && (clk_p->id <= CLKM_FVDP_PROC_FS))
		err = clkgenf_fsyn_set_rate(clk_p, freq);
	else if ((clk_p->id >= CLKM_PIX_MAIN_PIPE) &&
		(clk_p->id <= CLKM_PIX_AUX_PIPE)) {
		unsigned long div;

		/* Video Clock Controller clocks */
		div = clk_best_div(clk_p->parent->rate, freq);
		err = clkgenf_vcc_set_div(clk_p, &div);
	}
	/* CLKM_FVDP_PROC = special case (mux output) */
	else if (clk_p->id == CLKM_FVDP_PROC)
		err = clkgenf_set_rate(clk_p->parent, freq);
	else
		return CLK_ERR_BAD_PARAMETER;

	if (!err)
		err = clkgenf_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgenf_fsyn_xable
   Description: Enable/disable FSYN
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_fsyn_xable(clk_t *clk_p, unsigned long enable)
{
	unsigned long cfg559_0_3, chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id < CLKM_F_FS_VCO || clk_p->id > CLKM_FVDP_PROC_FS)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_F_FS_VCO)
		/* Powering down/up ANALOG part */
		if (enable) /* Power up */
			SYSCONF_WRITE(0, 558, 27, 27, 1);
		else
			SYSCONF_WRITE(0, 558, 27, 27, 0);
	else {
		/* Powering down/up DIGITAL part */
		cfg559_0_3 = SYSCONF_READ(0, 559, 0, 3);
		chan = clk_p->id - CLKM_PIX_MAIN_VIDFS;
		if (enable)
			cfg559_0_3 |= (1 << chan);
		else
			cfg559_0_3 &= ~(1 << chan);
		SYSCONF_WRITE(0, 559, 0, 3, cfg559_0_3);
	}

	/* Freq recalc required only if a channel is enabled */
	if (enable)
		return clkgenf_fsyn_recalc(clk_p);
	else
		clk_p->rate = 0;
	return 0;
}

/* ========================================================================
   Name:        clkgenf_enable
   Description: Enable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_enable(clk_t *clk_p)
{
	return clkgenf_fsyn_xable(clk_p, 1);
}

/* ========================================================================
   Name:        clkgenf_disable
   Description: Disable clock
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenf_disable(clk_t *clk_p)
{
	return clkgenf_fsyn_xable(clk_p, 0);
}

/* ========================================================================
   Name:        clkgenf_observe
   Description: Clocks observation function
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenf_observe(clk_t *clk_p, unsigned long *div_p)
{
	unsigned long chan;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (clk_p->id != CLKM_PIX_MAIN_PIPE && clk_p->id != CLKM_PIX_AUX_PIPE &&
		clk_p->id != CLKM_PIX_MAIN_VIDFS)
		return CLK_ERR_BAD_PARAMETER;

	/* Observation points:
	 * VCC channels => PIO107[0] alt 3
	 * PIX_MAIN_VIDFS => PIO107[2] alt 2
	 */

	/* Configuring appropriate PIO */
	if (clk_p->id == CLKM_PIX_MAIN_PIPE || clk_p->id == CLKM_PIX_AUX_PIPE) {
		chan = clk_p->id - CLKM_PIX_MAIN_PIPE;
		SYSCONF_WRITE(0, 566, 3, 6, chan);
		SYSCONF_WRITE(0, 504, 0, 1, 3);         /* Alternate mode */
		SYSCONF_WRITE(0, 506, 0, 0, 1);         /* Enabling IO */
	} else {
		SYSCONF_WRITE(0, 504, 8, 10, 2);        /* Alternate mode */
		SYSCONF_WRITE(0, 506, 2, 2, 1);         /* Enabling IO */
	}
	*div_p = 1; /* No divider available */

	return 0;
}

/******************************************************************************
CLOCKGEN D (DDR sub-systems)
******************************************************************************/

/* ========================================================================
   Name:        clkgenddr_recalc
   Description: Get CKGD (LMI) clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgenddr_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_DDR_REF)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id == CLKM_DDR_IC_LMI0 || clk_p->id == CLKM_DDR_IC_LMI1) {
		#if !defined(CLKLLA_NO_PLL)

		unsigned long idf, ndiv, odf, vcoby2_rate;
		int err;

		idf = SYSCONF_READ(0, 601, 25, 27);
		ndiv = SYSCONF_READ(0, 603, 0, 7);
		err = clk_pll3200c32_get_rate
			(clk_p->parent->rate, idf, ndiv, &vcoby2_rate);
		if (clk_p->id == CLKM_DDR_IC_LMI0)
			odf = SYSCONF_READ(0, 603, 8, 13);
		else
			odf = SYSCONF_READ(0, 603, 14, 19);
		if (odf == 0)
			odf = 1;
		clk_p->rate = vcoby2_rate / odf;

		#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
		#endif
	} else if (clk_p->id == CLKM_DDR_DDR0 || clk_p->id == CLKM_DDR_DDR1)
		clk_p->rate = clk_p->parent->rate * 4;
	else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:        clkgenddr_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgenddr_init(clk_t *clk_p)
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

static int clkgenddr_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long odf, idf, ndiv, cp, vcoby2_rate;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKM_DDR_IC_LMI0) || (clk_p->id > CLKM_DDR_IC_LMI1))
		return CLK_ERR_BAD_PARAMETER;

	return 0;
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

	err = clk_pll3200c32_get_params(clk_p->parent->rate, vcoby2_rate, &idf, &ndiv, &cp);
	if (err != 0)
		return err;

	/* WARNING: How to make it safe when code executed from DDR ? */

	SYSCONF_WRITE(0, 601, 0, 0, 1);/* Power down: syscfg601, bit 0 */
	SYSCONF_WRITE(0, 601, 25, 27, idf);/* idf: sys601, bits 25 to 27 */
	SYSCONF_WRITE(0, 603, 0, 7, ndiv);/* ndiv: syscfg603, bits 0 to 7 */
	SYSCONF_WRITE(0, 601, 0, 0, 0);/* Power up: syscfg601, bit 0 */

	/* Now should wait for PLL lock
	   TO BE COMPLETED !!! */

	if (clk_p->id == CLKM_DDR_IC_LMI0)
		SYSCONF_WRITE(0, 603, 8, 13, odf);
	else
		SYSCONF_WRITE(0, 603, 14, 19, odf);

	return clkgenddr_recalc(clk_p);
}

/******************************************************************************
CA9 PLL
******************************************************************************/

/* ========================================================================
   Name:        clkgena9_recalc
   Description: Get clocks frequencies (in Hz)
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_recalc(clk_t *clk_p)
{
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_A9_REF || clk_p->id == CLKM_A9)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id == CLKM_A9_PHI0) {
		unsigned long idf, ndiv, vcoby2_rate, odf;
		idf = SYSCONF_READ(0, 654, 22, 24);
		ndiv = SYSCONF_READ(0, 654, 9, 16);
		if (SYSCONF_READ(0, 654, 0, 0))
			clk_p->rate = 0;	/* PLL disabled */
		else
			err = clk_pll3200c32_get_rate
				(clk_p->parent->rate, idf, ndiv, &vcoby2_rate);
			if (err)
				return CLK_ERR_BAD_PARAMETER;
		odf = SYSCONF_READ(0, 654, 3, 8);
		if (odf == 0)
			odf = 1;
		clk_p->rate = vcoby2_rate / odf;
	} else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:        clkgena9_identify_parent
   Description: Identify parent clock for clockgen A clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgena9_identify_parent(clk_t *clk_p)
{
	if (clk_p->id != CLKM_A9) /* Other clocks have static parent */
		return 0;

	if (SYSCONF_READ(0, 654, 2, 2)) /* Is CA9 clock sourced from PLL or A10-10 ? */
		if (SYSCONF_READ(0, 654, 1, 1))
			clk_p->parent = &clk_clocks[CLKM_A9_EXT2F];
		else
			clk_p->parent = &clk_clocks[CLKM_A9_EXT2F_DIV2];
	else
		clk_p->parent = &clk_clocks[CLKM_A9_PHI0];

	return 0;
}

/* ========================================================================
   Name:        clkgena9_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgena9_init(clk_t *clk_p)
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

static int clkgena9_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long odf, idf, ndiv, cp, vcoby2_rate;
	int err = 0;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	if (clk_p->id == CLKM_A9)
		return clkgena9_set_rate(clk_p->parent, freq);
	if (clk_p->id != CLKM_A9_PHI0)
		return CLK_ERR_BAD_PARAMETER;

	if (freq < 800000000) {
		odf = 800000000 / freq;
		if (800000000 % freq)
			odf = odf + 1;
	} else
		odf = 1;
	vcoby2_rate = freq * odf;
	err = clk_pll3200c32_get_params(clk_p->parent->rate, vcoby2_rate, &idf, &ndiv, &cp);
	if (err != 0)
		return err;

	SYSCONF_WRITE(0, 654, 2, 2, 1);		/* Bypassing PLL */

	SYSCONF_WRITE(0, 654, 0, 0, 1);		/* Disabling PLL */
	SYSCONF_WRITE(0, 654, 22, 24, idf);	/* IDF: syscfg654, bits 22 to 24 */
	SYSCONF_WRITE(0, 654, 9, 16, ndiv);	/* NDIV: syscfg654, bits 9 to 16 */
	SYSCONF_WRITE(0, 654, 3, 8, odf);
	SYSCONF_WRITE(0, 654, 0, 0, 0);		/* Reenabling PLL */
	/* Now wait for lock */
	while (!SYSCONF_READ(0, 681, 0, 0))
		;
	/* Can't put any delay because may rely on a clock that is currently
	   changing (running from CA9 case). */

	SYSCONF_WRITE(0, 654, 2, 2, 0);		/* Selecting internal PLL */

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

static int clkgengpu_recalc(clk_t *clk_p)
{
	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	if (clk_p->id == CLKM_GPU_REF || clk_p->id == CLKM_GPU)
		clk_p->rate = clk_p->parent->rate;
	else if (clk_p->id == CLKM_GPU_PHI) {
		/* This clock is FVCO/ODF output */
		#if !defined(CLKLLA_NO_PLL)
		unsigned long idf, ldf, odf;

		/* Is the PLL enabled ? */
		if (!SYSCONF_READ(0, 417, 3, 3) ||
		    !(CLK_READ(mali_base + 4) & 1)) {
			clk_p->rate = 0; /* PLL is disabled */
			return 0;
		}

		/* PLL is ON */
		idf = CLK_READ(mali_base + 0) & 0x3;
		ldf = (CLK_READ(mali_base + 0) >> 3) & 0x7f;
		odf = (CLK_READ(mali_base + 0) >> 10) & 0x3f;
		return clk_pll1200c32_get_rate
			(clk_p->parent->rate, idf, ldf, odf, &(clk_p->rate));
		#else
		if (clk_p->nominal_rate)
			clk_p->rate = clk_p->nominal_rate;
		else
			clk_p->rate = 12121212;
		#endif
	} else
		return CLK_ERR_BAD_PARAMETER;	/* Unknown clock */

	return 0;
}

/* ========================================================================
   Name:        clkgengpu_identify_parent
   Description: Identify parent clock for clockgen GPU clocks.
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgengpu_identify_parent(clk_t *clk_p)
{
	if (clk_p->id != CLKM_GPU) /* Other clocks have static parent */
		return 0;

	/* GPU clock PLL PHI (FVCO/ODF), or SATA clock, or clk_je_ref.
	   Note that clk_fe_ref is unsupported by this LLA. */
	if (SYSCONF_READ(0, 417, 2, 2))
		clk_p->parent = &clk_clocks[CLKM_GPU_PHI];
	else
		clk_p->parent = &clk_clocks[CLKM_GPU_REF];

	return 0;
}

/* ========================================================================
   Name:        clkgengpu_init
   Description: Read HW status to initialize 'clk_t' structure.
   Returns:     'clk_err_t' error code.
   ======================================================================== */

static int clkgengpu_init(clk_t *clk_p)
{
	int err;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;

	err = clkgengpu_identify_parent(clk_p);
	if (!err)
		err = clkgengpu_recalc(clk_p);

	return err;
}

/* ========================================================================
   Name:        clkgengpu_set_rate
   Description: Set clock frequency
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int clkgengpu_set_rate(clk_t *clk_p, unsigned long freq)
{
	unsigned long idf, ldf, odf;
	int err = 0;
	unsigned long val;

	if (!clk_p)
		return CLK_ERR_BAD_PARAMETER;
	if ((clk_p->id < CLKM_GPU_PHI) || (clk_p->id > CLKM_GPU))
		return CLK_ERR_BAD_PARAMETER;

	/* We need a parent for these clocks */
	if (!clk_p->parent)
		return CLK_ERR_INTERNAL;

	#if !defined(CLKLLA_NO_PLL)
	switch (clk_p->id) {
	case CLKM_GPU_PHI:
		err = clk_pll1200c32_get_params(clk_p->parent->rate,
			freq, &idf, &ldf, &odf);
		if (err != 0)
			break;

/* WARNING: there is probably something to check/do before changing PLL freq.
   Shouldn't we bypass it first ????
 */

		CLK_WRITE(mali_base + 0, odf << 10 | ldf << 3 | idf);
		val = CLK_READ(mali_base + 0x4);
		CLK_WRITE(mali_base + 0x4, val | (1 << 4)); /* Strobe UP */
		CLK_WRITE(mali_base + 0x4, val); /* Strobe DOWN */
		break;
	case CLKM_GPU:
		if (clk_p->parent->id == CLKM_GPU_PHI)
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
