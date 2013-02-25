/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/stm/emi.h>
#include <linux/stm/pad.h>
#include <linux/stm/device.h>
#include <linux/stm/soc.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih416.h>
#include <linux/stm/mpe42-periphs.h>
#include <linux/stm/sasg2-periphs.h>

#include <asm/mach/map.h>
#include <mach/soc-stih416.h>
#include <mach/hardware.h>

#include "../pio-control.h"


/* EMI resources ---------------------------------------------------------- */
static void stih416_emi_power(struct stm_device_state *device_state,
			      enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, "EMI_PWR", value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, "EMI_ACK")
		    == value)
			break;
		mdelay(10);
	}
}

static struct platform_device stih416_emi = {
	.name = "emi",
	.id = -1,
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("emi memory", 0, 256 * 1024 * 1024),
		STM_PLAT_RESOURCE_MEM_NAMED("emi4 config", 0xfe900000, 0x874),
		STM_PLAT_RESOURCE_MEM_NAMED("emiss config", 0xfef01000, 0x80),
	},
	.dev.platform_data = &(struct stm_device_config){
		.sysconfs_num = 2,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(SYSCONF(1500), 0, 0, "EMI_PWR"),
			STM_DEVICE_SYSCONF(SYSCONF(1578), 0, 0, "EMI_ACK"),
		},
		.power = stih416_emi_power,
	},
};

/* NAND Resources --------------------------------------------------------- */
/*
 * stih415 and stih416 shares the same EMI-SubSystem
 */
static struct stm_plat_nand_flex_data stih416_nand_flex_data;
static struct stm_plat_nand_bch_data stih416_nand_bch_data;

static struct platform_device stih416_nandi_device = {
	.num_resources	= 3,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("nand_mem", 0xfe901000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("nand_dma", 0xfef00800, 0x0800),
		STIH416_RESOURCE_IRQ_NAMED("nand_irq", 139),
	},
};

void __init stih416_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_bch_data  *bch_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Not supported */
		BUG();
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		emiss_nandi_select(STM_NANDI_HAMMING);
		flex_data = &stih416_nand_flex_data;
		stih416_nandi_device.dev.platform_data = flex_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stih416_nandi_device.name = (config->driver == stm_nand_afm) ?
				"stm-nand-afm" : "stm-nand-flex";
		platform_device_register(&stih416_nandi_device);
		break;
	case stm_nand_bch:
		BUG_ON(config->nr_banks > 1);
		bch_data = &stih416_nand_bch_data;
		stih416_nandi_device.dev.platform_data = bch_data;
		bch_data->bank = config->banks;
		bch_data->bch_ecc_cfg = config->bch_ecc_cfg;
		stih416_nandi_device.name = "stm-nand-bch";
		platform_device_register(&stih416_nandi_device);
		break;
	default:
		BUG();
		return;
	}
}

/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stih416_asc_pad_configs[7] = {

	/* Comms block ASCs in SASG2 */
	[0] = {
		/* UART0 */
		/* Plus smartcard signals on PIO10[4] to PIO10[7] */
		.gpios_num = 5,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(10, 0, 2),/* TX */
			STIH416_PAD_PIO_IN(10, 1, 2),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(10, 2, 2, "CTS"),  /* CTS */
			STIH416_PAD_PIO_OUT_NAMED(10, 3, 2, "RTS"), /* RTS */
			STIH416_PAD_PIO_OUT_NAMED(10, 4, 2, "not_OE"),
			},
	},
	[1] = {
		/* UART1 */
		/* Tx: PIO11[0], Rx: PIO11[1], RTS: PIO11[3], CTS: PIO11[2] */
		.gpios_num = 5,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(11, 0, 2),/* TX */
			STIH416_PAD_PIO_IN(11, 1, 2),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(11, 2, 2, "CTS"),  /* CTS */
			STIH416_PAD_PIO_OUT_NAMED(11, 3, 2, "RTS"), /* RTS */
			STIH416_PAD_PIO_OUT_NAMED(11, 4, 2, "not_OE"),
			},
	},
	[2] = {
		/* UART2 */
		/* Tx: PIO17[4], Rx: PIO17[5], RTS: PIO17[7], CTS: PIO17[6]
		 * OE: PIO17[3]
		 */
		.gpios_num = 5,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(17, 4, 2),	/* TX */
			STIH416_PAD_PIO_IN(17, 5, 2),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(17, 6, 2, "CTS"),
			STIH416_PAD_PIO_OUT_NAMED(17, 7, 2, "RTS"),
			STIH416_PAD_PIO_OUT_NAMED(17, 3, 2, "not_OE"),
		},
	},
	[3] = {
		/* UART3 - not wired to pins */
		/* can works on 10.[4 - 7] or */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(10, 4, 5),	/* TX */
			STIH416_PAD_PIO_IN(10, 7, 5),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(10, 6, 5, "CTS"),
			STIH416_PAD_PIO_OUT_NAMED(10, 5, 5, "RTS"),
		},
	},

	[4] = {
		/* UART10 (MPE) */
		.gpios_num = 6,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(101, 1, 1), /* TX */
			STIH416_PAD_PIO_IN(101, 2, 1),  /* RX */
			STIH416_PAD_PIO_IN_NAMED(101, 3, 1, "CTS"),
			STIH416_PAD_PIO_OUT_NAMED(101, 4, 1, "RTS"),
			STIH416_PAD_PIO_OUT_NAMED(101, 5, 1, "not_OE"),
			STIH416_PAD_PIO_OUT(101, 6, 1), /* ClkGEN */
		},
	},

	/* SBC comms block ASCs */
	[5] = {
		/* SBC_UART0 */
		/* Tx: PIO3[4], Rx: PIO3[5], RTS: PIO3[7], CTS: PIO3[6] */
		/* OE: PIO4[0] */
		.gpios_num = 5,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(3, 4, 1),	/* TX */
			STIH416_PAD_PIO_IN(3, 5, 1),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(3, 6, 1, "CTS"),
			STIH416_PAD_PIO_OUT_NAMED(3, 7, 1, "RTS"),
			STIH416_PAD_PIO_OUT_NAMED(4, 0, 1, "not_OE"),
		},
	},
	[6] = {
		/* SBC_UART1 */
		/* Tx: PIO2[6], Rx: PIO2[7], RTS: PIO3[1], CTS: PIO3[0] */
		/* OE: PIO3[2] */
		.gpios_num = 5,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_OUT(2, 6, 3),	/* TX */
			STIH416_PAD_PIO_IN(2, 7, 3),	/* RX */
			STIH416_PAD_PIO_IN_NAMED(3, 0, 3, "CTS"),
			STIH416_PAD_PIO_OUT_NAMED(3, 1, 3, "RTS"),
			STIH416_PAD_PIO_OUT_NAMED(3, 2, 3, "not_OE"),
		},
	},
};

