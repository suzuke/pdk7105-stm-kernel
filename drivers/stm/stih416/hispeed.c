/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/stm/device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih416.h>
#include <linux/stm/amba_bridge.h>
#include <linux/delay.h>
#include <linux/phy.h>


#ifndef CONFIG_OF
/* All the Drivers are now configured using device trees so,
 * Please start using device trees */
#warning  "This code will disappear soon, you should use device trees"
#include "../pio-control.h"



/* USB resources ---------------------------------------------------------- */
static u64 stih416_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_HOST_PWR		"USB_HOST_PWR"
#define USB_PWR_ACK		"USB_PWR_ACK"
#define USB_IN_DC_SHIFT		"USB_IN_DC_SHIFT"
#define USB_IN_EDGE_CTRL	"USB_IN_EDGE_CTRL"

static int stih416_usb_init(struct stm_device_state *device_state)
{
	static struct sysconf_field *sc_osc_is_stable;
	if (!sc_osc_is_stable) {
		/*
		 * SYSCONF(2520)[6]: bit specifies whether the crystal
		 * oscillator clock is stable or not.
		 * To be set to '1' after boot of Orly-2
		 */
		sc_osc_is_stable = sysconf_claim(SYSCONF(2520), 6, 6, "USB");
		if (!sc_osc_is_stable)
			return -EBUSY;
	}
	sysconf_write(sc_osc_is_stable, 1);

	stm_device_sysconf_write(device_state, USB_IN_DC_SHIFT, 0);
	stm_device_sysconf_write(device_state, USB_IN_EDGE_CTRL, 1);

	return 0;
}

static void stih416_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_PWR_ACK)
			== value)
			break;
		mdelay(10);
	}
}

/* STBus Convertor config */
static struct stm_amba_bridge_config stih416_amba_usb_config = {
	STM_DEFAULT_USB_AMBA_PLUG_CONFIG(16),
	.type2.sd_config_missing = 1,
	.packets_in_chunk = 2,
	.max_opcode = stm_amba_opc_LD64_ST64,
};

static struct stm_plat_usb_data stih416_usb_platform_data[4] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih416_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih416_usb_init,
			.power = stih416_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 0, 0,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 0, 0,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 0, 0,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 3, 3,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STIH416_PAD_PIO_IN(9, 4, 1),
					/* USB power enable */
					STIH416_PAD_PIO_OUT(9, 5, 1),
				},
			},
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih416_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih416_usb_init,
			.power = stih416_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 1, 1,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 1, 1,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 1, 1,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 4, 4,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STIH416_PAD_PIO_IN(18, 0, 1),
					/* USB power enable */
					STIH416_PAD_PIO_OUT(18, 1, 1),
				},
			},
		},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih416_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih416_usb_init,
			.power = stih416_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 2, 2,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 2, 2,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 2, 2,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 5, 5,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STIH416_PAD_PIO_IN(18, 2, 1),
					/* USB power enable */
					STIH416_PAD_PIO_OUT(18, 3, 1),
				},
			},
		},
	},
	[3] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih416_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih416_usb_init,
			.power = stih416_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 6, 6,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 5, 5,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 8, 8,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(2520), 9, 9,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STIH416_PAD_PIO_IN(40, 0, 1),
					/* USB power enable */
					STIH416_PAD_PIO_OUT(40, 1, 1),
				},
			},
		},
	},
};

#define USB_DEVICE(_id, _base, _ehci_irq, _ohci_irq)			\
	[_id] = {							\
		.name = "stm-usb",					\
		.id = _id,						\
		.dev = {						\
			.dma_mask = &stih416_usb_dma_mask,		\
			.coherent_dma_mask = DMA_BIT_MASK(32),		\
			.platform_data = &stih416_usb_platform_data[_id],\
		},							\
		.num_resources = 6,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",		\
					_base, 0x100),			\
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",		\
					_base + 0x3c00, 0x100),		\
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",		\
					_base + 0x3e00, 0x100),		\
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",		\
					_base + 0x3f00, 0x100),		\
			STIH416_RESOURCE_IRQ_NAMED("ehci", _ehci_irq),	\
			STIH416_RESOURCE_IRQ_NAMED("ohci", _ohci_irq),	\
		},							\
	}

static struct platform_device stih416_usb_devices[] = {
	USB_DEVICE(0, 0xfe100000, 148, 149),
	USB_DEVICE(1, 0xfe200000, 150, 151),
	USB_DEVICE(2, 0xfe300000, 152, 153),
	USB_DEVICE(3, 0xfe340000, 154, 155),
};

void __init stih416_configure_usb(int port)
{
	static int configured[ARRAY_SIZE(stih416_usb_devices)];
	BUG_ON(port < 0 || port >= ARRAY_SIZE(stih416_usb_devices));

	BUG_ON(configured[port]++);
	platform_device_register(&stih416_usb_devices[port]);
}

/* --------------------------------------------------------------------
 *	 Ethernet MAC resources (PAD and Retiming)
 * --------------------------------------------------------------------*/

#define DATA_IN(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define DATA_OUT(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define CLOCK_IN(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define CLOCK_OUT(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define PHY_CLOCK(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_unknown, \
		.function = _func, \
		.name = "PHYCLK", \
		.priv = &(struct stm_pio_control_pad_config) { \
		.retime = _retiming, \
		}, \
	}

#define MDIO(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_bidir_pull_up, \
		.function = _func, \
		.name = "MDIO", \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}
#define MDC(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(STIH416_GPIO(_port), _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _func, \
		.name = "MDC", \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

struct stm_gmac_clks_n {
	char *clk_n;
	char *parent_clk_100MHz_n;
	char *parent_clk_10MHz_n;
};

const struct stm_gmac_clks_n gmac_clk_n[] = {
	{
		"CLK_S_GMAC0_PHY",
		"CLK_S_A1_PLL1",
		"CLK_S_A1_REF"
	}, {
		"CLK_S_ETH1_PHY",
		"CLK_S_A0_PLL1",
		"CLK_S_A0_REF"
	}
};

struct stm_gmac_clks {
	struct clk *clk;
	struct clk *parent_100MHz;
	struct clk *parent_10MHz;
};

static struct stm_gmac_clks gmac_clks[2];
static struct sysconf_field *gmac_gbit_sc[2];

static int
stih416_gmac_gmii_claim(struct stm_pad_state *state, void *priv)
{
	int port = (int) priv;
	const struct gmac_sysconf {
		long grp;
		long nr;
	} sc[2] = {
		{	SYSCONF(2559),	},
		{	SYSCONF(508)	}
	};

	if (gmac_gbit_sc[port])
		BUG();

	/*
	 * GMII needs only bit [6]
	 * but
	 * RGMII needs bit [8:6]
	 */
	gmac_gbit_sc[port] = sysconf_claim(sc[port].grp,
			sc[port].nr, 6, 8, "gmac");

	if (!gmac_gbit_sc[port])
		return -1;
	sysconf_write(gmac_gbit_sc[port], 0);
	return 0;
}

