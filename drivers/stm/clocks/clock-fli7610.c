/*****************************************************************************
 *
 * File name   : clock-fli7610.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
27/sep/11 srinivas.kandagatla@st.com
	  Preliminary version
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"

#else /* Linux */

#include <linux/stm/fli7610.h>
#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/io.h>
#include <linux/delay.h>

/*
 * This function is used in the clock-MPE file
 * and in the clock-SAS file to be able to compile
 * both the file __without__ any include chip_based
 */
struct sysconf_field *platform_sys_claim(int _nr, int _lsb, int _msb)
{
	return sysconf_claim(MPE_SYSCONFG_GROUP(_nr),
		MPE_SYSCONF_OFFSET(_nr), _lsb, _msb, "Clk lla");
}

#endif

#include "clock-fli7610.h"
#define         PIO_CLEAR_PnC0                0x28
#define         PIO_CLEAR_PnC1                0x38
#define         PIO_CLEAR_PnC2                0x48
#define         PIO_PnC0                      0x20
#define         PIO_PnC1                      0x30
#define         PIO_PnC2                      0x40
#define         PIO_SET_PnC0                  0x24
#define         PIO_SET_PnC1                  0x34
#define         PIO_SET_PnC2                  0x44
#include "clock-oslayer.h"
#include "clock-common.h"

/* External functions prototypes */
extern int mpe41_clk_init(clk_t *, clk_t *, clk_t *, clk_t *);	/* MPE41 */
extern int tae_clk_init(clk_t *);				/* SASG1 */

/* SOC top input clocks. */
#define SYS_CLKIN			30
#define SYS_CLKALTIN			30   /* MPE only alternate input */


static int clkgen_ism_enable(struct clk *clk)
{
	return 0;
}
static int clkgen_ism_disable(struct clk *clk)
{
	return 0;
}
static int clkgen_ism_set_rate(struct clk *clk, unsigned long rate)
{
	clk->rate = rate;
	return 0;
}

static struct clk_ops clkgen_ism_clk_ops = {
	.enable = clkgen_ism_enable,
	.disable = clkgen_ism_disable,
	.set_rate = clkgen_ism_set_rate,
};

static struct clk ism_clks[] = {
	{
		.name = "CLK_ISM0",
		.id = 0,
		.ops = &clkgen_ism_clk_ops,
		.rate = 0,
	}
};
static clk_t clk_clocks[] = {
	/* Top level clocks */
	_CLK_FIXED(CLK_SYSIN, SYS_CLKIN * 1000000,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_MPEALT, SYS_CLKALTIN * 1000000,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_PIX_AUX, 13500000,		/* To MPE */
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
};
/* ========================================================================
   Name:        plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int plat_clk_init(void)
{
	clk_t *clk_main = NULL, *clk_aux = NULL;

	clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);

	clk_register_table(ism_clks, ARRAY_SIZE(ism_clks), 0);

	/* Connecting inter-dies clocks */
	clk_main = &ism_clks[0];
	clk_aux = &clk_clocks[CLK_PIX_AUX];
	/* TAE clocks */
	tae_clk_init(&clk_clocks[0]);

	/* MPE31 clocks */
	mpe41_clk_init(&clk_clocks[0], &clk_clocks[1], clk_main, clk_aux);
	return 0;
}
