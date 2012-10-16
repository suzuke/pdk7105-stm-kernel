/*
 * drivers/stm/miphy_pcie_mp.c
 *
 *  Copyright (c) 2010 STMicroelectronics Limited
 *  Author: Srinivas.Kandagatla <srinivas.kandagatla@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 *
 * Support for the UPort interface to MiPhy.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/miphy.h>
#include "miphy.h"

#define NAME	"pcie-mp"

struct pcie_mp_device{
	struct stm_miphy_device miphy_dev;
	void __iomem *pcie_base; /* Base for uport connected to PCIE */
	void __iomem *sata_base; /* Base for uport connected to SATA */
	void __iomem *pipe_base; /* Pipe registers, standard for PCIe phy */ 
	void (*mp_select)(void *data, int port);
	void *priv_data;
};

/*
 * Select which base address we should talk to the miphy through. In older
 * parts, the miphy was only connected to the uport on the PCIe controller. In
 * newer parts there is a uport on both the PCIe controller and the sata
 * controller, which one actually talks to the miphy is controlled by a mux
 * connected to a sysconf which selects which one to use. Therefore, if we have
 * a pcie port and no sata port we use the pcie port. If we have both then we
 * use the appropriate one. This will need to be expanded when USB3 support is
 * added, but it is unknown how this will be integrated at the moment.
 */
static void __iomem *select_base_addr(struct pcie_mp_device *mp_dev,
				      enum miphy_mode mode)
{
	void __iomem *base;

	base = mp_dev->pcie_base; /* Assume pcie uport */

	switch (mode) {
	case PCIE_MODE:
		break;
	case SATA_MODE:
		/* Use the dedicated sata port if we have one */
		if (mp_dev->sata_base)
			base = mp_dev->sata_base;
		break;
	default:
		BUG();
	}

	return base;
}



static void stm_pcie_mp_register_write(struct stm_miphy *miphy,
				       u8 address, u8 data)
{
	struct pcie_mp_device *mp_dev;
	void __iomem *base;

	mp_dev = container_of(miphy->dev, struct pcie_mp_device, miphy_dev);

	BUG_ON(!mp_dev || !mp_dev->mp_select);

	base =  select_base_addr(mp_dev, miphy->mode);

	/* Select the correct port, usually diddling a sysconf */
	mp_dev->mp_select(mp_dev->priv_data, miphy->port);

	writeb(data, base + address);
}

static u8 stm_pcie_mp_register_read(struct stm_miphy *miphy, u8 address)
{
	struct pcie_mp_device *mp_dev;
	void __iomem *base;
	u8 data;

	mp_dev = container_of(miphy->dev, struct pcie_mp_device, miphy_dev);

	BUG_ON(!mp_dev || !mp_dev->mp_select);

	base =  select_base_addr(mp_dev, miphy->mode);

	mp_dev->mp_select(mp_dev->priv_data, miphy->port);
	data = readb(base + address);
	return data;
}

static void stm_pcie_mp_pipe_write(struct stm_miphy *miphy,
				   u32 addr, u32 data)
{
	struct pcie_mp_device *mp_dev;

	mp_dev = container_of(miphy->dev, struct pcie_mp_device, miphy_dev);

	/* 
	 * The pipe interface only exists on the PCIE cell, so you are 
	 * doing something seriously wrong if you try to use it in any
	 * other mode
	 */
	BUG_ON(miphy->mode != PCIE_MODE);
	BUG_ON(!mp_dev || !mp_dev->pipe_base);

	/* Seriously bizzare interface */
	if (addr < 0x1000)
		writeb(data & 0xff, mp_dev->pipe_base + addr);
	else if (addr <= 0x1034) {
		writeb(data & 0xff, mp_dev->pipe_base + addr);
		writeb((data >> 8), mp_dev->pipe_base + addr + 1);
		writeb((data >> 16) & 0x1f, mp_dev->pipe_base + addr + 2);
		writeb((data >> 24) & 0x1, mp_dev->pipe_base + addr + 3);
	} else
		writel(data, mp_dev->pipe_base + addr);
}

static u32 stm_pcie_mp_pipe_read(struct stm_miphy *miphy, u32 addr)
{
	struct pcie_mp_device *mp_dev;
	u32 data;

	mp_dev = container_of(miphy->dev, struct pcie_mp_device, miphy_dev);

	BUG_ON(miphy->mode != PCIE_MODE);
	BUG_ON(!mp_dev || !mp_dev->pipe_base);

	/* Seriously bizzare interface */
	if (addr < 0x1000)
		data = readb(mp_dev->pipe_base + addr);
	else if (addr <= 0x1034)
		data  = readb(mp_dev->pipe_base + addr) |
			(readb(mp_dev->pipe_base + addr + 1) << 8) |
			((readb(mp_dev->pipe_base + addr + 2) & 0x1f)<<16) |
		        ((readb(mp_dev->pipe_base + addr + 3) & 0x1) << 24);
	else
		data = readl(mp_dev->pipe_base + addr);

	return data;
}
static char *miphy_modes[] = {
		[UNUSED_MODE]	= "unused",
		[SATA_MODE]	= "sata",
		[PCIE_MODE]	= "pcie",
};

