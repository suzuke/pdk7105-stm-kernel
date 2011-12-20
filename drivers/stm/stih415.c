/*
 * (c) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/clk.h>
#include <linux/stm/emi.h>
#include <linux/stm/pad.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#include <mach/soc-stih415.h>
#include <mach/hardware.h>
#endif

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#include "pio-control.h"

/* NAND Resources --------------------------------------------------------- */

static struct platform_device stih415_nandi_device = {
	.num_resources          = 2,
	.resource               = (struct resource[]) {
	STM_PLAT_RESOURCE_MEM_NAMED("flex_mem", 0xFE901000, 0x1000),
		STIH415_RESOURCE_IRQ(146),
	},
	.dev.platform_data      = &(struct stm_plat_nand_flex_data) {
	},
};

void __init stih415_configure_nand(struct stm_nand_config *config)
{
	struct stm_plat_nand_flex_data *flex_data;
	struct stm_plat_nand_emi_data *emi_data;

	switch (config->driver) {
	case stm_nand_emi:
		/* Not supported */
		BUG();
		break;
	case stm_nand_flex:
	case stm_nand_afm:
		/* Configure device for stm-nand-flex/afm driver */
		flex_data = stih415_nandi_device.dev.platform_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		stih415_nandi_device.name = (config->driver == stm_nand_afm) ?
					"stm-nand-afm" : "stm-nand-flex";
		platform_device_register(&stih415_nandi_device);
		break;
	default:
		return;
	}
}


/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stih415_asc_pad_configs[6] = {

	/* Comms block ASCs in SASG1 */
	[0] = {
		/* UART0 */
		/* Tx: PIO10[0], Rx: PIO10[1], RTS: PIO10[3], CTS: PIO10[2] */
		/* Plus smartcard signals on PIO10[4] to PIO10[7] */
	},
	[1] = {
		/* UART1 */
		/* Tx: PIO11[0], Rx: PIO11[1], RTS: PIO11[3], CTS: PIO11[2] */
		/* Plus smartcard signals on PIO10[4] to PIO10[7] */
	},
	[2] = {
		/* UART2 */
		/* Tx: PIO17[4], Rx: PIO17[5], RTS: PIO17[7], CTS: PIO17[6] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(17, 4, 2),	/* TX */
			STM_PAD_PIO_IN(17, 5, 2),	/* RX */
			STM_PAD_PIO_IN_NAMED(17, 6, 2, "CTS"),
			STM_PAD_PIO_OUT_NAMED(17, 7, 2, "RTS"),
		},
	},
	[3] = {
		/* UART3 - not wired to pins */
	},

	/* SBC comms block ASCs in SASG1 */
	[4] = {
		/* SBC_UART0 (aka UART10) */
		/* Tx: PIO3[4], Rx: PIO3[5], RTS: PIO3[7], CTS: PIO3[6] */
		/* OE: PIO4[0] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(3, 4, 1),	/* TX */
			STM_PAD_PIO_IN(3, 5, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 6, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 7, 1, "RTS"),
		},
	},
	[5] = {
		/* SBC_UART1 (aka UART11) */
		/* Tx: PIO2[6], Rx: PIO2[7], RTS: PIO3[1], CTS: PIO3[0] */
		/* OE: PIO3[2] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 6, 3),	/* TX */
			STM_PAD_PIO_IN(2, 7, 3),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 0, 3, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 1, 3, "RTS"),
		},
	},

#if 0
	/* MPE41 UART */
	[6] = {
		/* UART100 */
		/* Tx: PIO101[1], Rx: PIO101[2], RTS: PIO101[4], CTS: PIO101[3] */
	},
#endif
};

static struct platform_device stih415_asc_devices[] = {