static void
stih416_gmac_gmii_release(struct stm_pad_state *state, void *priv)
{
	int port = (int) priv;

	if (!gmac_gbit_sc[port])
		BUG();
	sysconf_release(gmac_gbit_sc[port]);
	gmac_gbit_sc[port] = NULL;
}

static struct stm_pad_config stih416_ethernet_mii_pad_configs[] = {
	[0] = {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(13, 5, 2, RET_NICLK(0, 1)),/* PHYCLK */
			DATA_IN(13, 6, 2, RET_BYPASS(0)),/* MDINT */
			DATA_OUT(13, 7, 2, RET_SE_NICLK_IO(0, 0)),/* TXEN */

			DATA_OUT(14, 0, 2, RET_SE_NICLK_IO(0, 0)),/* TXD[0] */
			DATA_OUT(14, 1, 2, RET_SE_NICLK_IO(0, 0)),/* TXD[1] */
			DATA_OUT(14, 2, 2, RET_SE_NICLK_IO(0, 1)),/* TXD[2] */
			DATA_OUT(14, 3, 2, RET_SE_NICLK_IO(0, 1)),/* TXD[3] */

			CLOCK_IN(15, 0, 2,
				(&(struct stm_pio_control_retime_config){
					.clk = 0,
					.clknotdata = 1,
			      })),/* TXCLK */
			DATA_OUT(15, 1, 2, RET_SE_NICLK_IO(0, 0)),/* TXER */
			DATA_IN(15, 2, 2, RET_BYPASS(1000)), /* CRS */
			DATA_IN(15, 3, 2, RET_BYPASS(1000)), /* COL */

			MDIO(15, 4, 2, RET_BYPASS(1500)), /* MDIO*/
			MDC(15, 5, 2,
				(&(struct stm_pio_control_retime_config){
					.clk = 1,
					.clknotdata = 1,})),/* MDC */
			DATA_IN(16, 0, 2, RET_SE_NICLK_IO(0, 0)),/* 5 RXD[0] */
			DATA_IN(16, 1, 2, RET_SE_NICLK_IO(0, 0)),/* RXD[1] */
			DATA_IN(16, 2, 2, RET_SE_NICLK_IO(0, 0)),/* RXD[2] */
			DATA_IN(16, 3, 2, RET_SE_NICLK_IO(0, 0)),/* RXD[3] */
			DATA_IN(15, 6, 2, RET_SE_NICLK_IO(0, 0)),/* RXDV */
			DATA_IN(15, 7, 2, RET_SE_NICLK_IO(0, 0)),/* RX_ER */

			CLOCK_IN(17, 0, 2, RET_NICLK(0, 0)),
		},
		.sysconfs_num = 6,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYSCONF(SYSCONF(1539), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(2559), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(2559), 2, 4, 0),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(2559), 5, 5, 1),
			/* TXCLK_NOT_CLK125 */
			STM_PAD_SYSCONF(SYSCONF(2559), 6, 6, 1),
			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(2559), 8, 8, 0),
		},
	},
	[1] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {

			DATA_OUT(0, 0, 1, RET_SE_NICLK_IO(0, 0)), /* TXD[0]*/
			DATA_OUT(0, 1, 1, RET_SE_NICLK_IO(0, 0)), /* TXD[1]*/
			DATA_OUT(0, 2, 1, RET_SE_NICLK_IO(0, 0)), /* TXD[2]*/
			DATA_OUT(0, 3, 1, RET_SE_NICLK_IO(0, 0)), /* TXD[3]*/
			DATA_OUT(0, 5, 1, RET_SE_NICLK_IO(0, 0)), /*TXEN */

			CLOCK_IN(0, 6, 1,
			    (&(struct stm_pio_control_retime_config){
					.clk = 0,
					.clknotdata = 1,
			      })),/* TXCLK */

			DATA_OUT(0, 4, 1, RET_SE_NICLK_IO(0, 0)), /*TXER*/
			DATA_IN(1, 2, 1, RET_BYPASS(1000)), /* CRS */
			DATA_IN(0, 7, 1, RET_BYPASS(1000)), /* COL */

			MDIO(1, 0, 1, RET_BYPASS(1500)), /* MDIO*/
			MDC(1, 1, 1,
				(&(struct stm_pio_control_retime_config){
					.clk = 0,
					.clknotdata = 1,})),/* MDC */

			DATA_IN(1, 3, 1, RET_BYPASS(0)),/* MDINT */
			DATA_IN(1, 4, 1, RET_SE_NICLK_IO(0, 0)), /*RXD0*/
			DATA_IN(1, 5, 1, RET_SE_NICLK_IO(0, 0)),
			DATA_IN(1, 6, 1, RET_SE_NICLK_IO(0, 0)),
			DATA_IN(1, 7, 1, RET_SE_NICLK_IO(0, 0)),
			DATA_IN(2, 0, 1, RET_SE_NICLK_IO(0, 0)),
			DATA_IN(2, 1, 1, RET_SE_NICLK_IO(0, 0)),
			CLOCK_IN(2, 2, 1, RET_NICLK(0, 0)), /* TXCLK */

			PHY_CLOCK(2, 3, 1, RET_NICLK(0, 0)), /* PHYCLK */
		},
		.sysconfs_num = 6,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYSCONF(SYSCONF(510), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(508), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(508), 2, 4, 0),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(508), 5, 5, 1),
			/* TXCLK_NOT_CLK125 */
			STM_PAD_SYSCONF(SYSCONF(508), 6, 6, 1),
			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(508), 8, 8, 0),
		},
	},
};

