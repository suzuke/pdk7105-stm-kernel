/*
 *  ------------------------------------------------------------------------
 *  stm_devices_of.c Support for STMicroelectronics pad manager device trees
 *
 *  For drivers which require some kind of callbacks.
 *  Most of these callbacks are generic in some sense, and can be extended
 *  if required.
 *  However, if some of the SOC's callbacks do not fit this flow, a new
 *  soc level callbacks can be implemented.
 *
 *  Copyright (c) 2012 STMicroelectronics Limited
 *  Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 *  ------------------------------------------------------------------------
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *  ------------------------------------------------------------------------
 *
 */
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>


#include <linux/stm/platform.h>
#include <linux/stm/mmc.h>
#include <linux/stm/pad.h>


/* MMC */
static int stm_of_claim_resources(struct platform_device *pdev,
		void **custom_cfg, void **custom_data)
{
	/* Get Pad configs from the device trees and update priv */
	if (pdev->dev.of_node)
		*custom_cfg = stm_of_get_pad_config(&pdev->dev);

	*custom_data = devm_stm_pad_claim(&pdev->dev,
			(struct stm_pad_config *)(*custom_cfg),
			dev_name(&pdev->dev));

	if (!*custom_data)
		return -ENODEV;

	return 0;
}


static int stm_of_mmc_claim_resources(struct platform_device *pdev)
{
	struct stm_mmc_platform_data *data = pdev->dev.platform_data;
	return stm_of_claim_resources(pdev,
				&data->custom_cfg, &data->custom_data);
}

static void stm_of_mmc_release_resource(struct platform_device *pdev)
{
	struct stm_mmc_platform_data *plat_dat = pdev->dev.platform_data;

	if (!plat_dat->custom_data)
		return;
	devm_stm_device_exit(&pdev->dev, plat_dat->custom_data);
	plat_dat->custom_data = NULL;
}
struct stm_mmc_platform_data mmc_platform_data = {
	.init = &stm_of_mmc_claim_resources,
	.exit = &stm_of_mmc_release_resource,
};

/* Ethernet */

void stmmac_of_clk_fixup(struct device_node *np, struct device_node *fixup_np)
{
	uint32_t phy_clk_rate;
	const char *clk_name;
	struct clk *phy_clk;

	if (of_get_property(fixup_np, "clk", NULL)) {
		of_property_read_u32(fixup_np, "clk", &phy_clk_rate);
		of_property_read_string(np, "st,phy-clk-name", &clk_name);
		phy_clk = clk_get(NULL, clk_name);
		clk_set_rate(phy_clk, phy_clk_rate);
	}

	return;
}


void stmmac_of_fixup(struct device *dev, struct device_node *mac_np,
	 struct device_node *fixup_np,	struct stm_device_state *dev_state)
{
	/* Fixup Clk, Pads, sysconfs */
	stmmac_of_clk_fixup(mac_np, fixup_np);
	stm_of_dev_config_fixup(dev, fixup_np, dev_state);
	return;
}

static int stmmac_of_init(struct platform_device *pdev)
{
	int ret = 0;
	struct plat_stmmacenet_data *plat_dat = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	plat_dat->custom_cfg = stm_of_get_dev_config(dev);
	plat_dat->custom_data = devm_stm_device_init(dev,
			(struct stm_device_config *)plat_dat->custom_cfg);
	if (!plat_dat->custom_data) {
		pr_err("%s: failed to request pads!\n", __func__);
		ret = -ENODEV;
	}


	/* Will be used in speed selection */
	plat_dat->bsp_priv = pdev;
	return ret;
}

static void stm_of_release_resource(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat = pdev->dev.platform_data;

	if (!plat_dat->custom_data)
		return;
	devm_stm_device_exit(&pdev->dev, plat_dat->custom_data);
	plat_dat->custom_data = NULL;
}

static void *ethernet_bus_setup(void __iomem *ioaddr,
					struct device *dev, void *data)
{
	struct stm_amba_bridge *amba;
	struct device_node *np = dev->of_node;
	struct stm_amba_bridge_config *config;
	struct device_node *cn = of_parse_phandle(np, "amba-config", 0);
	u32 reg_offset;
	of_property_read_u32(cn, "reg-offset", &reg_offset);

	if (!np)
		return NULL;

	if (!data) {
		config = stm_of_get_amba_config(dev);
		amba = stm_amba_bridge_create(dev, ioaddr + reg_offset,
						config);
		if (IS_ERR(amba)) {
			dev_err(dev, " Unable to create amba plug\n");
			return NULL;
		}
	} else
		amba = (struct stm_amba_bridge *) data;

	stm_amba_bridge_init(amba);

	return (void *) amba;
}


