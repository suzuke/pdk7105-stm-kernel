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
#include <linux/module.h>
#include <linux/err.h>
#include <linux/mmc/host.h>
#include <linux/stm/mmc.h>

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

static int __devinit sdhci_stm_probe(struct platform_device *pdev)
{
	struct sdhci_host *host;
	struct stm_mmc_platform_data *pdata;
	struct sdhci_pltfm_host *pltfm_host;
	struct clk *clk;
	int ret;

	pr_debug("sdhci STM platform driver\n");

	pdata = pdev->dev.platform_data;

	host = sdhci_pltfm_init(pdev, &sdhci_stm_pdata);
	if (IS_ERR(host))
		return PTR_ERR(host);

	host->mmc->caps |= MMC_CAP_8_BIT_DATA | MMC_CAP_BUS_WIDTH_TEST;

	/* To manage eMMC */
	if (pdata->nonremovable)
		host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	/* Invoke specific MMC function to configure HW resources */
	if (pdata && pdata->init) {
		ret = pdata->init(pdev);
		if (ret)
			return ret;
	}

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

	if (pdata && pdata->exit)
		pdata->exit(pdev);

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

	if (pdata && pdata->exit)
		pdata->exit(pdev);

	return sdhci_stm_suspend(dev);
}

static int sdhci_stm_restore(struct device *dev)
{
	int ret;
	struct stm_mmc_platform_data *pdata = dev_get_platdata(dev);
	struct platform_device *pdev = to_platform_device(dev);

	if (pdata && pdata->init) {
		ret = pdata->init(pdev);
		if (ret)
			return ret;
	}
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

static struct platform_driver sdhci_stm_driver = {
	.driver = {
		   .name = "sdhci-stm",
		   .owner = THIS_MODULE,
		   .pm = SDHCI_STM_PMOPS,
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