struct platform_device stih416_asc_devices[7] = {

	/* Comms block ASCs in SASG2 */
	/*
	 * Assuming these are UART0 to UART2 in the PIO document.
	 * Note no UART3.
	 * Assuming these are asc_100 to asc_103 in interrupt document
	 */
	[0] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_ASC0_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(195),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[0],
			.regs = (void __iomem *)IO_ADDRESS(SASG2_ASC0_BASE),
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_ASC1_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(196),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_ASC2_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(197),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[2],
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_ASC3_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(198),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[3],
		},
	},

	[4] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(MPE42_ASC10_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(117),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[4],
		},
	},

	/* SBC comms block ASCs in SASG2 */
	/*
	 * Assuming these are UART10 and UART11 in the PIO document.
	 * Assuming these are lpm_uart_0 and lpm_uart_1 in interrupt document.
	 */
	[5] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_SBC_ASC0_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(209),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[5],
			.clk_id = "sbc_comms_clk",
		},
	},
	[6] = {
		.name = "stm-asc",
		/* .id set in stih416_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_SBC_ASC1_BASE, 0x2c),
			STIH416_RESOURCE_IRQ(210),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih416_asc_pad_configs[6],
			.clk_id = "sbc_comms_clk",
		},
	},

};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
struct platform_device *stm_asc_console_device;

/* Platform devices to register */
static unsigned int __initdata stm_asc_configured_devices_num;
static struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stih416_asc_devices)];

void __init stih416_configure_asc(int asc, struct stih416_asc_config *config)
{
	static int configured[ARRAY_SIZE(stih416_asc_devices)];
	static int tty_id;
	struct stih416_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;
	struct stm_pad_config *pad_config = &stih416_asc_pad_configs[asc];

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stih416_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stih416_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->force_m1 = config->force_m1;

	if (!config->hw_flow_control) {
		/* Don't claim RTS/CTS pads */
		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (!config->use_oe_signal)
		/* Don't claim 'not_OE' pad */
		stm_pad_set_pio_ignored(pad_config, "not_OE");

	if (config->is_console)
		stm_asc_console_device = pdev;

	switch (asc) {
	case 4:
		clk_add_alias_platform_device(NULL, pdev,
			"CLK_M_ICN_REG_12", NULL);
		break;
	case 5 ... 6:
		clk_add_alias_platform_device(NULL, pdev,
			"sbc_comms_clk", NULL);
		break;
	default:
		/* it will use the standard comms_clk */
		break;
	}

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stih416_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stih416_add_asc);



/* PIO ports resources ---------------------------------------------------- */
#define STIH416_NR_GPIO_BANK			30

static int stih416_pio_pin_name(char *name, int size, int port, int pin)
{
	/*
	 * for clarification have a look @
	 * include/linux/stm/stih416.h
	 */
	switch (port) {
	case 6 ... 13:
		port--;
		break;
	case 14 ... 15:
		port += (30 - 14);
		break;
	case 16 ... 21:
		port -= 3;
		break;
	case 22 ... 29:
		port += (100 - 22);
		break;
	case 5:
		port = 40;
	case 0 ... 4:
		break;
	}

	return snprintf(name, size, "PIO%d.%d", port, pin);
}
/*
 * H416 has several holes in the GPIO_Bank numbering...
 * Please don't force ordering in the array
 */
#define STIH416_GPIO_ENTRY(_num, _base)					\
	[STIH416_GPIO(_num)] = {					\
		.name = "stm-gpio",					\
		.id = STIH416_GPIO(_num),				\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_base, 0x100),		\
		},							\
		.dev.platform_data = &(struct stm_plat_pio_data) {	\
			.regs = (void __iomem *)IO_ADDRESS(_base),	\
			.pin_name = stih416_pio_pin_name,		\
		},							\
	}

