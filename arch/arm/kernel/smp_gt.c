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
#include <linux/io.h>

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
static unsigned long gt_periphclk;

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

	return res.cycles;
}

static struct clocksource gt_clocksource = {
	.name	= "Global Timer CS",
	.rating	= 300,
	.read	= gt_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(64),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

static void __init gt_clocksource_init(void)
{
	writel(0, gt_base + GT_CONTROL);
	writel(0, gt_base + GT_COUNTER0);
	writel(0, gt_base + GT_COUNTER1);
	writel(GT_CONTROL_TIMER_ENABLE, gt_base + GT_CONTROL);

	gt_clocksource.shift = 20;
	gt_clocksource.mult =
		clocksource_hz2mult(gt_periphclk, gt_clocksource.shift);
	clocksource_register(&gt_clocksource);
}

void __init global_timer_init(void __iomem *base, unsigned int timer_irq,
			      unsigned long freq)
{
	unsigned int cpu = smp_processor_id();
	struct clock_event_device *evt = &per_cpu(gt_clockevent, cpu);
	gt_base = base;
	gt_periphclk = freq;
	gt_clocksource_init();
	evt->irq = timer_irq;
	evt->cpumask = cpumask_of(cpu);
	gt_clockevents_init(evt);
}
