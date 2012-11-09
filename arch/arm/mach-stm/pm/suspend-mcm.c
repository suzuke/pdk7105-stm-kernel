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

#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <asm/hardware/gic.h>	/* gic offset and struct gic_chip_data */

static struct stm_wakeup_devices wkd;
static suspend_state_t target_state;

struct stm_mcm_suspend *main_mcm;
struct stm_mcm_suspend *peripheral_mcm;

int stm_dual_mcm_suspend_begin(suspend_state_t state)
{
	int ret = 0;

	pr_info("stm pm: Analyzing the wakeup devices\n");

	stm_check_wakeup_devices(&wkd);
	target_state = state;

	ret = stm_suspend_mcm_begin(main_mcm, state, &wkd);
	if (ret)
		return ret;

	ret = stm_suspend_mcm_begin(peripheral_mcm, state, &wkd);
	if (ret)
		stm_suspend_mcm_end(main_mcm, state);


	return ret;
}

int stm_dual_mcm_suspend_pre_enter(suspend_state_t state)
{
	int ret = 0;

	ret = stm_suspend_mcm_pre_enter(main_mcm, state, &wkd);

	if (ret)
		return ret;

	ret = stm_suspend_mcm_pre_enter(peripheral_mcm, state, &wkd);
	if (ret)
		stm_suspend_mcm_post_enter(main_mcm, state);

	return ret;
}

void stm_dual_mcm_suspend_post_enter(suspend_state_t state)
{
	stm_suspend_mcm_post_enter(peripheral_mcm, state);
	stm_suspend_mcm_post_enter(main_mcm, state);
}

void stm_dual_mcm_suspend_end(void)
{
	stm_suspend_mcm_end(peripheral_mcm, target_state);
	stm_suspend_mcm_end(main_mcm, target_state);
}

int stm_get_wake_irq(void *gic_cpu)
{
	int irq = 0;
	struct irq_data *d;

	irq = readl(gic_cpu + GIC_CPU_INTACK);
	d = irq_get_irq_data(irq);
	writel(d->hwirq, gic_cpu + GIC_CPU_EOI);

	return irq;
}
