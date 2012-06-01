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
#include <linux/stm/amba_bridge.h>

#define MMC_AHB2STBUS_BASE	 (0x104 - 4)

struct stm_mmc_platform_data {
	/* Initialize board-specific MMC resources (e.g. PAD) */
	int (*init)(struct platform_device *pdev);
	void (*exit)(struct platform_device *pdev);
	void *custom_cfg;
	void *custom_data;

	/* AMBA bridge structure */
	struct stm_amba_bridge_config *amba_config;
	struct stm_amba_bridge *amba_bridge;

	/* Non removable e.g. eMMC */
	unsigned nonremovable;
};

#endif /* __STM_MMC_H */
