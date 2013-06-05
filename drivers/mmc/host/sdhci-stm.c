/*
 * sdhci-stm.c Support for SDHCI on STMicroelectronics SoCs
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 * Based on sdhci-cns3xxx.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/mmc/host.h>
#include <linux/stm/mmc.h>
#include <linux/stm/platform.h>

#include "sdhci-pltfm.h"

static int sdhci_stm_8bit_width(struct sdhci_host *host, int width)
{
	u8 ctrl;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);

	switch (width) {
	case MMC_BUS_WIDTH_8:
		ctrl |= SDHCI_CTRL_8BITBUS;
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl |= SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	default:
		ctrl &= ~(SDHCI_CTRL_8BITBUS | SDHCI_CTRL_4BITBUS);
		break;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	return 0;
}

static unsigned int sdhci_stm_get_max_clk(struct sdhci_host *host)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	return clk_get_rate(pltfm_host->clk);
}

static u32 sdhci_stm_readl(struct sdhci_host *host, int reg)
{
	u32 ret;

	switch (reg) {
	case SDHCI_CAPABILITIES:
		ret = readl(host->ioaddr + reg);
		/* Only support 3.3V */
		ret &= ~(SDHCI_CAN_VDD_180 | SDHCI_CAN_VDD_300);
		break;
	default:
		ret = readl(host->ioaddr + reg);
	}
	return ret;
}

static struct sdhci_ops sdhci_stm_ops = {
	.get_max_clock = sdhci_stm_get_max_clk,
	.platform_8bit_width = sdhci_stm_8bit_width,
	.read_l = sdhci_stm_readl,
};

static struct sdhci_pltfm_data sdhci_stm_pdata = {
	.ops = &sdhci_stm_ops,
	.quirks = SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC |
	    SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN,
};

#ifdef CONFIG_OF
static void __devinit *stm_sdhci_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_mmc_platform_data *priv_data = devm_kzalloc(
		&pdev->dev, sizeof(*priv_data), GFP_KERNEL);

	if (!priv_data) {
		dev_err(&pdev->dev, "Unable to allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	priv_data->custom_cfg = stm_of_get_pad_config(&pdev->dev);
	priv_data->nonremovable =
		of_property_read_bool(np, "st,mmc-non-removable");
	priv_data->amba_config = stm_of_get_amba_config(&pdev->dev);

	return priv_data;
}
#else
static void __devinit *stm_sdhci_dt_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

static int __devinit sdhci_stm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct stm_mmc_platform_data *pdata;
	struct sdhci_pltfm_host *pltfm_host;
	struct clk *clk;
	int ret = 0;

	pr_debug("sdhci STM platform driver\n");

	if (pdev->dev.of_node)
		pdev->dev.platform_data = stm_sdhci_dt_get_pdata(pdev);

	if (!pdev->dev.platform_data || IS_ERR(pdev->dev.platform_data)) {
		dev_err(&pdev->dev, "No platform data found\n");
		return -ENODEV;
	}

	pdata = pdev->dev.platform_data;

	if (pdata->custom_cfg) {
		pdata->custom_data = devm_stm_pad_claim(&pdev->dev,
			(struct stm_pad_config *)(pdata->custom_cfg),
			dev_name(&pdev->dev));
		if (!pdata->custom_data) {
			dev_err(&pdev->dev, "Failed on pad_claim\n");
			return -ENODEV;
		}
	} else
		pr_warning("%s: no custom_cfg found\n", __func__);

	host = sdhci_pltfm_init(pdev, &sdhci_stm_pdata);
	if (IS_ERR(host))
		return PTR_ERR(host);

	host->mmc->caps |= MMC_CAP_8_BIT_DATA | MMC_CAP_BUS_WIDTH_TEST;

	/* To manage eMMC */
	if (pdata->nonremovable)
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	clk = clk_get(mmc_dev(host->mmc), NULL);
	if (IS_ERR(clk)) {
		dev_err(mmc_dev(host->mmc), "clk err\n");
		return PTR_ERR(clk);
	}
	clk_prepare_enable(clk);
	pltfm_host = sdhci_priv(host);
	pltfm_host->clk = clk;

	ret = sdhci_add_host(host);
	if (ret) {
		sdhci_pltfm_free(pdev);
		goto err_out;
	}

	if (pdata->amba_config) {

		pdata->amba_bridge = stm_amba_bridge_create(host->mmc->parent,
							    host->ioaddr +
							    MMC_AHB2STBUS_BASE,
							    pdata->amba_config);
		if (IS_ERR(pdata->amba_bridge)) {
			dev_err(host->mmc->parent, "Cannot create amba plug\n");
			ret = PTR_ERR(pdata->amba_bridge);
			goto err_out;
		}

		stm_amba_bridge_init(pdata->amba_bridge);
	} else
		pr_warning("%s: amba bridge not supported\n", __func__);

	platform_set_drvdata(pdev, host);

	return ret;

err_out:
	clk_disable_unprepare(clk);
	clk_put(clk);

	return ret;
}