static struct stm_pad_config stih416_ethernet_reverse_mii_pad_configs[] = {
	[0] = {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(14, 0, 2, RET_BYPASS(0)),/* TXD[0] */
			DATA_OUT(14, 1, 2, RET_BYPASS(0)),/* TXD[1] */
			DATA_OUT(14, 2, 2, RET_BYPASS(0)),/* TXD[2] */
			DATA_OUT(14, 3, 2, RET_BYPASS(0)),/* TXD[3] */
			DATA_OUT(15, 1, 2, RET_SE_NICLK_IO(0, 0)),/* TXER */
			DATA_OUT(13, 7, 2, RET_SE_NICLK_IO(0, 0)),/* TXEN */
			CLOCK_IN(15, 0, 2, RET_NICLK(0, -1)),/* TXCLK */

			DATA_OUT(15, 3, 3, RET_BYPASS(0)),/* COL */
			MDIO(15, 4, 2, RET_BYPASS(500)),/* MDIO*/
			MDC(15, 5, 3, RET_NICLK(0, 0)),/* MDC */
			DATA_OUT(15, 2, 3, RET_BYPASS(0)),/* CRS */

			DATA_IN(13, 6, 2, RET_BYPASS(0)),/* MDINT */
			DATA_IN(16, 0, 2, RET_SE_NICLK_IO(500, 0)),/* RXD[0] */
			DATA_IN(16, 1, 2, RET_SE_NICLK_IO(500, 0)),/* RXD[1] */
			DATA_IN(16, 2, 2, RET_SE_NICLK_IO(500, 0)),/* RXD[2] */
			DATA_IN(16, 3, 2, RET_SE_NICLK_IO(500, 0)),/* RXD[3] */
			DATA_IN(15, 6, 2, RET_SE_NICLK_IO(500, 0)),/* RXDV */
			DATA_IN(15, 7, 2, RET_SE_NICLK_IO(500, 0)),/* RX_ER */
			CLOCK_IN(17, 0, 2, RET_NICLK(0, -1)),/* RXCLK */
			PHY_CLOCK(13, 5, 2, RET_NICLK(0, 0)),/* PHYCLK */
		},
		.sysconfs_num = 6,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYSCONF(SYSCONF(1539), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(2559), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(2559), 2, 4, 0),
			/* ENMIIx: reverse mii */
			STM_PAD_SYSCONF(SYSCONF(2559), 5, 5, 0),
			/* TXCLK_NOT_CLK125 */
			STM_PAD_SYSCONF(SYSCONF(2559), 6, 6, 1),
			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(2559), 8, 8, 0),
		},
	},
	[1] =  {
		.gpios_num = 20,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 0, 1, RET_SE_NICLK_IO(0, 1)),/* TXD[0] */
			DATA_OUT(0, 1, 1, RET_SE_NICLK_IO(0, 1)),/* TXD[1] */
			DATA_OUT(0, 2, 1, RET_SE_NICLK_IO(0, 1)),/* TXD[2] */
			DATA_OUT(0, 3, 1, RET_SE_NICLK_IO(0, 1)),/* TXD[3] */
			DATA_OUT(0, 4, 1, RET_SE_NICLK_IO(0, 1)),/* TXER */
			DATA_OUT(0, 5, 1, RET_SE_NICLK_IO(0, 1)),/* TXEN */
			CLOCK_IN(0, 6, 1, RET_NICLK(0, -1)),/* TXCLK */
			DATA_OUT(0, 7, 2, RET_BYPASS(0)),/* COL */


			DATA_IN(2, 0, 1, RET_SE_NICLK_IO(500, 1)),/* RXDV */
			DATA_IN(2, 1, 1, RET_SE_NICLK_IO(500, 1)),/* RX_ER */
			CLOCK_IN(2, 2, 1, RET_NICLK(0, -1)),/* RXCLK */
			PHY_CLOCK(2, 3, 1, RET_NICLK(0, 0)),/* PHYCLK */

			MDIO(1, 0, 1, RET_BYPASS(500)),/* MDIO */
			MDC(1, 1, 2, RET_NICLK(0, 1)),/* MDC */
			DATA_OUT(1, 2, 2, RET_BYPASS(0)),/* CRS */
			DATA_IN(1, 3, 1, RET_BYPASS(0)),/* MDINT */
			DATA_IN(1, 4, 1, RET_SE_NICLK_IO(500, 1)),/* RXD[0] */
			DATA_IN(1, 5, 1, RET_SE_NICLK_IO(500, 1)),/* RXD[1] */
			DATA_IN(1, 6, 1, RET_SE_NICLK_IO(500, 1)),/* RXD[2] */
			DATA_IN(1, 7, 1, RET_SE_NICLK_IO(500, 1)),/* RXD[3] */

		},
		.sysconfs_num = 6,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYSCONF(SYSCONF(510), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(508), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(508), 2, 4, 0),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(508), 5, 5, 0),
			/* TXCLK_NOT_CLK125 */
			STM_PAD_SYSCONF(SYSCONF(508), 6, 6, 1),
			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(508), 8, 8, 0),
		},
	},
};

static struct stm_pad_config stih416_ethernet_rmii_pad_configs[] = {
	[0] = {
		.gpios_num = 11,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(13, 5, 2, RET_NICLK(0, 0)),/* PHYCLK */
			DATA_IN(13, 6, 2, RET_BYPASS(0)),/* MDINT */
			DATA_OUT(13, 7, 2, RET_SE_NICLK_IO(0, 0)),/* TXEN */
			DATA_OUT(14, 0, 2, RET_SE_NICLK_IO(0, 0)),/* TXD[0] */
			DATA_OUT(14, 1, 2, RET_SE_NICLK_IO(0, 0)),/* TXD[1] */
			MDIO(15, 4, 2, RET_BYPASS(0)),/* MDIO */
			MDC(15, 5, 2,
				(&(struct stm_pio_control_retime_config){
					.clk = 1,
					.clknotdata = 1,})),/* MDC */
			DATA_IN(15, 6, 2, RET_SE_NICLK_IO(0, 1)),/* RXDV */
			DATA_IN(15, 7, 2, RET_SE_NICLK_IO(1500, 1)),/* RX_ER */
			DATA_IN(16, 0, 2, RET_SE_NICLK_IO(0, 1)),/* RXD.0 */
			DATA_IN(16, 1, 2, RET_SE_NICLK_IO(0, 1)),/* RXD.1 */
		},
		.sysconfs_num = 5,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYSCONF(SYSCONF(1539), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(2559), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(2559), 2, 4, 4),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(2559), 5, 5, 1),

			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(2559), 8, 8, 1),
		},
	},
	[1] = {
		.gpios_num = 11,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 0, 1, RET_SE_NICLK_IO(0, 0)),/* TXD[0] */
			DATA_OUT(0, 1, 1, RET_SE_NICLK_IO(0, 0)),/* TXD[1] */
			DATA_OUT(0, 5, 1, RET_SE_NICLK_IO(0, 0)),/* TXEN */
			MDIO(1, 0, 1, RET_BYPASS(0)),/* MDIO */
			MDC(1, 1, 1,
				(&(struct stm_pio_control_retime_config){
					.clk = 0,
					.clknotdata = 1,})),/* MDC */
			DATA_IN(1, 3, 1, RET_BYPASS(0)),/* MDINT */
			DATA_IN(1, 4, 1, RET_SE_NICLK_IO(0, 1)),/* RXD[0] */
			DATA_IN(1, 5, 1, RET_SE_NICLK_IO(0, 1)),/* RXD[1] */
			DATA_IN(2, 0, 1, RET_SE_NICLK_IO(0, 1)),/* RXDV */
			DATA_IN(2, 1, 1, RET_SE_NICLK_IO(0, 1)),/* RX_ER */
			PHY_CLOCK(2, 3, 1, RET_NICLK(0, 0)),/* PHYCLK */
		},
		.sysconfs_num = 5,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYSCONF(SYSCONF(510), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(508), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(508), 2, 4, 4),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(508), 5, 5, 1),
			/* TX_RETIMING_CLK */
			STM_PAD_SYSCONF(SYSCONF(508), 8, 8, 1),
		},
	},
};


