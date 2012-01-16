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
	clk_add_alias("comms_clk", NULL, "CLKA_IC_100", NULL);
	/* module clk ?!?!?! */
	clk_add_alias("module_clk", NULL, "CLKA_IC_100", NULL);
	/* EMI clk */
	clk_add_alias("emi_clk", NULL, "CLKA_EMI", NULL);
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

	/* fdmas TAE clocks */
	clk_add_alias("fdma_slim_clk", "stm-fdma.3", "CLKA_FDMA", NULL);
	clk_add_alias("fdma_slim_clk", "stm-fdma.4", "CLKA_FDMA", NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.3", "CLKA_IC_200",  NULL);
	clk_add_alias("fdma_hi_clk", "stm-fdma.4", "CLKA_IC_200",  NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.3", "CLKA_IC_100", NULL);
	clk_add_alias("fdma_low_clk", "stm-fdma.4", "CLKA_IC_100", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.3", "CLKA_IC_200", NULL);
	clk_add_alias("fdma_ic_clk", "stm-fdma.4", "CLKA_IC_200", NULL);

	return 0;
}

