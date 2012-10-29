/*
 * (c) 2010,2011, 2112 STMicroelectronics Limited
 *
 * Authors:
 *   Pawel Moll <pawel.moll@st.com>
 *   Stuart Menefy <stuart.menefy@st.com>
 *   Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_DRIVERS_STM_PIO_CONTROL_H
#define __LINUX_DRIVERS_STM_PIO_CONTROL_H

#include <linux/stm/pad.h>
#include <linux/stm/pio-control.h>

/*
 * stm_pio_control_retime_style_packed
 * -----------------------------------
 *
 * Each GPIO port has 2 sysconf registers for all 8 pins. Each field
 * has 8 bits, corresponding to the 8 pins. Unfortunately two
 * different register layouts have been seen:
 *
 * STx7108:
 *             retiming[0]   retiming[1]
 *           +-------------+-------------+
 * 31..24    | double_edge | reserved    |
 *           +-------------+-------------+
 * 23..16    | delay_lsb   | delay_msb   |
 *           +-------------+-------------+
 * 15..8     | clknotdata  | retime      |
 *           +-------------+-------------+
 *  7..0     | clk1notclk0 | invertclk   |
 *           +-------------+-------------+
 *
 * STxH415 and STxH205:
 * 
 *             retiming[0]   retiming[1]
 *           +-------------+-------------+
 * 31..24    | delay_msb   | double_edge |
 *           +-------------+-------------+
 * 23..16    | delay_lsb   | clknotdata  |
 *           +-------------+-------------+
 * 15..8     | reserved    | retime      |
 *           +-------------+-------------+
 *  7..0     | clk1notclk0 | invertclk   |
 *           +-------------+-------------+
 *
 * The offsets of each of the fields are described using struct
 * stm_pio_control_retime_offset.
 *
 * stm_pio_control_retime_style_dedicated
 * --------------------------------------
 *
 * Each GPIO pin has its own retiming register.
 *
 *            retime[pin]
 *        +----------------+
 * 31..11 | reserved       |
 *        +----------------+
 *   10   | retime         |
 *        +----------------+
 *    9   | invertclk      |
 *        +----------------+
 *    8   | double_edge    |
 *        +----------------+
 *    7   | delay_innotout |
 *        +----------------+
 *  6..3  | delay          |
 *        +----------------+
 *    2   | clknotdata     |
 *        +----------------+
 *  1..0  | clk            |
 *        +----------------+
 *
 */

enum stm_pio_control_retime_style {
	stm_pio_control_retime_style_none,
	stm_pio_control_retime_style_packed,
	stm_pio_control_retime_style_dedicated,
};

struct stm_pio_control_retime_params {
	const struct stm_pio_control_retime_offset *retime_offset;
	const unsigned int *delay_times_in;
	int num_delay_times_in;
	const unsigned int *delay_times_out;
	int num_delay_times_out;
};

struct stm_pio_control_config {
	struct {
		u8 group;
		u16 num;
	} alt;
	struct {
		u8 group;
		u16 num;
		u8 lsb, msb;
	} oe, pu, od;
	enum stm_pio_control_retime_style retime_style:4;
	unsigned int retime_pin_mask:8;
	const struct stm_pio_control_retime_params *retime_params;
	struct {
		u8 group;
		u16 num;
	} retiming[8];
};

struct stm_pio_control {
	const struct stm_pio_control_config *config;
	struct sysconf_field *alt;
	struct sysconf_field *oe, *pu, *od;
	struct sysconf_field *retiming[8];
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

struct stm_pio_control *of_stm_pio_control_init(void);

int stm_pio_control_config_all(unsigned gpio,
		enum stm_pad_gpio_direction direction, int function,
		struct stm_pio_control_pad_config *config,
		struct stm_pio_control *pio_controls,
		int num_gpios, int num_functions);

void stm_pio_control_report_all(int gpio,
		struct stm_pio_control *pio_controls,
		char *buf, int len);

#endif