	/* Comms block ASCs in SASG1 */
	/*
	 * Assuming these are UART0 to UART2 in the PIO document.
	 * Note no UART3.
	 * Assuming these are asc_100 to asc_103 in interrupt document
	 */
	[0] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC0_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(195),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[0],
			.regs = (void __iomem *)IO_ADDRESS(STIH415_ASC0_BASE),
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC1_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(196),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC2_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(197),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[2],
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC3_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(198),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[3],
		},
	},

	/* SBC comms block ASCs in SASG1 */
	/*
	 * Assuming these are UART10 and UART11 in the PIO document.
	 * Assuming these are lpm_uart_0 and lpm_uart_1 in interrupt document.
	 */
	[4] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_ASC0_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(209),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[4],
		},
	},
	[5] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_ASC1_BASE, 0x2c),
			STIH415_RESOURCE_IRQ(210),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_configs[5],
		},
	},

	/*
	 * Note the interrupt document also describes a top level UART
	 * in the MPE41, described simply as asc_0. 
	 * Memory map describes a top level UART_0, at 0xFD4FB000.
	 */
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num = 0;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(stih415_asc_devices)];

void __init stih415_configure_asc(int asc, struct stih415_asc_config *config)
{
	static int configured[ARRAY_SIZE(stih415_asc_devices)];
	static int tty_id;
	struct stih415_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(stih415_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &stih415_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	//plat_data->txfifo_bug = 1;
	plat_data->force_m1 = config->force_m1;

	if (!config->hw_flow_control) {
		/* Don't claim RTS/CTS pads */
		struct stm_pad_config *pad_config;
		pad_config = &stih415_asc_pad_configs[asc];
		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	if (asc > 3)
		clk_add_alias_platform_device(NULL, pdev,
			"sbc_comms_clk", NULL);

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init stih415_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(stih415_add_asc);



/* PIO ports resources ---------------------------------------------------- */

#define STIH415_PIO_ENTRY(_num, _base)					\
	[_num] = {							\
		.name = "stm-gpio",					\
		.id = _num,						\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_base, 0x100),		\
		},							\
		.dev.platform_data = &(struct stm_plat_pio_data) {	\
			.regs = (void __iomem *)IO_ADDRESS(_base),	\
		},							\
	}

static struct platform_device stih415_pio_devices[27] = {
	/* SAS */
	/* NB the datsheet shows these starting at 0xfd611000 */
	/* 0-4: SBC_PIO */
	STIH415_PIO_ENTRY(0, 0xfe610000),
	STIH415_PIO_ENTRY(1, 0xfe611000),
	STIH415_PIO_ENTRY(2, 0xfe612000),
	STIH415_PIO_ENTRY(3, 0xfe613000),
	STIH415_PIO_ENTRY(4, 0xfe614000),
	/* 5-12: PIO_FRONT */
	STIH415_PIO_ENTRY(5, 0xfee00000),
	STIH415_PIO_ENTRY(6, 0xfee01000),
	STIH415_PIO_ENTRY(7, 0xfee02000),
	STIH415_PIO_ENTRY(8, 0xfee03000),
	STIH415_PIO_ENTRY(9, 0xfee04000),
	STIH415_PIO_ENTRY(10, 0xfee05000),
	STIH415_PIO_ENTRY(11, 0xfee06000),
	STIH415_PIO_ENTRY(12, 0xfee07000),
	/* 13-18: PIO_REAR */
	STIH415_PIO_ENTRY(13, 0xfe820000),
	STIH415_PIO_ENTRY(14, 0xfe821000),
	STIH415_PIO_ENTRY(15, 0xfe822000),
	STIH415_PIO_ENTRY(16, 0xfe823000),
	STIH415_PIO_ENTRY(17, 0xfe824000),
	STIH415_PIO_ENTRY(18, 0xfe825000),

	/* MPE */
	/* NB the data sheet has these two reversed, which is correct? */
	/* 100-102: PIO_RIGHT (aka MPE_PIO) */
	STIH415_PIO_ENTRY(19, 0xfd6b0000),
	STIH415_PIO_ENTRY(20, 0xfd6b1000),
	STIH415_PIO_ENTRY(21, 0xfd6b2000),
	/* 103-107: PIO_LEFT (aka PIO_1_MPE) */
	STIH415_PIO_ENTRY(22, 0xfd330000),
	STIH415_PIO_ENTRY(23, 0xfd331000),
	STIH415_PIO_ENTRY(24, 0xfd332000),
	STIH415_PIO_ENTRY(25, 0xfd333000),
	STIH415_PIO_ENTRY(26, 0xfd334000),
};

/* Interrupts
PIO_RIGHT: 113
PIO_LEFT 114
PIO_SBC 180
PIO_FRONT 181
PIO_REAR 182
Need to add 32 for A9
*/

#define STIH415_PIO_ENTRY_CONTROL(_num, _alt_num,				\
		_oe_num, _pu_num, _od_num, _lsb, _msb,			\
		_rt)				\
	[_num] = {							\
		.alt = { SYSCONF(_alt_num) },			\
		.oe = { SYSCONF(_oe_num), _lsb, _msb },			\
		.pu = { SYSCONF(_pu_num), _lsb, _msb },			\
		.od = { SYSCONF(_od_num), _lsb, _msb },			\
		.retiming = {						\
			{ SYSCONF(_rt) },				\
			{ SYSCONF(_rt+1) }				\
		},							\
 	}