static struct platform_device stih416_pio_devices[STIH416_NR_GPIO_BANK] = {
	/* SAS */
	/* [0,4] [40]: SBC_PIO */
	STIH416_GPIO_ENTRY(0, SASG2_SBC_PIO_BASE + 0x0000),
	STIH416_GPIO_ENTRY(1, SASG2_SBC_PIO_BASE + 0x1000),
	STIH416_GPIO_ENTRY(2, SASG2_SBC_PIO_BASE + 0x2000),
	STIH416_GPIO_ENTRY(3, SASG2_SBC_PIO_BASE + 0x3000),
	STIH416_GPIO_ENTRY(4, SASG2_SBC_PIO_BASE + 0x4000),
	STIH416_GPIO_ENTRY(40, SASG2_SBC_PIO_BASE + 0x5000),
	/* [5, 12],[30, 31]: PIO_FRONT */
	STIH416_GPIO_ENTRY(5, SASG2_PIO_FRONT_BASE + 0x0000),
	STIH416_GPIO_ENTRY(6, SASG2_PIO_FRONT_BASE + 0x1000),
	STIH416_GPIO_ENTRY(7, SASG2_PIO_FRONT_BASE +  0x2000),
	STIH416_GPIO_ENTRY(8, SASG2_PIO_FRONT_BASE + 0x3000),
	STIH416_GPIO_ENTRY(9, SASG2_PIO_FRONT_BASE + 0x4000),
	STIH416_GPIO_ENTRY(10, SASG2_PIO_FRONT_BASE + 0x5000),
	STIH416_GPIO_ENTRY(11, SASG2_PIO_FRONT_BASE + 0x6000),
	STIH416_GPIO_ENTRY(12, SASG2_PIO_FRONT_BASE + 0x7000),
	STIH416_GPIO_ENTRY(30, SASG2_PIO_FRONT_BASE + 0x8000),
	STIH416_GPIO_ENTRY(31, SASG2_PIO_FRONT_BASE + 0x9000),
	/* [13,18]: PIO_REAR */
	STIH416_GPIO_ENTRY(13, SASG2_PIO_REAR_BASE + 0x0000),
	STIH416_GPIO_ENTRY(14, SASG2_PIO_REAR_BASE + 0x1000),
	STIH416_GPIO_ENTRY(15, SASG2_PIO_REAR_BASE + 0x2000),
	STIH416_GPIO_ENTRY(16, SASG2_PIO_REAR_BASE + 0x3000),
	STIH416_GPIO_ENTRY(17, SASG2_PIO_REAR_BASE + 0x4000),
	STIH416_GPIO_ENTRY(18, SASG2_PIO_REAR_BASE + 0x5000),

	/* MPE */
	/* [100, 102]: PIO_10 */
	STIH416_GPIO_ENTRY(100,	MPE42_PIO_10_BASE + 0x0000),
	STIH416_GPIO_ENTRY(101,	MPE42_PIO_10_BASE + 0x1000),
	STIH416_GPIO_ENTRY(102, MPE42_PIO_10_BASE + 0x2000),
	/* [103,107]: PIO_11 */
	STIH416_GPIO_ENTRY(103,	MPE42_PIO_11_BASE + 0x0000),
	STIH416_GPIO_ENTRY(104,	MPE42_PIO_11_BASE + 0x1000),
	STIH416_GPIO_ENTRY(105,	MPE42_PIO_11_BASE + 0x2000),
	STIH416_GPIO_ENTRY(106,	MPE42_PIO_11_BASE + 0x3000),
	STIH416_GPIO_ENTRY(107,	MPE42_PIO_11_BASE + 0x4000)
};

/*
 * SOC desinged confirmed the delay_value are
 * direction independant
 */