static int __devexit sdhci_stm_remove(struct platform_device *pdev)
{
	struct stm_mmc_platform_data *pdata = pdev->dev.platform_data;
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);

	if (pdata->custom_data)
		devm_stm_pad_release(&pdev->dev, pdata->custom_data);
	clk_disable_unprepare(pltfm_host->clk);
	clk_put(pltfm_host->clk);

	return sdhci_pltfm_unregister(pdev);
}

#ifdef CONFIG_PM
static int sdhci_stm_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	int ret = sdhci_suspend_host(host);

	if (ret)
		goto out;

	if (pltfm_host->clk)
		clk_disable_unprepare(pltfm_host->clk);
out:
	return ret;
}

static int sdhci_stm_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct stm_mmc_platform_data *pdata = dev_get_platdata(dev);

	if (pltfm_host->clk)
		clk_prepare_enable(pltfm_host->clk);

	if (pdata->amba_bridge)
		stm_amba_bridge_init(pdata->amba_bridge);

	return sdhci_resume_host(host);
}

static int sdhci_stm_freeze(struct device *dev)
{
	struct stm_mmc_platform_data *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);

	if (pdata->custom_data) {
		devm_stm_pad_release(&pdev->dev, pdata->custom_data);
		pdata->custom_data = NULL;
	}
	return sdhci_stm_suspend(dev);
}

static int sdhci_stm_restore(struct device *dev)
{
	struct stm_mmc_platform_data *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);

	if (pdata->custom_cfg)
		pdata->custom_data = devm_stm_pad_claim(&pdev->dev,
			(struct stm_pad_config *)(pdata->custom_cfg),
			dev_name(&pdev->dev));
	return sdhci_stm_resume(dev);
}

const struct dev_pm_ops sdhci_stm_pmops = {
	.suspend = sdhci_stm_suspend,
	.resume = sdhci_stm_resume,
	.freeze = sdhci_stm_freeze,
	.thaw = sdhci_stm_restore,
	.restore = sdhci_stm_restore,
};

#define SDHCI_STM_PMOPS (&sdhci_stm_pmops)
#else
#define SDHCI_STM_PMOPS NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
static struct of_device_id stm_sdhci_match[] = {
	{
		.compatible = "st,sdhci",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_sdhci_match);
#endif

static struct platform_driver sdhci_stm_driver = {
	.driver = {
		   .name = "sdhci-stm",
		   .owner = THIS_MODULE,
		   .pm = SDHCI_STM_PMOPS,
			.of_match_table = of_match_ptr(stm_sdhci_match),
		   },
	.probe = sdhci_stm_probe,
	.remove = __devexit_p(sdhci_stm_remove),
};

static int __init sdhci_stm_init(void)
{
	return platform_driver_register(&sdhci_stm_driver);
}

module_init(sdhci_stm_init);

static void __exit sdhci_stm_exit(void)
{
	platform_driver_unregister(&sdhci_stm_driver);
}

module_exit(sdhci_stm_exit);

MODULE_DESCRIPTION("SDHCI driver for STMicroelectronics SoCs");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL v2");
