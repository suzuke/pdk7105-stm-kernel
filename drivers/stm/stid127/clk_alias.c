/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STiD127 (Alicante cut 1)
 */

#include <linux/init.h>
#include <linux/clk.h>

int __init stid127_plat_clk_alias_init(void)
{
	/* CA9 clock */
	clk_add_alias("cpu_clk", NULL, "CLK_A9", NULL);

	/*
	 * The local/global timer
	 * may not actually be configured, but there is no harm in creating
	 * the aliases anyway.
	 */
	clk_add_alias(NULL, "smp_gt", "CLK_A9_PERIPHS", NULL);
	clk_add_alias(NULL, "smp_twd", "CLK_A9_PERIPHS", NULL);

	/* comms clocks */
	clk_add_alias("comms_clk", NULL, "CLK_IC_LP_HD", NULL);

	/* SPI clock */
	clk_add_alias("emi_clk", NULL, "CLK_IC_SPI", NULL);

	/* FDMA clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLK_FDMA_0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLK_FDMA_1", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLK_FDMA_TEL", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.0", "CLK_IC_LP_HD", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.1", "CLK_IC_LP_HD", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.2", "CLK_IC_LP_ETH", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.0", "CLK_IC_DMA", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.1", "CLK_IC_DMA", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.2", "CLK_IC_ROUTER", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.0", "CLK_IC_LP_HD", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.1", "CLK_IC_LP_HD", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.2", "CLK_IC_LP_ETH", NULL);

	/* TELSS clocks */
	clk_add_alias("telss_comms_clk", NULL, "CLK_IC_LP_ETH", NULL);

	/* USB clocks */
	clk_add_alias("usb_ic_clk", NULL, "CLK_IC_LP_HD", NULL);
	clk_add_alias("usb_phy_clk", NULL, "CLK_USB_REF", NULL);

	/* ETH */
	clk_add_alias(NULL, "stmmaceth.0", "CLK_IC_LP_DQAM", NULL);

	return 0;
}
