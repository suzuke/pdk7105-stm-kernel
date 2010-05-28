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
#include <linux/stm/nice.h>

#include <asm/mach/map.h>

#include <mach/soc-nice.h>
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

static struct stm_pad_config nice_asc_pad_config[4];

static struct platform_device nice_asc_devices[] = {
	[0] = {
		.name = "stm-asc",
		/* .id set in nice_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_ASC0_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(171+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 11),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 15),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &nice_asc_pad_config[0],
			.regs = (void __iomem *)IO_ADDRESS(NICE_ASC0_BASE),
		},
	},
	[1] = {
		.name = "stm-asc",
		/* .id set in nice_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_ASC1_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(172+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 12),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 16),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &nice_asc_pad_config[1],
		},
	},
	[2] = {
		.name = "stm-asc",
		/* .id set in nice_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_ASC2_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(173+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 13),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 17),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &nice_asc_pad_config[2],
		},
	},
	[3] = {
		.name = "stm-asc",
		/* .id set in nice_configure_asc() */
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_ASC3_BASE, 0x2c),
			STM_PLAT_RESOURCE_IRQ(174+32),
			STM_PLAT_RESOURCE_DMA_NAMED("rx_half_full", 14),
			STM_PLAT_RESOURCE_DMA_NAMED("tx_half_empty", 18),
		},
		.dev.platform_data = &(struct stm_plat_asc_data) {
			.pad_config = &nice_asc_pad_config[3],
		},
	},
};

/* Note these three variables are global, and shared with the stasc driver
 * for console bring up prior to platform initialisation. */

/* the serial console device */
int __initdata stm_asc_console_device;

/* Platform devices to register */
unsigned int __initdata stm_asc_configured_devices_num = 0;
struct platform_device __initdata
		*stm_asc_configured_devices[ARRAY_SIZE(nice_asc_devices)];

void __init nice_configure_asc(int asc, struct nice_asc_config *config)
{
	static int configured[ARRAY_SIZE(nice_asc_devices)];
	static int tty_id;
	struct nice_asc_config default_config = {};
	struct platform_device *pdev;
	struct stm_plat_asc_data *plat_data;

	BUG_ON(asc < 0 || asc >= ARRAY_SIZE(nice_asc_devices));

	BUG_ON(configured[asc]);
	configured[asc] = 1;

	if (!config)
		config = &default_config;

	pdev = &nice_asc_devices[asc];
	plat_data = pdev->dev.platform_data;

	pdev->id = tty_id++;
	plat_data->hw_flow_control = config->hw_flow_control;

	if (config->is_console)
		stm_asc_console_device = pdev->id;

	stm_asc_configured_devices[stm_asc_configured_devices_num++] = pdev;
}

/* Add platform device as configured by board specific code */
static int __init nice_add_asc(void)
{
	return platform_add_devices(stm_asc_configured_devices,
			stm_asc_configured_devices_num);
}
arch_initcall(nice_add_asc);



/* PIO ports resources ---------------------------------------------------- */

static struct platform_device nice_pio_devices[] = {
	/* MPE PIO block */
	[0] = {
		.name = "stm-gpio",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_PIO_MPE_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(NICE_PIO_MPE_BASE),
		},
	},
	/* SAS rear PIO block */
	[1] = {
		.name = "stm-gpio",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_PIO_SAS_REAR_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(NICE_PIO_SAS_REAR_BASE),
		},
	},
	/* SAS front PIO block */
	[2] = {
		.name = "stm-gpio",
		.id = 2,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(NICE_PIO_SAS_FRONT_BASE, 0x100),
		},
		.dev.platform_data = &(struct stm_plat_pio_data) {
			.regs = (void __iomem *)IO_ADDRESS(NICE_PIO_SAS_FRONT_BASE),
		},
	},
};

static int nice_pio_config(unsigned gpio,
                enum stm_pad_gpio_direction direction, int function)
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



/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init nice_early_device_init(void)
{
#if 0
	/* Initialise PIO and sysconf drivers */
	sysconf_early_init(&nice_sysconf_device, 1);
#endif
	stm_gpio_early_init(nice_pio_devices,
			ARRAY_SIZE(nice_pio_devices),
			256);
	stm_pad_init(ARRAY_SIZE(nice_pio_devices) * STM_GPIO_PINS_PER_PORT,
		     0, 0, nice_pio_config);
}

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
	return 100000000;
}
