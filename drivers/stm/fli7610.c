
/*
 * (c) 2011 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
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
#include <linux/stm/emi.h>
#include <linux/stm/pad.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/soc.h>
#include <linux/stm/fli7610.h>
#include <linux/stm/fli7610-periphs.h>
#include <linux/stm/clk.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#include <mach/soc-fli7610.h>
#include <mach/hardware.h>
#endif

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#include "pio-control.h"





/* ASC resources
 * 2 UARTS in TAE
 * 2 UARTs in LPM TAE
 */

static struct stm_pad_config fli7610_asc_pad_configs[4] = {

	/* Comms block ASCs in TAE */
	[0] = {
		/* UART0 */
		/* Tx: PIO15[4], Rx: PIO15[3], RTS: PIO15[2], CTS: PIO15[1] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(15, 4, 1),	/* TX */
			STM_PAD_PIO_IN(15, 3, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(15, 2, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(15, 1, 1, "RTS"),
		},
	},
	[1] = {
		/* UART1 goes to smartcard too */
	},

	/* SBC comms block ASCs in TAE */
	[2] = {
		/* SBC_UART0 */
		/* Tx: PIO6[6], Rx: PIO6[5], RTS: PIO6[4], CTS: PIO6[3] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(6, 6, 1),	/* TX */
			STM_PAD_PIO_IN(6, 5, 1),	/* RX */
			STM_PAD_PIO_IN_NAMED(6, 4, 1, "CTS"),
			STM_PAD_PIO_OUT_NAMED(6, 3, 1, "RTS"),
		},
	},
	[3] = {
		/* SBC_UART1 (aka UART11) */
		/* Tx: PIO2[6], Rx: PIO2[7], RTS: PIO3[1], CTS: PIO3[0] */
		/* OE: PIO3[2] */
		.gpios_num = 4,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_OUT(2, 6, 2),	/* TX */
			STM_PAD_PIO_IN(2, 5, 2),	/* RX */
			STM_PAD_PIO_IN_NAMED(3, 4, 2, "CTS"),
			STM_PAD_PIO_OUT_NAMED(3, 3, 2, "RTS"),
		},
	},

};

static struct platform_device fli7610_asc_devices[] = {