#define STIH415_PIO_ENTRY_CONTROL4(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _rt)			\
	STIH415_PIO_ENTRY_CONTROL(_num,   _alt_num,			\
		_oe_num, _pu_num, _od_num,  0,  7,		\
		_rt),					\
	STIH415_PIO_ENTRY_CONTROL(_num+1, _alt_num+1,		\
		_oe_num, _pu_num, _od_num,  8, 15,		\
		_rt+2),					\
	STIH415_PIO_ENTRY_CONTROL(_num+2, _alt_num+2,		\
		_oe_num, _pu_num, _od_num, 16, 23,		\
		_rt+4),					\
	STIH415_PIO_ENTRY_CONTROL(_num+3, _alt_num+3,		\
		_oe_num, _pu_num, _od_num, 24, 31,		\
		_rt+6)

static const struct stm_pio_control_config stih415_pio_control_configs[27] = {
	/*                  pio, alt,  oe,  pu,  od,lsb,msb, rt */
	/* 0-4: SBC */
	STIH415_PIO_ENTRY_CONTROL4( 0,   0,   5,   7,   9,          16),
	STIH415_PIO_ENTRY_CONTROL(  4,   4,   6,   8,  10,  0,  7,  24),
	/* 5-12: SAS_FRONT */
	STIH415_PIO_ENTRY_CONTROL4( 5, 100, 108, 110, 112,         116),
	STIH415_PIO_ENTRY_CONTROL4( 9, 104, 109, 111, 113,         124),
	/* 13-18: SAS_REAR */
	STIH415_PIO_ENTRY_CONTROL4(13, 300, 306, 308, 310,         338),
	STIH415_PIO_ENTRY_CONTROL( 17, 304, 307, 309, 311,  0,  7, 346),
	STIH415_PIO_ENTRY_CONTROL( 18, 305, 307, 309, 311,  8, 15, 348),
	/* 100-102: MPE_PIO */
	STIH415_PIO_ENTRY_CONTROL( 19, 400, 403, 404, 405,  0,  7, 406),
	STIH415_PIO_ENTRY_CONTROL( 20, 401, 403, 404, 405,  8, 15, 408),
	STIH415_PIO_ENTRY_CONTROL( 21, 402, 403, 404, 405, 16, 23, 410),
	/* 103-107: PIO_1_MPE */
	STIH415_PIO_ENTRY_CONTROL4(22, 500, 505, 507, 509,         511),
	STIH415_PIO_ENTRY_CONTROL( 26, 504, 506, 508, 510,  0,  7, 519),
};

static struct stm_pio_control stih415_pio_controls[27];

