/*
 * This is the driver to manage the FlashSS MMC sub-system and it is used for
 * configuring the Arasan eMMC host controller Core. This is mandatory
 * before installing the SDHCI driver otherwise the controller will be
 * not fully setup as, for example MMC4.5, remaining to the default setup
 * usually provided to only fix boot requirements on ST platforms.
 *
 * (c) 2013 STMicroelectronics Limited
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 * Author: Youssef Triki <youssef.triki@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>
#include <linux/stm/flashss.h>
#include <linux/clk.h>
#include <linux/of.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include "mmc_core.h"

#undef MMC_CORE_DEBUG
/* #define MMC_CORE_DEBUG */

/**
 * flashss_start_mmc_tuning: program DLL
 * @ioaddr: base address
 * Description: this function is to start the MMC auto-tuning.
 * It calls the flashss_emmc_set_dll to program the DLL and the checks if
 * the DLL procedure is finished.
 */
int flashss_start_mmc_tuning(void __iomem *ioaddr)
{
	unsigned long curr, value;
	unsigned long finish = jiffies + HZ;

	/* Enable synamic tuning */
	flashss_emmc_set_dll();

	/* Check the status */
	do {
		curr = jiffies;
		value = readl(ioaddr + STATUS_R);
		if (value & 0x1)
			return 0;

		cpu_relax();
	} while (!time_after_eq(curr, finish));

	return -EBUSY;
}
EXPORT_SYMBOL_GPL(flashss_start_mmc_tuning);

static void flashss_mmc_dump_register(void __iomem *ioaddr)
{
#ifdef MMC_CORE_DEBUG
	pr_debug("MMC0 Core ...\n");
	pr_debug("cfg1 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_1));
	pr_debug("cfg2 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_2));
	pr_debug("cfg3 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_3));
	pr_debug("cfg4 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_4));
	pr_debug("cfg5 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_5));
	pr_debug("cfg6 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_6));
	pr_debug("cfg7 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_7));
	pr_debug("cfg8 0x%x\n", readl(ioaddr + FLASHSS_MMC_CORE_CONFIG_8));
#endif
}

/**
 * flashss_mmc_core_config: configure the Arasan HC
 * @ioaddr: base address
 * Description: this function is to configure the arasan MMC HC.
 * This should be called when the system starts in case of, on the SoC,
 * it is needed to configure the host controller.
 * This happens on some SoCs, i.e. StiH407, where the MMC0 inside the flashSS
 * needs to be configured as MMC 4.5 to have full capabilities.
 * W/o these settings the SDHCI could configure and use the embedded controller
 * with limited features.
 */
void flashss_mmc_core_config(void __iomem *ioaddr)
{
	flashss_mmc_dump_register(ioaddr);

	pr_debug("MMC_CORE Core Configuration\n");

	writel(STM_FLASHSS_MMC_CORE_CONFIG_1,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_1);
	writel(STM_FLASHSS_MMC_CORE_CONFIG_2,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_2);
	writel(STM_FLASHSS_MMC_CORE_CONFIG_3,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_3);
	writel(STM_FLASHSS_MMC_CORE_CONFIG_4,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_4);
	writel(STM_FLASHSS_MMC_CORE_CONFIG_5,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_5);
	writel(FLASHSS_MMC_CORE_CONFIG_6,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_6);
	writel(FLASHSS_MMC_CORE_CONFIG_7,
	       ioaddr + FLASHSS_MMC_CORE_CONFIG_7);

	flashss_mmc_dump_register(ioaddr);
}
EXPORT_SYMBOL_GPL(flashss_mmc_core_config);
