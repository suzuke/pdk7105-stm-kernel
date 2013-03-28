/*
 * Copyright (C) 2012 STMicroelectronics Limited.
 *
 * Author(s): Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of.h>
#include <linux/stm/device.h>
#include <linux/of_platform.h>

#include <asm/smp_twd.h>
#include <asm/smp_gt.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>

static const struct of_device_id dt_irq_match[] = {
	{ .compatible = "arm,cortex-a9-gic", .data = gic_of_init, },
	{},
};
/* GIC */
void __init stm_of_gic_init(void)
{
	of_irq_init(dt_irq_match);
}

/* Global and Local timer setup */
void __init stm_of_timer_init(void)
{
	smp_gt_of_register();
	twd_local_timer_of_register();
}

/* L2 Setup */
static int __init stm_of_l2x0_init(void)
{

	u32 way_size = 0x4;
	u32 aux_ctrl;

	if (of_machine_is_compatible("st,stig125"))
		way_size = 0x3;

	aux_ctrl = (0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_DATA_PREFETCH_SHIFT) |
		(0x1 << L2X0_AUX_CTRL_INSTR_PREFETCH_SHIFT) |
		(way_size << L2X0_AUX_CTRL_WAY_SIZE_SHIFT);
#define STM_L2X0_AUX_CTRL_MASK \
	(0xffffffff & ~(L2X0_AUX_CTRL_WAY_SIZE_MASK | \
			L2X0_AUX_CTRL_SHARE_OVERRIDE_MASK | \
			L2X0_AUX_CTRL_PREFETCH_MASK))
	return l2x0_of_init(aux_ctrl, STM_L2X0_AUX_CTRL_MASK);

}
arch_initcall(stm_of_l2x0_init);



static struct class *stm_of_class;
struct platform_device *stm_of_platform_device_create(
			struct device_node *np, const char *name)
{
	struct stm_device_config *cfg;
	struct device *dev;
	if (!stm_of_class)
		stm_of_class = class_create(NULL, "stm-of");
	dev = device_create(stm_of_class, NULL, 0, NULL, "stm-of-%s", name);

	cfg = stm_of_get_dev_config_from_node(dev,
			of_parse_phandle(np, "st,device-config", 0));

	stm_device_init(cfg, dev);
	of_node_put(np);
	return of_platform_device_create(np, name, dev);
}
/* PMU Setup */
static int __init stm_of_pmu_init(void)
{
	struct device_node *np;
	struct of_device_id matches[] = {
		{ .compatible = "arm,cortex-a9-pmu", },
		{},
	};
	for_each_matching_node(np, matches) {
		stm_of_platform_device_create(np, "arm-pmu");
	}
	return 0;
}
device_initcall(stm_of_pmu_init);
