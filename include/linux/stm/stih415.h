/*
 * Copyright (c) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_STIH415_H
#define __LINUX_STM_STIH415_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>

#define SYSCONFG_GROUP(x) \
	(((x) < 100) ? 0 : (((x) < 300) ? 1 : (((x)/100)-1)))
#define SYSCONF_OFFSET(x) \
	(((x) >= 100 && (x) < 300) ? ((x) - 100) : ((x) % 100))

#define SYSCONF(x) \
	SYSCONFG_GROUP(x), SYSCONF_OFFSET(x)

#define STIH415_PIO(x) \
	(((x) < 100) ? (x) : ((x)-100+19))

#define LPM_SYSCONF_BANK	(6)

struct stih415_pio_config {
        struct stm_pio_control_mode_config *mode;
        struct stm_pio_control_retime_config *retime;
};


void stih415_early_device_init(void);

struct stih415_asc_config {
	int hw_flow_control;
	int is_console;
	int force_m1;
};
void stih415_configure_asc(int asc, struct stih415_asc_config *config);

void stih415_configure_usb(int port);

struct stih415_ethernet_config {
	enum {
		stih415_ethernet_mode_mii,
		stih415_ethernet_mode_gmii,
		stih415_ethernet_mode_gmii_gtx,
		stih415_ethernet_mode_rmii,
		stih415_ethernet_mode_rgmii_gtx,
		stih415_ethernet_mode_reverse_mii
	} mode;
	int ext_clk;
	int phy_bus;
	int phy_addr;
	void (*txclk_select)(int txclk_250_not_25_mhz);
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stih415_configure_ethernet(int port,
		struct stih415_ethernet_config *config);

void stih415_configure_mali(struct stm_mali_config *config);

#endif