	/* Comms block ASCs in TAE */
	/*
	 * Assuming these are UART0 to UART2 in the PIO document.
	 * Note no UART3.
	 * Assuming these are asc_100 to asc_103 in interrupt document
	 */
	[0] = {
		.name = "stm-asc",
		/* .id set in fli7610_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_ASC0_BASE, 0x2c),
			FLI7610_RESOURCE_IRQ(195),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 5),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 6),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7610_asc_pad_configs[0],
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_TAE_ASC0_BASE),
			.clk_id = "comms_clk",
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in fli7610_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_TAE_ASC1_BASE, 0x2c),
			FLI7610_RESOURCE_IRQ(196),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 7),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 8),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7610_asc_pad_configs[1],
			.clk_id = "comms_clk",
		},
	},

	/* SBC comms block ASCs in TAE */
	/*
	 * Assuming these are lpm_uart_0 and lpm_uart_1 in interrupt document.
	 */
	[2] = {
		.name = "stm-asc",
		/* .id set in fli7610_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_ASC0_BASE, 0x2c),
			FLI7610_RESOURCE_IRQ(209),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 21),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 22),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7610_asc_pad_configs[2],
			.regs = (void __iomem *)IO_ADDRESS(
						FLI7610_SBC_ASC0_BASE),
			.clk_id = "sbc_comms_clk",
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in fli7610_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_ASC1_BASE, 0x2c),
			FLI7610_RESOURCE_IRQ(210),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 23),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 24),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &fli7610_asc_pad_configs[3],
			.clk_id = "sbc_comms_clk",
		},
	},

	/*
	 * Note the interrupt document also describes a top level UART
	 * in the MPE41, described simply as asc_0.
	 * Memory map describes a top level UART_0, at 0xFD4FB000.
	 */
};

/* the serial console device */
struct platform_device *stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(fli7610_asc_devices)];

void __init fli7610_configure_asc(int asc, struct fli7610_asc_config *config)
{
	static int configured[ARRAY_SIZE(fli7610_asc_devices)];
	static int tty_id;
	struct fli7610_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(fli7610_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &fli7610_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;
	plat_data->txfifo_bug = 1;
	plat_data->force_m1 = config->force_m1;

	if (!config->hw_flow_control) {
		/* Don't claim RTS/CTS pads */
		struct stm_pad_config *pad_config;
		pad_config = &fli7610_asc_pad_configs[asc];
		stm_pad_set_pio_ignored(pad_config, "RTS");
		stm_pad_set_pio_ignored(pad_config, "CTS");
	}

	if (config->is_console)
		stm_asc_console_device = pdev;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init fli7610_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(fli7610_add_asc);



/* PIO ports resources ---------------------------------------------------- */

static int fli7610_pio_pin_name(char *name, int size, int port, int pin)
{
	if (port >= FLI7610_PIO(100))
		port += 100-FLI7610_PIO(100);
	return snprintf(name, size, "PIO%d.%d", port, pin);
}

#define FLI7610_PIO_ENTRY(_num, _base)					\
	[_num] = {							\
		.name = "stm-gpio",					\
		.id = _num,						\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM(_base, 0x100),		\
		},							\
		.dev.platform_data = &(struct stm_plat_pio_data) {	\
			.regs = (void __iomem *)IO_ADDRESS(_base),	\
			.pin_name = fli7610_pio_pin_name,		\
		},							\
	}

static struct platform_device fli7610_pio_devices[34] = {
	/* 0-25 TAE PIO */
	FLI7610_PIO_ENTRY(0, 0xfe610000),
	FLI7610_PIO_ENTRY(1, 0xfe611000),
	FLI7610_PIO_ENTRY(2, 0xfe612000),
	FLI7610_PIO_ENTRY(3, 0xfe613000),
	FLI7610_PIO_ENTRY(4, 0xfe614000),
	FLI7610_PIO_ENTRY(5, 0xfe615000),
	FLI7610_PIO_ENTRY(6, 0xfe616000),
	FLI7610_PIO_ENTRY(7, 0xfe617000),
	FLI7610_PIO_ENTRY(8, 0xfe618000),
	FLI7610_PIO_ENTRY(9, 0xfe619000),
	FLI7610_PIO_ENTRY(10, 0xfee00000),
	FLI7610_PIO_ENTRY(11, 0xfee01000),
	FLI7610_PIO_ENTRY(12, 0xfee02000),
	FLI7610_PIO_ENTRY(13, 0xfee03000),
	FLI7610_PIO_ENTRY(14, 0xfee04000),
	FLI7610_PIO_ENTRY(15, 0xfee05000),
	FLI7610_PIO_ENTRY(16, 0xfee06000),
	FLI7610_PIO_ENTRY(17, 0xfee30000),
	FLI7610_PIO_ENTRY(18, 0xfee31000),
	FLI7610_PIO_ENTRY(19, 0xfee32000),
	FLI7610_PIO_ENTRY(20, 0xfee33000),
	FLI7610_PIO_ENTRY(21, 0xfee40000),
	FLI7610_PIO_ENTRY(22, 0xfee41000),
	FLI7610_PIO_ENTRY(23, 0xfee42000),
	FLI7610_PIO_ENTRY(24, 0xfee43000),
	FLI7610_PIO_ENTRY(25, 0xfee44000),

	/* MPE */
	/* 100-102: PIO_RIGHT (aka MPE_PIO) */
	FLI7610_PIO_ENTRY(26, MPE41_PIO_RIGHT_BASE + 0x0000),
	FLI7610_PIO_ENTRY(27, MPE41_PIO_RIGHT_BASE + 0x1000),
	FLI7610_PIO_ENTRY(28, MPE41_PIO_RIGHT_BASE + 0x2000),
	/* 103-107: PIO_LEFT (aka PIO_1_MPE) */
	FLI7610_PIO_ENTRY(29, MPE41_PIO_LEFT_BASE + 0x0000),
	FLI7610_PIO_ENTRY(30, MPE41_PIO_LEFT_BASE + 0x1000),
	FLI7610_PIO_ENTRY(31, MPE41_PIO_LEFT_BASE + 0x2000),
	FLI7610_PIO_ENTRY(32, MPE41_PIO_LEFT_BASE + 0x3000),
	FLI7610_PIO_ENTRY(33, MPE41_PIO_LEFT_BASE + 0x4000),
};

/* Interrupts
PIO_RIGHT: 113
PIO_LEFT 114
PIO_SBC 180
PIO_FRONT 181
PIO_REAR 182
Need to add 32 for A9
*/

static const struct stm_pio_control_retime_offset fli7610_pio_retime_offset = {
	.clk1notclk0_offset	= 0,
	.delay_lsb_offset	= 2,
	.delay_msb_offset	= 3,
	.invertclk_offset	= 4,
	.retime_offset		= 5,
	.clknotdata_offset	= 6,
	.double_edge_offset	= 7,
};

static unsigned int fli7610_pio_control_delays_in[] = {
	0,	/* 00: 0.0ns */
	500,	/* 01: 0.5ns */
	1000,	/* 10: 1.0ns */
	1500,	/* 11: 1.5ns */
};

static unsigned int fli7610_pio_control_delays_out[] = {
	0,	/* 00: 0.0ns */
	1000,	/* 01: 1.0ns */
	2000,	/* 10: 2.0ns */
	3000,	/* 11: 3.0ns */
};

static const struct stm_pio_control_retime_params fli7610_retime_params = {
	.retime_offset = &fli7610_pio_retime_offset,
	.delay_times_in = fli7610_pio_control_delays_in,
	.num_delay_times_in = ARRAY_SIZE(fli7610_pio_control_delays_in),
	.delay_times_out = fli7610_pio_control_delays_out,
	.num_delay_times_out = ARRAY_SIZE(fli7610_pio_control_delays_out),
};

#define FLI7610_PIO_ENTRY_CONTROL(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _lsb, _msb,			\
		_rt)				\
	[_num] = {							\
		.alt = { TAE_SYSCONF(_alt_num) },			\
		.oe = { TAE_SYSCONF(_oe_num), _lsb, _msb },		\
		.pu = { TAE_SYSCONF(_pu_num), _lsb, _msb },		\
		.od = { TAE_SYSCONF(_od_num), _lsb, _msb },		\
		.retime_style = stm_pio_control_retime_style_packed,	\
		.retime_pin_mask = 0xff,				\
		.retime_params = &fli7610_retime_params,		\
		.retiming = {						\
			{ TAE_SYSCONF(_rt) },				\
			{ TAE_SYSCONF(_rt+1) }				\
		},							\
	}

#define FLI7610_PIO_ENTRY_CONTROL4(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _rt)				\
	FLI7610_PIO_ENTRY_CONTROL(_num,   _alt_num,			\
		_oe_num, _pu_num, _od_num,  0,  7,			\
		_rt),							\
	FLI7610_PIO_ENTRY_CONTROL(_num+1, _alt_num+1,			\
		_oe_num, _pu_num, _od_num,  8, 15,			\
		_rt+2),							\
	FLI7610_PIO_ENTRY_CONTROL(_num+2, _alt_num+2,			\
		_oe_num, _pu_num, _od_num, 16, 23,			\
		_rt+4),							\
	FLI7610_PIO_ENTRY_CONTROL(_num+3, _alt_num+3,			\
		_oe_num, _pu_num, _od_num, 24, 31,			\
		_rt+6)


#define FLI7610_PIO_ENTRY_CONTROL_NO_RET(_num, _alt_num,		\
		_oe_num, _pu_num, _od_num, _lsb, _msb)			\
	[_num] = {							\
		.alt = { TAE_SYSCONF(_alt_num) },			\
		.oe = { TAE_SYSCONF(_oe_num), _lsb, _msb },		\
		.pu = { TAE_SYSCONF(_pu_num), _lsb, _msb },		\
		.od = { TAE_SYSCONF(_od_num), _lsb, _msb },		\
		.retime_style = stm_pio_control_retime_style_none,	\
	}

