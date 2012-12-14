/*****************************************************************************
 *
 * File name   : clock-stxh416.c
 * Description : Low Level API - HW specific implementation
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 __ONLY__.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
25/may/12 fabrice.charpentier@st.com
	  Added HDMI RX clock link + fixes
14/mar/12 fabrice.charpentier@st.com
	  Preliminary version for Orly2
*/

/* Includes --------------------------------------------------------------- */

#ifdef ST_OS21

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "clock.h"
#include "clock-stxh416.h"
#define CLKLLA_SYSCONF_UNIQREGS		1 /* Required for oslayer */
#include "clock-oslayer.h"

#else /* Linux */

#include <linux/stm/stih416.h>
#include <linux/stm/clk.h>
#include <linux/stm/sysconf.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "clk-common.h"
#include "clock-oslayer.h"

#include "clock-stih416.h"
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

/* External functions prototypes */
extern int mpe42_clk_init(struct clk *, struct clk *,
			  struct clk *, struct clk *,
			  struct clk *);
extern int sasg2_clk_init(struct clk *);

/* SOC top input clocks. */
#define SYS_CLKIN	30
#define SYS_CLKALTIN	30   /* MPE only alternate input */

static struct clk clk_clocks[] = {
	/* Top level clocks */
	_CLK_FIXED(CLK_SYSIN, SYS_CLKIN * 1000000, CLK_ALWAYS_ENABLED),
	_CLK_FIXED(CLK_MPEALT, SYS_CLKALTIN * 1000000, CLK_ALWAYS_ENABLED),
};

#if defined(CLKLLA_NO_PLL)
static int vp_set_rate();
#endif

/* ========================================================================
   Name:        plat_clk_init()
   Description: SOC specific LLA initialization
   Returns:     'clk_err_t' error code.
   ======================================================================== */

int __init plat_clk_init(void)
{
	struct clk *clk_main, *clk_aux, *clk_hdmirx;

#ifndef ST_OS21
	clk_register_table(clk_clocks, ARRAY_SIZE(clk_clocks), 0);
#endif

	/* SASG2 clocks */
	sasg2_clk_init(&clk_clocks[0]);

	/* Connecting inter-dies clocks */
	clk_main = clk_get(NULL, "CLK_S_PIX_MAIN");
	clk_aux = clk_get(NULL, "CLK_S_PIX_AUX");
	clk_hdmirx = clk_get(NULL, "CLK_S_PIX_HDMIRX");

	/* MPE42 clocks */
	mpe42_clk_init(&clk_clocks[0], &clk_clocks[1],
		clk_main, clk_aux, clk_hdmirx);

/* In case of virtual platform, taking PLL/FS injected frequencies
   from 'vp_freqs.txt' in the platform itself */
#if defined(CLKLLA_NO_PLL)
	vp_set_rate();
#endif
	return 0;
}

#if defined(CLKLLA_NO_PLL)
/* ========================================================================
   Name:        vp_set_rate
   Description: Update PLL & FS rates from file.
		For coemulation platforms only
   Returns:     'clk_err_t' error code
   ======================================================================== */

static int vp_set_rate()
{
	FILE *in = NULL;
	struct clk *clk_p;
	char line[201];
	char name[40], tmp[20];
	unsigned long freq;
	int i, j;

	in = fopen("clocklla_vp_freqs.txt", "r");
	if (!in) {
		printf("WARNING: No 'clocklla_vp_freqs.txt' file\n");
		printf("         Displayed frequencies may not be ",
			"realistic at all\n");
		return -1;
	}

	/* Expected file format:
	ClockLLAName FreqInHz
	 */
	printf("Reading 'clocklla_vp_freqs.txt'\n");
	while (1) {
		/* Reading line & splitting params */
		if (!fgets(line, 200, in))
			break;
		if (!line[0])
			continue;
		for (i = 0; isspace(line[i]); i++)
			;
		if (line[i] == '#')
			continue;

		for (j = i; line[i] && !isspace(line[i]); i++)
			;
		memcpy(name, line + j, i - j);
		name[i - j] = 0;

		for (; isspace(line[i]); i++)
			;
		for (j = i; line[i] && !isspace(line[i]); i++)
			;
		memcpy(tmp, line + j, i - j);
		tmp[i - j] = 0;
		freq = atoi(tmp);

		/* Looking for clock */
		clk_p = clk_get(NULL, name);
		if (!clk_p) {
			printf("  ERROR: Clock '%s' is unknown\n", name);
			continue;
		}
		/* Setting clock rate */
		clk_p->rate = freq;
		printf("  %s clock set to %u\n", name, freq);
	}
	fclose(in);

	return 0;
}
#endif
