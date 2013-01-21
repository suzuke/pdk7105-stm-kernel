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
#include <linux/stm/pio.h>
#include <linux/stm/platform.h>
#include <linux/stm/miphy.h>
#include "miphy.h"

#define NAME	"pcie-mp"

struct pcie_mp_device{
	struct stm_miphy_device miphy_dev;
	void __iomem *pcie_base; /* Base for uport connected to PCIE */
	void __iomem *sata_base; /* Base for uport connected to SATA */
	void (*mp_select)(int port);
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
	mp_dev->mp_select(miphy->port);

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

	mp_dev->mp_select(miphy->port);
	data = readb(base + address);
	return data;
}

static int __devinit pcie_mp_probe(struct platform_device *pdev)
{
	struct pcie_mp_device *mp_dev;
	struct resource *res;
	struct stm_miphy_device *miphy_dev;
	struct stm_plat_pcie_mp_data *data =
			(struct stm_plat_pcie_mp_data *)pdev->dev.platform_data;
	int result;

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

	mp_dev->mp_select = data->mp_select;

	miphy_dev = &mp_dev->miphy_dev;
	miphy_dev->type = UPORT_IF;
	miphy_dev->miphy_first = data->miphy_first;
	miphy_dev->miphy_count = data->miphy_count;
	miphy_dev->modes = data->miphy_modes;
	miphy_dev->tx_pol_inv = data->tx_pol_inv;
	miphy_dev->rx_pol_inv = data->rx_pol_inv;
	miphy_dev->parent = &pdev->dev;
	miphy_dev->reg_write = stm_pcie_mp_register_write;
	miphy_dev->reg_read = stm_pcie_mp_register_read;
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

	mp_dev = platform_get_drvdata(pdev);

	miphy_unregister_device(&mp_dev->miphy_dev);

	return 0;
}

static struct platform_driver pcie_mp_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
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
