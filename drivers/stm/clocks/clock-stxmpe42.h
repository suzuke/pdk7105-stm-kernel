/*****************************************************************************
 *
 * File name   : clock-stxmpe42.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

enum {
	/* Clockgen A10 */
	CLK_M_A0_REF,		/* OSC clock */
	CLK_M_A0_PLL0,		/* FVCOby2 output */
	CLK_M_A0_PLL1,		/* FVCOby2 output */
	CLK_M_A0_PLL0_PHI0,	/* PHI0 (FVCOBY2/ODF0) */
	CLK_M_A0_PLL0_PHI1,	/* PHI1 (FVCOBY2/ODF1) */
	CLK_M_A0_PLL0_PHI2,	/* PHI2 (FVCOBY2/ODF2) */
	CLK_M_A0_PLL0_PHI3,	/* PHI3 (FVCOBY2/ODF3) */
	CLK_M_A0_PLL1_PHI0,	/* PHI0 (FVCOBY2/ODF0) */
	CLK_M_A0_PLL1_PHI1,	/* PHI1 (FVCOBY2/ODF1) */
	CLK_M_A0_PLL1_PHI2,	/* PHI2 (FVCOBY2/ODF2) */
	CLK_M_A0_PLL1_PHI3,	/* PHI3 (FVCOBY2/ODF3) */

	CLK_M_A0_SPARE_0,
	CLK_M_A0_SPARE_1,
	CLK_M_FDMA_12,
	CLK_M_A0_SPARE_3,
	CLK_M_PP_DMU_0,
	CLK_M_PP_DMU_1,
	CLK_M_ICN_LMI,
	CLK_M_VID_DMU_0,
	CLK_M_VID_DMU_1,
	CLK_M_A0_SPARE_9,
	CLK_M_A9_EXT2F,
	CLK_M_ST40RT,
	CLK_M_ST231_DMU_0,
	CLK_M_ST231_DMU_1,
	CLK_M_ST231_AUD,
	CLK_M_ST231_GP_0,
	CLK_M_ST231_GP_1,
	CLK_M_ICN_CPU,
	CLK_M_ICN_STAC,
	CLK_M_TX_ICN_DMU_0,
	CLK_M_TX_ICN_DMU_1,
	CLK_M_TX_ICN_TS,
	CLK_M_TX_ICN_VDP_0,
	CLK_M_TX_ICN_VDP_1,
	CLK_M_A0_SPARE_24,
	CLK_M_A0_SPARE_25,
	CLK_M_A0_SPARE_26,
	CLK_M_A0_SPARE_27,
	CLK_M_ICN_VP8,
	CLK_M_A0_SPARE_29,
	CLK_M_ICN_REG_11,
	CLK_M_A9_TRACE,
	CLK_M_A9_EXT2F_DIV2,	/* CLK_M_A9_EXT2F divided by 2 */

	/* Clockgen A11 */
	CLK_M_A1_REF,		/* OSC clock */
	CLK_M_A1_PLL0,		/* FVCOby2 output */
	CLK_M_A1_PLL1,		/* FVCOby2 output */
	CLK_M_A1_PLL0_PHI0,
	CLK_M_A1_PLL0_PHI1,
	CLK_M_A1_PLL0_PHI2,
	CLK_M_A1_PLL0_PHI3,
	CLK_M_A1_PLL1_PHI0,
	CLK_M_A1_PLL1_PHI1,
	CLK_M_A1_PLL1_PHI2,
	CLK_M_A1_PLL1_PHI3,

	CLK_M_A1_SPARE_0,
	CLK_M_FDMA_10,
	CLK_M_FDMA_11,
	CLK_M_HVA_ALT,
	CLK_M_PROC_SC,
	CLK_M_TP,
	CLK_M_RX_ICN_DMU_0,
	CLK_M_RX_ICN_DMU_1,
	CLK_M_RX_ICN_TS,
	CLK_M_RX_ICN_VDP_0,
	CLK_M_A1_SPARE_10,
	CLK_M_PRV_T1_BUS,
	CLK_M_ICN_REG_12,
	CLK_M_ICN_REG_10,
	CLK_M_A1_SPARE_14,
	CLK_M_ICN_ST231,
	CLK_M_FVDP_PROC_ALT,	/* To mux for CLK_M_FVDP_PROC */
	CLK_M_ICN_REG_13,
	CLK_M_TX_ICN_GPU,
	CLK_M_RX_ICN_GPU,
	CLK_M_A1_SPARE_20,
	CLK_M_A1_SPARE_21,
	CLK_M_APB_PM_12,
	CLK_M_A1_SPARE_23,
	CLK_M_A1_SPARE_24,
	CLK_M_A1_SPARE_25,
	CLK_M_A1_SPARE_26,
	CLK_M_A1_SPARE_27,
	CLK_M_A1_SPARE_28,
	CLK_M_A1_SPARE_29,
	CLK_M_A1_SPARE_30,
	CLK_M_GPU_ALT,

	/* Clockgen A12 */
	CLK_M_A2_REF,		/* OSC clock */
	CLK_M_A2_PLL0,		/* FVCOby2 output */
	CLK_M_A2_PLL1,		/* FVCOby2 output */
	CLK_M_A2_PLL0_PHI0,
	CLK_M_A2_PLL0_PHI1,
	CLK_M_A2_PLL0_PHI2,
	CLK_M_A2_PLL0_PHI3,
	CLK_M_A2_PLL1_PHI0,
	CLK_M_A2_PLL1_PHI1,
	CLK_M_A2_PLL1_PHI2,
	CLK_M_A2_PLL1_PHI3,

	CLK_M_VTAC_MAIN_PHY,
	CLK_M_VTAC_AUX_PHY,
	CLK_M_STAC_PHY,
	CLK_M_STAC_SYS,
	CLK_M_MPESTAC_PG,
	CLK_M_MPESTAC_WC,
	CLK_M_MPEVTACAUX_PG,
	CLK_M_MPEVTACMAIN_PG,
	CLK_M_MPEVTACRX0_WC,
	CLK_M_MPEVTACRX1_WC,
	CLK_M_COMPO_MAIN,
	CLK_M_COMPO_AUX,
	CLK_M_BDISP_0,
	CLK_M_BDISP_1,
	CLK_M_ICN_BDISP,
	CLK_M_ICN_COMPO,
	CLK_M_ICN_VDP_2,
	CLK_M_A2_SPARE_17,
	CLK_M_ICN_REG_14,
	CLK_M_MDTP,
	CLK_M_JPEGDEC,
	CLK_M_A2_SPARE_21,
	CLK_M_DCEPHY_IMPCTRL,
	CLK_M_A2_SPARE_23,
	CLK_M_APB_PM_11,
	CLK_M_A2_SPARE_25,
	CLK_M_A2_SPARE_26,
	CLK_M_A2_SPARE_27,
	CLK_M_A2_SPARE_28,
	CLK_M_A2_SPARE_29,
	CLK_M_A2_SPARE_30,
	CLK_M_A2_SPARE_31,

	/* Clockgen E (Internal video sources DMA) */
	CLK_M_E_REF,
	CLK_M_E_FS_VCO,		/* FS embedded PLL VCOCLK output */
	CLK_M_PIX_MDTP_0,	/* FS out 0 */
	CLK_M_PIX_MDTP_1,	/* FS out 1 */
	CLK_M_PIX_MDTP_2,	/* FS out 2 */
	CLK_M_MPELPC,		/* FS out 3 = CLK_M_THERMAL_SENS */

	/* Clockgen F (Video post processing) */
	CLK_M_F_REF,

	/* Clockgen F: FS */
	CLK_M_F_FS_VCO,		/* FS embedded PLL VCOCLK output */
	CLK_M_PIX_MAIN_VIDFS,	/* FS out 0. To VCC "clk_hd_ext" input */
	CLK_M_HVA_FS,		/* FS out 1. To HVA mux and VCC clk_sd mux */
	CLK_M_FVDP_VCPU,	/* FS out 2 = clk_vcpu */
	CLK_M_FVDP_PROC_FS,	/* FS out 3. To mux for CLK_M_FVDP_PROC */

	/* Clockgen F: From SAS */
	CLK_M_PIX_MAIN_SAS,	/* PIX_HD from SAS. To clk_hd VCC MUX */
	CLK_M_PIX_AUX_SAS,	/* PIX_SD from SAS. To clk_sd VCC MUX */
	CLK_M_PIX_HDMIRX_SAS,	/* PIX HDMI RX from SAS. To VCC "clk_sd_ext" */

	/* Clockgen F: Muxes outputs */
	CLK_M_HVA,		/* Mux output.
				   Inputs=CLK_M_HVA_ALT or CLK_M_HVA_FS */
	CLK_M_FVDP_PROC,		/* Mux output.
				   Inputs=CLK_M_FVDP_PROC_ALT or
					  CLK_M_FVDP_PROC_FS
				 */
	CLK_M_F_VCC_HD,		/* Mux output to VCC "clk_hd".
				   Inputs=CLK_M_PIX_MAIN_SAS or
					  CLK_M_PIX_MAIN_VIDFS
				 */
	CLK_M_F_VCC_SD,		/* Mux output to VCC "clk_sd".
				   Inputs=CLK_M_PIX_AUX_SAS or CLK_M_HVA_FS */

	/* Clockgen F: Video Clock Controller */
	CLK_M_PIX_MAIN_PIPE,	/* VCC out 0 */
	CLK_M_PIX_AUX_PIPE,	/* VCC out 1 */
	CLK_M_PIX_MAIN_CRU,	/* VCC out 2 */
	CLK_M_PIX_AUX_CRU,	/* VCC out 3 */
	CLK_M_XFER_BE_COMPO,	/* VCC out 4 */
	CLK_M_XFER_PIP_COMPO,	/* VCC out 5 */
	CLK_M_XFER_AUX_COMPO,	/* VCC out 6 */
	CLK_M_VSENS,		/* VCC out 7 */
	CLK_M_PIX_HDMIRX_0,	/* VCC out 8 */
	CLK_M_PIX_HDMIRX_1,	/* VCC out 9 */

	/* Clockgen DDR */
	CLK_M_DDR_REF,
	CLK_M_DDR_IC_LMI0,	/* PHI0 (FVCOBY2/ODF0) */
	CLK_M_DDR_IC_LMI1,	/* PHI1 (FVCOBY2/ODF1) */
	CLK_M_DDR_DDR0,		/* DDR0 clock (IC_LMIx4) */
	CLK_M_DDR_DDR1,		/* DDR0 clock (IC_LMIx4) */

	/* CA9 PLL3200 */
	CLK_M_A9_REF,
	CLK_M_A9_PHI0,		/* PLL3200 FVCOBY2/ODF0 */
	CLK_M_A9,		/* CA9 clock */
	CLK_M_A9_PERIPHS,	/* CA9.gt CA9.twd clock */

	/* MALI400/GPU PLL1200 */
	CLK_M_GPU_REF,		/* clk_pll_ref = PLL INFF input */
	CLK_M_GPU_PHI,		/* PLL1200 PHI output (FVCO/ODF)*/
	CLK_M_GPU		/* GPU clock (clk_mali) */
};
