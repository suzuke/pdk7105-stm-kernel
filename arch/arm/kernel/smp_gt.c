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

/* We are clocked by PERIPHCLK */
/* Assume this is 100MHz for now (as QEMU does) */
/* Note we are using a prescaler value of zero currently, so this is
 * the units for all operations. */
#define PERIPHCLK	100000000

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
	switch(mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		gt_compare_set(PERIPHCLK/HZ, 1);
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
static struct clock_event_device gt_clockevent = {
	.name		= "Global Timer CE",
	.features	= CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_mode	= gt_clockevent_set_mode,
	.set_next_event	= gt_clockevent_set_next_event,
	.rating		= 300,
	.cpumask	= cpu_all_mask,
};

#ifndef CONFIG_SMP
static irqreturn_t gt_clockevent_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	writel(GT_INT_STATUS_EVENT_FLAG,
	       gt_base + GT_INT_STATUS);

	evt->event_handler(evt);

	return IRQ_HANDLED;
}

static struct irqaction gt_clockevent_irq = {
	.handler = gt_clockevent_interrupt,
	.dev_id = &gt_clockevent,
	.flags = IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
};
#endif

#ifdef CONFIG_SMP
/*
 * local_timer_ack: checks for a local timer interrupt.
 *
 * If a local timer interrupt has occurred, acknowledge and return 1.
 * Otherwise, return 0.
 */
int twd_timer_ack(void)
{
	if (__raw_readl(gt_base + GT_INT_STATUS)) {
		__raw_writel(1, gt_base + GT_INT_STATUS);
		return 1;
	}

	return 0;
}
#endif

static void __init gt_clockevents_init(int timer_irq)
{
	unsigned long flags;

	gt_clockevent.irq = timer_irq;
	gt_clockevent.shift = 32;
	gt_clockevent.mult =
		div_sc(PERIPHCLK, NSEC_PER_SEC, gt_clockevent.shift);
	gt_clockevent.max_delta_ns =
		clockevent_delta2ns(0xffffffff, &gt_clockevent);
	gt_clockevent.min_delta_ns =
		clockevent_delta2ns(0xf, &gt_clockevent);

#ifdef CONFIG_SMP
	/* Make sure our local interrupt controller has this enabled */
	local_irq_save(flags);
	get_irq_chip(timer_irq)->unmask(timer_irq);
	local_irq_restore(flags);
#else
	gt_clockevent_irq.name = gt_clockevent.name;
	gt_clockevent_irq.irq = timer_irq;

	setup_irq(timer_irq, &gt_clockevent_irq);
#endif

	clockevents_register_device(&gt_clockevent);
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
		clocksource_hz2mult(PERIPHCLK, gt_clocksource.shift);
	clocksource_register(&gt_clocksource);
}

void __init global_timer_init(void __iomem *base, unsigned int timer_irq)
{
	gt_base = base;

	gt_clocksource_init();
	gt_clockevents_init(timer_irq);
}