static const unsigned int stih416_pio_control_delays_in_out[] = {
	0,	/* 0000: 0.0 ns		*/
	300,	/* 0001: 0.3 ns		*/
	500,	/* 0010: 0.5 ns		*/
	750,	/* 0011: 0.75 ns	*/
	1000,	/* 0100: 1.0 ns		*/
	1250,	/* 0101: 1.25 ns	*/
	1500,	/* 0110: 1.5 ns		*/
	1750,	/* 0111: 1.75 ns	*/
	2000,	/* 1000: 2.0 ns		*/
	2250,	/* 1001: 2.25 ns	*/
	2500,	/* 1010: 2.5 ns		*/
	2750,	/* 1011: 2.75 ns	*/
	3000,	/* 1100: 3.0 ns		*/
	3250,	/* 1101: 3.25 ns	*/
};

static const struct stm_pio_control_retime_params stih416_retime_params = {
	.delay_times_in = stih416_pio_control_delays_in_out,
	.num_delay_times_in = ARRAY_SIZE(stih416_pio_control_delays_in_out),
	.delay_times_out = stih416_pio_control_delays_in_out,
	.num_delay_times_out = ARRAY_SIZE(stih416_pio_control_delays_in_out),
};

#define STIH416_GPIO_ENTRY_CONTROL(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _lsb, _msb,			\
		_rt)							\
	[STIH416_GPIO(_num)] = {					\
		.alt = { SYSCONF(_alt_num) },				\
		.oe = { SYSCONF(_oe_num), _lsb, _msb },			\
		.pu = { SYSCONF(_pu_num), _lsb, _msb },			\
		.od = { SYSCONF(_od_num), _lsb, _msb },			\
		.retime_style = stm_pio_control_retime_style_dedicated,	\
		.retime_pin_mask = 0xff,				\
		.retime_params = &stih416_retime_params,		\
		.retiming = {						\
			{ SYSCONF(_rt) },				\
			{ SYSCONF((_rt) + 1) },				\
			{ SYSCONF((_rt) + 2) },				\
			{ SYSCONF((_rt) + 3) },				\
			{ SYSCONF((_rt) + 4) },				\
			{ SYSCONF((_rt) + 5) },				\
			{ SYSCONF((_rt) + 6) },				\
			{ SYSCONF((_rt) + 7) }				\
		},							\
	}

#define STIH416_GPIO_ENTRY_CONTROL_x_4(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _rt)				\
	STIH416_GPIO_ENTRY_CONTROL(_num,   _alt_num,			\
		_oe_num, _pu_num, _od_num,  0,  7,			\
		_rt),							\
	STIH416_GPIO_ENTRY_CONTROL(_num+1, _alt_num+1,			\
		_oe_num, _pu_num, _od_num,  8, 15,			\
		_rt + 8),						\
	STIH416_GPIO_ENTRY_CONTROL(_num+2, _alt_num+2,			\
		_oe_num, _pu_num, _od_num, 16, 23,			\
		_rt + 16),						\
	STIH416_GPIO_ENTRY_CONTROL(_num+3, _alt_num+3,			\
		_oe_num, _pu_num, _od_num, 24, 31,			\
		_rt + 24)

#define STIH416_GPIO_ENTRY_RET_CUSTOM(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _lsb, _msb, ...)		\
	[STIH416_GPIO(_num)] = {					\
		.alt = { SYSCONF(_alt_num) },				\
		.oe = { SYSCONF(_oe_num), _lsb, _msb },			\
		.pu = { SYSCONF(_pu_num), _lsb, _msb },			\
		.od = { SYSCONF(_od_num), _lsb, _msb },			\
		.retime_style = stm_pio_control_retime_style_dedicated,	\
		.retime_params = &stih416_retime_params,		\
		__VA_ARGS__,						\
	}