static int stih415_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void* priv)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stih415_pio_config *config = priv;

	BUG_ON(port > ARRAY_SIZE(stih415_pio_devices));
	BUG_ON(function < 0 || function > 7);

	if (function == 0) {
		switch (direction) {
		case stm_pad_gpio_direction_in:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
			break;
		case stm_pad_gpio_direction_out:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_OUT);
			break;
		case stm_pad_gpio_direction_bidir:
			stm_gpio_direction(gpio, STM_GPIO_DIRECTION_BIDIR);
			break;
		default:
			BUG();
			break;
		}
	} else {
		stm_pio_control_config_direction(port, pin, direction,
				config ? config->mode : NULL);
	}

	stm_pio_control_config_function(port, pin, function);

	if (config && config->retime)
		stm_pio_control_config_retime(port, pin, config->retime);

	return 0;
}

static const struct stm_pio_control_retime_offset stih415_pio_retime_offset = {
	.clk1notclk0_offset 	= 0,
	.delay_lsb_offset	= 2,
	.delay_msb_offset	= 3,
	.invertclk_offset	= 4,
	.retime_offset		= 5,
	.clknotdata_offset	= 6,
	.double_edge_offset	= 7,
};

static void __init stih415_pio_init(void)
{
	stm_pio_control_init(stih415_pio_control_configs, stih415_pio_controls,
			     ARRAY_SIZE(stih415_pio_control_configs),
			     &stih415_pio_retime_offset);
}

/* MMC/SD resources ------------------------------------------------------ */
/* Custom PAD configuration for the MMC Host controller */
#define STIH415_PIO_MMC_CLK_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 4, \
		.name = "MMCCLK", \
		.priv = &(struct stih415_pio_config) {	\
			.mode = &(struct stm_pio_control_mode_config) { \
				.oe = 1, \
				.pu = 1, \
				.od = 1, \
			}, \
			.retime = &(struct stm_pio_control_retime_config) { \
				.retime = 0, \
				.clk1notclk0 = 1, \
				.clknotdata = 1, \
				.double_edge = 0, \
				.invertclk = 0, \
				.delay_input = 0, \
			}, \
		}, \
	}

#define STIH415_PIO_MMC_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 4, \
		.priv = &(struct stih415_pio_config) {	\
			.mode = &(struct stm_pio_control_mode_config) { \
				.oe = 1, \
				.pu = 1, \
				.od = 1, \
			}, \
		}, \
	}
#define STIH415_PIO_MMC_BIDIR(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_custom, \
		.function = 4, \
		.priv = &(struct stih415_pio_config) {	\
			.mode = &(struct stm_pio_control_mode_config) { \
				.oe = 1, \
				.pu = 0, \
				.od = 0, \
			}, \
		}, \
	}
#define STIH415_PIO_MMC_IN(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = 1, \
	}

static struct stm_pad_config stih415_mmc_pad_config = {
	.gpios_num = 14,
	.gpios = (struct stm_pad_gpio []) {
		STIH415_PIO_MMC_CLK_OUT(13, 4),
		STIH415_PIO_MMC_BIDIR(14, 4),	/* MMC Data[0]*/
		STIH415_PIO_MMC_BIDIR(14, 5),	/* MMC Data[1]*/
		STIH415_PIO_MMC_BIDIR(14, 6),	/* MMC Data[2]*/
		STIH415_PIO_MMC_BIDIR(14, 7),	/* MMC Data[3]*/

		STIH415_PIO_MMC_OUT(15, 1),	/* MMC command */
		STIH415_PIO_MMC_IN(15, 3),	/* MMC Write Protection */

		STIH415_PIO_MMC_BIDIR(16, 4),	/* MMC Data[4]*/
		STIH415_PIO_MMC_BIDIR(16, 5),	/* MMC Data[5]*/
		STIH415_PIO_MMC_BIDIR(16, 6),	/* MMC Data[6]*/
		STIH415_PIO_MMC_BIDIR(16, 7),	/* MMC Data[7]*/

		STIH415_PIO_MMC_OUT(17, 1),	/* MMC Card PWR */
		STIH415_PIO_MMC_IN(17, 2),	/* MMC Card Detect */
		STIH415_PIO_MMC_OUT(17, 3),	/* MMC LED on */
	},
};