static struct stm_pad_config stih416_ethernet_gmii_pad_configs[] = {
	[0] =  {
		.gpios_num = 29,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(13, 5, 4, RET_NICLK(0, 1)),/* GTXCLK */
			DATA_IN(13, 6, 2, RET_BYPASS(0)),/* MDINT */
			DATA_OUT(13, 7, 2, RET_SE_NICLK_IO(1500, 0)),/* TXEN */

			DATA_OUT(14, 0, 2, RET_SE_NICLK_IO(1500, 0)),/*TXD[0]*/
			DATA_OUT(14, 1, 2, RET_SE_NICLK_IO(1500, 0)),/*TXD[1]*/
			DATA_OUT(14, 2, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[2]*/
			DATA_OUT(14, 3, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[3]*/
			DATA_OUT(14, 4, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[4]*/
			DATA_OUT(14, 5, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[5]*/
			DATA_OUT(14, 6, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[6]*/
			DATA_OUT(14, 7, 2, RET_SE_NICLK_IO(1500, 1)),/*TXD[7]*/

			CLOCK_IN(15, 0, 2, RET_NICLK(0, 0)),/* TXCLK */
			DATA_OUT(15, 1, 2, RET_SE_NICLK_IO(1500, 0)),/* TXER */
			DATA_IN(15, 2, 2, RET_BYPASS(500)),/* CRS */
			DATA_IN(15, 3, 2, RET_BYPASS(500)),/* COL */
			MDIO(15, 4, 2, RET_BYPASS(1500)),/* MDIO */
			MDC(15, 5, 2,
				(&(struct stm_pio_control_retime_config) {
					.clk = 1,
					.clknotdata = 1,})),/* MDC */

			DATA_IN(15, 6, 2, RET_SE_NICLK_IO(1500, 0)),/* RXDV */
			DATA_IN(15, 7, 2, RET_SE_NICLK_IO(1500, 0)),/* RX_ER */

			DATA_IN(16, 0, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[0] */
			DATA_IN(16, 1, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[1] */
			DATA_IN(16, 2, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[2] */
			DATA_IN(16, 3, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[3] */
			DATA_IN(16, 4, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[4] */
			DATA_IN(16, 5, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[5] */
			DATA_IN(16, 6, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[6] */
			DATA_IN(16, 7, 2, RET_SE_NICLK_IO(1500, 0)),/* RXD[7] */

			CLOCK_IN(17, 0, 2, RET_NICLK(0, 0)),/* RXCLK */
			CLOCK_IN(17, 6, 1, RET_NICLK(0, 0)),/* 125MHZ i/p clk */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYSCONF(SYSCONF(1539), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(2559), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(2559), 2, 4, 0),
			/* ENMIIx */
			STM_PAD_SYSCONF(SYSCONF(2559), 5, 5, 1),
		},
		.custom_claim   = stih416_gmac_gmii_claim,
		.custom_release = stih416_gmac_gmii_release,
		.custom_priv	= (void *)0,
	},
	[1] =  {
		.gpios_num = 29,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 0, 1, RET_SE_NICLK_IO(750, 0)),/* TXD[0] */
			DATA_OUT(0, 1, 1, RET_SE_NICLK_IO(750, 0)),/* TXD[1] */
			DATA_OUT(0, 2, 1, RET_SE_NICLK_IO(750, 0)),/* TXD[2] */
			DATA_OUT(0, 3, 1, RET_SE_NICLK_IO(750, 0)),/* TXD[3] */
			DATA_OUT(0, 4, 1, RET_SE_NICLK_IO(750, 0)),/* TXER */
			DATA_OUT(0, 5, 1, RET_SE_NICLK_IO(0, 0)),/* TXEN */
			CLOCK_IN(0, 6, 1, RET_NICLK(0, 0)),/* TXCLK */
			DATA_IN(0, 7, 1, RET_BYPASS(500)),/* COL */

			MDIO(1, 0, 1, RET_BYPASS(750)),/* MDIO */
			MDC(1, 1, 1, RET_NICLK(0, 0)),/* MDC */

			DATA_IN(1, 2, 1, RET_BYPASS(500)),/* CRS */
			DATA_IN(1, 3, 1, RET_BYPASS(0)),/* MDINT */
			DATA_IN(1, 4, 1, RET_SE_NICLK_IO(1500, 0)),/* RXD[0] */
			DATA_IN(1, 5, 1, RET_SE_NICLK_IO(1500, 0)),/* RXD[1] */
			DATA_IN(1, 6, 1, RET_SE_NICLK_IO(1500, 0)),/* RXD[2] */
			DATA_IN(1, 7, 1, RET_SE_NICLK_IO(1500, 0)),/* RXD[3] */

			DATA_IN(2, 0, 1, RET_SE_NICLK_IO(1500, 0)),/* RXDV */
			DATA_IN(2, 1, 1, RET_SE_NICLK_IO(1500, 0)),/* RX_ER */
			CLOCK_IN(2, 2, 1, RET_NICLK(0, 0)),/* RXCLK */

			PHY_CLOCK(2, 3, 4, RET_NICLK(0, 1)), /* GTXCLK */

			DATA_OUT(2, 6, 4, RET_SE_NICLK_IO(750, 0)),/* TXD[4] */
			DATA_OUT(2, 7, 4, RET_SE_NICLK_IO(750, 0)),/* TXD[5] */

			DATA_IN(3, 0, 4, RET_SE_NICLK_IO(1500, 0)),/* RXD[4] */
			DATA_IN(3, 1, 4, RET_SE_NICLK_IO(1500, 0)),/* RXD[5] */
			DATA_IN(3, 2, 4, RET_SE_NICLK_IO(1500, 0)),/* RXD[6] */
			DATA_IN(3, 3, 4, RET_SE_NICLK_IO(1500, 0)),/* RXD[7] */

			CLOCK_IN(3, 7, 4, RET_NICLK(0, 0)),/* 125MHZ IN clk */

			DATA_OUT(4, 1, 4, RET_SE_NICLK_IO(750, 0)),/* TXD[6] */
			DATA_OUT(4, 2, 4, RET_SE_NICLK_IO(750, 0)),/* TXD[7] */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYSCONF(SYSCONF(510), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(508), 0, 0, 0),
			/* MIIx_PHY_SEL:
			 * 000: GMII/MII (default)
			 * 001: RGMII
			 * 010: SGMII
			 * 100: RMII
			 */
			STM_PAD_SYSCONF(SYSCONF(508), 2, 4, 0),
			/* ENMIIx: standard mii */
			STM_PAD_SYSCONF(SYSCONF(508), 5, 5, 1),
		},
		.custom_claim   = stih416_gmac_gmii_claim,
		.custom_release = stih416_gmac_gmii_release,
		.custom_priv	= (void *)1,
	},
};

static struct stm_pad_config stih416_ethernet_rgmii_pad_configs[] = {
	[0] =  {
		.gpios_num = 16,
		.gpios = (struct stm_pad_gpio []) {
			PHY_CLOCK(13, 5, 4, RET_NICLK(0, 1)),/* GTXCLK */

			DATA_OUT(13, 7, 2, RET_DE_IO(0, 0)),/* TXEN */

			DATA_OUT(14, 0, 2, RET_DE_IO(500, 0)),/* TXD[0] */
			DATA_OUT(14, 1, 2, RET_DE_IO(500, 0)),/* TXD[1] */
			DATA_OUT(14, 2, 2, RET_DE_IO(500, 1)),/* TXD[2] */
			DATA_OUT(14, 3, 2, RET_DE_IO(500, 1)),/* TXD[3] */

			/* TX Clock inversion is not set for 1000Mbps */
			CLOCK_IN(15, 0, 2, RET_NICLK(0, 0)),/* TXCLK */

			MDIO(15, 4, 2, RET_BYPASS(0)),/* MDIO */
			MDC(15, 5, 2,
				(&(struct stm_pio_control_retime_config){
					.clk = 1,
					.clknotdata = 1,})),/* MDC */

			DATA_IN(15, 6, 2, RET_DE_IO(500, 0)),/* RXDV */

			DATA_IN(16, 0, 2, RET_DE_IO(500, 0)),/* RXD[0] */
			DATA_IN(16, 1, 2, RET_DE_IO(500, 0)),/* RXD[1] */
			DATA_IN(16, 2, 2, RET_DE_IO(500, 0)),/* RXD[2] */
			DATA_IN(16, 3, 2, RET_DE_IO(500, 0)),/* RXD[3] */

			CLOCK_IN(17, 0, 2, RET_NICLK(0, 0)),/* RXCLK */
			CLOCK_IN(17, 6, 1, RET_NICLK(0, 0)),/* 125MHZ i/p clk */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC0 */
			STM_PAD_SYSCONF(SYSCONF(1539), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(2559), 0, 0, 0),
			/* MIIx_PHY_SEL */
			STM_PAD_SYSCONF(SYSCONF(2559), 2, 4, 1),
			/* ENMIIx */
			STM_PAD_SYSCONF(SYSCONF(2559), 5, 5, 1),

		},
		.custom_claim   = stih416_gmac_gmii_claim,
		.custom_release = stih416_gmac_gmii_release,
		.custom_priv	= (void *)0,
	},
	[1] =  {
		.gpios_num = 16,
		.gpios = (struct stm_pad_gpio []) {
			DATA_OUT(0, 0, 1, RET_DE_IO(500, 0)),/* TXD[0] */
			DATA_OUT(0, 1, 1, RET_DE_IO(500, 0)),/* TXD[1] */
			DATA_OUT(0, 2, 1, RET_DE_IO(500, 0)),/* TXD[2] */
			DATA_OUT(0, 3, 1, RET_DE_IO(500, 0)),/* TXD[3] */
			DATA_OUT(0, 5, 1, RET_DE_IO(500, 0)),/* TXEN */
			CLOCK_IN(0, 6, 1, RET_NICLK(0, 0)),/* TXCLK */

			MDIO(1, 0, 1, RET_BYPASS(0)),/* MDIO */
			MDC(1, 1, 1,
				(&(struct stm_pio_control_retime_config){
					.clk = 0,
					.clknotdata = 1,})),/* MDC */
			DATA_IN(1, 4, 1, RET_DE_IO(500, 0)),/* RXD[0] */
			DATA_IN(1, 5, 1, RET_DE_IO(500, 0)),/* RXD[1] */
			DATA_IN(1, 6, 1, RET_DE_IO(500, 0)),/* RXD[2] */
			DATA_IN(1, 7, 1, RET_DE_IO(500, 0)),/* RXD[3] */

			DATA_IN(2, 0, 1, RET_DE_IO(500, 0)),/* RXDV */
			CLOCK_IN(2, 2, 1, RET_NICLK(0, 0)),/* RXCLK */
			PHY_CLOCK(2, 3, 4, RET_NICLK(0, 1)), /* GTXCLK */

			CLOCK_IN(3, 7, 4, RET_NICLK(0, 0)),/* 125MHZ input clock */
		},
		.sysconfs_num = 4,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* EN_GMAC1 */
			STM_PAD_SYSCONF(SYSCONF(510), 0, 0, 1),
			/* Pwr Down Req */
			STM_PAD_SYSCONF(SYSCONF(508), 0, 0, 0),
			/* MIIx_PHY_SEL */
			STM_PAD_SYSCONF(SYSCONF(508), 2, 4, 1),
			/* ENMIIx */
			STM_PAD_SYSCONF(SYSCONF(508), 5, 5, 1),
		},
		.custom_claim   = stih416_gmac_gmii_claim,
		.custom_release = stih416_gmac_gmii_release,
		.custom_priv	= (void *)1,
	},
};

static void stih416_ethernet_gtx_speed(void *priv, unsigned int speed)
{
	void (*txclk_select)(int txclk_250_not_25_mhz) = priv;
	if (txclk_select)
		txclk_select(speed == SPEED_1000);
}

static void stih416_ethernet_gmii_speed(int port, void *priv,
					unsigned int speed)
{
	/* TX Clock inversion is not set for 1000Mbps */
	sysconf_write(gmac_gbit_sc[port], (speed == SPEED_1000) ? 0 : 1);

	stih416_ethernet_gtx_speed(priv, speed);
}

static void stih416_ethernet_gmii0_speed(void *priv, unsigned int speed)
{
	stih416_ethernet_gmii_speed(0, priv, speed);
}
static void stih416_ethernet_gmii1_speed(void *priv, unsigned int speed)
{
	stih416_ethernet_gmii_speed(1, priv, speed);
}

static void stih416_ethernet_rgmii_speed(int port, void *priv,
					 unsigned int speed)
{
	/* TX Clock inversion is not set for 1000Mbps */
	if (speed == SPEED_1000)
		/* output clock driver by MII_TXCLK
		 * 125Mhz Clock from PHY is used for retiming
		 * and also to drive GTXCLK
		 */
		sysconf_write(gmac_gbit_sc[port], 0);
	else {
		/* output clock driver by Clockgen
		 * 125MHz clock provided by PHY is not suitable for retiming.
		 * So TXPIO retiming must therefore be clocked by an
		 * internal 2.5/25Mhz clock generated by Clockgen.
		 */
		sysconf_write(gmac_gbit_sc[port], 6);

		clk_set_parent(gmac_clks[port].clk,
			(speed  == SPEED_100) ?
				gmac_clks[port].parent_100MHz :
				gmac_clks[port].parent_10MHz);
		clk_set_rate(gmac_clks[port].clk,
			(speed  == SPEED_100) ? 25000000 : 2500000);
	}
	stih416_ethernet_gtx_speed(priv, speed);
}

static void stih416_ethernet_rgmii0_gtx_speed(void *priv, unsigned int speed)
{
	stih416_ethernet_rgmii_speed(0, priv, speed);
}

static void stih416_ethernet_rgmii1_gtx_speed(void *priv, unsigned int speed)
{
	stih416_ethernet_rgmii_speed(1, priv, speed);
}

static struct stmmac_dma_cfg gmac_dma_setting = {
	.pbl = 32,
	.mixed_burst = 1,
};

/* STBus Convertor config */
static struct stm_amba_bridge_config stih416_amba_stmmac_config = {
	.type = stm_amba_type2,
	.chunks_in_msg = 1,
	.packets_in_chunk = 2,
	.write_posting = stm_amba_write_posting_enabled,
	.max_opcode = stm_amba_opc_LD64_ST64,
	.type2.threshold = 512,
	.type2.sd_config_missing = 1,
	.type2.trigger_mode = stm_amba_stbus_threshold_based,
	.type2.read_ahead = stm_amba_read_ahead_enabled,
};

#define GMAC_AHB2STBUS_BASE		(0x2000 - 4)
static void *stih416_ethernet_bus_setup(void __iomem *ioaddr,
			struct device *dev, void *data)
{
	struct stm_amba_bridge *amba;

	if (!data) {
		amba = stm_amba_bridge_create(dev, ioaddr + GMAC_AHB2STBUS_BASE,
				&stih416_amba_stmmac_config);
		if (IS_ERR(amba)) {
			dev_err(dev, " Unable to create amba plug\n");
			return NULL;
		}
	} else
		amba = (struct stm_amba_bridge *) data;

	stm_amba_bridge_init(amba);

	return (void *) amba;
}

static struct plat_stmmacenet_data stih416_ethernet_platform_data[] = {
	{
		.dma_cfg = &gmac_dma_setting,
		.has_gmac = 1,
		.enh_desc = 1,
		.force_sf_dma_mode = 1,
		.bugged_jumbo = 1,
		.pmt = 1,
		.init = &stmmac_claim_resource,
		.exit = &stmmac_release_resource,
		.bus_setup = &stih416_ethernet_bus_setup,
	}, {
		.dma_cfg = &gmac_dma_setting,
		.has_gmac = 1,
		.enh_desc = 1,
		.force_sf_dma_mode = 1,
		.bugged_jumbo = 1,
		.pmt = 1,
		.init = &stmmac_claim_resource,
		.exit = &stmmac_release_resource,
		.bus_setup = &stih416_ethernet_bus_setup,
	}
};


static u64 stih416_eth_dma_mask = DMA_BIT_MASK(32);
static struct platform_device stih416_ethernet_devices[] = {
	{
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xFE810000, 0x8000),
			STIH416_RESOURCE_IRQ_NAMED("macirq", 133),
			STIH416_RESOURCE_IRQ_NAMED("eth_wake_irq", 134),
			STIH416_RESOURCE_IRQ_NAMED("eth_lpi", 135),
			/* MDINT irq ? */
		},
		.dev = {
			.dma_mask = &stih416_eth_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stih416_ethernet_platform_data[0],
		},
	}, {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xFEF08000, 0x8000),
			STIH416_RESOURCE_IRQ_NAMED("macirq", 136),
			STIH416_RESOURCE_IRQ_NAMED("eth_wake_irq", 137),
			STIH416_RESOURCE_IRQ_NAMED("eth_lpi", 138),
		},

		.dev = {
			.dma_mask = &stih416_eth_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stih416_ethernet_platform_data[1],
		},
	}
};

void __init stih416_configure_ethernet(int port,
		struct stih416_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stih416_ethernet_devices)];
	struct stih416_ethernet_config default_config;
	struct plat_stmmacenet_data *plat_data;
	struct stm_pad_config *pad_config;
	struct clk *clk;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stih416_ethernet_devices));
	BUG_ON(configured[port]++);