static const struct stm_pio_control_config
stih416_pio_control_configs[STIH416_NR_GPIO_BANK] = {
	/*			      pio, alt,  oe,    pu,   od, lsb, msb, rt */
	/* [0:4][40]: SBC */
	STIH416_GPIO_ENTRY_CONTROL_x_4(  0,   0,   40,   50,   60,          100),
	STIH416_GPIO_ENTRY_CONTROL(      4,   4,   41,   51,   61,  0,  7,  132),
	STIH416_GPIO_ENTRY_RET_CUSTOM(  40,   5,   41,   51,   61,  8, 15,
		.retime_pin_mask = 0x7f,
		.retiming = {
			{ SYSCONF(140) },
			{ SYSCONF(140 + 1) },
			{ SYSCONF(140 + 2) },
			{ SYSCONF(140 + 3) },
			{ SYSCONF(140 + 4) },
			{ SYSCONF(140 + 5) },
			{ SYSCONF(140 + 6) },
		}),
	/* [5:12][30:31]: SAS_FRONT */
	STIH416_GPIO_ENTRY_CONTROL_x_4(  5, 1000, 1040, 1050, 1060,        1100),
	STIH416_GPIO_ENTRY_CONTROL_x_4(  9, 1004, 1041, 1051, 1061,        1132),
	STIH416_GPIO_ENTRY_CONTROL(     30, 1008, 1042, 1052, 1062, 0,  7, 1164),
	STIH416_GPIO_ENTRY_CONTROL(     31, 1009, 1042, 1052, 1062, 8, 15, 1172),

	/* [13:18]: SAS_REAR */
	STIH416_GPIO_ENTRY_CONTROL_x_4( 13, 2000, 2040, 2050, 2060,         2100),
	STIH416_GPIO_ENTRY_CONTROL(     17, 2004, 2041, 2051, 2061,  0,  7, 2132),
	STIH416_GPIO_ENTRY_RET_CUSTOM(  18, 2005, 2041, 2051, 2061,  8, 15,
		.retime_pin_mask = 0xf,
		.retiming = {
			{ SYSCONF(2140) },
			{ SYSCONF(2140 + 1) },
			{ SYSCONF(2140 + 2) },
			{ SYSCONF(2140 + 3) },
		}),

	/* [100:102]: MPE_PIO_10 */
	STIH416_GPIO_ENTRY_CONTROL(    100, 5000, 5040, 5050, 5060,  0,  7, 5100),
	STIH416_GPIO_ENTRY_CONTROL(    101, 5001, 5040, 5050, 5060,  8, 15, 5108),
	STIH416_GPIO_ENTRY_CONTROL(    102, 5002, 5040, 5050, 5060, 16, 23, 5116),

	/* [103:107]: MPE_PIO_11 */
	STIH416_GPIO_ENTRY_CONTROL_x_4(103, 6000, 6040, 6050, 6060,         6100),
	STIH416_GPIO_ENTRY_RET_CUSTOM( 107, 6004, 6041, 6051, 6061,  0,  7,
		.retime_pin_mask = 0xf,
		.retiming = {
			{ SYSCONF(6132) },
			{ SYSCONF(6132 + 1) },
			{ SYSCONF(6132 + 2) },
			{ SYSCONF(6132 + 3) },
		}),
};

static struct stm_pio_control stih416_pio_controls[STIH416_NR_GPIO_BANK];

static int stih416_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	struct stm_pio_control_pad_config *config = priv;

	return stm_pio_control_config_all(gpio, direction, function, config,
		stih416_pio_controls,
		ARRAY_SIZE(stih416_pio_devices), 8);
}

#ifdef CONFIG_DEBUG_FS
static void stih416_pio_report(unsigned gpio, char *buf, int len)
{
	stm_pio_control_report_all(gpio, stih416_pio_controls,
		buf, len);
}
#else
#define stih416_pio_report NULL
#endif

static const struct stm_pad_ops stih416_pad_ops = {
	.gpio_config = stih416_pio_config,
	.gpio_report = stih416_pio_report,
};

static void __init stih416_pio_init(void)
{
	stm_pio_control_init(stih416_pio_control_configs, stih416_pio_controls,
			     ARRAY_SIZE(stih416_pio_control_configs));
}


static struct platform_device stih416_pio_irqmux_devices[5] = {
	{
		/* PIO0-4: SBC_PIO (aka pio_sas_sbc, PIO_SBC) */
		/* PIO40 */
		.name = "stm-gpio-irqmux",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_SBC_PIO_BASE
						+ 0xf080, 0x4),
			STIH416_RESOURCE_IRQ(182),
		},
		.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
			.port_first = STIH416_GPIO(0),
			.ports_num = 6,
		}
	}, {
		/* PIO5-12: PIO_FRONT (aka pio_sas_front) */
		/* PIO30-31 */
		.name = "stm-gpio-irqmux",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_PIO_FRONT_BASE +
						0xf080, 0x4),
			STIH416_RESOURCE_IRQ(183),
		},
		.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
			.port_first = STIH416_GPIO(5),
			.ports_num = 10,
		}
	}, {
		/* PIO13-18: PIO_REAR (aka pio_sas_rear) */
		.name = "stm-gpio-irqmux",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(SASG2_PIO_REAR_BASE +
						0xf080, 0x4),
			STIH416_RESOURCE_IRQ(184),
		},
		.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
			.port_first = STIH416_GPIO(13),
			.ports_num = 6,
		}
	}, {
		/* PIO100-102: PIO_RIGHT (aka MPE_PIO, PIO0_MPE) */
		.name = "stm-gpio-irqmux",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(MPE42_PIO_10_BASE +
						0xf080, 0x4),
			STIH416_RESOURCE_IRQ(113),
		},
		.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
			.port_first = STIH416_GPIO(100),
			.ports_num = 3,
		}
	}, {
		/* PIO103-107: PIO_LEFT (aka PIO1_MPE) */
		.name = "stm-gpio-irqmux",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(MPE42_PIO_11_BASE +
						0xf080, 0x4),
			STIH416_RESOURCE_IRQ(114),
		},
		.dev.platform_data = &(struct stm_plat_pio_irqmux_data) {
			.port_first = STIH416_GPIO(103),
			.ports_num = 5,
		}
	}
};

