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
 */

#ifndef __LINUX_DRIVERS_STM_PIO_CONTROL_H
#define __LINUX_DRIVERS_STM_PIO_CONTROL_H

#include <linux/stm/pad.h>
#include <linux/stm/pio-control.h>

struct stm_pio_control_config {
	struct {
		u8 group, num;
	} alt;
	struct {
		u8 group, num, lsb, msb;
	} oe, pu, od;
	struct {
		u8 group, num;
	} retiming[2];
	unsigned int no_retiming:1;
};

struct stm_pio_control {
	struct sysconf_field *alt;
	struct sysconf_field *oe, *pu, *od;
	struct sysconf_field *retiming[2];
};

/* Byte positions in 2 sysconf words, starts from 0 */
struct stm_pio_control_retime_offset {
	int retime_offset;
	int clk1notclk0_offset;
	int clknotdata_offset;
	int double_edge_offset;
	int invertclk_offset;
	int delay_lsb_offset;
	int delay_msb_offset;
};

void __init stm_pio_control_init(const struct stm_pio_control_config *config,
		struct stm_pio_control *pio_control, int num);

int stm_pio_control_config_all(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function,
		struct stm_pio_control_pad_config *config,
		struct stm_pio_control *pio_controls,
		const struct stm_pio_control_retime_offset *retime_offset,
		int num_gpios, int num_functions);

#endif