	clk = clk_get(NULL, port ? "CLK_S_ETH1_PHY" : "CLK_S_GMAC0_PHY");
	clk_set_rate(clk, 25000000);
	/*
	 * Request all the needed clocks just once
	 */
	gmac_clks[port].clk = clk_get(NULL, gmac_clk_n[port].clk_n);
	gmac_clks[port].parent_100MHz =
		clk_get(NULL, gmac_clk_n[port].parent_clk_100MHz_n);
	gmac_clks[port].parent_10MHz =
		clk_get(NULL, gmac_clk_n[port].parent_clk_10MHz_n);

	if (!config)
		config = &default_config;

	plat_data = &stih416_ethernet_platform_data[port];

	switch (config->interface) {
	case PHY_INTERFACE_MODE_MII:
		pad_config = &stih416_ethernet_mii_pad_configs[port];
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		else
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		break;
	case PHY_INTERFACE_MODE_REV_MII:
		pad_config = &stih416_ethernet_reverse_mii_pad_configs[port];
		if (config->ext_clk)
			stm_pad_set_pio_ignored(pad_config, "PHYCLK");
		else
			stm_pad_set_pio_out(pad_config, "PHYCLK", 1 + port);
		break;
	case PHY_INTERFACE_MODE_RMII: {
		struct sysconf_field *sc;
		pad_config = &stih416_ethernet_rmii_pad_configs[port];

		/* SEL_INTERNAL_NO_EXT_PHYCLK */
		if (!port)
			sc  = sysconf_claim(SYSCONF(508), 7, 7, "rmii");
		else
			sc  = sysconf_claim(SYSCONF(2559), 7, 7, "rmii");

		if (config->ext_clk) {
			stm_pad_set_pio_in(pad_config, "PHYCLK", 3 - port);
			/* SEL_INTERNAL_NO_EXT_PHYCLK */
			sysconf_write(sc, 0);
		} else {
			stm_pad_set_pio_out(pad_config, "PHYCLK", 2 - port);
			clk_set_rate(gmac_clks[port].clk, 50000000);
			/* SEL_INTERNAL_NO_EXT_PHYCLK */
			sysconf_write(sc, 1);
			}
		} break;
	case PHY_INTERFACE_MODE_GMII:
		pad_config = &stih416_ethernet_gmii_pad_configs[port];
		stm_pad_set_pio_out(pad_config, "PHYCLK", 4);
		if (port == 0)
			plat_data->fix_mac_speed =
				stih416_ethernet_gmii0_speed;
		else
			plat_data->fix_mac_speed =
				stih416_ethernet_gmii1_speed;

		plat_data->bsp_priv = config->txclk_select;
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		pad_config = &stih416_ethernet_rgmii_pad_configs[port];
		stm_pad_set_pio_out(pad_config, "PHYCLK", 4);
		if (port == 0)
			plat_data->fix_mac_speed =
				stih416_ethernet_rgmii0_gtx_speed;
		else
			plat_data->fix_mac_speed =
				stih416_ethernet_rgmii1_gtx_speed;

		/* Configure so that stmmac gets clock */
		plat_data->bsp_priv = config->txclk_select;

		break;
	default:
		pr_err("STMMAC Mode unsupported!!!\n");
		BUG();
		return;
	};

