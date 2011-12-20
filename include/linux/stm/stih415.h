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

/*
 * ARM and ST40 interrupts are virtually identical, so we can use the same
 * parameter for both. Only mailbox and some A/V interrupts are connected
 * to the ST200's, however 4 ILC outputs are available, which could be
 * used if required.
 */
#if defined(CONFIG_SUPERH)
#define STIH415_IRQ(irq) ILC_IRQ(irq)
#elif defined(CONFIG_ARM)
#define STIH415_IRQ(irq) ((irq)+32)
#endif

#define STIH415_RESOURCE_IRQ(_irq) \
	{ \
		.start = STIH415_IRQ(_irq), \
		.end = STIH415_IRQ(_irq),	\
		.flags = IORESOURCE_IRQ, \
	}

#define STIH415_RESOURCE_IRQ_NAMED(_name, _irq)	\
	{ \
		.start = STIH415_IRQ(_irq), \
		.end = STIH415_IRQ(_irq), \
		.name = (_name), \
		.flags = IORESOURCE_IRQ, \
	}

#ifndef CONFIG_ARM
#define IO_ADDRESS(x) 0
#endif


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

void stih415_configure_nand(struct stm_nand_config *config);

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
		stih415_ethernet_mode_rgmii,
		stih415_ethernet_mode_rmii,
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

struct stih415_ssc_config {
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
};

#define STIH415_SBC_SSC(num)		(num + 7)
#define STIH415_SSC(num)		(num)

/* Use the above macros while passing SSC number. */
int stih415_configure_ssc_spi(int ssc, struct stih415_ssc_config *config);
int stih415_configure_ssc_i2c(int ssc);


struct stih415_lirc_config {
	enum {
		stih415_lirc_rx_disabled,
		stih415_lirc_rx_mode_ir,
		stih415_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stih415_configure_lirc(struct stih415_lirc_config *config);

struct stih415_pwm_config {
	enum {
		stih415_sas_pwm = 0,
		stih415_sbc_pwm
	} pwm;
	int out0_enabled;
	int out1_enabled;
};
void stih415_configure_pwm(struct stih415_pwm_config *config);

void stih415_configure_mmc(int emmc);

void stih415_configure_mali(struct stm_mali_config *config);

struct stih415_audio_config {
	enum {
		stih415_pcm_player_1_output_disabled,
		stih415_pcm_player_1_output_2_channels,
		stih415_pcm_player_1_output_4_channels,
		stih415_pcm_player_1_output_6_channels,
		stih415_pcm_player_1_output_8_channels,
	} pcm_player_1_output;

	int spdif_player_output_enabled;

	int pcm_reader_input_enabled;
};
void stih415_configure_audio(struct stih415_audio_config *config);

struct stih415_miphy_config {
        int force_jtag;         /* Option available for CUT2.0 */
        enum miphy_mode *modes;
};
void stih415_configure_miphy(struct stih415_miphy_config *config);

struct stih415_sata_config {
};
void stih415_configure_sata(int port, struct stih415_sata_config *config);

#endif