#define FLI7610_PIO_ENTRY_CONTROL4_NO_RET(_num, _alt_num,		\
		_oe_num, _pu_num, _od_num)				\
	FLI7610_PIO_ENTRY_CONTROL_NO_RET(_num,   _alt_num,		\
		_oe_num, _pu_num, _od_num,  0,  7),			\
	FLI7610_PIO_ENTRY_CONTROL_NO_RET(_num+1, _alt_num+1,		\
		_oe_num, _pu_num, _od_num,  8, 15),			\
	FLI7610_PIO_ENTRY_CONTROL_NO_RET(_num+2, _alt_num+2,		\
		_oe_num, _pu_num, _od_num, 16, 23),			\
	FLI7610_PIO_ENTRY_CONTROL_NO_RET(_num+3, _alt_num+3,		\
		_oe_num, _pu_num, _od_num, 24, 31)

#define FLI7610_MPE_PIO_ENTRY_CONTROL(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _lsb, _msb,			\
		_rt)							\
	[_num] = {							\
		.alt = { MPE_SYSCONF(_alt_num) },			\
		.oe = { MPE_SYSCONF(_oe_num), _lsb, _msb },		\
		.pu = { MPE_SYSCONF(_pu_num), _lsb, _msb },		\
		.od = { MPE_SYSCONF(_od_num), _lsb, _msb },		\
		.retime_style = stm_pio_control_retime_style_packed,	\
		.retime_pin_mask = 0xff,				\
		.retiming = {						\
			{ MPE_SYSCONF(_rt) },				\
			{ MPE_SYSCONF(_rt+1) }				\
		},							\
	}

