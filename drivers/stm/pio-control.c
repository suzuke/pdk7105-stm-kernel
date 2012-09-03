/*
 * (c) 2010,2011 STMicroelectronics Limited
 *
 * Authors:
 *   Pawel Moll <pawel.moll@st.com>
 *   Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * For more details on the retiming logic which this code controls, see
 *    Generic Retime Padlogic functional spec (ADCS 8198257)
 * and
 *    STi7108 Generic Retime Padlogic Application Note
 */

#include <linux/stm/sysconf.h>
#include "pio-control.h"


static void stm_pio_control_config_direction(
		struct stm_pio_control *pio_control,
		int pin, enum stm_pad_gpio_direction direction,
		struct stm_pio_control_mode_config *custom_mode)
{
	struct sysconf_field *output_enable;
	struct sysconf_field *pull_up;
	struct sysconf_field *open_drain;
	unsigned long oe_value, pu_value, od_value;
	unsigned long mask;

	pr_debug("%s(pin=%d, direction=%d)\n",
			__func__, pin, direction);

	output_enable = pio_control->oe;
	pull_up = pio_control->pu;
	open_drain = pio_control->od;

	mask = 1 << pin;

	oe_value = sysconf_read(output_enable);
	pu_value = sysconf_read(pull_up);
	od_value = sysconf_read(open_drain);

	switch (direction) {
	case stm_pad_gpio_direction_in:
		/* oe = 0, pu = 0, od = 0 */
		oe_value &= ~mask;
		pu_value &= ~mask;
		od_value &= ~mask;
		break;
	case stm_pad_gpio_direction_out:
		/* oe = 1, pu = 0, od = 0 */
		oe_value |= mask;
		pu_value &= ~mask;
		od_value &= ~mask;
		break;
	case stm_pad_gpio_direction_bidir:
		/* oe = 1, pu = 0, od = 1 */
		oe_value |= mask;
		pu_value &= ~mask;
		od_value |= mask;
		break;
	case stm_pad_gpio_direction_custom:
		BUG_ON(!custom_mode);
		if (custom_mode->oe)
			oe_value |= mask;
		else
			oe_value &= ~mask;
		if (custom_mode->pu)
			pu_value |= mask;
		else
			pu_value &= ~mask;
		if (custom_mode->od)
			od_value |= mask;
		else
			od_value &= ~mask;
		break;
	default:
		BUG();
		break;
	}

	sysconf_write(output_enable, oe_value);
	sysconf_write(pull_up, pu_value);
	sysconf_write(open_drain, od_value);
}

static void stm_pio_control_config_function(
		struct stm_pio_control *pio_control,
		int pin, int function)
{
	struct sysconf_field *selector;
	int offset;
	unsigned long val;

	pr_debug("%s(pin=%d, function=%d)\n",
			__func__, pin, function);

	selector = pio_control->alt;

	offset = pin * 4;

	val = sysconf_read(selector);
	val &= ~(0xf << offset);
	val |= function << offset;
	sysconf_write(selector, val);
}

static void stm_pio_control_config_retime(
		struct stm_pio_control *pio_control,
		const struct stm_pio_control_retime_offset *offset,
		int pin, const struct stm_pio_control_retime_config *rt)
{
	struct sysconf_field **regs;
	unsigned long values[2];
	unsigned long mask;
	int i, j;

	unsigned long retime_mask =
		(rt->clk1notclk0   >= 0 ? 1<<offset->clk1notclk0_offset : 0) |
		(rt->clknotdata    >= 0 ? 1<<offset->clknotdata_offset : 0) |
		(rt->delay_input   >= 0 ? (1<<offset->delay_lsb_offset |
					   1<<offset->delay_msb_offset) : 0) |
		(rt->double_edge   >= 0 ? 1<<offset->double_edge_offset : 0) |
		(rt->invertclk     >= 0 ? 1<<offset->invertclk_offset : 0) |
		(rt->retime        >= 0 ? 1<<offset->retime_offset : 0);

	unsigned long retime_config =
		(rt->clk1notclk0       ? 1<<offset->clk1notclk0_offset : 0) |
		(rt->clknotdata        ? 1<<offset->clknotdata_offset : 0) |
		((rt->delay_input & 1) ? 1<<offset->delay_lsb_offset : 0) |
		((rt->delay_input & 2) ? 1<<offset->delay_msb_offset : 0) |
		(rt->double_edge       ? 1<<offset->double_edge_offset : 0) |
		(rt->invertclk         ? 1<<offset->invertclk_offset : 0) |
		(rt->retime            ? 1<<offset->retime_offset : 0);


	pr_debug("%s(pin=%d, retime_mask=%02lx, retime_config=%02lx)\n",
		 __func__, pin, retime_mask, retime_config);

	regs = pio_control->retiming;

	values[0] = sysconf_read(regs[0]);
	values[1] = sysconf_read(regs[1]);

	for (i = 0; i < 2; i++) {
		mask = 1 << pin;
		for (j = 0; j < 4; j++) {
			if (retime_mask & 1) {
				if (retime_config & 1)
					values[i] |= mask;
				else
					values[i] &= ~mask;
			}
			mask <<= 8;
			retime_mask >>= 1;
			retime_config >>= 1;
		}
	}

	sysconf_write(regs[0], values[0]);
	sysconf_write(regs[1], values[1]);
}

static int stm_pio_control_report_direction(struct stm_pio_control *pio_control,
		int pin, char *buf, int len)
{
	unsigned long oe_value, pu_value, od_value;

	oe_value = sysconf_read(pio_control->oe);
	pu_value = sysconf_read(pio_control->pu);
	od_value = sysconf_read(pio_control->od);

	return snprintf(buf, len, "oe %ld, pu %ld, od %ld",
		(oe_value >> pin) & 1,
		(pu_value >> pin) & 1,
		(od_value >> pin) & 1);
}

