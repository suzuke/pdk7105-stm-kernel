/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagtla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/delay.h>
#include <linux/stm/pad.h>
#include <linux/stm/soc.h>
#include <linux/stm/gpio.h>

#include "pio-control.h"

#define MAX_ALT_FUNCS		8

static int gpio_banks;
static struct stm_pio_control *core_of_pio_controls;

static int core_of_pio_config(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function, void *priv)
{
	struct stm_pio_control_pad_config *config = priv;

	return stm_pio_control_config_all(gpio, direction, function, config,
		core_of_pio_controls, gpio_banks, MAX_ALT_FUNCS);
}

#ifdef CONFIG_DEBUG_FS
static void core_of_pio_report(unsigned gpio, char *buf, int len)
{
	stm_pio_control_report_all(gpio, core_of_pio_controls,
		buf, len);
}
#else
#define core_of_pio_report NULL
#endif

static const struct stm_pad_ops core_of_pad_ops = {
	.gpio_config = core_of_pio_config,
	.gpio_report = core_of_pio_report,
};

/* Early initialisation-----------------------------------------------------*/

/* Initialise devices which are required early in the boot process. */
void __init core_of_early_device_init(void)
{
	struct sysconf_field *sc;
	struct device_node *np;
	/* Initialise PIO and sysconf drivers */
	of_sysconf_early_init();
	core_of_pio_controls = of_stm_pio_control_init();
	gpio_banks = of_stm_gpio_early_init(256);
	stm_pad_init(gpio_banks * STM_GPIO_PINS_PER_PORT,
		     0, 0, &core_of_pad_ops);

	np = of_find_node_by_path("/soc");
	if (np) {
		sc = stm_of_sysconf_claim(np, "id");
		stm_soc_set(sysconf_read(sc), -1, -1);
		of_node_put(np);
	}
}

/* Pre-arch initialisation ------------------------------------------------ */
static int __init core_of_postcore_setup(void)
{
	int i;
	char name[20];
	struct device_node *np;
	i = 0;
	/* Gpio devices setup */
	for_each_node_by_name(np, "gpio-bank") {
		sprintf(name, "stm-gpio.%d", i++);
		of_platform_device_create(np, name, NULL);
	}

	/* Gpio Irq mux devices setup */
	i = 0;
	for_each_node_by_name(np, "gpio-irqmux") {
		sprintf(name, "stm-gpio-irqmux.%d", i++);
		of_platform_device_create(np, name, NULL);
	}

	i = 0;
	for_each_node_by_name(np, "irqmux") {
		sprintf(name, "stm-irqmux.%d", i++);
		of_platform_device_create(np, name, NULL);
	}

	np = of_find_compatible_node(NULL, NULL, "st,emi");
	if (np)
		of_platform_device_create(np, "emi", NULL);
	return 0;
}
postcore_initcall(core_of_postcore_setup);
