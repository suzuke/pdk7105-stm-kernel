/*****************************************************************************
 *
 * File name   : clock-stxH415.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
04/May/11 Francesco Virlinzi
	  Inter-dies clock management
	  Linux-Arm (anticipation)
17/mar/11 fabrice.charpentier@st.com
	  Added inter dies clock setup.
08/oct/10 fabrice.charpentier@st.com
	  Aligned OS21 & Linux init functions to "plat_clk_init()". Removed CLK_LAST.
18/jun/10 fabrice.charpentier@st.com
	  Preliminary version
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"

#else /* Linux */

#include <linux/stm/stih415.h>
#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clock-utils.h"

/*
 * This function is used in the clock-MPE file
 * and in the clock-SAS file to be able to compile
 * both the file __without__ any include chip_based
 */
struct sysconf_field *platform_sys_claim(int _nr, int _lsb, int _msb)
{
	return sysconf_claim(SYSCONFG_GROUP(_nr), 
		SYSCONF_OFFSET(_nr), _lsb, _msb, "Clk lla");
}

#endif

#include "clock-stih415.h"
#define CLKLLA_SYSCONF_UNIQREGS		1 /* Required for oslayer */
#define PIO_BASE_ADDRESS(bank)		0 /* Required for oslayer */
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

/* External functions prototypes */
extern int mpe31_clk_init(clk_t *, clk_t *, clk_t *, clk_t *);	/* MPE31 */
extern int sasg1_clk_init(clk_t *);				/* SASG1 */

/* SOC top input clocks. */
#define SYS_CLKIN			30
#define SYS_CLKALTIN			30   /* MPE only alternate input */

static clk_t clk_clocks[] = {
	/* Top level clocks */
	_CLK_FIXED(CLK_SYSIN, SYS_CLKIN * 1000000,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_MPEALT, SYS_CLKALTIN * 1000000,
		  CLK_RATE_PROPAGATES | CLK_ALWAYS_ENABLED),
};

/* ========================================================================
   Name:        plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int plat_clk_init(void)
{
	clk_t *clk_main, *clk_aux;

	clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);

	/* SASG1 clocks */
	sasg1_clk_init(&clk_clocks[0]);

	/* Connecting inter-dies clocks */
	clk_main = clk_get(NULL,"CLKS_C_PIX_MAIN");
	clk_aux = clk_get(NULL,"CLKS_C_PIX_AUX");

	/* MPE31 clocks */
	mpe31_clk_init(&clk_clocks[0], &clk_clocks[1], clk_main, clk_aux);

	return 0;
}
