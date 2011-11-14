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

#include <linux/stm/stih415.h>
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
	return sysconf_claim(SYSCONFG_GROUP(_nr), 
		SYSCONF_OFFSET(_nr), _lsb, _msb, "Clk lla");
}

#include "clock-stih415.h"
#undef SYSCONF
#include "clock-oslayer.h"
#include "clock-common.h"

#define CLKLLA_SYSCONF_UNIQREGS		1 /* Required for oslayer */

/* External functions prototypes */
extern int mpe41_clk_init(clk_t *, clk_t *, clk_t *, clk_t *);	/* MPE41 */
extern int sasg1_clk_init(clk_t *);				/* SASG1 */

/* SOC top input clocks. */
#define SYS_CLKIN			30
#define SYS_CLKALTIN			30   /* MPE only alternate input */

static struct sysconf_field *usb2_triple_phy_sc;
static int clk_usb_phy_init(struct clk *clk)
{
	usb2_triple_phy_sc = sysconf_claim(SYSCONFG_GROUP(332),
					 SYSCONF_OFFSET(332), 6, 6, "USB");
	return 0;
}
static int clk_usb_phy_enable(struct clk *clk)
{
	sysconf_write(usb2_triple_phy_sc, 1);
	return 0;
}
static int clk_usb_phy_disable(struct clk *clk)
{
	sysconf_write(usb2_triple_phy_sc, 0);
	return 0;
}

static struct clk_ops clk_usb_phy_clk_ops = {
	.init = clk_usb_phy_init,
	.enable = clk_usb_phy_enable,
	.disable = clk_usb_phy_disable,
};

static struct clk usb2_triple_phy_clks[] = {
	{
		.name = "USB2_TRIPLE_PHY",
		.id = 0,
		.ops = &clk_usb_phy_clk_ops,
		.rate = SYS_CLKIN*1000000,
	}
};
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
	clk_register_table(usb2_triple_phy_clks,
				 ARRAY_SIZE(usb2_triple_phy_clks), 0);

	/* SASG1 clocks */
	sasg1_clk_init(&clk_clocks[0]);

	/* Connecting inter-dies clocks */
	clk_main = clk_get(NULL,"CLKS_C_PIX_MAIN");
	clk_aux = clk_get(NULL,"CLKS_C_PIX_AUX");

	/* MPE41 clocks */
	mpe41_clk_init(&clk_clocks[0], &clk_clocks[1], clk_main, clk_aux);

	return 0;
}