#define FLI7610_MPE_PIO_ENTRY_CONTROL4(_num, _alt_num,			\
		_oe_num, _pu_num, _od_num, _rt)				\
	FLI7610_MPE_PIO_ENTRY_CONTROL(_num,   _alt_num,			\
		_oe_num, _pu_num, _od_num,  0,  7,			\
		_rt),							\
	FLI7610_MPE_PIO_ENTRY_CONTROL(_num+1, _alt_num+1,		\
		_oe_num, _pu_num, _od_num,  8, 15,			\
		_rt+2),							\
	FLI7610_MPE_PIO_ENTRY_CONTROL(_num+2, _alt_num+2,		\
		_oe_num, _pu_num, _od_num, 16, 23,			\
		_rt+4),							\
	FLI7610_MPE_PIO_ENTRY_CONTROL(_num+3, _alt_num+3,		\
		_oe_num, _pu_num, _od_num, 24, 31,			\
		_rt+6)
/*
 * NOTE: PIO Banks 9, 12, 20 have less then 8 pins.
 * Currently we are ignoring this fact and configuring as if
 * they have 8 pins. Should not have any side effect.
 * PIO0, 1, 2
 */
static const struct stm_pio_control_config fli7610_pio_control_configs[34] = {
	/*	          		 pio, alt,  oe,  pu,  od,lsb,msb, rt */
	/* PIO_0-3 in SYS_CONF_BANK 0 */
	FLI7610_PIO_ENTRY_CONTROL4	 (0,   0,   10,  13,  16, 50),
	FLI7610_PIO_ENTRY_CONTROL4_NO_RET(4,   4,   11,  14,  17),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (8,   8,   12,  15,  18, 0, 7),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (9,   9,   12,  15,  18, 8, 15),

	/* PIO_10-16 in SYS_CONF_BANK 1 */
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (10, 100, 107, 109, 111,  0,  7),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (11, 101, 107, 109, 111,  8, 15),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (12, 102, 107, 109, 111, 16, 23),
	FLI7610_PIO_ENTRY_CONTROL	 (13, 103, 107, 109, 111, 24, 31, 150),
	FLI7610_PIO_ENTRY_CONTROL	 (14, 104, 108, 110, 112,  0,  7, 152),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (15, 105, 108, 110, 112,  8, 15),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (16, 106, 108, 110, 112, 16, 23),

	/* PIO_17-20 in SYS_CONF_BANK 2 */
	FLI7610_PIO_ENTRY_CONTROL	 (17, 200, 204, 206, 208,  0,  7, 250),
	FLI7610_PIO_ENTRY_CONTROL	 (18, 201, 204, 206, 208,  8, 15, 252),
	FLI7610_PIO_ENTRY_CONTROL	 (19, 202, 204, 206, 208, 16, 23, 254),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (20, 203, 204, 206, 208, 24, 31),

	/* PIO_ 21-25 in SYS_CONF_BANK 3 */
	FLI7610_PIO_ENTRY_CONTROL4_NO_RET(21, 300, 305, 307, 309),
	FLI7610_PIO_ENTRY_CONTROL_NO_RET (25, 304, 306, 308, 310, 0, 7),

	/* 100-102: MPE_PIO */
	FLI7610_MPE_PIO_ENTRY_CONTROL	(26, 400, 403, 404, 405,  0,  7, 406),
	FLI7610_MPE_PIO_ENTRY_CONTROL	(27, 401, 403, 404, 405,  8, 15, 408),
	FLI7610_MPE_PIO_ENTRY_CONTROL	(28, 402, 403, 404, 405, 16, 23, 410),
	/* 103-107: PIO_1_MPE */
	FLI7610_MPE_PIO_ENTRY_CONTROL4	(29, 500, 505, 507, 509,         511),
	FLI7610_MPE_PIO_ENTRY_CONTROL	(33, 504, 506, 508, 510,  0,  7, 519),

};