	plat_data->custom_cfg = (void *) pad_config;
	plat_data->interface = config->interface;
	plat_data->bus_id = config->phy_bus;
	plat_data->phy_bus_name = config->phy_bus_name;
	plat_data->phy_addr = config->phy_addr;
	plat_data->mdio_bus_data = config->mdio_bus_data;
	if (!config->mdio_bus_data) {
		stm_pad_set_pio_ignored(pad_config, "MDC");
		stm_pad_set_pio_ignored(pad_config, "MDIO");
	}

	platform_device_register(&stih416_ethernet_devices[port]);
}

/*
 * MMC
 */
/* MMC/SD resources ------------------------------------------------------ */
/* Custom PAD configuration for the MMC0 Host controller */
#define STIH416_PIO_MMC_CLK_OUT(_port, _pin, _fnc)		\
	{							\
		.gpio = stih416_gpio(_port, _pin),		\
		.direction = stm_pad_gpio_direction_bidir_pull_up,\
		.function = _fnc,				\
		.name = "MMCCLK",				\
		.priv = &(struct stm_pio_control_pad_config) {	\
			.retime = &(struct stm_pio_control_retime_config) { \
				.retime = 0,			\
				.clk = 1,			\
				.clknotdata = 1,		\
				.double_edge = 0,		\
				.invertclk = 0,			\
				.delay = 0,			\
			},					\
		},						\
	}