static int mmc_pad_resources(struct sdhci_host *sdhci)
{
	if (!devm_stm_pad_claim(sdhci->mmc->parent, &stih415_mmc_pad_config,
				dev_name(sdhci->mmc->parent)))
		return -ENODEV;

	return 0;
}

static struct sdhci_pltfm_data stih415_mmc_platform_data = {
	.init = mmc_pad_resources,
	.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
};

static struct platform_device stih415_mmc_device = {
	.name = "sdhci",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfe81e000, 0x1000),
		STIH415_RESOURCE_IRQ_NAMED("mmcirq", 145),
	},
	.dev = {
		.platform_data = &stih415_mmc_platform_data,
	}
};

void __init stih415_configure_mmc(int emmc)
{
	struct sdhci_pltfm_data *plat_data;

	plat_data = &stih415_mmc_platform_data;

	if (unlikely(emmc))
		plat_data->quirks |= SDHCI_QUIRK_NONREMOVABLE_CARD;

	platform_device_register(&stih415_mmc_device);
}



/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs stih415_fdma_fw = {
	.rev_id    = 0x10000,
	.cmd_statn = 0x10200,
	.req_ctln  = 0x10240,
	.ptrn      = 0x10800,
	.cntn      = 0x10808,
	.saddrn    = 0x1080c,
	.daddrn    = 0x10810,
	.node_size = 128,
};

static struct stm_plat_fdma_hw stih415_fdma_hw = {
	.slim_regs = {
		.id       = 0x0000 + (0x000 << 2), /* 0x0000 */
		.ver      = 0x0000 + (0x001 << 2), /* 0x0004 */
		.en       = 0x0000 + (0x002 << 2), /* 0x0008 */
		.clk_gate = 0x0000 + (0x003 << 2), /* 0x000c */
	},
	.dmem = {
		.offset = 0x10000,
		.size   = 0xc00 << 2, /* 3072 * 4 = 12K */
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
		.offset = 0x18000,
		.size   = 0x1800 << 2, /* 6144 * 4 = 24K (18K populated) */
	},
};

static struct stm_plat_fdma_data stih415_mpe_fdma_platform_data = {
	.hw = &stih415_fdma_hw,
	.fw = &stih415_fdma_fw,
	.xbar = 0,
};

static struct platform_device stih415_mpe_fdma_devices[] = {
	{
		/* FDMA_0_MPE: */
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd600000, 0x20000),
			STIH415_RESOURCE_IRQ(10),
		},
		.dev.platform_data = &stih415_mpe_fdma_platform_data,
	}, {
		/* FDMA_1_MPE: */
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd620000, 0x20000),
			STIH415_RESOURCE_IRQ(18),
		},
		.dev.platform_data = &stih415_mpe_fdma_platform_data,
	}, {
		/* FDMA_2_MPE: */
		.name = "stm-fdma",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd640000, 0x20000),
			STIH415_RESOURCE_IRQ(26),
		},
		.dev.platform_data = &stih415_mpe_fdma_platform_data,
	}
};

/* FDMA_MUX_MPE: 96 way */
static struct platform_device stih415_mpe_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd6df000, 0x1000),
	},
	.dev.platform_data = &(struct stm_plat_fdma_xbar_data) {
		.first_fdma_id = 0,
		.last_fdma_id = 2,
	},
};

/* TVOUT_FDMA at 0xfe000000 ??? */

static struct stm_plat_fdma_data stih415_sas_fdma_platform_data = {
	.hw = &stih415_fdma_hw,
	.fw = &stih415_fdma_fw,
	.xbar = 1,
};

static struct platform_device stih415_sas_fdma_devices[] = {
	{
		/* FDMA_100: SAS FDMA 0 */
		.name = "stm-fdma",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfea00000, 0x20000),
			STIH415_RESOURCE_IRQ(121),
		},
		.dev.platform_data = &stih415_sas_fdma_platform_data,
	}, {
		/* FDMA_101: SAS FDMA 1 */
		.name = "stm-fdma",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfea20000, 0x20000),
			STIH415_RESOURCE_IRQ(129),
		},
		.dev.platform_data = &stih415_sas_fdma_platform_data,
	}
};

