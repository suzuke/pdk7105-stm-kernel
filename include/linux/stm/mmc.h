/*
 * MMC definitions for STMicroelectronics
 *
 * Copyright (C) 2012 STMicroelectronics
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STM_MMC_H
#define __STM_MMC_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mmc/sdhci.h>

struct stm_mmc_platform_data {
	/* Initialize board-specific MMC resources */
	int (*init)(struct sdhci_host *host);
	void (*exit)(struct sdhci_host *host);

	/* nonremovable e.g. eMMC */
	unsigned nonremovable;
};

#endif /* __STM_MMC_H */