#define STIH416_PIO_MMC_OUT(_port, _pin, _fnc)			\
	{							\
		.gpio = stih416_gpio(_port, _pin),		\
		.direction = stm_pad_gpio_direction_out,	\
		.function = _fnc,				\
	}

#define STIH416_PIO_MMC_OUT_NAMED(_port, _pin, _fnc, _name)	\
	{							\
		.gpio = stih416_gpio(_port, _pin),		\
		.direction = stm_pad_gpio_direction_out,	\
		.function = _fnc,				\
		.name = _name,					\
	}

#define STIH416_PIO_MMC_BIDIR(_port, _pin, _fnc)		\
	{							\
		.gpio = stih416_gpio(_port, _pin),		\
		.direction = stm_pad_gpio_direction_bidir_pull_up,\
		.function = _fnc,				\
	}

#define STIH416_PIO_MMC_IN(_port, _pin, _fnc)			\
	{							\
		.gpio = stih416_gpio(_port, _pin),		\
		.direction = stm_pad_gpio_direction_in,		\
		.function = _fnc,				\
	}

static struct stm_pad_config stih416_mmc_pad_config[] = {
	[0] =  {
		.gpios_num = 15,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PIO_MMC_CLK_OUT(13, 4, 4),
			STIH416_PIO_MMC_BIDIR(14, 4, 4), /* MMC Data[0]*/
			STIH416_PIO_MMC_BIDIR(14, 5, 4), /* MMC Data[1]*/
			STIH416_PIO_MMC_BIDIR(14, 6, 4), /* MMC Data[2]*/
			STIH416_PIO_MMC_BIDIR(14, 7, 4), /* MMC Data[3]*/

			STIH416_PIO_MMC_BIDIR(15, 1, 4), /* MMC command */
			STIH416_PIO_MMC_IN(15, 3, 4), /* MMC Write Protection */

			STIH416_PIO_MMC_BIDIR(16, 4, 4), /* MMC Data[4]*/
			STIH416_PIO_MMC_BIDIR(16, 5, 4), /* MMC Data[5]*/
			STIH416_PIO_MMC_BIDIR(16, 6, 4), /* MMC Data[6]*/
			STIH416_PIO_MMC_BIDIR(16, 7, 4), /* MMC Data[7]*/

			STIH416_PIO_MMC_OUT(17, 1, 4),  /* MMC Card PWR */
			STIH416_PIO_MMC_IN(17, 2, 4),   /* MMC Card Detect */
			STIH416_PIO_MMC_OUT(17, 3, 4),  /* MMC LED on */
			STIH416_PIO_MMC_OUT_NAMED(17, 4, 4,
				"mmc_reset"),  /* MMC Reset_n */
		},
	},
	[1] =  {
		.gpios_num = 12,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PIO_MMC_CLK_OUT(15, 0, 3),
			STIH416_PIO_MMC_BIDIR(13, 7, 3), /* MMC Data[0]*/
			STIH416_PIO_MMC_BIDIR(14, 1, 3), /* MMC Data[1]*/
			STIH416_PIO_MMC_BIDIR(14, 2, 3), /* MMC Data[2]*/
			STIH416_PIO_MMC_BIDIR(14, 3, 3), /* MMC Data[3]*/

			STIH416_PIO_MMC_BIDIR(15, 4, 3), /* MMC command */

			STIH416_PIO_MMC_BIDIR(15, 6, 3), /* MMC Data[4]*/
			STIH416_PIO_MMC_BIDIR(15, 7, 3), /* MMC Data[5]*/
			STIH416_PIO_MMC_BIDIR(16, 0, 3), /* MMC Data[6]*/
			STIH416_PIO_MMC_BIDIR(16, 1, 3), /* MMC Data[7]*/
			STIH416_PIO_MMC_OUT(16, 2, 3),   /* MMC Card power */
			STIH416_PIO_MMC_OUT(13, 6, 3),  /* MMC Reset_n */
		},
		.sysconfs_num = 1,
		.sysconfs = (struct stm_pad_sysconf []) {
			/* MMC1_BOOT_ENABLE_CONFIG */
			STM_PAD_SYSCONF(SYSCONF(2582), 0, 0, 0),
		},
	},
};

static struct stm_amba_bridge_config stih416_amba_mmc_config = {
	.type = stm_amba_type2,
	.chunks_in_msg = 1,
	.packets_in_chunk = 2,
	.write_posting = stm_amba_write_posting_enabled,
	.max_opcode = stm_amba_opc_LD64_ST64,
	.type2.threshold = 128,
	.type2.sd_config_missing = 1,
	.type2.trigger_mode = stm_amba_stbus_threshold_based,
	.type2.read_ahead = stm_amba_read_ahead_enabled,
};

static struct stm_mmc_platform_data stih416_mmc_platform_data[] = {
	{
		.init = mmc_claim_resource,
		.exit = mmc_release_resource,
		.nonremovable = false,
		.custom_cfg = &stih416_mmc_pad_config[0],
		.amba_config = &stih416_amba_mmc_config,
	}, {
		.init = mmc_claim_resource,
		.exit = mmc_release_resource,
		.nonremovable = false,
		.custom_cfg = &stih416_mmc_pad_config[1],
		.amba_config = &stih416_amba_mmc_config,
	}
};

static struct platform_device stih416_mmc_device[] = {
	{
		.name = "sdhci-stm",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe81e000, 0x1000),
			STIH416_RESOURCE_IRQ_NAMED("mmcirq", 127),
		},
		.dev = {
			.platform_data = &stih416_mmc_platform_data[0],
		}
	}, {
		.name = "sdhci-stm",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfe81f000, 0x1000),
			STIH416_RESOURCE_IRQ_NAMED("mmcirq", 128),
		},
			.dev = {
			.platform_data = &stih416_mmc_platform_data[1],
		}
	}
};

extern struct platform_device stih416_asc_devices[];
extern struct platform_device *stm_asc_console_device;

void __init stih416_configure_mmc(int port, int is_emmc)
{
	static int configured[ARRAY_SIZE(stih416_mmc_device)];

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stih416_mmc_device));
	BUG_ON(configured[port]++);

	 if (is_emmc)
		stih416_mmc_platform_data[port].nonremovable = true;

	/*
	 * ASC-2 and MMC-0 share the same GPIO[17][4]...
	 * but on several board the ASC-2 is used as console.
	 * Due to the fact the MMC works fine also without the
	 * GPIO_Reset... when the ASC-2 is the console in the MMC
	 * configuration the GPIO[17][4] is ingnored
	 */
	if (!port && (stm_asc_console_device - stih416_asc_devices) == 2)
		stm_pad_set_pio_ignored(&stih416_mmc_pad_config[0],
			"mmc_reset");
	platform_device_register(&stih416_mmc_device[port]);
}
#endif /* CONFIG_OF */

