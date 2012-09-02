/*****************************************************************************
 *
 * File name   : clock-stxSASC1.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2012 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 *****************************************************************************/

enum {
	/* Clockgen A-0 */
	CLK_S_A0_REF,		/* OSC clock */
	CLK_S_A0_PLL0HS,
	CLK_S_A0_PLL0LS,	/* LS = HS/2 */
	CLK_S_A0_PLL1HS,
	CLK_S_A0_PLL1LS,	/* LS = HS/2 */

	CLK_S_A0_CT_DIAG,
	CLK_S_A0_FDMA_0,
	CLK_S_A0_FDMA_1,
	CLK_S_A0_SPARE_3,
	CLK_S_A0_SPARE_4,
	CLK_S_A0_SPARE_5,
	CLK_S_A0_IC_DVA_ST231,
	CLK_S_A0_IC_SEC_ST231,
	CLK_S_A0_SPARE_8,
	CLK_S_A0_SPARE_9,
	CLK_S_A0_IC_CM_ST40,
	CLK_S_A0_IC_DVA_ST40,
	CLK_S_A0_IC_CPU,
	CLK_S_A0_IC_MAIN,
	CLK_S_A0_IC_ROUTER,
	CLK_S_A0_IC_PCIE_SATA,
	CLK_S_A0_SPARE_16,
	CLK_S_A0_IC_FHASH,
	CLK_S_A0_IC_STFE,
	CLK_S_A0_SPARE_19,
	CLK_S_A0_GLOBAL_ROUTER,
	CLK_S_A0_GLOBAL_SATAPCI,
	CLK_S_A0_GLOBAL_PCI_TARG,
	CLK_S_A0_GLOBAL_NETWORK,
	CLK_S_A0_A9_TRACE_INT,
	CLK_S_A0_A9_EXT2F,
	CLK_S_A0_A9_EXT2F_DIV2, /* CLKS_A0_A9_EXT2F divided by 2 */

	/* Clockgen A-1 */
	CLK_S_A1_REF,		/* OSC clock */
	CLK_S_A1_PLL0HS,
	CLK_S_A1_PLL0LS,	/* LS = HS/2 */
	CLK_S_A1_PLL1HS,
	CLK_S_A1_PLL1LS,	/* LS = HS/2 */

	CLK_S_A1_STAC_TX_PHY,
	CLK_S_A1_STAC_BIST,
	CLK_S_A1_VTAC_BIST,
	CLK_S_A1_SPARE_3,
	CLK_S_A1_SPARE_4,
	CLK_S_A1_IC_DDR,
	CLK_S_A1_SPARE_6,
	CLK_S_A1_BLIT_PROC,
	CLK_S_A1_SPARE_8,
	CLK_S_A1_SPARE_9,
	CLK_S_A1_SYS_MMC_SS,
	CLK_S_A1_CARD_MMC_SS,
	CLK_S_A1_IC_EMI,
	CLK_S_A1_BCH_NAND,
	CLK_S_A1_IC_STAC,
	CLK_S_A1_IC_BDISP,
	CLK_S_A1_IC_TANGO,
	CLK_S_A1_IC_GLOBAL_STFE_STAC,
	CLK_S_A1_IC_LP,
	CLK_S_A1_IC_LP_CPU,
	CLK_S_A1_IC_LP_HD,
	CLK_S_A1_IC_DMA,
	CLK_S_A1_IC_SECURE,
	CLK_S_A1_IC_LP_D3,
	CLK_S_A1_IC_LP_DQAM,
	CLK_S_A1_IC_LP_ETH,

	/* CCM div8 */
	CLK_S_CH34REF_DIV_1,
	CLK_S_CH34REF_DIV_2,
	CLK_S_CH34REF_DIV_4,
	CLK_S_CH34REF_DIV_X,

	/* Clockgen B/Video,TANGO */
	CLK_S_B_REF,
	CLK_S_B_VCO,
	CLK_S_B_TP,			/* CLKS_B_CH0 */
	CLK_S_B_HD,			/* CLKS_B_CH1 */
	CLK_S_B_SD,			/* CLKS_B_CH2 */
	CLK_S_B_SECURE,		/* CLKS_B_CH3 */
	CLK_S_B_TMDS,			/* VCC CLK_IN_2 */