static void ethernet_fix_mac_speed(void *priv, unsigned int speed)
{
	struct platform_device *pdev = priv;
	struct device_node *np = pdev->dev.of_node;
	struct plat_stmmacenet_data *plat_dat = pdev->dev.platform_data;
	struct device_node *speed_np;
	char speed_node_path[512] = "";
	speed_np = of_parse_phandle(np, "st,fix-mac-speed", 0);

	if (!speed_np)
		return;

	sprintf(speed_node_path, "%s", speed_np->full_name);
	switch (plat_dat->interface) {
	case PHY_INTERFACE_MODE_MII:
		strcat(speed_node_path, "/mii-speed-sel");
	break;

	case PHY_INTERFACE_MODE_GMII:
		strcat(speed_node_path, "/gmii-speed-sel");
	break;


	case PHY_INTERFACE_MODE_RMII:
		strcat(speed_node_path, "/rmii-speed-sel");
	break;

	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
		strcat(speed_node_path, "/rgmii-speed-sel");
	break;
	default:
		return;
	break;
	}

	if (speed == SPEED_1000)
		strcat(speed_node_path, "/speed-1000");
	else if (speed == SPEED_100)
		strcat(speed_node_path, "/speed-100");
	else
		strcat(speed_node_path, "/speed-10");

	speed_np = of_find_node_by_path(speed_node_path);
	if (speed_np)
		stmmac_of_fixup(&pdev->dev, np, speed_np,
				plat_dat->custom_data);

	return;
}

struct plat_stmmacenet_data ethernet_platform_data = {
	.init = &stmmac_of_init,
	.exit = &stm_of_release_resource,
	.bus_setup = &ethernet_bus_setup,
	.fix_mac_speed = &ethernet_fix_mac_speed,
};

void stm_pcie_mp_select(void *priv_data, int port)
{
	struct stm_device_state *state = priv_data;
	BUG_ON(port < 0 || port > 1);
	if (state)
		stm_device_sysconf_write(state, "MIPHY_SELECT", port);
	return;
}

void *stm_pcie_mp_init(struct platform_device *pdev)
{
	struct stm_plat_pcie_mp_data *data = pdev->dev.platform_data;
	struct stm_device_state *state;
	struct stm_device_config *config;

	config	= stm_of_get_dev_config(&pdev->dev);
	if (!config)
		return NULL;

	data->priv_data = devm_stm_device_init(&pdev->dev, config);

	state = data->priv_data;

	/* Deassert Soft Reset to SATA0 */
	stm_device_sysconf_write(state, "SATA0_SOFT_RESET", 1);
	/* If the 100MHz xtal for PCIe is present, then the microport interface
	* will already have a clock, so there is no need to flip to the 30MHz
	* clock here. If it isn't then we have to switch miphy lane 1 to use
	* the 30MHz clock, as otherwise we will not be able to talk to lane 0
	* since the uport interface itself is clocked from lane1
	*/

	if (data->miphy_modes[1] != PCIE_MODE) {
		/* Put MiPHY1 in reset - rst_per_n[32] */
		stm_device_sysconf_write(state, "MIPHY1_SOFT_RESET", 0);
		/* Put SATA1 HC in reset - rst_per_n[30] */
		stm_device_sysconf_write(state, "SATA1_SOFT_RESET", 0);
		/* Now switch to Phy interface to SATA HC not PCIe HC */
		stm_device_sysconf_write(state, "SELECT_SATA", 1);
		/* Select the Uport to use MiPHY1 */
		stm_pcie_mp_select(pdev, 1);
		/* Take SATA1 HC out of reset - rst_per_n[30] */
		stm_device_sysconf_write(state, "SATA1_SOFT_RESET", 1);
		/* MiPHY1 needs to be using the MiPHY0 reference clock */
		stm_device_sysconf_write(state, "SATAPHY1_OSC_FORCE_EXT", 1);
		/* Take MiPHY1 out of reset - rst_per_n[32] */
		stm_device_sysconf_write(state, "MIPHY1_SOFT_RESET", 1);
	}
	return data->priv_data;
}

struct stm_plat_pcie_mp_data	pcie_mp_platform_data = {
	.mp_select	= stm_pcie_mp_select,
	.init		= stm_pcie_mp_init,
};

static void *stm_of_pcie_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct stm_device_config *cfg;
	struct stm_device_state *state = NULL;

	if (!np)
		return NULL;

	cfg = stm_of_get_dev_config(dev);
	if (!cfg)
		return NULL;
	state = devm_stm_device_init(dev, cfg);

	/* Drive RST_N low, set device type */
	stm_device_sysconf_write(state, "PCIE_SOFT_RST", 1);
	stm_device_sysconf_write(state, "PCIE_DEVICE_TYPE", 1);

	mdelay(1);
	return state;
}

static void stm_of_pcie_enable_ltssm(void *handle)
{
	struct stm_device_state *state = handle;
	if (state)
		stm_device_sysconf_write(state, "PCIE_APP_LTSSM_ENABLE", 1);
}

static void stm_of_pcie_disable_ltssm(void *handle)
{
	struct stm_device_state *state = handle;
	if (state)
		stm_device_sysconf_write(state, "PCIE_APP_LTSSM_ENABLE", 0);
}

/* Ops to drive the platform specific bits of the interface */
static struct stm_plat_pcie_ops stm_pcie_ops = {
	.init		= stm_of_pcie_init,
	.enable_ltssm	= stm_of_pcie_enable_ltssm,
	.disable_ltssm	= stm_of_pcie_disable_ltssm,
};


struct stm_plat_pcie_config stm_pcie_config = {
	.ops = &stm_pcie_ops,
};
