/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Nunzio Raciti <nunzio.raciti@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_FLASHSS_H
#define __LINUX_FLASHSS_H

#include <linux/stm/nandi.h>

enum vsense_devices {
	VSENSE_DEV_EMMC,
	VSENSE_DEV_NAND,
	VSENSE_DEV_SPI
};

enum vsense_voltages {
	VSENSE_1V8,
	VSENSE_3V3
};

enum vsense_voltages flashss_get_vsense(enum vsense_devices fdev);
int flashss_set_vsense(enum vsense_devices fdev, enum vsense_voltages v);
void flashss_emmc_set_dll(void);
void flashss_show_top_registers(void);
void flashss_nandi_select(enum nandi_controllers controller);

/* Specific MMC core callbacks */
int flashss_start_mmc_tuning(void __iomem *mmc_ioaddr);
void flashss_mmc_core_config(void __iomem *mmc_ioaddr);
#endif
