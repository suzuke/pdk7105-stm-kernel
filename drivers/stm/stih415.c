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
#include <linux/stm/emi.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih415.h>

#include <asm/mach/map.h>

#include <mach/soc-stih415.h>
#include <mach/hardware.h>

/* Currently STM_PLAT_RESOURCE_IRQ only works for SH4 and ST200 */
#undef STM_PLAT_RESOURCE_IRQ
#define STM_PLAT_RESOURCE_IRQ(_irq) \
	{ \
		.start = (_irq), \
		.end = (_irq), \
		.flags = IORESOURCE_IRQ, \
	}


/* ASC resources ---------------------------------------------------------- */

static struct stm_pad_config stih415_asc_pad_config[6] = {

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
	},
	[3] = {
		/* UART3 - not wired to pins */
	},

	/* SBC comms block ASCs in SASG1 */
	[4] = {
		/* UART10 */
		/* Tx: PIO3[4], Rx: PIO3[5], RTS: PIO3[7], CTS: PIO3[6] */
	},
	[5] = {
		/* UART11 */
		/* Tx: PIO2[6], Rx: PIO2[7], RTS: PIO3[0], CTS: PIO3[1] */
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
			STM_PLAT_RESOURCE_IRQ(195+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[0],
			.regs = (void __iomem *)IO_ADDRESS(STIH415_ASC0_BASE),
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC1_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(196+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC2_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(197+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[2],
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_ASC3_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(198+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[3],
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
			STM_PLAT_RESOURCE_IRQ(209+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[4],
		},
	},
#if 0
	[5] = {
		.name = "stm-asc",
		/* .id set in stih415_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_SBC_ASC1_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(210+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &stih415_asc_pad_config[5],
		},
	},
#endif

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
	plat_data->force_m1 = config->force_m1;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

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

static struct platform_device stih415_pio_devices[] = {
	/* MPE PIO block */
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_PIO_MPE_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_PIO_MPE_BASE),
		},
	},
	/* SAS rear PIO block */
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_PIO_SAS_REAR_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_PIO_SAS_REAR_BASE),
		},
	},
	/* SAS front PIO block */
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(STIH415_PIO_SAS_FRONT_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(STIH415_PIO_SAS_FRONT_BASE),
		},
	},
};

static int stih415_pio_config(unsigned gpio,
                enum stm_pad_gpio_direction direction, int function, void* priv)
{
	switch (direction) {
	case stm_pad_gpio_direction_in:
		BUG_ON(function != -1);
		stm_gpio_direction(gpio, STM_GPIO_DIRECTION_IN);
		break;
	case stm_pad_gpio_direction_out:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_OUT :
				STM_GPIO_DIRECTION_OUT);
		break;
	case stm_pad_gpio_direction_bidir:
		BUG_ON(function < 0);
		BUG_ON(function > 1);
		stm_gpio_direction(gpio, function ?
				STM_GPIO_DIRECTION_ALT_BIDIR :
				STM_GPIO_DIRECTION_BIDIR);
		break;
	default:
		BUG();
		break;
	}

	return 0;
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

static struct stm_plat_fdma_data stih415_fdma_platform_data = {
	.hw = &stih415_fdma_hw,
	.fw = &stih415_fdma_fw,
};

static struct platform_device stih415_mpe_fdma_devices[] = {
	{
		/* FDMA_0_MPE: */
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd600000, 0x20000),
			STM_PLAT_RESOURCE_IRQ(10+32),
		},
		.dev.platform_data = &stih415_fdma_platform_data,
	}, {
		/* FDMA_1_MPE: */
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd620000, 0x20000),
			STM_PLAT_RESOURCE_IRQ(18+32),
		},
		.dev.platform_data = &stih415_fdma_platform_data,
	}, {
		/* FDMA_2_MPE: */
		.name = "stm-fdma",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfd640000, 0x20000),
			STM_PLAT_RESOURCE_IRQ(26+32),
		},
		.dev.platform_data = &stih415_fdma_platform_data,
	}
};

/* FDMA_MUX_MPE: 96 way */
static struct platform_device stih415_mpe_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfd6df000, 0x1000),
	},
};

/* TVOUT_FDMA at 0xfe000000 ??? */

static struct platform_device stih415_sas_fdma_devices[] = {
	{
		/* FDMA_100: SAS FDMA 0 */
		.name = "stm-fdma",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfea00000, 0x20000),
			STM_PLAT_RESOURCE_IRQ(121+32),
		},
		.dev.platform_data = &stih415_fdma_platform_data,
	}, {
		/* FDMA_101: SAS FDMA 1 */
		.name = "stm-fdma",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[2]) {
			STM_PLAT_RESOURCE_MEM(0xfea20000, 0x20000),
			STM_PLAT_RESOURCE_IRQ(129+32),
		},
		.dev.platform_data = &stih415_fdma_platform_data,
	}
};

/* FDMA_MUX_SAS: 64 way */
static struct platform_device stih415_sas_fdma_xbar_device = {
	.name = "stm-fdma-xbar",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xfee61000, 0x1000),
	},
};



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init stih415_early_device_init(void)
{
#if 0
	/* Initialise PIO and sysconf drivers */
	sysconf_early_init(&stih415_sysconf_device, 1);
#endif
	stm_gpio_early_init(stih415_pio_devices,
			ARRAY_SIZE(stih415_pio_devices),
			256);
	stm_pad_init(ARRAY_SIZE(nice_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, nice_pio_config);
}

/* Late initialisation ---------------------------------------------------- */

static struct platform_device *stih415_devices[] __initdata = {
	&stih415_mpe_fdma_devices[0],
	//&stih415_mpe_fdma_devices[1],
	//&stih415_mpe_fdma_devices[2],
	&stih415_mpe_fdma_xbar_device,
};

static int __init stih415_devices_setup(void)
{
	return platform_add_devices(stih415_devices,
			ARRAY_SIZE(stih415_devices));
}
device_initcall(stih415_devices_setup);


/* Horrible hacks! ---------------------------------------------------------*/

void clk_get()
{
	return 0;
}

void clk_put()
{
	return 0;
}

int clk_get_rate()
{
	/* Return correct value for comms clock, so ASC works. */
	return 450000;
}

int clk_enable()
{
	return 0;
}
