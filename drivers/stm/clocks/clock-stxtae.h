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