/* sysconf resources ------------------------------------------------------ */
#define STIH416_SYSCONF(_group, _name, _iomem, _size)			\
[_group] = {								\
		.name		= "sysconf",				\
		.id		= _group,				\
		.num_resources	= 1,					\
		.resource	= (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_iomem, _size),		\
		},							\
		.dev.platform_data = &(struct stm_plat_sysconf_data) {	\
			.regs = (void __iomem *)IO_ADDRESS(_iomem),	\
			.groups_num = 1,				\
			.groups = (struct stm_plat_sysconf_group []) {	\
				{					\
					.group = _group,		\
					.offset = 0,			\
					.name = _name,			\
				}					\
			},						\
		}							\
	}
static struct platform_device stih416_sysconf_devices[] = {
	/* SAS */

	/* SYSCFG_SBC (0000-0999) */
	/* Stand-By Controler System configuration registers */
	STIH416_SYSCONF(0, "SYSCFG_SBC",
			SASG2_SBC_SYSCONF_BASE, 0x1000),

	/* SYSCFG_SAS_FRONT (1000-1999) */
	/* SAS System configuration registers */
	STIH416_SYSCONF(1, "SYSCFG_FRONT",
			SASG2_FRONT_SYSCONF_BASE, 0x1000),

	/* SYSCFG_SAS_REAR (2000-2999) */
	/* SAS System configuration registers */
	STIH416_SYSCONF(2, "SYSCFG_REAR",
			SASG2_REAR_SYSCONF_BASE, 0x1000),

	/* MPE */

	/* MPE_FVDP_FE (5000-5999) */
	/* MPE System configuration registers */
	STIH416_SYSCONF(3, "SYSCFG_FVDP_FE",
			MPE42_FVDP_FE_SYSCON_BASE, 0x1000),

	/* SYSCFG_FVDP_LITE (6000-6999) */
	/* MPE System configuration registers */
	STIH416_SYSCONF(4, "SYSCFG_FVDP_LITE",
			MPE42_FVDP_LITE_SYSCONF_BASE, 0x1000),
	/* SYSCFG_CPU (7000-7999) */
	/* MPE System configuration registers */
	STIH416_SYSCONF(5, "SYSCFG_CPU",
			MPE42_CPU_SYSCONF_BASE, 0x1000),
	/* SYSCFG_COMPO (8000-8999) */
	/* MPE System configuration registers */
	STIH416_SYSCONF(6, "SYSCFG_COMPO",
			MPE42_COMPO_SYSCONF_BASE, 0x1000),

	/* SYSCFG_TRANSPORT (9000-9999) */
	/* MPE System configuration registers */
	STIH416_SYSCONF(7, "SYSCFG_TRANSPORT",
			MPE42_TRANSPORT_SYSCONF_BASE, 0x1000),

	/* SBC */
	/* LPM CONFIG and Status Registers */
	STIH416_SYSCONF(8, "LPM_CFG_REGS",
			SASG2_SBC_LPM_CONF_BASE, 0x54),
};

/* Utility functions  ------------------------------------------------------*/

void stih416_reset(char mode, const char *cmd)
{
	struct sysconf_field *sc = sysconf_claim(SYSCONF(504),
					0, 0, "LPM_SW_RST_N");
	sysconf_write(sc, 0);
}

/* SPI FSM Resources ------------------------------------------------------ */

static struct stm_pad_config stih416_spifsm_pad_config = {
	.gpios_num = 6,
	.gpios = (struct stm_pad_gpio[]) {
		STIH416_PAD_PIO_OUT_NAMED(12, 2, 1, "spi-fsm-clk"),
		STIH416_PAD_PIO_OUT_NAMED(12, 3, 1, "spi-fsm-cs"),
		/* To support QUAD mode operations, each of the following pads
		 * may be used by the IP as an input or an output.  Here we
		 * specify either PIO_OUT or PIO_IN, which sets pu = 0 && od =
		 * 0. 'oe' is taken from a signal generated by the SPI-FSM IP
		 * itself.
		 */
		STIH416_PAD_PIO_OUT_NAMED(12, 4, 1, "spi-fsm-mosi"),
		STIH416_PAD_PIO_IN_NAMED(12, 5, 1, "spi-fsm-miso"),
		STIH416_PAD_PIO_OUT_NAMED(12, 6, 1, "spi-fsm-hold"),
		STIH416_PAD_PIO_OUT_NAMED(12, 7, 1, "spi-fsm-wp"),
	}
};

static struct platform_device stih416_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("spi-fsm", 0xFE902000, 0x1000),
	},
};

void __init stih416_configure_spifsm(struct stm_plat_spifsm_data *data)
{
	stih416_spifsm_device.dev.platform_data = data;

	data->pads = &stih416_spifsm_pad_config;

	/* SoC/IP Capabilities */
	data->capabilities.no_read_repeat = 1;
	data->capabilities.no_write_repeat = 1;
	data->capabilities.read_status_bug = spifsm_read_status_clkdiv4;

	platform_device_register(&stih416_spifsm_device);
}