	/* Clockgen B: Video Clock Controller */
	CLK_S_B_PIX_MAIN,	/* Chan 0 = clk_pix_main */
	CLK_S_B_PIX_AUX,	/* Chan 1 = clk_pix_aux */
	CLK_S_B_PIX_HDMI,	/* Chan 2 = clk_pix_hdmi */
	CLK_S_B_PIX_DVO,	/* Chan 3 = clk_pix_dvo */
	CLK_S_B_PIX_HDDAC,	/* Chan 4 = clk_pix_hddac */
	CLK_S_B_DENC,		/* Chan 5 = clk_denc */
	CLK_S_B_OUT_HDDAC,	/* Chan 6 = clk_out_hddac */
	CLK_S_B_OUT_SDDAC,	/* Chan 7 = clk_out_sddac */
	CLK_S_B_OUT_DVO,	/* Chan 8 = clk_out_dvo */
	CLK_S_B_HDMI_PLL,	/* Chan 9 = clk_hdmi_pll */
	CLK_S_B_HD_MCRU,	/* Chan 10 = clk_hd_mcru */
	CLK_S_B_SD_MCRU,	/* Chan 11 = clk_sd_mcru */
	CLK_S_B_XD_MCRU,	/* Chan 12 = clk_xd_mcru */
	CLK_S_B_PACE0,		/* Chan 13 = clk_pace */
	CLK_S_B_TMDS_HDMI,	/* Chan 14 = clk_tmds_hdmi */

	/* Clockgen C: Audio FS */
	CLK_S_C_REF,
	CLK_S_C_VCO,
	CLK_S_C_PCM0,		/* CLKS_C_CH0 */
	CLK_S_C_PCM1,		/* CLKS_C_CH1 */
	CLK_S_C_PCM2,		/* CLKS_C_CH2 */
	CLK_S_C_PCM3,		/* CLKS_C_CH3 */

	/* Clockgen D: Telephony subsystem */
	CLK_S_D_REF,
	CLK_S_D_VCO,
	CLK_S_D_FDMA_TEL,	/* CLKS_D_CH0 */
	CLK_S_D_ZSI,		/* CLKS_D_CH1 */
	CLK_S_D_ISIS_ETH,	/* CLKS_D_CH2 */
	CLK_S_D_SPARE,		/* CLKS_D_CH3 */

	/* Clockgen D: Clock Controller Module CCM Clocks */
	/* CCM TEL - CCM A */
	CLK_S_D_TEL_ZSI_TEL,	/* div_1 */
	CLK_S_D_TEL_DIV_2,	/* not used */
	CLK_S_D_TEL_DIV_4,	/* not used */
	CLK_S_D_TEL_ZSI_APPL,
	/* CCM USB - CCM B */
	CLK_S_D_USB_DIV_1,	/* not used */
	CLK_S_D_USB_DIV_2,	/* not used */
	CLK_S_D_USB_DIV_4,	/* not used */
	CLK_S_D_USB_REF,	/* div_x */
	/* CCM ETH - CCM C */
	CLK_S_D_ISIS_ETH_250,	/* div_1 */
	CLK_S_D_ISIS_ETH_125,	/* div_2 */
	CLK_S_D_ISIS_DIV_4,	/* not used */
	CLK_S_D_ISIS_DIV_X,	/* not used */

	/* Clockgen E: DOCSIS subsystem */
	CLK_S_E_REF,
	CLK_S_E_VCO,
	CLK_S_E_FP,			/* CLKS_E_CH0 */
	CLK_S_E_D3_XP70,	/* CLKS_E_CH1 */
	CLK_S_E_IFE,		/* CLKS_E_CH2 */
	CLK_S_E_IFE_WB,		/* CLKS_E_CH3 */

	/* Clockgen E: Clock Controller Module CCM Clocks */
	/* CCM IFE - CCM A */
	CLK_S_E_IFE_216,
	CLK_S_E_IFE_108,
	CLK_S_E_IFE_54,
	CLK_S_E_MCHI,
	/* CCM WB - CCM B */
	CLK_S_E_WB_DIV_1,	/* not used */
	CLK_S_E_WB_2,
	CLK_S_E_WB_DIV_4,	/* not used */
	CLK_S_E_WB_1,

	/* Clockgen F: Miscellanous clocks */
	CLK_S_F_REF,
	CLK_S_F_VCO,
	CLK_S_F_DSS,		/* CLKS_F_CH0 */
	CLK_S_F_PACE1,		/* CLKS_F_CH1 */
	CLK_S_F_PAD_OUT,	/* CLKS_F_CH2 */
	CLK_S_F_TSOUT1_SRC,	/* CLKS_F_CH3 */

	/* CCM LPC */
	CLK_S_G_REF,
	CLK_S_G_REF_DIV50,
	CLK_S_G_DIV_0,		/* not used */
	CLK_S_G_DIV_1,		/* not used */
	CLK_S_G_TMP_SENS,
	CLK_S_G_LPC,

	/* CA9 PLL3200 */
	CLK_S_A9_REF,
	CLK_S_A9_PHI0,		/* PLL1600 FVCOBY2/ODF0 */
	CLK_S_A9,			/* CA9 clock */
	CLK_S_A9_PERIPH		/* ARM.GT and ARM.TWD clock */
};
