/*
 * Copyright (C) 2011 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Code to handle the arch clocks on the STiH415.
 */

#include <linux/init.h>
#include <linux/stm/clk.h>
#include <linux/stm/stih415.h>

int __init arch_clk_init(void)
{
	int ret;

	ret = stih415_plat_clk_init();
	if (ret)
		return ret;

	ret = stih415_plat_clk_alias_init();
	if (ret)
		return ret;

	return ret;
}
