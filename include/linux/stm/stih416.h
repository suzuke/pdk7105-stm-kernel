/*
 * Copyright (c) 2012 STMicroelectronics Limited
 * Author: Francesco.Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_STIH416_H
#define __LINUX_STM_STIH416_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>
#include <linux/phy.h>

#define STIH416_IRQ(irq) ((irq) + 32)

#define STIH416_RESOURCE_IRQ(_irq)		\
	{					\
		.start = STIH416_IRQ(_irq),	\
		.end = STIH416_IRQ(_irq),	\
		.flags = IORESOURCE_IRQ,	\
	}

#define STIH416_RESOURCE_IRQ_NAMED(_name, _irq)	\
	{					\
		.start = STIH416_IRQ(_irq),	\
		.end = STIH416_IRQ(_irq),	\
		.name = (_name),		\
		.flags = IORESOURCE_IRQ,	\
	}

/*
 * There is an hole in the sysconf numbering between:
 * 2999 and 5000.
 * It has to be hidded with the SYSCONFG_GROUP() macro
 * because the sysconf layer needs sysconf_numbering contigue
 *
 */
#define SYSCONFG_GROUP(x)	(((x) / 1000) < 3 ? ((x) / 1000) : (((x) / 1000)) - 2)
#define SYSCONF_OFFSET(x)	((x) % 1000)

#define SYSCONF(x)		SYSCONFG_GROUP(x), SYSCONF_OFFSET(x)

#define LPM_SYSCONF_BANK	(8)
#define LPM_CONFIG(x)		LPM_SYSCONF_BANK, (x)

#define STIH416_ASC(num)	(num)
#define STIH416_SBC_ASC(num)	(num + 5)

/*
 * GPIO bank (re-)mapping on STiH416:
 *
 *		 Hw_GPIO_Port_Nr	Linux GPIO Subsystem
 * SBC_PIO:
 *		   [0 : 4]		  [ 0  :  4 ]
 *		    [40]		      [5]
 * PIO_FRONT:
 *		   [5 : 12]		  [  6 : 13 ]
 *		   [30 : 31]		  [ 14 : 15 ]
 * PIO_REAR:
 *		   [13 : 18]		  [ 16 : 21 ]
 * MPE_PIO_10:
 *		   [100 : 102]		  [ 22 : 24 ]
 * MPE_PIO_11:
 *		   [103 : 107]		  [ 25 : 29 ]
 */
#define STIH416_GPIO(x)		(((x) <   5) ?  (x) :		\
				((x) <  13) ? (x) + 1 :		\
				((x) <  19) ? (x) + 3 :		\
				((x) <  32) ? (x) - 30 + 14 :	\
				((x) <  41) ? (x) - 40 + 5 :	\
				((x) < 103) ? (x) - 100 + 22 : (x) - 103 + 25)
/*
 * On STIH416 instead of the standard stm_gpio macro
 * it has to be used the following stih416_gpio macro
 */
#define stih416_gpio(port, pin)	stm_gpio(STIH416_GPIO(port), pin)

#define STIH416_PAD_PIO_IN(_port, _pin, _function)			\
	STM_PAD_PIO_IN(STIH416_GPIO(_port), _pin, _function)

#define STIH416_PAD_PIO_IN_NAMED(_port, _pin, _function, _name)		\
	STM_PAD_PIO_IN_NAMED(STIH416_GPIO(_port), _pin, _function, _name)

#define STIH416_PAD_PIO_OUT(_port, _pin, _function)			\
	STM_PAD_PIO_OUT(STIH416_GPIO(_port), _pin, _function)

#define STIH416_PAD_PIO_OUT_NAMED(_port, _pin, _function, _name)	\
	STM_PAD_PIO_OUT_NAMED(STIH416_GPIO(_port), _pin, _function, _name)

#define STIH416_PAD_PIO_BIDIR(_port, _pin, _function)		\
	STM_PAD_PIO_BIDIR(STIH416_GPIO(_port), _pin, _function)

#define STIH416_PAD_PIO_BIDIR_NAMED(_port, _pin, _function, _name)	\
	STM_PAD_PIO_BIDIR_NAMED(STIH416_GPIO(_port), _pin, _function, _name)


void stih416_early_device_init(void);

/*void stih416_configure_nand(struct stm_nand_config *config);*/

void stih416_configure_spifsm(struct stm_plat_spifsm_data *data);

