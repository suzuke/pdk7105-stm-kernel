/*
 * STMicroelectronics Key Scanning driver
 *
 * Copyright (c) 2012 STMicroelectonics Ltd.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Based on sh_keysc.c, copyright 2008 Magnus Damm
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/stm/platform.h>

#define KEYSCAN_CONFIG_OFF		0x000
#define   KEYSCAN_CONFIG_ENABLE		1
#define KEYSCAN_DEBOUNCE_TIME_OFF	0x004
#define KEYSCAN_MATRIX_STATE_OFF	0x008
#define KEYSCAN_MATRIX_DIM_OFF		0x00c
#define   KEYSCAN_MATRIX_DIM_X_SHIFT	0
#define   KEYSCAN_MATRIX_DIM_Y_SHIFT	2

struct stm_keyscan_priv {
	void __iomem *base;
	int irq;
	struct clk *clk;
	struct input_dev *input;
	struct stm_keyscan_config *config;
	unsigned int last_state;
};

static irqreturn_t stm_keyscan_isr(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct stm_keyscan_priv *priv = platform_get_drvdata(pdev);
	int state;
	int change;

	state = readl(priv->base + KEYSCAN_MATRIX_STATE_OFF) & 0xffff;
	change = priv->last_state ^ state;

	while (change) {
		int scancode = __ffs(change);
		int down = state & (1 << scancode);

		input_report_key(priv->input, priv->config->keycodes[scancode],
				 down);
		change ^= 1 << scancode;
	};

	input_sync(priv->input);

	priv->last_state = state;

	return IRQ_HANDLED;
}

static void stm_keyscan_start(struct stm_keyscan_priv *priv)
{
	clk_enable(priv->clk);

	writel(priv->config->debounce_us * (clk_get_rate(priv->clk) / 1000000),
	       priv->base + KEYSCAN_DEBOUNCE_TIME_OFF);
	writel(((priv->config->num_in_pads - 1) << KEYSCAN_MATRIX_DIM_X_SHIFT) |
	       ((priv->config->num_out_pads - 1) << KEYSCAN_MATRIX_DIM_Y_SHIFT),
	       priv->base + KEYSCAN_MATRIX_DIM_OFF);
	writel(KEYSCAN_CONFIG_ENABLE, priv->base + KEYSCAN_CONFIG_OFF);
}

static void stm_keyscan_stop(struct stm_keyscan_priv *priv)
{
	writel(0, priv->base + KEYSCAN_CONFIG_OFF);

	clk_disable(priv->clk);
}

static int __devinit stm_keyscan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm_plat_keyscan_data *pdata = dev->platform_data;
	struct stm_keyscan_priv *priv;
	struct resource *res;
	int irq;
	int len;
	struct input_dev *input;
	int error;
	int i;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "no I/O memory specified\n");
		return -ENXIO;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "no IRQ specified\n");
		return -ENXIO;
	}

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, priv);
	priv->config = &pdata->keyscan_config;

	len = resource_size(res);
	if (!devm_request_mem_region(dev, res->start, len, pdev->name)) {
		dev_err(dev, "failed to reserve I/O memory\n");
		return -EBUSY;
	}

	priv->base = devm_ioremap_nocache(dev, res->start, len);
	if (priv->base == NULL) {
		dev_err(dev, "failed to remap I/O memory\n");
		return -ENXIO;
	}

	error = devm_request_irq(dev, irq, stm_keyscan_isr, 0,
				 pdev->name, pdev);
	if (error) {
		dev_err(dev, "failed to request IRQ\n");
		return error;
	}
	priv->irq = irq;

	priv->clk = devm_clk_get(dev, NULL);
	if (IS_ERR(priv->clk)) {
		dev_err(dev, "cannot get clock");
		return PTR_ERR(priv->clk);
	}

	if (!devm_stm_pad_claim(dev, pdata->pad_config, pdev->name))
		return -EBUSY;

	priv->input = input_allocate_device();
	if (!priv->input) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	input = priv->input;
	input->evbit[0] = BIT_MASK(EV_KEY);

	input->name = pdev->name;
	input->phys = "stm-keyscan-keys/input0";
	input->dev.parent = dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Mapping from scancodes to keycodes */
	input->keycode = priv->config->keycodes;
	input->keycodesize = sizeof(priv->config->keycodes[0]);
	input->keycodemax = ARRAY_SIZE(priv->config->keycodes);

	for (i = 0; i < STM_KEYSCAN_MAXKEYS; i++)
		__set_bit(priv->config->keycodes[i], input->keybit);
	__clear_bit(KEY_RESERVED, input->keybit);

	error = input_register_device(input);
	if (error) {
		dev_err(dev, "failed to register input device\n");
		input_free_device(input);
		platform_set_drvdata(pdev, NULL);
		return error;
	}

	stm_keyscan_start(priv);

	device_init_wakeup(&pdev->dev, 1);

	return 0;
}

static int __devexit stm_keyscan_remove(struct platform_device *pdev)
{
	struct stm_keyscan_priv *priv = platform_get_drvdata(pdev);

	stm_keyscan_stop(priv);

	input_unregister_device(priv->input);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int stm_keyscan_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_keyscan_priv *priv = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		enable_irq_wake(priv->irq);
	else
		stm_keyscan_stop(priv);

	return 0;
}

static int stm_keyscan_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_keyscan_priv *priv = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev))
		disable_irq_wake(priv->irq);
	else
		stm_keyscan_start(priv);

	return 0;
}

static const struct dev_pm_ops stm_keyscan_dev_pm_ops = {
	.suspend = stm_keyscan_suspend,
	.resume = stm_keyscan_resume,
};

struct platform_driver stm_keyscan_device_driver = {
	.probe		= stm_keyscan_probe,
	.remove		= __devexit_p(stm_keyscan_remove),
	.driver		= {
		.name	= "stm-keyscan",
		.pm	= &stm_keyscan_dev_pm_ops,
	}
};

static int __init stm_keyscan_init(void)
{
	return platform_driver_register(&stm_keyscan_device_driver);
}

static void __exit stm_keyscan_exit(void)
{
	platform_driver_unregister(&stm_keyscan_device_driver);
}

module_init(stm_keyscan_init);
module_exit(stm_keyscan_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics keyscan device driver");
MODULE_LICENSE("GPL");