/* FDMA_MUX_SAS: 64 way */
static struct platform_device stih415_sas_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = 1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfee61000, 0x1000),
	},
	.dev.platform_data = &(struct stm_plat_fdma_xbar_data) {
		.first_fdma_id = 3,
		.last_fdma_id = 4,
	},
};



/* sysconf resources ------------------------------------------------------ */

static struct platform_device stih415_sysconf_devices[] = {
	/* SAS */
	{
		/* CONFIG 0-33, STATUS 34-44 */
		/* SYSCFG_SBC (aka SBC_SYSCFG) */
		/* Stand-By Controler System configuration registers */
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_SYSCONF_BASE, 0xb4),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_SBC_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 0,
					.offset = 0,
					.name = "SYSCFG_SBC",
				}
			},
		}
	}, {
		/* CONFIG 100-185, STATUS 186-200 */
		/* SYSCFG_FRONT (aka SYSCFG_1_SAS) */
		/* SAS System configuration registers */
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SAS_FRONT_SYSCONF_BASE, 0x194),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_SAS_FRONT_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 1,
					.offset = 0,
					.name = "SYSCFG_FRONT",
				}
			},
		}
	}, {
		/* SYSCONFIG 300-383, STATUS 384-399 */
		/* SYSCFG_REAR (aka SYSCFG_2_SAS) */
		/* SAS System configuration registers */
		.name		= "sysconf",
		.id		= 2,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SAS_REAR_SYSCONF_BASE, 0x190),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_SAS_REAR_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 2,
					.offset = 0,
					.name = "SYSCFG_REAR",
				}
			},
		}
	},

	/* MPE */
	{
		/* SYSCONFIG 400-421, SYSSTATUS 423-429 */
		/* Note no 422, but fortunatly there is a hole in addressing */
		/* SYSCFG_LEFT (aka SYSCFG_1_MPE, SYSCFG_TRANSPORT) */
		/* MPE System configuration registers 1 */
		.name		= "sysconf",
		.id		= 3,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_MPE_LEFT_SYSCONF_BASE, 0x78),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_MPE_LEFT_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 3,
					.offset = 0,
					.name = "SYSCFG_LEFT",
				}
			},
		}
	}, {
		/* SYSCONFIG 500-573, SYSSTATUS 574-595 */
		/* SYSCFG_RIGHT (aka SYSCFG_0_MPE, SYSCFG_VIDEO) */
		/* MPE System configuration registers 0 */
		.name		= "sysconf",
		.id		= 4,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_MPE_RIGHT_SYSCONF_BASE, 0x180),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_MPE_RIGHT_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 4,
					.offset = 0,
					.name = "SYSCFG_RIGHT",
				}
			},
		}
	}, {
		/* SYSCONFIG 600-661, SYSSTATUS 662-686 */
		/* SYSCFG_SYSTEM (aka SYSCFG_3_MPE, SYSCFG_CPU) */
		/* MPE System configuration registers 3 */
		.name		= "sysconf",
		.id		= 5,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_MPE_SYSTEM_SYSCONF_BASE, 0x15c),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_MPE_SYSTEM_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 5,
					.offset = 0,
					.name = "SYSCFG_SYSTEM",
				}
			},
		}
	}, {
		/* LPM CONFIG and Status Registers */
		.name		= "sysconf",
		.id		= 6,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_LPM_CONF_BASE, 0x54),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_SBC_LPM_CONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 6,
					.offset = 0,
					.name = "LPM_CFG_REGS",
				}
			},
		}
	},
};
/* Mali resources --------------------------------------------------------- */

