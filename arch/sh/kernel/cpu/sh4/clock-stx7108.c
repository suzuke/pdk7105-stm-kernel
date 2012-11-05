/*
 * Copyright (C) 2010 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STx7105.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>
#include <linux/stm/stx7108.h>

int __init arch_clk_init(void)
{
	int ret;

	ret = stx7108_plat_clk_init();
	if (ret)
		return ret;

	ret = stx7108_plat_clk_alias_init();
	if (ret)
		return ret;

	return ret;
}
