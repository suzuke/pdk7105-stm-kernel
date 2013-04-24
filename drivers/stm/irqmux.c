/*
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author: Francesco Virlinzi <francesco.virlinzist.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>
#include <asm/irq-ilc.h>
#ifdef CONFIG_ARM
#include <mach/hardware.h>
#endif

#define IRQMUX_OUTPUT_MASK		(0x3fffffff)
#define IRQMUX_INPUT_ENABLE		(1 << 31)
#define IRQMUX_INPUT_INVERT		(1 << 30)


static DEFINE_MUTEX(irqmux_list_mutex);
static LIST_HEAD(irqmux_list);

struct irq_mux_drv_data {
	struct list_head list;
	void __iomem *base;
	struct platform_device *pdev;
};

static int apply_irqmux_mapping(struct device *dev)
{
	struct irq_mux_drv_data *drv_data = dev_get_drvdata(dev);
	struct stm_plat_irq_mux_data *pdata = dev_get_platdata(dev);
	int ret;

	if (pdata->custom_mapping) {
		long input, output, invert, enable;
		for (input = 0; input < pdata->num_input; ++input) {
			long mapping_value;
			output = invert = enable = 0;
			ret = pdata->custom_mapping(dev,
					input, &enable, &output, &invert);
			if (ret) {
				dev_err(dev, "Mapping failed for input %ld\n",
					input);
				return ret;
			}
			mapping_value = (output & IRQMUX_OUTPUT_MASK);
			mapping_value |= (enable ? IRQMUX_INPUT_ENABLE : 0);
			mapping_value |= (invert ? IRQMUX_INPUT_INVERT : 0);
			writel(mapping_value, drv_data->base + input * 4);
		}
	}

	return 0;
}

static int of_generic_irqmux_mapping(struct device *dev,
		long input, long *enable, long *output, long *inv)
{
	struct stm_plat_irq_mux_data *pdata = dev_get_platdata(dev);
	struct device_node *np;
	np = dev->of_node;
	if (of_property_read_bool(np, "interrupts-enable"))
		*enable = 1;
	else
		*enable = 0;
	if (of_property_read_bool(np, "interrupts-invert"))
		*inv = 1;
	else
		*inv = 0;
	*output = input % pdata->num_output;
	return 0;
}

static int of_custom_irqmux_mapping(struct device *dev,
		long input, long *enable, long *output, long *inv)
{
	struct device_node *np;
	union {
		u32 array[5];
		struct {
			u32 begin;
			u32 size;
			u32 output;
			u32 enabled;
			u32 inverted;
		};
	} irqmux_map_entry;
	const __be32 *list;
	const struct property *pp;
	int remaining;

	np = dev->of_node;
	pp = of_find_property(np, "irqmux-map", NULL);
	BUG_ON(!pp);

	list = pp->value;
	remaining = pp->length / sizeof(*list);
	while (remaining >= ARRAY_SIZE(irqmux_map_entry.array)) {
		int i;
		for (i = 0; i < ARRAY_SIZE(irqmux_map_entry.array); ++i)
			irqmux_map_entry.array[i] = be32_to_cpup(list++);

		if (input >= irqmux_map_entry.begin &&
		    input < (irqmux_map_entry.begin + irqmux_map_entry.size))
			goto found;

		remaining -= ARRAY_SIZE(irqmux_map_entry.array);
	}

	return -EINVAL;

found:
	*enable = irqmux_map_entry.enabled;
	*inv = irqmux_map_entry.inverted;
	*output = irqmux_map_entry.output;

	return 0;
}

static void *irq_mux_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_irq_mux_data *data;
	u32 num_inputs, num_outputs;
	if (!np)
		return dev_get_platdata(&pdev->dev);

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	of_property_read_string(np, "irqmux-name", (const char **)&data->name);
	of_property_read_u32(np, "irqmux-inputs", &num_inputs);
	of_property_read_u32(np, "irqmux-outputs", &num_outputs);
	data->num_output = num_outputs;
	data->num_input = num_inputs;
	pdev->dev.platform_data = data;

	if (of_find_property(np, "irqmux-map", NULL))
		data->custom_mapping = of_custom_irqmux_mapping;
	else
		data->custom_mapping = of_generic_irqmux_mapping;

	return data;
}
static int __devinit irq_mux_driver_probe(struct platform_device *pdev)
{
	struct stm_plat_irq_mux_data *pdata;
	struct irq_mux_drv_data *drv_data;
	struct resource *res;
	int ret;

	pdata = irq_mux_get_pdata(pdev);

	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->pdev = pdev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drv_data->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv_data->base)
		return -ENOMEM;

	platform_set_drvdata(pdev, drv_data);

	ret = apply_irqmux_mapping(&pdev->dev);
	if (ret)
	     return ret;

	mutex_lock(&irqmux_list_mutex);
	list_add_tail(&drv_data->list, &irqmux_list);
	mutex_unlock(&irqmux_list_mutex);

	dev_info(&pdev->dev, "%s configured\n", pdata->name);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_irqmux_match[] = {
	{
		.compatible = "st,irqmux",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_irqmux_match);
#endif

static struct platform_driver irq_mux_driver = {
	.driver.name = "irq_mux",
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(stm_irqmux_match),
	.probe = irq_mux_driver_probe,
};

static int __init irqmux_init(void)
{
	return platform_driver_register(&irq_mux_driver);
}
core_initcall(irqmux_init);

#ifdef CONFIG_HIBERNATION
static void irqmux_resume(void)
{
	struct irq_mux_drv_data *irq_mux;

	list_for_each_entry(irq_mux, &irqmux_list, list)
		apply_irqmux_mapping(&irq_mux->pdev->dev);
}

static struct syscore_ops irqmux_syscore = {
	.resume = irqmux_resume,
};

static int __init irqmux_syscore_init(void)
{
	register_syscore_ops(&irqmux_syscore);
	return 0;
}
module_init(irqmux_syscore_init);
#endif