/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stih416_early_device_init(void)
{
	struct sysconf_field *sc;

	/* Initialise PIO and sysconf drivers */
	sysconf_early_init(stih416_sysconf_devices,
			   ARRAY_SIZE(stih416_sysconf_devices));
	stih416_pio_init();
	stm_gpio_early_init(stih416_pio_devices,
			ARRAY_SIZE(stih416_pio_devices),
			256
		);
	stm_pad_init(ARRAY_SIZE(stih416_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, &stih416_pad_ops);

	/* Version information in SYSTEM_STATUS9516 */
	sc = sysconf_claim(SYSCONF(9516), 0, 31, "devid");
	stm_soc_set(sysconf_read(sc), -1, -1);
}



/* Pre-arch initialisation ------------------------------------------------ */

static int __init stih416_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(stih416_pio_devices); i++)
		platform_device_register(&stih416_pio_devices[i]);
	for (i = 0; i < ARRAY_SIZE(stih416_pio_irqmux_devices); i++)
		platform_device_register(&stih416_pio_irqmux_devices[i]);

	return platform_device_register(&stih416_emi);
}
postcore_initcall(stih416_postcore_setup);



/* Internal temperature sensor resources ---------------------------------- */
static void stih416_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static struct clk *temp_clk;

static int stih416_temp_init(struct stm_device_state *device_state)
{
	struct clk *parent;
	int ret;

	temp_clk = clk_get(NULL, "CLK_S_THSENS");
	if (IS_ERR(temp_clk)) {
		ret = PTR_ERR(temp_clk);
		goto failed;
	}

	parent = clk_get(NULL, "CLK_S_C_FS0_CH2");
	if (IS_ERR(parent)) {
		ret = PTR_ERR(parent);
		goto failed_put_clk;
	}

	ret = clk_set_parent(temp_clk, parent);
	if (ret)
		goto failed_put_both;

	ret = clk_prepare_enable(temp_clk);
	if (ret)
		goto failed_put_both;

	ret = clk_set_rate(parent, 625000);
	if (ret)
		goto failed_disable;

	ret = clk_set_rate(temp_clk, clk_get_rate(parent) >> 2);
	if (ret)
		goto failed_disable;

	clk_put(parent);
	return 0;

failed_disable:
	clk_disable_unprepare(temp_clk);
failed_put_both:
	clk_put(parent);
failed_put_clk:
	clk_put(temp_clk);
failed:
	return ret;
}

static int stih416_temp_exit(struct stm_device_state *state)
{
	clk_disable_unprepare(temp_clk);
	clk_put(temp_clk);

	return 0;
}

static struct platform_device stih416_temp_device[] = {
	[0] = {
		/* Thermal sensor on SAS */
		.name = "stm-temp",
		.id = 0,
		.dev.platform_data = &(struct plat_stm_temp_data) {
			.dcorrect = { SYSCONF(1552), 4, 8 },
			.overflow = { SYSCONF(1594), 8, 8 },
			.data = { SYSCONF(1594), 10, 16 },
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.init = stih416_temp_init,
				.exit = stih416_temp_exit,
				.power = stih416_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYSCONF(SYSCONF(1552),
						9, 9, "TEMP_PWR"),
				},
			}
		},
	},
};

/*
 * FDMA resources --------------------------------
 */
static struct stm_plat_fdma_fw_regs stih416_fdma_fw = {
	.rev_id		= 0x10000,
	.cmd_statn	= 0x10200,
	.req_ctln	= 0x10240,
	.ptrn		= 0x10800,
	.cntn		= 0x10808,
	.saddrn		= 0x1080c,
	.daddrn		= 0x10810,
	.node_size	= 128,
};

static struct stm_plat_fdma_hw stih416_fdma_hw = {
	.slim_regs = {
		.id	  = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver	  = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en	  = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset	  = 0x10000,
		.size	  = 0xc00 << 2, /* 3072 * 4 = 12K */
	},
	.periph_regs = {
		.sync_reg = 0x17f88,
		.cmd_sta  = 0x17fc0,
		.cmd_set  = 0x17fc4,
		.cmd_clr  = 0x17fc8,
		.cmd_mask = 0x17fcc,
		.int_sta  = 0x17fd0,
		.int_set  = 0x17fd4,
		.int_clr  = 0x17fd8,
		.int_mask = 0x17fdc,
	},
	.imem = {
		.offset	  = 0x18000,
		.size	  = 0x1800 << 2, /* 6144 * 4 = 24K (18K populated) */
	},
};

static struct stm_plat_fdma_data stih416_fdma_platform_data = {
	.hw = &stih416_fdma_hw,
	.fw = &stih416_fdma_fw,
	.xbar = 0,
};

