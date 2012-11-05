/*****************************************************************************
 *
 * File name   : clock-stig125.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2012 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
26/jun/12 ravinder-dlh.singh@st.com - francesco.virlinzi@st.com
	  extended VCC clock support
25/may/12 ravinder-dlh.singh@st.com
	  Some minor updates for adding support for virtual platforms
22/may/12 francesco.virlinzi@st.com
      General review for integration in stlinux
14/may/12 ravinder-dlh.singh@st.com
	  Preliminary version
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"

#else /* Linux */

#include <linux/stm/stig125.h>
#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/io.h>
#include <linux/delay.h>

struct sysconf_field *stig125_platform_sys_claim(int _nr, int _lsb, int _msb)
{
	return sysconf_claim(SYSCONFG_GROUP(_nr),
		SYSCONF_OFFSET(_nr), _lsb, _msb, "Clk lla");
}

#endif

#include "clock-stig125.h"
#include "clock-oslayer.h"
#include "clock-common.h"

/* External functions prototypes */
extern int sasc1_clk_init(struct clk *, struct clk *,
	struct clk *, struct clk *, struct clk *);

/* SOC top input clocks. */
#define SYS_CLKIN			30
#define SYS_CLKALTIN			27	/* alternate input */
#define SYS_CH34REF			216	/* VCXO */
#define SYS_IFE_REF			27	/* IFE ref clock */
#define SYS_TMDS_HDMS			297	/* VCC CLK_IN_2 */

static struct clk clk_clocks[] = {
	/* Top level clocks */
	_CLK_FIXED(CLK_SYSIN, SYS_CLKIN * 1000000, CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_SYSALT, SYS_CLKALTIN * 1000000, CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_CH34REF, SYS_CH34REF * 1000000, CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_IFE_REF, SYS_IFE_REF * 1000000, CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_TMDS_HDMS, SYS_TMDS_HDMS * 1000000, CLK_ALWAYS_ENABLED),
};

/* ========================================================================
   Name:        stig125_plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int __init stig125_plat_clk_init(void)
{
	clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 1);

	platform_sys_claim = stig125_platform_sys_claim;
	/* SASC1 clocks */
	sasc1_clk_init(&clk_clocks[0], &clk_clocks[1],
		&clk_clocks[2], &clk_clocks[3], &clk_clocks[4]);

	return 0;
}
