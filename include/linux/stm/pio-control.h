/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PIO_CONTROL_H
#define __LINUX_STM_PIO_CONTROL_H

struct stm_pio_control_retime_config {
	unsigned int retime:1;
	unsigned int clk:2;
	unsigned int clknotdata:1;
	unsigned int double_edge:1;
	unsigned int invertclk:1;
	unsigned int force_delay:1;
	unsigned int force_delay_innotout:1;
	unsigned int delay:16;
};

struct stm_pio_control_pad_config {
	struct stm_pio_control_retime_config *retime;
};

/*
 * 	Generic Retime Padlogic possible modes
 * Refer to GRP Functional specs (ADCS 8198257) for more details
 */

/*
 * B Mode
 * Bypass retime with optional delay
 */
#define RET_BYPASS(_delay) (&(struct stm_pio_control_retime_config){ \
	.retime = 0, \
	.clk = 0, \
	.clknotdata = 0, \
	.double_edge = 0, \
	.invertclk = 0, \
	.delay = _delay, \
})

/*
 * R0, R1, R0D, R1D modes
 * single-edge data non inverted clock, retime data with clk
 */
#define RET_SE_NICLK_IO(_delay, _clk) (&(struct stm_pio_control_retime_config){ \
	.retime = 1, \
	.clk = _clk, \
	.clknotdata = 0, \
	.double_edge = 0, \
	.invertclk = 0, \
	.delay = _delay, \
})

/*
 * RIV0, RIV1, RIV0D, RIV1D modes
 * single-edge data inverted clock, retime data with clk
 */
#define RET_SE_ICLK_IO(_delay, _clk) (&(struct stm_pio_control_retime_config){ \
	.retime = 1, \
	.clk = _clk, \
	.clknotdata = 0, \
	.double_edge = 0, \
	.invertclk = 1, \
	.delay = _delay, \
})

/*
 * R0E, R1E, R0ED, R1ED modes
 * double-edge data, retime data with clk
 */
#define RET_DE_IO(_delay, _clk) (&(struct stm_pio_control_retime_config){ \
	.retime = 1, \
	.clk = _clk, \
	.clknotdata = 0, \
	.double_edge = 1, \
	.invertclk = 0, \
	.delay = _delay, \
})

/*
 * CIV0, CIV1 modes with inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define RET_ICLK(_delay, _clk) (&(struct stm_pio_control_retime_config){ \
	.retime = 1, \
	.clk = _clk, \
	.clknotdata = 1, \
	.double_edge = 0, \
	.invertclk = 1, \
	.delay = _delay, \
})

/*
 * CLK0, CLK1 modes with non-inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define RET_NICLK(_delay, _clk) (&(struct stm_pio_control_retime_config){ \
	.retime = 1, \
	.clk = _clk, \
	.clknotdata = 1, \
	.double_edge = 0, \
	.invertclk = 0, \
	.delay = _delay, \
})

#endif
