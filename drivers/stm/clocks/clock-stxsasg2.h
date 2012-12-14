/*****************************************************************************
 *
 * File name   : clock-stxsasg2.h
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
	CLK_S_A0_PLL1,

	CLK_S_FDMA_0,
	CLK_S_FDMA_1,
	CLK_S_JIT_SENSE,
	CLK_S_A0_SPARE_3,
	CLK_S_ICN_REG_0,
	CLK_S_ICN_IF_0,
	CLK_S_ICN_REG_LP_0,
	CLK_S_EMISS,
	CLK_S_ETH1_PHY,
	CLK_S_MII1_REF_OUT,
	CLK_S_A0_SPARE_10,
	CLK_S_A0_SPARE_11,
	CLK_S_A0_SPARE_12,
	CLK_S_A0_SPARE_13,
	CLK_S_A0_SPARE_14,
	CLK_S_A0_SPARE_15,
	CLK_S_A0_SPARE_16,
	CLK_S_A0_SPARE_17,

	/* Clockgen A-1 */
	CLK_S_A1_REF,		/* OSC clock */
	CLK_S_A1_PLL0HS,
	CLK_S_A1_PLL0LS,	/* LS = HS/2 */
	CLK_S_A1_PLL1,

	CLK_S_ADP_WC_STAC,
	CLK_S_ADP_WC_VTAC,
	CLK_S_STAC_TX_CLK_PLL,
	CLK_S_STAC,
	CLK_S_ICN_IF_2,
	CLK_S_CARD_MMC_0,
	CLK_S_ICN_IF_1,
	CLK_S_GMAC0_PHY,
	CLK_S_NAND_CTRL,
	CLK_S_DCEIMPD_CTRL,
	CLK_S_MII0_REF_OUT,
	CLK_S_A1_SPARE_11,
	CLK_S_CARD_MMC_1,
	CLK_S_A1_SPARE_13,
	CLK_S_A1_SPARE_14,
	CLK_S_A1_SPARE_15,
	CLK_S_A1_SPARE_16,
	CLK_S_TST_MVTAC_SYS,

	/* Clockgen B/Audio-USB48-transport */
	CLK_S_B_REF,
	CLK_S_USB48,		/* FS216-0, chan 0, 48Mhz (ex-CLK_PIX_HD) */
	CLK_S_DSS,		/* FS216-0, chan 1, 36.864Mhz */
	CLK_S_STFE_FRC_2,	/* FS216-0, chan 2, 32.768Mhz */
	CLK_S_THSENS_SCARD,	/* FS216-0, chan 3, 36.864Mhz = CLK_LPC */
	CLK_S_PCM_0,		/* FS216-1, chan 0, HDMI */
	CLK_S_PCM_1,		/* FS216-1, chan 1, Multi-chan I2S out */
	CLK_S_PCM_2,		/* FS216-1, chan 2, Stereo analog DACs */
	CLK_S_PCM_3,		/* FS216-1, chan 3, SPDIF out */

	/* Clockgen C: Video FS */
	CLK_S_C_REF,
	CLK_S_C_FS0_CH0,	/* Chan 0: To PIX_HD_VCC mux */
	CLK_S_VCC_SD,		/* Chan 1: To 'clk_sd' VCC input */
	CLK_S_C_FS0_CH2,	/* Chan 2: To 'clk_sd_ext', for THSENS only */
	CLK_S_C_FS0_CH3,	/* Chan 3: UNUSED */

	CLK_S_VCC_HD,		/* Mux output. To 'clk_hd' VCC input */
	CLK_S_TMDS_FROMPHY,	/* TMDS clock from HDMI PHY */

	/* Clockgen C: Video Clock Controller */
	CLK_S_PIX_HDMI,		/* Chan 0 = clk_pix_hdmi */
	CLK_S_PIX_DVO,		/* Chan 1 = clk_pix_dvo */
	CLK_S_OUT_DVO,		/* Chan 2 = clk_out_dvo */
	CLK_S_PIX_HD,		/* Chan 3 = clk_pix_hddac */
	CLK_S_HDDAC,		/* Chan 4 = clk_out_hddac */
	CLK_S_DENC,		/* Chan 5 = clk_denc */
	CLK_S_SDDAC,		/* Chan 6 = clk_out_sddac */
	CLK_S_PIX_MAIN,		/* Chan 7 = clk_pix_main, clk_disp_hd, to MPE */
	CLK_S_PIX_AUX,		/* Chan 8 = clk_pix_aux, to MPE */
	CLK_S_STFE_FRC_0,	/* Chan 9 = clk_pix_aux_stfe */
	CLK_S_REF_MCRU,		/* Chan 10 */
	CLK_S_SLAVE_MCRU,	/* Chan 11 */
	CLK_S_TMDS_HDMI,	/* Chan 12 */
	CLK_S_HDMI_REJECT_PLL,/* Chan 13 */
	CLK_S_THSENS,		/* Chan 14 */

	/* Clockgen D: FS216 */
	CLK_S_D_REF,
	CLK_S_CCSC,		/* FS216-0, chan 0, CLK_S_TSOUT_0 */
	CLK_S_STFE_FRC_1,	/* FS216-0, chan 1 */
	CLK_S_TSOUT_1,		/* FS216-0, chan 2 */
	CLK_S_MCHI,		/* FS216-0, chan 3 */

	/* HDMI RX IP */
	CLK_S_PIX_HDMIRX
};
