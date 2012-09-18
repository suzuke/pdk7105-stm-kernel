/*
 * linux/arch/arm/kernel/smp_gt.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <asm/mach/irq.h>
#include <asm/smp_gt.h>

#include <mach/hardware.h>

#define GT_COUNTER0	0x00
#define GT_COUNTER1	0x04

#define GT_CONTROL	0x08
#define GT_CONTROL_TIMER_ENABLE		(1<<0)
#define GT_CONTROL_COMP_ENABLE		(1<<1)	/* banked */
#define GT_CONTROL_IRQ_ENABLE		(1<<2)	/* banked */
#define GT_CONTROL_AUTO_INC		(1<<3)	/* banked */

#define GT_INT_STATUS	0x0c
#define GT_INT_STATUS_EVENT_FLAG	(1<<0)

#define GT_COMP0	0x10
#define GT_COMP1	0x14
#define GT_AUTO_INC	0x18

static void __iomem *gt_base;

/* We are clocked by PERIPHCLK */
/* Note we are using a prescaler value of zero currently, so this is
 * the units for all operations. */
static struct clk *gt_clk;
static unsigned long gt_periphclk;

#ifndef CONFIG_CPU_FREQ
#define gt_history	0
#define gt_multiplier	1
#else
static unsigned long gt_multiplier = 1;
static cycle_t gt_history;
#endif

union gt_counter {
	cycle_t cycles;
	struct {
		uint32_t lower;
		uint32_t upper;
	};
};

static union gt_counter gt_counter_read(void)
{
	union gt_counter res;
	uint32_t upper;

	upper = readl(gt_base + GT_COUNTER1);
	do {
		res.upper = upper;
		res.lower = readl(gt_base + GT_COUNTER0);
		upper = readl(gt_base + GT_COUNTER1);
	} while (upper != res.upper);

	return res;
}

static void gt_compare_set(unsigned long delta, int periodic)
{
	union gt_counter counter = gt_counter_read();
	unsigned long ctrl = readl(gt_base + GT_CONTROL);

	BUG_ON(!(ctrl & GT_CONTROL_TIMER_ENABLE));
	BUG_ON(ctrl & (GT_CONTROL_COMP_ENABLE |
		       GT_CONTROL_IRQ_ENABLE |
		       GT_CONTROL_AUTO_INC));

	counter.cycles += delta;
	writel(counter.lower, gt_base + GT_COMP0);
	writel(counter.upper, gt_base + GT_COMP1);

	ctrl |= GT_CONTROL_COMP_ENABLE |
		GT_CONTROL_IRQ_ENABLE;

	if (periodic) {
		writel(delta, gt_base + GT_AUTO_INC);
		ctrl |= GT_CONTROL_AUTO_INC;
	}

	writel(ctrl, gt_base + GT_CONTROL);
}


static void gt_clockevent_set_mode(enum clock_event_mode mode,
				   struct clock_event_device *clk)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		gt_compare_set(gt_periphclk/HZ, 1);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		/* period set, and timer enabled in 'next_event' hook */
		BUG_ON(readl(gt_base + GT_CONTROL) &
		       (GT_CONTROL_COMP_ENABLE |
			GT_CONTROL_IRQ_ENABLE |
			GT_CONTROL_AUTO_INC));
		/* Fall through */
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	default:
		writel(GT_CONTROL_TIMER_ENABLE, gt_base + GT_CONTROL);
		break;
	}
}

static int gt_clockevent_set_next_event(unsigned long evt,
					struct clock_event_device *unused)
{
	gt_compare_set(evt, 0);

	return 0;
}

/* clock_event used when non_SMP (or initial timer when SMP and global
 * timer is used as local timer???) */

static DEFINE_PER_CPU(struct clock_event_device, gt_clockevent);
static irqreturn_t gt_clockevent_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = *(struct clock_event_device **)dev_id;
	writel(GT_INT_STATUS_EVENT_FLAG,
	       gt_base + GT_INT_STATUS);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct clock_event_device __percpu * *gt_evt;
static void __init gt_clockevents_init(struct clock_event_device *clk)
{
	struct clock_event_device **this_cpu_clk;
	int err;

	gt_evt = alloc_percpu(struct clock_event_device *);
	if (!gt_evt) {
		printk(KERN_WARNING "smp-gt: can't allocate memory\n");
		return;
	}
	err = request_percpu_irq(clk->irq, gt_clockevent_interrupt,
				 "gt", gt_evt);
	if (err) {
		printk(KERN_WARNING "smp-gt: can't register interrupt %d (%d)\n",
		       clk->irq, err);
		return;
	}

	clk->name		= "Global Timer CE";
	clk->features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT;
	clk->set_mode	= gt_clockevent_set_mode;
	clk->set_next_event	= gt_clockevent_set_next_event;
	clk->shift = 32;
	clk->mult = div_sc(gt_periphclk, NSEC_PER_SEC, clk->shift);
	clk->max_delta_ns = clockevent_delta2ns(0xffffffff, clk);
	clk->min_delta_ns = clockevent_delta2ns(0xf, clk);

	this_cpu_clk = __this_cpu_ptr(gt_evt);
	*this_cpu_clk = clk;

	clockevents_register_device(clk);
	enable_percpu_irq(clk->irq, IRQ_TYPE_NONE);
}

static cycle_t gt_clocksource_read(struct clocksource *cs)
{
	union gt_counter res = gt_counter_read();

	res.cycles *= gt_multiplier;
	res.cycles += gt_history;

	return res.cycles;
}

