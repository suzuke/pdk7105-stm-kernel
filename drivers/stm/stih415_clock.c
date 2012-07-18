/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7108.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	clk_add_alias("cpu_clk", NULL, "CLKM_A9", NULL);
#if defined(CONFIG_ARM) && defined(CONFIG_HAVE_ARM_TWD)
	clk_add_alias(NULL, "smp_twd", "arm_periph_clk", NULL);
#endif
	/* comms clk */
	clk_add_alias("comms_clk", NULL, "CLKS_ICN_REG_0", NULL);
	/* module clk ?!?!?! */
	clk_add_alias("module_clk", NULL, "CLKS_ICN_IF_0", NULL);
	/* EMI clk */
	clk_add_alias("emi_clk", NULL, "CLKS_EMISS", NULL);
	/* SBC clk */
	clk_add_alias("sbc_comms_clk", NULL, "CLK_SYSIN", NULL);

	/* fdmas MPE41 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLKM_FDMA_10", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLKM_FDMA_11", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLKM_FDMA_12", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.0", "CLKM_ICN_TS", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.1", "CLKM_ICN_TS", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.2", "CLKM_ICN_TS", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.0", "CLKM_ICN_REG_LP_10", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.1", "CLKM_ICN_REG_LP_10", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.2", "CLKM_ICN_REG_LP_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.0", "CLKM_ICN_REG_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.1", "CLKM_ICN_REG_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.2", "CLKM_ICN_REG_10", NULL);

	/* fdmas SASG1 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.3", "CLKS_FDMA_0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.4", "CLKS_FDMA_1", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.3", "CLKS_ICN_IF_0",  NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.4", "CLKS_ICN_IF_0",  NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.3", "CLKS_ICN_REG_LP_0", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.4", "CLKS_ICN_REG_LP_0", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.3", "CLKS_ICN_REG_0", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.4", "CLKS_ICN_REG_0", NULL);

	/* PCI clk */
	clk_add_alias("pci_clk", NULL, "CLKS_ICN_IF_2", NULL);
	clk_add_alias("pcie_clk", NULL, "CLKS_ICN_IF_2", NULL);

	/* USB */
	clk_add_alias("usb_48_clk", NULL, "CLKS_B_USB48", NULL);
	clk_add_alias("usb_ic_clk", NULL, "CLKS_IC_IF_2", NULL);
	clk_add_alias("usb_phy_clk", NULL, "USB2_TRIPLE_PHY", NULL);

	/* SDHCI clocks */
	clk_add_alias(NULL, "sdhci-stm.0", "CLKS_CARD_MMC",  NULL);

	/* LPC */
	clk_add_alias("lpc_clk", NULL, "CLKM_MPELPC", NULL);

	/* ETH-1 */
	clk_add_alias("stmmac_clk", NULL, "CLKS_ICN_REG_0", NULL);

	/* Uniperipheral player clocks */
	clk_add_alias("uni_player_clk", "snd_uni_player.0", "CLKS_B_PCM_FSYN0",
		NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.1", "CLKS_B_PCM_FSYN1",
		NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.2", "CLKS_B_PCM_FSYN2",
		NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.3", "CLKS_B_PCM_FSYN3",
		NULL);
	return 0;
}
