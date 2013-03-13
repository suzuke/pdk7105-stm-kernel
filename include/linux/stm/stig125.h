/*
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: Nunzio Raciti <nunzio.raciti@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_STIG125_H
#define __LINUX_STM_STIG125_H

#include <linux/device.h>
#include <linux/stm/platform.h>

#define STIG125_IRQ(irq) ((irq)+32)

#define STIG125_IRQMUX_NUM_INPUT	173
#define STIG125_IRQMUX_NUM_OUTPUT	51
#define STIG125_IRQMUX_GIC_START	173

#define STIG125_IRQMUX_MAPPING(irq)	((irq) % STIG125_IRQMUX_NUM_OUTPUT)
#define STIG125_IRQMUX(irq)	STIG125_IRQ(STIG125_IRQMUX_MAPPING(irq) + \
					    STIG125_IRQMUX_GIC_START)

#define STIG125_PIO_IRQMUX_NUM_INPUT	27
#define STIG125_PIO_IRQMUX_NUM_OUTPUT	10

#define STIG125_RESOURCE_IRQ(_irq) \
	{ \
		.start = STIG125_IRQ(_irq), \
		.end = STIG125_IRQ(_irq),	\
		.flags = IORESOURCE_IRQ, \
	}

#define STIG125_RESOURCE_IRQ_NAMED(_name, _irq)	\
	{ \
		.start = STIG125_IRQ(_irq), \
		.end = STIG125_IRQ(_irq), \
		.name = (_name), \
		.flags = IORESOURCE_IRQ, \
	}

#define SYSCONFG_GROUP(x) \
	(((x) < 600) ? ((x)/200) : (((x)/100)-4))
#define SYSCONF_OFFSET(x) \
	((x) % 100)

#define SYSCONF(x) \
	SYSCONFG_GROUP(x), SYSCONF_OFFSET(x)

#define SYSCFG_SBC_BANK	(6)
#define SBC_SYSCONF(x) SYSCFG_SBC_BANK, (x)

#define LPM_SYSCONF_BANK (7)
#define LPM_CONFIG(x)	LPM_SYSCONF_BANK, (x)

#define STIG125_SBC_PIO(x) ((x)+23)

void stig125_early_device_init(void);

#define STIG125_SBC_ASC(num)		(num + 7)
#define STIG125_ASC(num)		(num)
#define STIG125_TELSS_ASC		(9)

struct stig125_asc_config {
	int hw_flow_control;
	int is_console;
	int force_m1;
};
void stig125_configure_asc(int asc, struct stig125_asc_config *config);

#define STIG125_SSC(num)		(num)
#define STIG125_SBC_SSC(num)		(num + 10)
#define STIG125_TELSS_SSC		(13)
#define STIG125_HDMI_SSC		(7)
#define STIG125_FE_SSC			(1)
#define STIG125_BE_SSC			(8)

struct stig125_ssc_config {
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
};

int stig125_configure_ssc_i2c(int ssc, unsigned i2c_bus_speed);
int stig125_configure_ssc_spi(int ssc, struct stig125_ssc_config *config);

void stig125_configure_keyscan(const struct stm_keyscan_config *config);
void stig125_configure_spifsm(struct stm_plat_spifsm_data *data);

struct stig125_lirc_config {
	enum {
		stig125_lirc_rx_disabled,
		stig125_lirc_rx_mode_ir,
		stig125_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};

void stig125_configure_lirc(struct stig125_lirc_config *config);
void stig125_configure_fp(void);
void stig125_configure_usb(int port);

struct stig125_pwm_config {
	int out0_enabled;
	int out1_enabled;
};

void stig125_configure_pwm(struct stig125_pwm_config *config);

void __init stig125_configure_mmc(int emmc);

struct stig125_miphy_config {
	int id;
	int mode;
	int iface;
	/* 1 if TXN/TXP has inverted polarity */
	int tx_pol_inv;
	/* 1 if RXN/RXP has inverted polarity */
	int rx_pol_inv;
};

void stig125_configure_miphy(struct stig125_miphy_config *cfg);

void sti125_configure_sata(unsigned int sata_port);

void stig125_configure_pcie(int controller);

void stig125_reset(char mode, const char *cmd);

struct stig125_audio_config {
	enum {
		stig125_uni_player_1_pcm_disabled,
		stig125_uni_player_1_pcm_2_channels,
		stig125_uni_player_1_pcm_4_channels,
		stig125_uni_player_1_pcm_6_channels,
		stig125_uni_player_1_pcm_8_channels,
	} uni_player_1_pcm_mode;

	int uni_player_4_spdif_enabled;

	enum {
		stig125_uni_reader_0_pcm_disabled,
		stig125_uni_reader_0_pcm_2_channels,
		stig125_uni_reader_0_pcm_4_channels,
		stig125_uni_reader_0_pcm_6_channels,
		stig125_uni_reader_0_pcm_8_channels,
	} uni_reader_0_pcm_mode;
};
void stig125_configure_audio(struct stig125_audio_config *config);

#endif