int stm_of_get_miphy_mode(struct device_node *np, int idx)
{
	const char *mode;
	int k;
	of_property_read_string_index(np, "miphy-modes", idx, &mode);
	for (k = 0; k < ARRAY_SIZE(miphy_modes); k++)
		if (!strcasecmp(mode, miphy_modes[k]))
			return k;
	return 0;
}
void *pcie_mp_of_get_pdata(struct platform_device *pdev)
{
	struct stm_plat_pcie_mp_data *data;
	int i;
	struct device_node *np = pdev->dev.of_node;
	if (pdev->dev.platform_data) /* callbacks may be set via auxdata */
		data = pdev->dev.platform_data;
	else
		data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);

	of_property_read_u32(np, "miphy-start", &data->miphy_first);
	data->miphy_count = of_property_count_strings(np, "miphy-modes");
	data->miphy_modes = devm_kzalloc(&pdev->dev,
				sizeof(enum miphy_mode) * data->miphy_count,
				GFP_KERNEL);
	data->ten_bit_symbols = of_property_read_bool(np, "ten-bit-symbol");
	for (i = 0; i < data->miphy_count; i++)
		data->miphy_modes[i] = stm_of_get_miphy_mode(np, i);

	if (of_get_property(np, "tx-pol-inv", NULL))
		data->tx_pol_inv = 1;

	if (of_get_property(np, "rx-pol-inv", NULL))
		data->rx_pol_inv = 1;

	of_property_read_string(np, "style",
				(const char **)&data->style_id);

	return data;
}

static int __devinit pcie_mp_probe(struct platform_device *pdev)
{
	struct pcie_mp_device *mp_dev;
	struct stm_miphy_device *miphy_dev;
	struct resource *res;
	struct stm_plat_pcie_mp_data *data;
	int result;

	if (pdev->dev.of_node)
		data = pcie_mp_of_get_pdata(pdev);
	else
		data = pdev->dev.platform_data;

	mp_dev = devm_kzalloc(&pdev->dev,
			sizeof(struct pcie_mp_device), GFP_KERNEL);
	if (!mp_dev)
		return -ENOMEM;

	/* Do we have a dedicated PCIE uport ?*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-uport");
	if (res)
		mp_dev->pcie_base = devm_request_and_ioremap(&pdev->dev, res);

	/* Do we have a dedicated SATA uport ?*/
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sata-uport");
	if (res)
		mp_dev->sata_base = devm_request_and_ioremap(&pdev->dev, res);

	/* We have got to have at least one of these! */
	if (!mp_dev->pcie_base && !mp_dev->sata_base) {
		dev_err(&pdev->dev, "Must have either a sata or pcie uport");
		return -EINVAL;
	}

	/* Check for PIPE registers, onyl present for PCIe controllers */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-pipe");
	if (res) {
		mp_dev->pipe_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!mp_dev->pipe_base) {
			dev_err(&pdev->dev, "Unable to map PIPE registers\n");
			return -EINVAL;
		}
	}

	mp_dev->mp_select = data->mp_select;

	if (data->init)
		mp_dev->priv_data = data->init(pdev);

	miphy_dev = &mp_dev->miphy_dev;
	miphy_dev->type = UPORT_IF;
	miphy_dev->miphy_first = data->miphy_first;
	miphy_dev->miphy_count = data->miphy_count;
	miphy_dev->modes = data->miphy_modes;
	miphy_dev->tx_pol_inv = data->tx_pol_inv;
	miphy_dev->rx_pol_inv = data->rx_pol_inv;
	miphy_dev->sata_gen = data->sata_gen;
	miphy_dev->ten_bit_symbols = data->ten_bit_symbols;
	miphy_dev->parent = &pdev->dev;
	miphy_dev->reg_write = stm_pcie_mp_register_write;
	miphy_dev->reg_read = stm_pcie_mp_register_read;
	miphy_dev->pipe_write = stm_pcie_mp_pipe_write;
	miphy_dev->pipe_read = stm_pcie_mp_pipe_read;
	miphy_dev->style_id = data->style_id;
	platform_set_drvdata(pdev, mp_dev);

	result = miphy_register_device(miphy_dev);

	if (result) {
		printk(KERN_ERR "Unable to Register uPort MiPHY device\n");
		return result;
	}

	return 0;
}
static int pcie_mp_remove(struct platform_device *pdev)
{
	struct pcie_mp_device *mp_dev;
	struct stm_plat_pcie_mp_data *data;

	mp_dev = platform_get_drvdata(pdev);
	data = pdev->dev.platform_data;
	if (!data->exit)
		data->exit(pdev);
	miphy_unregister_device(&mp_dev->miphy_dev);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id miphy_mp_match[] = {
	{
		.compatible = "st,miphy-mp",
	},
	{},
};

MODULE_DEVICE_TABLE(of, miphy_mp_match);
#endif

static struct platform_driver pcie_mp_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.driver.of_match_table = of_match_ptr(miphy_mp_match),
	.probe = pcie_mp_probe,
	.remove = pcie_mp_remove,
};

static int __init pcie_mp_init(void)
{
	return platform_driver_register(&pcie_mp_driver);
}

postcore_initcall(pcie_mp_init);

MODULE_AUTHOR("Srinivas.Kandagatla <srinivas.kandagatla@st.com>");
MODULE_DESCRIPTION("STM PCIE-UPort driver");
MODULE_LICENSE("GPL");
