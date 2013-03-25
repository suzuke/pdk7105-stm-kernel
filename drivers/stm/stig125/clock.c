/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>
 *            Nunzio Raciti <nunzio.raciti@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/clk.h>

int __init plat_clk_alias_init(void)
{
	/* CA9 clock */
	clk_add_alias("cpu_clk", NULL, "CLK_S_A9", NULL);
	/*
	 * The local/global timer
	 * may not actually be configured, but there is no harm in creating
	 * the aliases anyway.
	 */
	clk_add_alias(NULL, "smp_twd", "CLK_S_A9_PERIPH", NULL);
	clk_add_alias(NULL, "smp_gt", "CLK_S_A9_PERIPH", NULL);

	/* comms clock */
	clk_add_alias("comms_clk", NULL, "CLK_S_A1_IC_LP_HD", NULL);
	clk_add_alias("ssc_comms_clk", NULL, "CLK_S_A1_IC_LP_CPU", NULL);
	clk_add_alias("sbc_comms_clk", NULL, "CLK_SYSIN", NULL);
	clk_add_alias("telss_comms_clk", NULL, "CLK_S_A1_IC_LP_ETH", NULL);

	/* EMI clock */
	clk_add_alias("emi_clk", NULL, "CLK_S_A1_IC_EMI", NULL);
	/* LPC clock */
	clk_add_alias("lpc_clk", NULL, "CLK_S_G_DIV_3", NULL);
	/* SDHCI clocks */
	clk_add_alias(NULL, "sdhci-stm.0", "CLK_S_A1_CARD_MMC_SS",  NULL);
	/* PCIe clocks */
	clk_add_alias("pcie_clk", NULL, "CLK_S_A0_IC_PCIE_SATA", NULL);

	/* FDMA clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLK_S_A0_FDMA_0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLK_S_A0_FDMA_1", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLK_S_D_FDMA_TEL", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.2", "CLK_S_A1_IC_LP_ETH", NULL);
	clk_add_alias("fdma_hi_clk", NULL, "CLK_S_A1_IC_LP_HD", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.2", "CLK_S_A0_IC_ROUTER", NULL);
	clk_add_alias("fdma_low_clk", NULL, "CLK_S_A1_IC_DMA", NULL);
	clk_add_alias("fdma_ic_clk", NULL, "CLK_S_A1_IC_LP_HD", NULL);

	/* USB clocks */
	clk_add_alias("usb_ic_clk", NULL, "CLK_S_A1_IC_LP_HD", NULL);
	clk_add_alias("usb_phy_clk", NULL, "CLK_S_D_USB_REF", NULL);

	/* AHCI */
	clk_add_alias("ahci_clk", NULL, "CLK_S_A0_GLOBAL_SATAPCI", NULL);

	/* Uniperipheral player clocks */
	clk_add_alias("uni_player_clk", "snd_uni_player.0", "CLK_S_C_PCM0",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.1", "CLK_S_C_PCM1",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.2", "CLK_S_C_PCM2",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.3", "CLK_S_C_PCM0",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.4", "CLK_S_C_PCM3",
			NULL);

	/* Uniperipheral TDM clocks */
	clk_add_alias("uniperif_tdm_clk", "snd_uniperif_tdm.0",
			"CLK_S_D_ZSI", NULL);
	clk_add_alias("uniperif_tdm_pclk", "snd_uniperif_tdm.0",
			"CLK_S_D_TEL_ZSI_APPL", NULL);

	return 0;
}