static struct stm_pio_control fli7610_pio_controls[34];

static int fli7610_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	struct stm_pio_control_pad_config *config = priv;

	return stm_pio_control_config_all(gpio, direction, function, config,
		fli7610_pio_controls,
		ARRAY_SIZE(fli7610_pio_controls), 8);
}

#ifdef CONFIG_DEBUG_FS
static void fli7610_pio_report(unsigned gpio, char *buf, int len)
{
	stm_pio_control_report_all(gpio, fli7610_pio_controls,
		buf, len);
}
#else
#define fli7610_pio_report NULL
#endif

static const struct stm_pad_ops fli7610_pad_ops = {
	.gpio_config = fli7610_pio_config,
	.gpio_report = fli7610_pio_report,
};

static void __init fli7610_pio_init(void)
{
	BUG_ON(ARRAY_SIZE(fli7610_pio_devices) !=
	       ARRAY_SIZE(fli7610_pio_control_configs));
	BUG_ON(ARRAY_SIZE(fli7610_pio_devices) !=
	       ARRAY_SIZE(fli7610_pio_controls));

	stm_pio_control_init(fli7610_pio_control_configs, fli7610_pio_controls,
			     ARRAY_SIZE(fli7610_pio_control_configs));
}

/* EMI resources ---------------------------------------------------------- */
/* PM support NOT TESTED */
static void fli7610_emi_power(struct stm_device_state *device_state,
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

static struct platform_device fli7610_emi = {
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
			STM_DEVICE_SYSCONF(TAE_SYSCONF(364), 0, 0, "EMI_PWR"),
			STM_DEVICE_SYSCONF(TAE_SYSCONF(368), 0, 0, "EMI_ACK"),
		},
		.power = fli7610_emi_power,
	},
};

/* NAND Resources --------------------------------------------------------- */
static struct stm_plat_nand_flex_data fli7610_nand_flex_data;
static struct stm_plat_nand_bch_data fli7610_nand_bch_data;

static struct platform_device fli7610_nandi_device = {
	.num_resources          = 3,
	.resource               = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("nand_mem", 0xFE901000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("nand_dma", 0xFEF00800, 0x0800),
		FLI7610_RESOURCE_IRQ_NAMED("nand_irq", 146),
	},
};

