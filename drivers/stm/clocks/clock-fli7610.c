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

#include <linux/stm/fli7610.h>
#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/io.h>
#include <linux/delay.h>

#define clk_t	struct clk
#include "clock-common.h"

static struct clk clocks[] = {
	{
		.name = "module_clk",
		.rate = 100000000,
	}, {
		.name = "comms_clk",
		.rate = 100000000,
	}, {
		.name = "sbc_comms_clk",
		.rate = 30000000,
	}
};
/* ========================================================================
   Name:        plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int plat_clk_init(void)
{
	clk_t *clk_main, *clk_aux;

	clk_register_table(clocks, ARRAY_SIZE(clocks), 1);

	return 0;
}
