/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the clock aliases on the STx7108.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>

int __init plat_clk_alias_init(void)
{
	clk_add_alias("cpu_clk", NULL, "CLKM_A9", NULL);
	/* comms clk */
	clk_add_alias("comms_clk", NULL, "CLKS_IC_REG", NULL);
	/* module clk ?!?!?! */
	clk_add_alias("module_clk", NULL, "CLKS_IC_IF_0", NULL);
	/* EMI clk */
	clk_add_alias("emi_clk", NULL, "CLKS_EMI_SS", NULL);
	/* SBC clk */
	clk_add_alias("sbc_comms_clk", NULL, "CLK_SYSIN", NULL);

	/* fdmas MPE41 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.0", "CLKM_FDMA10", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.1", "CLKM_FDMA11", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.2", "CLKM_FDMA12", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.0", "CLKM_IC_TS", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.1", "CLKM_IC_TS", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.2", "CLKM_IC_TS", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.0", "CLKM_IC_REG_LP", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.1", "CLKM_IC_REG_LP", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.2", "CLKM_IC_REG_LP", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.0", "CLKM_IC_REG", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.1", "CLKM_IC_REG", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.2", "CLKM_IC_REG", NULL);

	/* fdmas SASG1 clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.3", "CLKS_FDMA0", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.4", "CLKS_FDMA1", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.3", "CLKS_IC_REG",  NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.4", "CLKS_IC_REG",  NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.3", "CLKS_IC_REG_LP", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.4", "CLKS_IC_REG_LP", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.3", "CLKS_IC_REG", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.4", "CLKS_IC_REG", NULL);

	/* PCI clk */
	clk_add_alias("pci_clk", NULL, "CLKS_IC_IF_2", NULL);
	clk_add_alias("pcie_clk", NULL, "CLKS_IC_IF_2", NULL);

	/* USB */
	clk_add_alias("usb_48_clk", NULL, "CLKS_B_USB48", NULL);
	clk_add_alias("usb_ic_clk", NULL, "CLKS_IC_IF_2", NULL);
	clk_add_alias("usb_phy_clk", NULL, "USB2_TRIPLE_PHY", NULL);

	return 0;
}
