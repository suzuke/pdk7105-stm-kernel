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

#include <linux/stm/stih416.h>
#include <linux/stm/mpe42-periphs.h>
#include <linux/stm/sasg2-periphs.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <mach/mpe42.h>
#include <mach/soc-stih416.h>
#include <asm/hardware/gic.h>	/* gic offset and struct gic_chip_data */

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include "./suspend-mcm.h"

struct stm_mcm_suspend *stx_mpe42_suspend_setup(void);
struct stm_mcm_suspend *stx_sasg2_suspend_setup(void);

static int stih416_get_wake_irq(void)
{
	return stm_get_wake_irq(__io_address(MPE42_GIC_CPU_BASE));
}

static struct stm_platform_suspend stih416_suspend = {
	.ops.begin = stm_dual_mcm_suspend_begin,
	.ops.end = stm_dual_mcm_suspend_end,

	.pre_enter = stm_dual_mcm_suspend_pre_enter,
	.post_enter = stm_dual_mcm_suspend_post_enter,

	.eram_iomem = (void *)0xc00be000,
	.get_wake_irq = stih416_get_wake_irq,
};

static int __init stih416_suspend_setup(void)
{
	int i;

	INIT_LIST_HEAD(&stih416_suspend.mem_tables);

	main_mcm = stx_mpe42_suspend_setup();

	peripheral_mcm = stx_sasg2_suspend_setup();

	if (!main_mcm || !peripheral_mcm) {
		pr_err("stm: Error on mcm registration\n");
		return -ENOSYS;
	}

	for (i = 0; i < main_mcm->nr_tables; ++i)
		list_add_tail(&main_mcm->tables[i].node,
			&stih416_suspend.mem_tables);

	return stm_suspend_register(&stih416_suspend);
}

module_init(stih416_suspend_setup);
