/*
 * Copyright (c) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_NICE_H
#define __LINUX_STM_NICE_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>

void nice_early_device_init(void);

struct nice_asc_config {
	union {
		enum { nice_asc0_pio0 } asc0;
	} routing;
	int hw_flow_control;
	int is_console;
	int force_m1;
};
void nice_configure_asc(int asc, struct nice_asc_config *config);

#endif