/*
 * AHCI support
 */
static void stih416_sata_mp_select(void *data, int port)
{
}

/*
 *  __both__ the miphy are multiplexed with PCIe
 */
static struct stm_plat_pcie_mp_data stih416_sata_mp_platform_data[2] = {
	{
		.style_id = ID_MIPHY365X,
		.miphy_first = 0,
		.miphy_count = 1,
		.miphy_modes = (enum miphy_mode[1]) {},
		.mp_select = stih416_sata_mp_select,
		.sata_gen = SATA_GEN3,
		.tx_pol_inv = 1,
	}, {
		.style_id = ID_MIPHY365X,
		.miphy_first = 1,
		.miphy_count = 1,
		.miphy_modes = (enum miphy_mode[1]) {},
		.mp_select = stih416_sata_mp_select,
		.sata_gen = SATA_GEN3,
		.tx_pol_inv = 1,
	},
};

#define MIPHY(_id, _iomem_sata, _iomem_pcie)				\
	{								\
		.name = "pcie-mp",					\
		.id     = _id,						\
		.num_resources = 2,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM_NAMED("sata-uport",	\
						    _iomem_sata, 0xff),	\
			STM_PLAT_RESOURCE_MEM_NAMED("pcie-uport",	\
						    _iomem_pcie, 0xff),	\
		},							\
		.dev.platform_data =					\
			&stih416_sata_mp_platform_data[_id],		\
	}

static struct platform_device stih416_miphy_devices[2] = {
	MIPHY(0, 0xfe382000, 0xfe394000),
	MIPHY(1, 0xfe38a000, 0xfe804000),
};

static struct stm_amba_bridge_config stih416_amba_sata_config = {
	.type = stm_amba_type2,
	.chunks_in_msg = 1,
	.packets_in_chunk = 1,
	.write_posting = stm_amba_write_posting_enabled,
	.max_opcode = stm_amba_opc_LD64_ST64,
	.type2.threshold = 128,
	.type2.sd_config_missing = 1,
	.type2.trigger_mode = stm_amba_stbus_threshold_based,
	.type2.read_ahead = stm_amba_read_ahead_enabled,
};

void stih416_configure_miphy(struct stih416_miphy_config *config)
{

	static int configured[ARRAY_SIZE(stih416_miphy_devices)];
	struct sysconf_field *sc;
	struct stm_plat_pcie_mp_data *pdata =
		&stih416_sata_mp_platform_data[config->id];
	const struct miphy_sysconf {
		unsigned short grp;
		unsigned short nr;
	} miphy_sc[2] = {
		{	SYSCONF(2521),},
		{	SYSCONF(2522)  }
	};

	BUG_ON(config->id < 0 ||
		config->id >= ARRAY_SIZE(stih416_miphy_devices));
	BUG_ON(configured[config->id]++);

	pdata->miphy_modes[0] = config->mode;

	sc = sysconf_claim(miphy_sc[config->id].grp, miphy_sc[config->id].nr,
				5, 6, "MiPhy");
	switch (config->mode) {
	case SATA_MODE:
		pdata->rx_pol_inv = config->rx_pol_inv;
		pdata->tx_pol_inv = config->tx_pol_inv;
		sysconf_write(sc, 0);
		break;
	case PCIE_MODE:
		/* TODO */
		sysconf_write(sc, 3);
		break;
	default:
		BUG();
	}

	platform_device_register(&stih416_miphy_devices[config->id]);
}

#define AHCI_HOST_PWR	   "SATA_HOST_PWR"
#define AHCI_HOST_ACK	   "SATA_HOST_ACK"
static void stih416_ahci_power(struct stm_device_state *device_state,
	enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, AHCI_HOST_PWR, value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, AHCI_HOST_ACK)
			== value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_ahci_data stm_ahci_plat_data[2] = {
	{
		.device_config = &(struct stm_device_config) {
			.power = stih416_ahci_power,
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 3, 3,
					AHCI_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 3, 3,
					AHCI_HOST_ACK),
				},
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 3,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* Select Sata instead of PCIe */
					STM_PAD_SYSCONF(SYSCONF(2521), 1, 1, 1),
					/* Sata-phy uses 30MHz for PLL */
					STM_PAD_SYSCONF(SYSCONF(2521), 2, 2, 0),
					/* speed mode gen3 */
					STM_PAD_SYSCONF(SYSCONF(2521), 3, 4, 2),
					},
			},
		},
		.amba_config = &stih416_amba_sata_config,
		.miphy_num = 0,
	}, {
		.device_config = &(struct stm_device_config){
			.power = stih416_ahci_power,
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(2525), 4, 4,
					AHCI_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(2583), 4, 4,
					AHCI_HOST_ACK),
				},
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 3,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* Select Sata instead of PCIe */
					STM_PAD_SYSCONF(SYSCONF(2522), 1, 1, 1),
					/* Sata-phy uses 30MHz for PLL */
					STM_PAD_SYSCONF(SYSCONF(2522), 2, 2, 0),
					/* speed mode gen3 */
					STM_PAD_SYSCONF(SYSCONF(2522), 3, 4, 2),
					},
			},
		},
		.amba_config = &stih416_amba_sata_config,
		.miphy_num = 1,
	},
};

static u64 stih416_ahci_dmamask = DMA_BIT_MASK(32);

#define AHCI_STM(_id, _iomem, _irq)				\
{								\
	.name = "ahci_stm",					\
	.id = _id,						\
	.num_resources = 3,					\
	.resource = (struct resource[]) {			\
		STM_PLAT_RESOURCE_MEM_NAMED("ahci", _iomem,	\
				 0x1000),			\
		STM_PLAT_RESOURCE_MEM_NAMED("ahci-amba",	\
				 _iomem + 0x3000, 0x1000),	\
		STIH416_RESOURCE_IRQ_NAMED("ahci", _irq),	\
	},							\
	.dev = {						\
		.dma_mask = &stih416_ahci_dmamask,		\
		.coherent_dma_mask = DMA_BIT_MASK(32),		\
		.platform_data = &stm_ahci_plat_data[_id],	\
	},							\
}

struct platform_device stih416_ahci_devices[] = {
	AHCI_STM(0, 0xfe380000, 157),
	AHCI_STM(1, 0xfe388000, 165),
};

void __init stih416_configure_sata(int sata_port)
{
	static int configured[ARRAY_SIZE(stih416_ahci_devices)];

	BUG_ON(sata_port < 0 || sata_port >= ARRAY_SIZE(stih416_ahci_devices));
	BUG_ON(configured[sata_port]++);

	platform_device_register(&stih416_ahci_devices[sata_port]);
}

