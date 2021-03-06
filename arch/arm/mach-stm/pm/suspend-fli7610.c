/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <linux/stm/fli7610.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <mach/mpe41.h>
#include <mach/soc-fli7610.h>

#include "./suspend-mcm.h"

struct stm_mcm_suspend *stx_mpe41_suspend_setup(void);
struct stm_mcm_suspend *stx_tae_suspend_setup(void);

static int fli7610_get_wake_irq(void)
{
	return stm_get_wake_irq(__io_address(MPE41_GIC_CPU_BASE));
}

static struct stm_platform_suspend fli7610_suspend = {
	.ops.begin = stm_dual_mcm_suspend_begin,
	.ops.end = stm_dual_mcm_suspend_end,

	.pre_enter = stm_dual_mcm_suspend_pre_enter,
	.post_enter = stm_dual_mcm_suspend_post_enter,

	.eram_iomem = (void *)0xc0080000,
	.get_wake_irq = fli7610_get_wake_irq,
};

static int __init fli7610_suspend_setup(void)
{
	int i;

	INIT_LIST_HEAD(&fli7610_suspend.mem_tables);

	main_mcm = stx_mpe41_suspend_setup();
	peripheral_mcm = stx_tae_suspend_setup();

	if (!main_mcm || !peripheral_mcm) {
		pr_err("stm: Error on mcm registration\n");
		return -ENOSYS;
	}

	for (i = 0; i < main_mcm->nr_tables; ++i)
		list_add_tail(&main_mcm->tables[i].node,
			&fli7610_suspend.mem_tables);

	return stm_suspend_register(&fli7610_suspend);
}

module_init(fli7610_suspend_setup);
