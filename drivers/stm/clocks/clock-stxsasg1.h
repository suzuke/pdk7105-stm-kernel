/*****************************************************************************
 *
 * File name   : clock-stxSASG1.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

enum {
	/* Clockgen A-0 (REAR) */
	CLKS_A0_REF,		/* OSC clock */
	CLKS_A0_PLL0HS,
	CLKS_A0_PLL0LS,		/* LS = HS/2 */
	CLKS_A0_PLL1,

	CLKS_FDMA_0,
	CLKS_FDMA_1,
	CLKS_JIT_SENSE,
	CLKS_A0_SPARE_3,
	CLKS_ICN_REG_0,
	CLKS_ICN_IF_0,
	CLKS_ICN_REG_LP_0,
	CLKS_EMISS,
	CLKS_ETH1_PHY,
	CLKS_MII1_REF_CLK_OUT,
	CLKS_A0_SPARE_10,
	CLKS_A0_SPARE_11,
	CLKS_A0_SPARE_12,
	CLKS_A0_SPARE_13,
	CLKS_A0_SPARE_14,
	CLKS_A0_SPARE_15,
	CLKS_A0_SPARE_16,
	CLKS_A0_SPARE_17,

	/* Clockgen A-1 (FRONT) */
	CLKS_A1_REF,		/* OSC clock */
	CLKS_A1_PLL0HS,
	CLKS_A1_PLL0LS,		/* LS = HS/2 */
	CLKS_A1_PLL1,

	CLKS_ADP_WC_STAC,
	CLKS_ADP_WC_VTAC,
	CLKS_STAC_TX_CLK_PLL,
	CLKS_STAC,
	CLKS_ICN_IF_2,
	CLKS_CARD_MMC,
	CLKS_ICN_IF_1,
	CLKS_GMAC0_PHY,
	CLKS_NAND_CTRL,
	CLKS_DCEIMPD_CTRL,
	CLKS_MII0_PHY_REF_CLK,
	CLKS_A1_SPARE_11,
	CLKS_A1_SPARE_12,
	CLKS_A1_SPARE_13,
	CLKS_A1_SPARE_14,
	CLKS_A1_SPARE_15,
	CLKS_A1_SPARE_16,
	CLKS_TST_MVTAC_SYS,

	/* Clockgen B/Audio */
	CLKS_B_REF,
	CLKS_B_USB48,		/* CLKS_B_FS0_CH1, 48Mhz (ex-CLK_PIX_HD) */
	CLKS_B_DSS,		/* CLKS_B_FS0_CH2, 36.864Mhz */
	CLKS_B_DAA,		/* CLKS_B_FS0_CH3, 32.768Mhz */
	CLKS_B_THSENS_SCARD,	/* CLKS_B_FS0_CH4, 36.864Mhz = CLK_LPC */
	CLKS_B_PCM_FSYN0,	/* CLKS_B_FS1_CH1, 49.15Mhz */
	CLKS_B_PCM_FSYN1,	/* CLKS_B_FS1_CH2, 49.15Mhz */
	CLKS_B_PCM_FSYN2,	/* CLKS_B_FS1_CH3, 49.15Mhz */
	CLKS_B_PCM_FSYN3,	/* CLKS_B_FS1_CH4, 49.15Mhz */

	/* Clockgen C: Video FS */
	CLKS_C_REF,
	CLKS_C_PIX_HD_VCC,	/* CLKS_C_FS0_CH1, clk_pix_hd_to_video_div, clk_hd */
	CLKS_C_PIX_SD_VCC, 	/* CLKS_C_FS0_CH2, clk_pix_sd_to_video_div, clk_sd */
	CLKS_C_FS0_CH3,		/* spare */
	CLKS_C_FS0_CH4,		/* spare */

	/* Clockgen C: Video Clock Controller */
	CLKS_C_PIX_HDMI,	/* Chan 0 = clk_pix_hdmi */
	CLKS_C_PIX_DVO,		/* Chan 1 = clk_pix_dvo */
	CLKS_C_OUT_DVO,		/* Chan 2 = clk_out_dvo */
	CLKS_C_PIX_HDDAC,	/* Chan 3 = clk_pix_hddac */
	CLKS_C_OUT_HDDAC,	/* Chan 4 = clk_out_hddac */
	CLKS_C_DENC,		/* Chan 5 = clk_denc */
	CLKS_C_OUT_SDDAC,	/* Chan 6 = clk_out_sddac */
	CLKS_C_PIX_MAIN,	/* Chan 7 = clk_pix_main, clk_disp_hd, to MPE */
	CLKS_C_PIX_AUX,		/* Chan 8 = clk_pix_aux, clk_hdmi_rejection_pll, to MPE */
	CLKS_C_PACE0,		/* Chan 9 = clk_pix_aux_stfe */
	CLKS_C_REF_MCRU,	/* Chan 10 */
	CLKS_C_SLAVE_MCRU,	/* Chan 11 */

	/* Clockgen D: CCSC, MCHI, TSout Src, ref clk for MMCRU */
	CLKS_D_REF,
	CLKS_D_CCSC,		/* CLKS_D_FS0_CH1, STFE */
	CLKS_D_PACE1,		/* CLKS_D_FS0_CH2, STFE, CLK_PIX_AUX1_STFE */
	CLKS_D_TSOUT1_SRC,	/* CLKS_D_FS0_CH3, STFE, CLK_TSOUT1_SRC */
	CLKS_D_MCHI		/* CLKS_D_FS0_CH4 */
};