static int stm_pio_control_report_retime_packed(
		struct stm_pio_control *pio_control,
		const struct stm_pio_control_retime_offset *offset,
		int pin, char *buf, int len)
{
	unsigned long rt_value[2];
	unsigned long rt_reduced;
	int i, j;

	rt_value[0] = sysconf_read(pio_control->retiming[0]);
	rt_value[1] = sysconf_read(pio_control->retiming[1]);

	rt_reduced = 0;
	for (i = 0; i < 2; i++) {
		for (j = 0; j < 4; j++) {
			if (rt_value[i] & (1<<((8*j)+pin)))
				rt_reduced |= 1 << ((i*4)+j);
		}
	}

	return snprintf(buf, len,
		 "rt %ld, c1nc0 %ld, cnd %ld, de %ld, ic %ld, dly %ld%ld",
		 (rt_reduced >> offset->retime_offset) & 1,
		 (rt_reduced >> offset->clk1notclk0_offset) & 1,
		 (rt_reduced >> offset->clknotdata_offset) & 1,
		 (rt_reduced >> offset->double_edge_offset) & 1,
		 (rt_reduced >> offset->invertclk_offset) & 1,
		 (rt_reduced >> offset->delay_msb_offset) & 1,
		 (rt_reduced >> offset->delay_lsb_offset) & 1);
}

void __init stm_pio_control_init(const struct stm_pio_control_config *config,
		struct stm_pio_control *pio_control, int num)
{
	int i, j;

	for (i=0; i<num; i++) {
		pio_control[i].config = &config[i];
		pio_control[i].alt = sysconf_claim(config[i].alt.group,
			config[i].alt.num, 0, 31,
			"PIO Alternative Function Selector");
		if (!pio_control[i].alt) goto failed;

		pio_control[i].oe = sysconf_claim(config[i].oe.group,
			config[i].oe.num, config[i].oe.lsb, config[i].oe.msb,
			"PIO Output Enable Control");
		if (!pio_control[i].oe) goto failed;
		pio_control[i].pu = sysconf_claim(config[i].pu.group,
			config[i].pu.num, config[i].pu.lsb, config[i].pu.msb,
			"PIO Pull Up Control");
		if (!pio_control[i].pu) goto failed;
		pio_control[i].od = sysconf_claim(config[i].od.group,
			config[i].od.num, config[i].od.lsb, config[i].od.msb,
			"PIO Open Drain Control");
		if (!pio_control[i].od) goto failed;

		switch (config[i].retime_style) {
		case stm_pio_control_retime_style_none:
			break;
		case stm_pio_control_retime_style_packed:
			for (j = 0; j < 2; j++) {
				pio_control[i].retiming[j] = sysconf_claim(
					config[i].retiming[j].group,
					config[i].retiming[j].num, 0, 31,
					"PIO Retiming Configuration");
				if (!pio_control[i].retiming[j])
					goto failed;
			}
			break;
		}
	}

	return;

failed:
	/* Can't do anything is early except panic */
	panic("Unable to allocate PIO control sysconfs");
}

/*
 * All the above functions only need basic sysconf capability.
 * However these are aware of the way PIO hardware is also involved
 * in pad control.
 */

#include <linux/stm/gpio.h>

int stm_pio_control_config_all(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function,
		struct stm_pio_control_pad_config *config,
		struct stm_pio_control *pio_controls,
		const struct stm_pio_control_retime_offset *retime_offset,
		int num_gpios, int num_functions)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stm_pio_control *pio_control;
	const struct stm_pio_control_config *pio_control_config;

	BUG_ON(port >= num_gpios);
	BUG_ON(function < 0 || function >= num_functions);

	pio_control = &pio_controls[port];
	pio_control_config = pio_control->config;

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
		stm_pio_control_config_direction(pio_control, pin,
				direction, config ? config->mode : NULL);
	}

	stm_pio_control_config_function(pio_control, pin, function);

	if ((config && config->retime) &&
	    ((1 << pin) & pio_control_config->retime_pin_mask)) {
		stm_pio_control_config_retime(pio_control,
			retime_offset, pin, config->retime);
	}

	return 0;
}

void stm_pio_control_report_all(int gpio,
		struct stm_pio_control *pio_controls,
		const struct stm_pio_control_retime_offset *retime_offset,
		char *buf, int len)
{
	int port = stm_gpio_port(gpio);
	int pin = stm_gpio_pin(gpio);
	struct stm_pio_control *pio_control = &pio_controls[port];
	const struct stm_pio_control_config *pio_control_config = pio_control->config;
	unsigned long alt_value;
	unsigned long function;
	int off;

	alt_value = sysconf_read(pio_control->alt);
	function = (alt_value >> (pin * 4)) & 0xf;
	off = snprintf(buf, len, "alt fn %ld - ", function);

	if (function == 0)
		off += snprintf(buf+off, len-off, "%s",
			stm_gpio_get_direction(gpio));
	else
		off += stm_pio_control_report_direction(pio_control,
			pin, buf+off, len-off);

	switch (pio_control->config->retime_style) {
	case stm_pio_control_retime_style_none:
		break;
	case stm_pio_control_retime_style_packed:
		off += snprintf(buf+off, len-off, " - ");
		off += stm_pio_control_report_retime_packed(pio_control,
			retime_offset, pin, buf+off, len-off);
		break;
	}
}