void __init fli7610_configure_nand(struct stm_nand_config *config)
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
		flex_data = &fli7610_nand_flex_data;
		fli7610_nandi_device.dev.platform_data = flex_data;
		flex_data->nr_banks = config->nr_banks;
		flex_data->banks = config->banks;
		flex_data->flex_rbn_connected = config->rbn.flex_connected;
		fli7610_nandi_device.name = (config->driver == stm_nand_afm) ?
					"stm-nand-afm" : "stm-nand-flex";
		platform_device_register(&fli7610_nandi_device);
		break;
	case stm_nand_bch:
		BUG_ON(config->nr_banks > 1);
		bch_data = &fli7610_nand_bch_data;
		fli7610_nandi_device.dev.platform_data = bch_data;
		bch_data->bank = config->banks;
		bch_data->bch_ecc_cfg = config->bch_ecc_cfg;
		fli7610_nandi_device.name = "stm-nand-bch";
		platform_device_register(&fli7610_nandi_device);
		break;
	default:
		BUG();
		return;
	}
}

/* SPI FSM Resources ------------------------------------------------------ */
static struct stm_pad_config fli7610_spifsm_pad_config = {
	.gpios_num = 6,
	.gpios = (struct stm_pad_gpio[]) {
		STM_PAD_PIO_OUT_NAMED(23, 3, 1, "spi-fsm-clk"),
		STM_PAD_PIO_OUT_NAMED(23, 5, 1, "spi-fsm-cs"),
		/* To support QUAD mode operations, each of the following pads
		 * may be used by the IP as an input or an output.  Here we
		 * specify either PIO_OUT or PIO_IN, which sets pu = 0 && od =
		 * 0. 'oe' is taken from a signal generated by the SPI-FSM IP
		 * itself.
		 */
		STM_PAD_PIO_OUT_NAMED(23, 4, 1, "spi-fsm-mosi"),
		STM_PAD_PIO_IN_NAMED(23, 6, 1, "spi-fsm-miso"),
		STM_PAD_PIO_OUT_NAMED(23, 7, 1, "spi-fsm-hold"),
		STM_PAD_PIO_OUT_NAMED(24, 8, 1, "spi-fsm-wp"),
	}
};

static struct platform_device fli7610_spifsm_device = {
	.name		= "stm-spi-fsm",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("spi-fsm", 0xFE902000, 0x1000),
	},
};

void __init fli7610_configure_spifsm(struct stm_plat_spifsm_data *data)
{
	struct sysconf_field *sc;

	fli7610_spifsm_device.dev.platform_data = data;

	data->pads = &fli7610_spifsm_pad_config;

	sc = sysconf_claim(TAE_SYSCONF(381), 0, 4, "mode-pins");

	/* SoC/IP Capabilities */
	data->capabilities.no_read_repeat = 1;
	data->capabilities.no_write_repeat = 1;
	data->capabilities.read_status_bug = spifsm_read_status_clkdiv4;
	data->capabilities.boot_from_spi = (sysconf_read(sc) == 0x1a) ? 1 : 0;

	sysconf_release(sc);
	platform_device_register(&fli7610_spifsm_device);
}


/* FDMA resources --------------------------------------------------------- */

static struct stm_plat_fdma_fw_regs fli7610_fdma_fw = {
	.rev_id    = 0x10000,
	.cmd_statn = 0x10200,
	.req_ctln  = 0x10240,
	.ptrn      = 0x10800,
	.cntn      = 0x10808,
	.saddrn    = 0x1080c,
	.daddrn    = 0x10810,
	.node_size = 128,
};

static struct stm_plat_fdma_hw fli7610_fdma_hw = {
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

static struct stm_plat_fdma_data fli7610_mpe_fdma_platform_data = {
	.hw = &fli7610_fdma_hw,
	.fw = &fli7610_fdma_fw,
	.xbar = 0,
};

static struct platform_device fli7610_mpe_fdma_devices[] = {
	{
		/* FDMA_0_MPE: */
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd600000, 0x20000),
			FLI7610_RESOURCE_IRQ(10),
		},
		.dev.platform_data = &fli7610_mpe_fdma_platform_data,
	}, {
		/* FDMA_1_MPE: */
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd620000, 0x20000),
			FLI7610_RESOURCE_IRQ(18),
		},
		.dev.platform_data = &fli7610_mpe_fdma_platform_data,
	}, {
		/* FDMA_2_MPE: */
		.name = "stm-fdma",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd640000, 0x20000),
			FLI7610_RESOURCE_IRQ(26),
		},
		.dev.platform_data = &fli7610_mpe_fdma_platform_data,
	}
};

