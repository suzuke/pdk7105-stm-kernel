/*****************************************************************************
 *
 * File name   : clock-fli7510.h
 * Description : Low Level API - Clocks identifiers
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/


enum {
	/* Top level clocks */
	/* Clockgen A */
	CLKA_REF,		/* Clockgen A reference clock */
	CLKA_PLL0HS,		/* PLL0 HS output */
	CLKA_PLL0LS,		/* PLL0 LS output */
	CLKA_PLL1,

	CLKA_VTAC_0_PHY,		/* HS[0] */
	CLKA_VTAC_1_PHY,
	CLKA_STAC_PHY,
	CLKA_DCE_PHY,

	CLKA_STAC_DIGITAL,		/* LS[4] */
	CLKA_AIP,
	CLKA_RESV0,
	CLKA_FDMA,
	CLKA_RESV1,
	CLKA_AATV,
	CLKA_EMI,
	CLKA_GMAC_LPM, /* ETH0 */
	CLKA_GMAC, /* Eth1 */
	CLKA_PCI,
	CLKA_IC_100,
	CLKA_IC_150,
	CLKA_ETHERNET,		/* Ls[16] */
	CLKA_IC_200,
	CLK_NOT_USED_2,
};

enum {
	/* 30Mhz OSC */
	CLKSOUTH_REF,

	CLK27_RECOVERY_1,
	CLK_SMARTCARD,
	CLK_NOUSED_0, /* CLK_VDAC */
	CLK_DENC,
	CLK_NOTUSED_1, /* CLK_PIX_AUX */
	CLK_NOTUSED_2, /* CLK_PIX_MDTP_1 */
	CLK_NOTUSED_3, /* CLK27_MCHI */
	CLK27_RECOVERY_2,

	CLK_CCSC,
	CLK_SECURE,
	CLK_SD_MS,
	CLK_NOTUSED_4, /* CLK_PIX_MDTP_2 */
	CLK_NOTUSED_5, /* CLK_GPADC */
	CLK_NOTUSED_6, /* CLK_RTC */

	CLK_USB1_48,
	CLK_USB2_48,
	CLK_USB1_60,
	CLK_NOTUSED_7, /* CLK_USB2_60 */
	CLK_NOTUSED_8, /* MCTI_VIP */
	CLK_MCTI_MCTI,
	CLK_NOTUSED_9, /* CLK_EXT_USB_PHY  */
	CLK_NOTUSED_10, /* CLK_GDP_PROC */

};
