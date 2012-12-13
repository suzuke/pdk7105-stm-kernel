/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"
#include <linux/err.h>
#include <linux/clk.h>

struct stm_mali_platform_data {
	int initialized;
	struct clk *clk;
	mali_power_mode power_mode;
};

static struct stm_mali_platform_data stm_mali;


_mali_osk_errcode_t mali_platform_init(void)
{
	char *mali_clk_n = "gpu_clk";

	if (stm_mali.initialized)
		MALI_SUCCESS;

	stm_mali.initialized = 1;

	stm_mali.clk = clk_get(NULL, mali_clk_n);
	if (IS_ERR(stm_mali.clk))
		MALI_DEBUG_PRINT(2, ("PM clk %s not found\n", mali_clk_n));
	else
		clk_prepare_enable(stm_mali.clk);

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	if (stm_mali.clk)
		clk_disable_unprepare(stm_mali.clk);
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
	if (!stm_mali.clk)
		MALI_SUCCESS;

	if (stm_mali.power_mode && power_mode)
		/*
		 * do nothig if switching between DEEP_SLEEP and LIGHT_SLEEP
		 */
		goto out;

	MALI_DEBUG_PRINT(4, ("PM mode_change %s\n",
				power_mode ? "SLEEP" : "ON"));
	if (power_mode)
		clk_disable_unprepare(stm_mali.clk);
	else
		clk_prepare_enable(stm_mali.clk);
out:
	stm_mali.power_mode = power_mode;
	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
}

void set_mali_parent_power_domain(void* dev)
{
}


