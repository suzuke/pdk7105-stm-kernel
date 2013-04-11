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
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/ahci_platform.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>

#define AHCI_OOBR		0xbc
#define AHCI_OOBR_WE		(1<<31)
#define AHCI_OOBR_CWMIN_SHIFT	24
#define AHCI_OOBR_CWMAX_SHIFT	16
#define AHCI_OOBR_CIMIN_SHIFT	8
#define AHCI_OOBR_CIMAX_SHIFT	0

struct ahci_stm_drv_data {
	struct clk *clk;
	struct stm_device_state *device_state;
	struct stm_amba_bridge *amba_bridge;
	void __iomem *amba_base;
	struct platform_device *ahci;
	struct stm_miphy *miphy;
};

static u64 stm_ahci_dma_mask = DMA_BIT_MASK(32);
static void *ahci_stm_get_platdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_ahci_data *data;
	if (!np)
		return dev_get_platdata(&pdev->dev);

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	of_property_read_u32(np, "miphy-num", (u32 *)&data->miphy_num);
	data->device_config = stm_of_get_dev_config(&pdev->dev);
	pdev->dev.platform_data = data;
	return data;
}

static void ahci_stm_configure_oob(void __iomem *mmio)
{
	unsigned long old_val, new_val;

	new_val = (0x02 << AHCI_OOBR_CWMIN_SHIFT) |
		  (0x04 << AHCI_OOBR_CWMAX_SHIFT) |
		  (0x08 << AHCI_OOBR_CIMIN_SHIFT) |
		  (0x0C << AHCI_OOBR_CIMAX_SHIFT);

	old_val = readl(mmio + AHCI_OOBR);
	writel(old_val | AHCI_OOBR_WE, mmio + AHCI_OOBR);
	writel(new_val | AHCI_OOBR_WE, mmio + AHCI_OOBR);
	writel(new_val, mmio + AHCI_OOBR);
}

static int ahci_stm_init(struct device *ahci_dev, void __iomem *mmio)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_stm_drv_data *drv_data = platform_get_drvdata(pdev);
	struct stm_plat_ahci_data *pdata = ahci_stm_get_platdata(pdev);
	struct resource *res;
	int ret;

	drv_data->clk = devm_clk_get(ahci_dev, "ahci_clk");
	if (IS_ERR(drv_data->clk))
		return PTR_ERR(drv_data->clk);

	ret = clk_prepare_enable(drv_data->clk);
	if (ret)
		return ret;

	drv_data->device_state =
		devm_stm_device_init(ahci_dev, pdata->device_config);
	if (!drv_data->device_state) {
		ret = -EBUSY;
		goto fail_clk_disable;
	}

	drv_data->miphy = stm_miphy_claim(pdata->miphy_num, SATA_MODE,
		ahci_dev);
	if (!drv_data->miphy) {
		ret = -EBUSY;
		goto fail_clk_disable;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ahci-amba");
	if ((res && !pdata->amba_config) || (!res && pdata->amba_config)) {
		ret = -EINVAL;
		goto fail_miphy_release;
	}

	if (pdata->amba_config) {
		unsigned long phys_base, phys_size;

		phys_base = res->start;
		phys_size = resource_size(res);

		if (!devm_request_mem_region(ahci_dev, phys_base,
			phys_size, "ahci-amba")) {
			ret = -EBUSY;
			goto fail_miphy_release;
		}

		drv_data->amba_base = devm_ioremap(ahci_dev, phys_base,
			phys_size);
		if (!drv_data->amba_base) {
			ret = -ENOMEM;
			goto fail_miphy_release;
		}

		drv_data->amba_bridge = stm_amba_bridge_create(ahci_dev,
			drv_data->amba_base, pdata->amba_config);
		if (!drv_data->amba_bridge) {
			ret = -ENOMEM;
			goto fail_miphy_release;
		}

		stm_amba_bridge_init(drv_data->amba_bridge);
	}

	ahci_stm_configure_oob(mmio);

	return 0;

fail_miphy_release:
	stm_miphy_release(drv_data->miphy);
fail_clk_disable:
	clk_disable_unprepare(drv_data->clk);
	return ret;
};

static void ahci_stm_exit(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_stm_drv_data *drv_data = platform_get_drvdata(pdev);

	stm_miphy_release(drv_data->miphy);
	stm_device_power(drv_data->device_state, stm_device_power_off);
	clk_disable_unprepare(drv_data->clk);
}

#ifdef CONFIG_PM
static int ahci_stm_suspend(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_stm_drv_data *drv_data = platform_get_drvdata(pdev);

	stm_device_power(drv_data->device_state, stm_device_power_off);
	clk_disable_unprepare(drv_data->clk);

	return 0;
}

static int ahci_stm_resume(struct device *ahci_dev)
{
	struct platform_device *pdev = to_platform_device(ahci_dev->parent);
	struct ahci_stm_drv_data *drv_data = platform_get_drvdata(pdev);

	clk_prepare_enable(drv_data->clk);
	stm_device_power(drv_data->device_state, stm_device_power_on);
	if (drv_data->amba_bridge)
		stm_amba_bridge_init(drv_data->amba_bridge);

	return 0;
}
#endif

static struct ahci_platform_data ahci_stm_platform_data = {
	.init = ahci_stm_init,
	.exit = ahci_stm_exit,
#ifdef CONFIG_PM
	.suspend = ahci_stm_suspend,
	.resume = ahci_stm_resume,
#endif
};

static int __devinit ahci_stm_driver_probe(struct platform_device *pdev)
{
	struct ahci_stm_drv_data *drv_data;
	struct device *dev = &pdev->dev;
	struct resource ahci_resource[2];
	struct resource *res;
	struct platform_device_info ahci_info = {
		.parent = dev,
		.name = "ahci",
		.id = pdev->id,
		.res = ahci_resource,
		.num_res = 2,
		.data = &ahci_stm_platform_data,
		.size_data = sizeof(ahci_stm_platform_data),
	};

	if (dev->of_node) {
		ahci_info.id = 	of_alias_get_id(dev->of_node, "sata");
		ahci_info.dma_mask = stm_ahci_dma_mask;
	} else if (dev->dma_mask) {	
		ahci_info.dma_mask = *dev->dma_mask;
	}

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, drv_data);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ahci");
	if (!res)
		return -EINVAL;
	ahci_resource[0] = *res;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -EINVAL;
	ahci_resource[1] = *res;

	drv_data->ahci = platform_device_register_full(&ahci_info);
	if (IS_ERR(drv_data->ahci))
		return PTR_ERR(drv_data->ahci);

	return 0;
}

static int ahci_stm_remove(struct platform_device *pdev)
{
	struct ahci_stm_drv_data *drv_data = platform_get_drvdata(pdev);

	platform_device_unregister(drv_data->ahci);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_ahci_match[] = {
	{
		.compatible = "st,ahci",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_ahci_match);
#endif

static struct platform_driver ahci_stm_driver = {
	.driver.name = "ahci_stm",
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(stm_ahci_match),
	.probe = ahci_stm_driver_probe,
	.remove = ahci_stm_remove,
};

module_platform_driver(ahci_stm_driver);
