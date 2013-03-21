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
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/ahci_platform.h>
#include <linux/pm_clock.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>

#define AHCI_OOBR	0xbc

struct ahci_stm_drv_data {
	struct clk *clk;
	struct stm_device_state *device_state;
	struct platform_device *ahci;
	struct stm_miphy *miphy;
};

static int ahci_stm_init(struct device *dev, void __iomem *mmio)
{
	writel(0x80000000, mmio + AHCI_OOBR);
	writel(0x8204080C, mmio + AHCI_OOBR);
	writel(0x0204080C, mmio + AHCI_OOBR);

	return 0;
};

static struct ahci_platform_data ahci_stm_platform_data = {
	.init = ahci_stm_init,
};

static int __devinit ahci_stm_driver_probe(struct platform_device *pdev)
{
	struct stm_plat_ahci_data *pdata = dev_get_platdata(&pdev->dev);
	struct ahci_stm_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct platform_device_info ahci_info = {
		.parent = dev,
		.name = "ahci",
		.id = pdev->id,
		.res = pdev->resource,
		.num_res = pdev->num_resources,
		.data = &ahci_stm_platform_data,
		.size_data = sizeof(ahci_stm_platform_data),
		.dma_mask = *dev->dma_mask,
	};
	int ret;

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	drv_data->clk = devm_clk_get(dev, "ahci_clk");
	if (IS_ERR(drv_data->clk))
		return PTR_ERR(drv_data->clk);

	pm_clk_init(dev);
	pm_clk_add(dev, "ahci_clk");

	ret = clk_prepare_enable(drv_data->clk);
	if (ret)
		goto fail_pm_cleanup;

	drv_data->device_state =
		devm_stm_device_init(&pdev->dev, pdata->device_config);
	if (!drv_data->device_state) {
		ret = -EBUSY;
		goto fail_clk_disable;
	}

	drv_data->miphy = stm_miphy_claim(pdata->miphy_num, SATA_MODE, dev);
	if (!drv_data->miphy) {
		ret = -EBUSY;
		goto fail_clk_disable;
	}

	platform_set_drvdata(pdev, drv_data);

	drv_data->ahci = platform_device_register_full(&ahci_info);
	if (IS_ERR(drv_data->ahci)) {
		ret = PTR_ERR(drv_data->ahci);
		goto fail_free_miphy;
	}

	return 0;

fail_free_miphy:
	stm_miphy_release(drv_data->miphy);
fail_clk_disable:
	clk_disable_unprepare(drv_data->clk);
fail_pm_cleanup:
	pm_clk_destroy(&pdev->dev);
	return ret;
}

#ifdef CONFIG_PM
static int ahci_stm_suspend(struct device *dev)
{
	struct ahci_stm_drv_data *drv_data = dev_get_drvdata(dev);

	stm_device_power(drv_data->device_state, stm_device_power_off);
	pm_clk_suspend(dev);

	return 0;
}

static int ahci_stm_resume(struct device *dev)
{
	struct ahci_stm_drv_data *drv_data = dev_get_drvdata(dev);

	pm_clk_resume(dev);
	stm_device_power(drv_data->device_state, stm_device_power_on);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ahci_stm_pm_ops, ahci_stm_suspend, ahci_stm_resume);

static int ahci_stm_remove(struct platform_device *pdev)
{
	struct ahci_stm_drv_data *drv_data = dev_get_drvdata(&pdev->dev);

	stm_miphy_release(drv_data->miphy);

	pm_clk_destroy(&pdev->dev);

	stm_device_power(drv_data->device_state, stm_device_power_off);

	platform_device_unregister(drv_data->ahci);

	return 0;
}

static struct platform_driver ahci_stm_driver = {
	.driver.name = "ahci_stm",
	.driver.owner = THIS_MODULE,
	.driver.pm = &ahci_stm_pm_ops,
	.probe = ahci_stm_driver_probe,
	.remove = ahci_stm_remove,
};

module_platform_driver(ahci_stm_driver);
