/*
 * Copyright (C) 2012 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7108.
 */

#include <linux/init.h>
#include <linux/clk.h>

int __init plat_clk_alias_init(void)
{
	clk_add_alias("cpu_clk", NULL, "CLK_M_A9", NULL);
#if defined(CONFIG_ARM) && defined(CONFIG_HAVE_ARM_TWD)
	clk_add_alias(NULL, "smp_twd", "CLK_M_A9_PERIPHS", NULL);
#endif
	clk_add_alias(NULL, "smp_gt", "CLK_M_A9_PERIPHS", NULL);
	/* comms clk */
	clk_add_alias("comms_clk", NULL, "CLK_S_ICN_REG_LP_0", NULL);
	/* SBC clk */
	clk_add_alias("sbc_comms_clk", NULL, "CLK_SYSIN", NULL);
	/* EMI clk */
	clk_add_alias("emi_clk", NULL, "CLK_S_EMISS", NULL);

	/* sdhci */
	clk_add_alias(NULL, "sdhci-stm.0", "CLK_S_CARD_MMC_0",  NULL);
	clk_add_alias(NULL, "sdhci-stm.1", "CLK_S_CARD_MMC_1",  NULL);

	/* fmdas  MPE42 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLK_M_FDMA_10", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLK_M_FDMA_11", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLK_M_FDMA_12", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.0", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.1", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.2", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.0", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.1", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.2", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.0", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.1", "CLK_M_ICN_REG_10", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.2", "CLK_M_ICN_REG_10", NULL);

	/* fdmas SASG2 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.3", "CLK_S_FDMA_0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.4", "CLK_S_FDMA_1", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.3", "CLK_S_ICN_REG_0",  NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.4", "CLK_S_ICN_REG_0",  NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.3", "CLK_S_ICN_REG_LP_0", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.4", "CLK_S_ICN_REG_LP_0", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.3", "CLK_S_ICN_REG_0", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.4", "CLK_S_ICN_REG_0", NULL);

	/* USB clk */
	clk_add_alias("usb_ic_clk", NULL, "CLK_S_ICN_REG_0",  NULL);
	clk_add_alias("usb_48_clk", NULL, "CLK_S_USB48", NULL);
	/* ETH */
	clk_add_alias(NULL, "stmmaceth.0", "CLK_S_ICN_REG_0", NULL);
	clk_add_alias(NULL, "stmmaceth.1", "CLK_S_ICN_REG_0", NULL);

	/* GPU */
	clk_add_alias("gpu_clk", NULL, "CLK_M_GPU", NULL);
	/* AHCI */
	clk_add_alias("ahci_clk", NULL, "CLK_S_ICN_REG_0", NULL);

	/* Uniperipheral player clocks */
	clk_add_alias("uni_player_clk", "snd_uni_player.0", "CLK_S_PCM_0",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.1", "CLK_S_PCM_1",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.2", "CLK_S_PCM_2",
			NULL);
	clk_add_alias("uni_player_clk", "snd_uni_player.3", "CLK_S_PCM_3",
			NULL);

	/* JPEG-Decoder */
	clk_add_alias("c8jpg_dec", NULL, "CLK_M_JPEGDEC", NULL);
	clk_add_alias("c8jpg_icn", NULL, "CLK_M_ICN_BDISP", NULL);
	clk_add_alias("c8jpg_targ", NULL, "CLK_M_ICN_REG_14", NULL);

	return 0;
}