/* FDMA_MUX_MPE: 96 way */
static struct platform_device fli7610_mpe_fdma_xbar_device = {
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

/* TAE_FDMA  */

static struct stm_plat_fdma_data fli7610_tae_fdma_platform_data = {
	.hw = &fli7610_fdma_hw,
	.fw = &fli7610_fdma_fw,
	.xbar = 1,
};

static struct platform_device fli7610_tae_fdma_devices[] = {
	{
		/* FDMA_100: TAE FDMA 0 */
		.name = "stm-fdma",
		.id = 3,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfea00000, 0x20000),
			FLI7610_RESOURCE_IRQ(121),
		},
		.dev.platform_data = &fli7610_tae_fdma_platform_data,
	}, {
		/* FDMA_101: TAE FDMA 1 */
		.name = "stm-fdma",
		.id = 4,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfea20000, 0x20000),
			FLI7610_RESOURCE_IRQ(129),
		},
		.dev.platform_data = &fli7610_tae_fdma_platform_data,
	}
};

/* FDMA_MUX_TAE: 96 way */
static struct platform_device fli7610_tae_fdma_xbar_device = {
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

/* NOTE: SYSCONFIG0
 * */
/* sysconf resources ------------------------------------------------------ */

static int fli7610_sysconf_reg_name_tae(char *name, int size,
					int group, int num)
{
	int start = (group == 4) ? 450 : (group * 100);

	return snprintf(name, size, "TAE_SYSCONF%d", start + num);
}

static int fli7610_sysconf_reg_name_mpe(char *name, int size,
					int group, int num)
{
	return snprintf(name, size, "MPE_SYSCONF%d", ((group-1) * 100) + num);
}

static struct platform_device fli7610_sysconf_devices[] = {
	/* TAE */
	{
		/* CONFIG 0-83 */
		/* Stand-By Controler System configuration registers */
		.name		= "sysconf",
		.id		= 0,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_SYSCONF_BASE, 0x150),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
						FLI7610_SBC_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 0,
					.offset = 0,
					.name = "SYSCFG_TAE_BANK0",
					.reg_name =
						fli7610_sysconf_reg_name_tae,
				}
			},
		}
	}, {
		/* CONFIG 100-172 */
		/* TAE System configuration registers */
		.name		= "sysconf",
		.id		= 1,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				FLI7610_TAE_SYSCONF_BANK1_BASE, 0x124),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_TAE_SYSCONF_BANK1_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 1,
					.offset = 0,
					.name = "SYSCFG_TAE_BANK1",
					.reg_name =
						fli7610_sysconf_reg_name_tae,
				}
			},
		}
	}, {
		/* SYSCONFIG 200-269 */
		/* TAE System configuration registers */
		.name		= "sysconf",
		.id		= 2,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				FLI7610_TAE_SYSCONF_BANK2_BASE, 0x118),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_TAE_SYSCONF_BANK2_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 2,
					.offset = 0,
					.name = "SYSCFG_TAE_BANK2",
					.reg_name =
						fli7610_sysconf_reg_name_tae,
				}
			},
		}
	}, {
		/* SYSCONFIG 300-445 */
		/* TAE System configuration registers */
		.name		= "sysconf",
		.id		= 3,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				FLI7610_TAE_SYSCONF_BANK3_BASE, 0x248),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_TAE_SYSCONF_BANK3_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 3,
					.offset = 0,
					.name = "SYSCFG_TAE_BANK3",
					.reg_name =
						fli7610_sysconf_reg_name_tae,
				}
			},
		}
	}, {
		/* SYSCONFIG 450-473 */
		/* TAE System configuration registers */
		.name		= "sysconf",
		.id		= 4,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
					FLI7610_TAE_SYSCONF_BANK4_BASE + 0xc8
					, 0x128 - 0xc8),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_TAE_SYSCONF_BANK4_BASE + 0xc8),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 4,
					.offset = 0,
					.name = "SYSCFG_TAE_BANK4",
					.reg_name =
						fli7610_sysconf_reg_name_tae,
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
		.id		= 5,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				MPE41_LEFT_SYSCONF_BASE, 0x78),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					MPE41_LEFT_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 5,
					.offset = 0,
					.name = "SYSCFG_MPE_LEFT",
					.reg_name =
						fli7610_sysconf_reg_name_mpe,
				}
			},
		}
	}, {
		/* SYSCONFIG 500-573, SYSSTATUS 574-595 */
		/* SYSCFG_RIGHT (aka SYSCFG_0_MPE, SYSCFG_VIDEO) */
		/* MPE System configuration registers 0 */
		.name		= "sysconf",
		.id		= 6,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				MPE41_RIGHT_SYSCONF_BASE, 0x180),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					MPE41_RIGHT_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 6,
					.offset = 0,
					.name = "SYSCFG_MPE_RIGHT",
					.reg_name =
						fli7610_sysconf_reg_name_mpe,
				}
			},
		}
	}, {
		/* SYSCONFIG 600-661, SYSSTATUS 662-686 */
		/* SYSCFG_SYSTEM (aka SYSCFG_3_MPE, SYSCFG_CPU) */
		/* MPE System configuration registers 3 */
		.name		= "sysconf",
		.id		= 7,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				MPE41_SYSTEM_SYSCONF_BASE, 0x15c),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					MPE41_SYSTEM_SYSCONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 7,
					.offset = 0,
					.name = "SYSCFG_MPE_SYSTEM",
					.reg_name =
						fli7610_sysconf_reg_name_mpe,
				}
			},
		}
	}, {
		/* LPM CONFIG and Status Registers */
		.name		= "sysconf",
		.id		= 8,
		.num_resources	= 1,
		.resource	= (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(FLI7610_SBC_LPM_CONF_BASE, 0x54),
		},
		.dev.platform_data = &(struct stm_plat_sysconf_data) {
			.regs = (void __iomem *)IO_ADDRESS(
					FLI7610_SBC_LPM_CONF_BASE),
			.groups_num = 1,
			.groups = (struct stm_plat_sysconf_group []) {
				{
					.group = 8,
					.offset = 0,
					.name = "LPM_CFG_REGS",
				}
			},
		}
	},
};