struct stih416_asc_config {
	int hw_flow_control;
	int is_console;
	int force_m1;
};
void stih416_configure_asc(int asc, struct stih416_asc_config *config);

void stih416_reset(char mode, const char *cmd);

struct stih416_ssc_config {
	union {
		struct {
			enum {
				stih416_ssc0_sclk_pio9_2,
				stih416_ssc0_sclk_pio12_5
			} sclk;
			enum {
				stih416_ssc0_mtsr_pio9_3,
				stih416_ssc0_mtsr_pio12_6
			} mtsr;
			enum {
				stih416_ssc0_mrst_pio9_6,
				stih416_ssc0_mrst_pio12_7
			} mrst;
		} ssc0;
		struct {
			enum {
				stih416_ssc2_sclk_pio7_6,
				stih416_ssc2_sclk_pio8_6
			} sclk;
			enum {
				stih416_ssc2_mtsr_pio7_7,
				stih416_ssc2_mtsr_pio8_7
			} mtsr;
			enum {
				/* no spi mode on gpio 7 */
				stih416_ssc2_mrst_pio8_5
			} mrst;
		} ssc2;
		struct {
			enum {
				stih416_ssc4_sclk_pio10_5,
				stih416_ssc4_sclk_pio31_4
			} sclk;
			enum {
				stih416_ssc4_mtsr_pio10_6,
				stih416_ssc4_mtsr_pio31_5
			} mtsr;
			enum {
				stih416_ssc4_mrst_pio10_7,
				stih416_ssc4_mrst_pio31_6
			} mrst;
		} ssc4;
		struct {
			enum {
				stih416_ssc5_sclk_pio14_1,
				stih416_ssc5_sclk_pio14_4
			} sclk;
			enum {
				stih416_ssc5_mtsr_pio14_2,
				stih416_ssc5_mtsr_pio14_5
			} mtsr;
			enum {
				stih416_ssc5_mrst_pio14_3,
				stih416_ssc5_mrst_pio14_6
			} mrst;
		} ssc5;

	} routing;
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
	unsigned int i2c_speed;
};
int stih416_configure_ssc_spi(int ssc, struct stih416_ssc_config *config);

int stih416_configure_ssc_i2c(int ssc, struct stih416_ssc_config *config);

#define STIH416_SBC_SSC(num)		(num + 8)
#define STIH416_SSC(num)		(num)

struct stih416_lirc_config {
	enum {
		stih416_lirc_rx_disabled,
		stih416_lirc_rx_mode_ir,
		stih416_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stih416_configure_lirc(struct stih416_lirc_config *config);

struct stih416_pwm_config {
	enum {
		stih416_sas_pwm = 0,
		stih416_sbc_pwm,
	} pwm;
	int out0_enabled;
	int out1_enabled;
	int out2_enabled;
	int out3_enabled;
};
void stih416_configure_pwm(struct stih416_pwm_config *config);

void stih416_configure_keyscan(const struct stm_keyscan_config *config);

void stih416_configure_usb(int port);

void stih416_configure_spifsm(struct stm_plat_spifsm_data *data);

void stih416_configure_nand(struct stm_nand_config *config);

struct stih416_ethernet_config {
	phy_interface_t interface;
	int ext_clk;
	int phy_bus;
	char *phy_bus_name;
	int phy_addr;
	void (*txclk_select)(int txclk_250_not_25_mhz);
	struct stmmac_mdio_bus_data *mdio_bus_data;
};
void stih416_configure_ethernet(int port,
		struct stih416_ethernet_config *config);

void stih416_configure_mmc(int port, int is_emmc);

struct stih416_audio_config {
	enum {
		stih416_uni_player_1_pcm_disabled,
		stih416_uni_player_1_pcm_2_channels,
		stih416_uni_player_1_pcm_4_channels,
		stih416_uni_player_1_pcm_6_channels,
		stih416_uni_player_1_pcm_8_channels,
	} uni_player_1_pcm_mode;

	int uni_player_3_spdif_enabled;

	int uni_reader_0_spdif_enabled;
};
void stih416_configure_audio(struct stih416_audio_config *config);

struct stih416_miphy_config {
	int id;
	enum miphy_mode mode;
	enum miphy_if_type iface;
	/* 1 if TXN/TXP has inverted polarity */
	int tx_pol_inv;
	/* 1 if RXN/RXP has inverted polarity */
	int rx_pol_inv;
};

void stih416_configure_miphy(struct stih416_miphy_config *cfg);

void stih416_configure_sata(int sata_port);
#endif