static struct platform_device stih415_mali_device = {
	.name = "mali",
	.id = 0,
	.num_resources = 21,
	.resource = (struct resource []) {
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400GP", 0xfd680000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400PP-0", 0xfd688000, 0x10F0),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400PP-1", 0xfd68A000, 0x10F0),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400PP-2", 0xfd68C000, 0x10F0),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400PP-3", 0xfd68E000, 0x10F0),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-1", 0xfd683000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-2", 0xfd684000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-3", 0xfd685000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-4", 0xfd686000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MMU-5", 0xfd687000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("MALI400L2",  0xfd681000, 0x1000),
		STIH415_RESOURCE_IRQ_NAMED("MALI400GP", 80),
		STIH415_RESOURCE_IRQ_NAMED("MALI400PP-0", 78),
		STIH415_RESOURCE_IRQ_NAMED("MALI400PP-1", 82),
		STIH415_RESOURCE_IRQ_NAMED("MALI400PP-2", 83),
		STIH415_RESOURCE_IRQ_NAMED("MALI400PP-3", 84),
		STIH415_RESOURCE_IRQ_NAMED("MMU-1", 81),
		STIH415_RESOURCE_IRQ_NAMED("MMU-2", 79),
		STIH415_RESOURCE_IRQ_NAMED("MMU-3", 85),
		STIH415_RESOURCE_IRQ_NAMED("MMU-4", 86),
		STIH415_RESOURCE_IRQ_NAMED("MMU-5", 87),
	},
};

void stih415_configure_mali(struct stm_mali_config *priv_data)
{
	stih415_mali_device.dev.platform_data = priv_data;
	platform_device_register(&stih415_mali_device);
}


/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stih415_early_device_init(void)
{
	/* Initialise PIO and sysconf drivers */
	sysconf_early_init(stih415_sysconf_devices,
			   ARRAY_SIZE(stih415_sysconf_devices));
	stih415_pio_init();
	stm_gpio_early_init(stih415_pio_devices,
			ARRAY_SIZE(stih415_pio_devices),
#ifdef CONFIG_ARM
			256
#else
			ILC_FIRST_IRQ + ILC_NR_IRQS 
#endif
		);
	stm_pad_init(ARRAY_SIZE(stih415_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, stih415_pio_config);

	/* Version information in SYSTEM_STATUS427 */
}

/* Internal temperature sensor resources ---------------------------------- */
static void stih415_temp_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, "TEMP_PWR", value);
}

static struct platform_device stih415_temp_device[] = {
	[0] = {
		/* Thermal sensor on SAS */
		.name = "stm-temp",
		.id = 0,
		.dev.platform_data = &(struct plat_stm_temp_data) {
			.dcorrect = { SYSCONF(178), 4, 8 },
			.overflow = { SYSCONF(198), 8, 8 },
			.data = { SYSCONF(198), 10, 16 },
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.power = stih415_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYSCONF(SYSCONF(178),
						9, 9, "TEMP_PWR"),
				},
			}
		},
	},
	[1] = {
		/* Thermal sensor on MPE */
		.name = "stm-temp",
		.id = 1,
		.dev.platform_data = &(struct plat_stm_temp_data) {
			.dcorrect = { SYSCONF(607), 3, 7 },
			.overflow = { SYSCONF(667), 9, 9 },
			.data = { SYSCONF(667), 11, 18 },
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 1,
				.power = stih415_temp_power,
				.sysconfs = (struct stm_device_sysconf []){
					STM_DEVICE_SYSCONF(SYSCONF(607),
						8, 8, "TEMP_PWR"),
				},
			}
		},
	}

};


/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stih415_devices[] __initdata = {
	&stih415_mpe_fdma_devices[0],
	&stih415_mpe_fdma_devices[1],
	&stih415_mpe_fdma_devices[2],
	&stih415_mpe_fdma_xbar_device,
	&stih415_sas_fdma_devices[0],
	&stih415_sas_fdma_devices[1],
	&stih415_sas_fdma_xbar_device,
	&stih415_temp_device[0],
	&stih415_temp_device[1],
};

static int __init stih415_devices_setup(void)
{
	return platform_add_devices(stih415_devices,
			ARRAY_SIZE(stih415_devices));
}
device_initcall(stih415_devices_setup);