void fli7610_reset(char mode, const char *cmd)
{
	struct sysconf_field *sc = sysconf_claim(TAE_SYSCONF(67),
						0, 0, "LPM_SW_RST_N");
	sysconf_write(sc, 0);
}

/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init fli7610_early_device_init(void)
{
	struct sysconf_field *sc;

	/* Initialise PIO and sysconf drivers */
	sysconf_early_init(fli7610_sysconf_devices,
			   ARRAY_SIZE(fli7610_sysconf_devices));
	fli7610_pio_init();
	stm_gpio_early_init(fli7610_pio_devices,
			ARRAY_SIZE(fli7610_pio_devices),
#ifdef CONFIG_ARM
			256
#else
			ILC_FIRST_IRQ + ILC_NR_IRQS
#endif
		);
	stm_pad_init(ARRAY_SIZE(fli7610_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, &fli7610_pad_ops);

	sc = sysconf_claim(MPE_SYSCONF(427), 0, 31, "devid");
	stm_soc_set(sysconf_read(sc), -1, -1);
}

/* Pre-arch initialisation ------------------------------------------------ */
static int __init fli7610_postcore_setup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fli7610_pio_devices); i++)
		platform_device_register(&fli7610_pio_devices[i]);

	return platform_device_register(&fli7610_emi);
}
postcore_initcall(fli7610_postcore_setup);


/* Late initialisation ---------------------------------------------------- */
static struct platform_device *fli7610_devices[] __initdata = {
	&fli7610_mpe_fdma_devices[0],
	&fli7610_mpe_fdma_devices[1],
	&fli7610_mpe_fdma_devices[2],
	&fli7610_mpe_fdma_xbar_device,
	&fli7610_tae_fdma_devices[0],
	&fli7610_tae_fdma_devices[1],
	&fli7610_tae_fdma_xbar_device,
};

static int __init fli7610_devices_setup(void)
{
	return platform_add_devices(fli7610_devices,
			ARRAY_SIZE(fli7610_devices));
}
device_initcall(fli7610_devices_setup);