#define STM_FDMA(_id, _iomem, _irq, _pd)				\
	{								\
		.name = "stm-fdma",					\
		.id = (_id),						\
		.num_resources = 2,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM((_iomem), 0x20000),	\
			STIH416_RESOURCE_IRQ(_irq),			\
		},							\
		.dev.platform_data = (_pd),				\
	}

#define STM_FDMA_MPE(_id, _iomem, _irq)	\
		STM_FDMA(_id, _iomem, _irq, &stih416_fdma_platform_data)

static struct platform_device stih416_mpe_fdma_devices[] = {
	STM_FDMA_MPE(0, 0xFD600000, 10),
	STM_FDMA_MPE(1, 0xFD620000, 12),
	STM_FDMA_MPE(2, 0xFD640000, 14)
};

static struct platform_device stih416_mpe_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xFD6DF000, 0x1000),
	},
	.dev.platform_data = &(struct stm_plat_fdma_xbar_data) {
		.first_fdma_id = 0,
		.last_fdma_id = 2,
	},
};

static struct stm_plat_fdma_data stih416_sas_fdma_platform_data = {
	.hw = &stih416_fdma_hw,
	.fw = &stih416_fdma_fw,
	.xbar = 1,
};

#define STM_FDMA_SAS(_id, _iomem, _irq)	\
		STM_FDMA(_id, _iomem, _irq, &stih416_sas_fdma_platform_data)

static struct platform_device stih416_sas_fdma_devices[2] = {
	STM_FDMA_SAS(3, 0xFEA00000, 121),
	STM_FDMA_SAS(4, 0xFEA20000, 124)
};

static struct platform_device stih416_sas_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = 1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xFEE61000, 0x1000),
	},
	.dev.platform_data = &(struct stm_plat_fdma_xbar_data) {
		.first_fdma_id = 3,
		.last_fdma_id = 4, /* what about TVOUT_FDMA */
	},
};

/* Hardware RNG driver */
static struct platform_device stih416_devhwrandom_devices[] = {
	{
	.name		= "stm-hwrandom", /* RNG_0 */
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource []) {
		STM_PLAT_RESOURCE_MEM(0xFEE80000, 0x1000),
		},
	},
};

/* System Trace Module resources------------------------------------------- */

static struct stm_pad_config stih416_systrace_pad_config = {
	.gpios_num = 5,
	.gpios = (struct stm_pad_gpio[]){
		 STIH416_PAD_PIO_OUT(101, 3, 4),	/* DATA0 */
		 STIH416_PAD_PIO_OUT(101, 4, 4),	/* DATA1 */
		 STIH416_PAD_PIO_OUT(101, 5, 4),	/* DATA2 */
		 STIH416_PAD_PIO_OUT(101, 6, 4),	/* DATA3 */
		 STIH416_PAD_PIO_OUT(101, 7, 4),	/* BCLK */
		 },
};

static struct platform_device stih416_systrace_device = {
	.name = "stm-systrace",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]){
		/* 8k (4k for each A9 initiator) */
		STM_PLAT_RESOURCE_MEM(MPE42_SYSTRACE_BASE, 0x2000),
		STM_PLAT_RESOURCE_MEM(MPE42_SYSTRACE_REGS, 0x1000),
		},
	.dev.platform_data = &(struct stm_plat_systrace_data) {
		.pad_config = &stih416_systrace_pad_config,
	},
};

/* RTC initialisation -----------------------------------*/
static struct platform_device stih416_lpc_device = {
	.name	= "stm-rtc",
	.id	= -1,
	.num_resources = 2,
	.resource = (struct resource[]){
		STM_PLAT_RESOURCE_MEM(0xfde05000, 0x1000),
		STIH416_RESOURCE_IRQ(118),
	},
	.dev.platform_data = &(struct stm_plat_rtc_lpc) {
		.need_wdt_reset = 1,
#ifdef CONFIG_ARM
		.irq_edge_level = IRQ_TYPE_EDGE_RISING,
#else
		.irq_edge_level = IRQ_TYPE_EDGE_FALLING,
#endif
		/*
		 * the lpc_clk is initialize @ 300 KHz to guarantee
		 * it's working also for the temperature sensor
		 */
		.force_clk_rate = 300000,
	}
};

/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stih416_devices[] __initdata = {
	&stih416_temp_device[0],
	&stih416_devhwrandom_devices[0],
	/* mpe fdmas */
	&stih416_mpe_fdma_devices[0],
	&stih416_mpe_fdma_devices[1],
	&stih416_mpe_fdma_devices[2],
	&stih416_mpe_fdma_xbar_device,
	/* sas fdmas */
	&stih416_sas_fdma_devices[0],
	&stih416_sas_fdma_devices[1],
	&stih416_sas_fdma_xbar_device,

	&stih416_systrace_device,
	&stih416_lpc_device,
};

static int __init stih416_devices_setup(void)
{
	return platform_add_devices(stih416_devices,
			ARRAY_SIZE(stih416_devices));
}
device_initcall(stih416_devices_setup);