#ifdef CONFIG_HIBERNATION
static void gt_clocksource_resume(struct clocksource *cs)
{
	writel(0, gt_base + GT_CONTROL);
	writel(0, gt_base + GT_COUNTER0);
	writel(0, gt_base + GT_COUNTER1);

	writel(GT_CONTROL_TIMER_ENABLE, gt_base + GT_CONTROL);
}

#else
#define gt_clocksource_resume	NULL
#endif

static struct clocksource gt_clocksource = {
	.name	= "Global Timer CS",
	.rating	= 300,
	.read	= gt_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
	.resume = gt_clocksource_resume,
};

static void __init gt_clocksource_init(void)
{
	writel(0, gt_base + GT_CONTROL);
	writel(0, gt_base + GT_COUNTER0);
	writel(0, gt_base + GT_COUNTER1);
	writel(GT_CONTROL_TIMER_ENABLE,	gt_base + GT_CONTROL);

	gt_clocksource.shift = 20;
	gt_clocksource.mult =
		clocksource_hz2mult(gt_periphclk, gt_clocksource.shift);
	clocksource_register(&gt_clocksource);
}
static struct clk *gt_get_clock(void)
{
	struct clk *clk;
	int err;

	clk = clk_get_sys("smp_gt", NULL);
	if (IS_ERR(clk)) {
		pr_err("smp_gt: clock not found: %d\n", (int)PTR_ERR(clk));
		return clk;
	}

	err = clk_prepare(clk);
	if (err) {
		pr_err("smp_gt: clock failed to prepare: %d\n", err);
		clk_put(clk);
		return ERR_PTR(err);
	}

	err = clk_enable(clk);
	if (err) {
		pr_err("smp_gt: clock failed to enable: %d\n", err);
		clk_unprepare(clk);
		clk_put(clk);
		return ERR_PTR(err);
	}

	return clk;
}


void __init global_timer_init(void __iomem *base, unsigned int timer_irq)
{
	unsigned int cpu = smp_processor_id();
	struct clock_event_device *evt = &per_cpu(gt_clockevent, cpu);
	gt_base = base;
	if (!gt_clk)
		gt_clk = gt_get_clock();

	gt_periphclk = clk_get_rate(gt_clk);
	gt_clocksource_init();
	evt->irq = timer_irq;
	evt->cpumask = cpumask_of(cpu);
	gt_clockevents_init(evt);

}


#ifdef CONFIG_OF
static struct of_device_id smp_gt_of_match[] __initconst = {
	{ .compatible = "arm,cortex-a9-global-timer",},
	{ },
};

void __init smp_gt_of_register(void)
{
	struct device_node *np;
	int err = 0;
	int smp_gt_ppi;
	static void __iomem *smp_gt_base;

	np = of_find_matching_node(NULL, smp_gt_of_match);
	if (!np) {
		err = -ENODEV;
		goto out;
	}

	smp_gt_ppi = irq_of_parse_and_map(np, 0);
	if (!smp_gt_ppi) {
		err = -EINVAL;
		goto out;
	}

	smp_gt_base = of_iomap(np, 0);
	if (!smp_gt_base) {
		err = -ENOMEM;
		goto out;
	}

	global_timer_init(smp_gt_base, smp_gt_ppi);


out:
	WARN(err, "twd_local_timer_of_register failed (%d)\n", err);
}
#endif


#ifdef CONFIG_CPU_FREQ

#include <linux/cpufreq.h>

static int gt_cpufre_update(struct notifier_block *nb,
		unsigned long val, void *_data)
{
	unsigned long new_multiplier;
	struct cpufreq_freqs *cpufreq = _data;
	struct clock_event_device **clk;
	unsigned long flags;
	unsigned long periph_timer_rate;

	if (val != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

	/*
	 * Convert the CPUfreq rate (in KHz and CPU_clk based)
	 * to the gt_clk rate
	 */
	periph_timer_rate = (cpufreq->new * 500);
	/*
	 * Evaluate the gt_clocksource multiplier to hide
	 * the clock scaling
	 */
	new_multiplier = gt_periphclk / periph_timer_rate;

	if (!new_multiplier) {
		pr_err("gt_multipler equals zero!\n");
		new_multiplier = 1;
	}
	local_irq_save(flags);
	writel(0, gt_base + GT_CONTROL);

	gt_history += (gt_counter_read().cycles * gt_multiplier);
	gt_multiplier = new_multiplier;

	/*
	 * restart gt_clocksource
	 */
	writel(0, gt_base + GT_COUNTER0);
	writel(0, gt_base + GT_COUNTER1);
	writel(GT_CONTROL_TIMER_ENABLE, gt_base + GT_CONTROL);

	/*
	 * Reconfigure clock_event_device
	 */
	clk = __this_cpu_ptr(gt_evt);
	(*clk)->mult = div_sc(periph_timer_rate, NSEC_PER_SEC, (*clk)->shift);
	(*clk)->max_delta_ns = clockevent_delta2ns(0xffffffff, *clk);
	(*clk)->min_delta_ns = clockevent_delta2ns(0xf, *clk);
	if ((*clk)->mode == CLOCK_EVT_MODE_PERIODIC)
		gt_compare_set(periph_timer_rate, 1);


	local_irq_restore(flags);
	return NOTIFY_OK;
}

static struct notifier_block gt_nb = {
	.notifier_call = gt_cpufre_update,
};

static int __init gt_cpufreq_init(void)
{
	cpufreq_register_notifier(&gt_nb, CPUFREQ_TRANSITION_NOTIFIER);
	return 0;
}

module_init(gt_cpufreq_init);
#endif
