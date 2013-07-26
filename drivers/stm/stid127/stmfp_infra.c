/*******************************************************************************
  This is the infra file for the ST fastpath on-chip Ethernet controllers.

	Copyright(C) 2009-2013 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Manish Rathi<manish.rathi@st.com>
*******************************************************************************/


#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/stm/platform.h>
#include <linux/stm/mmc.h>
#include <linux/stm/pad.h>
#include <linux/stmfp.h>

struct fastpath_clk_rate {
	char *clk_n;
	unsigned long rate;
};

int setup_fp_clocks(void)
{
	int ret, i;
	struct fastpath_clk_rate fastpath_clks[] = {
		{
			.clk_n = "CLK_DOC_VCO",
			.rate = 540000000,
		}, {
			.clk_n = "CLK_FP",
			.rate = 200000000,
		}, {
			.clk_n = "CLK_D3_XP70",
			.rate = 324000000,
		}, {
			.clk_n = "CLK_IFE",
			.rate = 216000000,
		},
	};

	for (i = 0; i < ARRAY_SIZE(fastpath_clks); ++i) {
		struct clk *clk;
		clk = clk_get(NULL, fastpath_clks[i].clk_n);
		if (IS_ERR(clk)) {
			ret = -EINVAL;
			goto on_error;
		}
		clk_prepare_enable(clk);
		ret = clk_set_rate(clk, fastpath_clks[i].rate);
		if (ret)
			goto on_error;
		clk_put(clk);
	}
	return ret;
on_error:
	pr_err("Error %d: setup FP clock %s to clock rate %lu\n", ret,
	       fastpath_clks[i].clk_n, fastpath_clks[i].rate);

	return ret;
}  /* end setup_fp_clocks() */


int stid127_fp_claim_resources(void *plat)
{
	struct platform_device *pdev = plat;
	int ret = 0;
	struct plat_stmfp_data *plat_dat = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;

	plat_dat->custom_cfg = stm_of_get_dev_config(dev);
	plat_dat->custom_data = devm_stm_device_init(dev,
			(struct stm_device_config *)plat_dat->custom_cfg);
	if (!plat_dat->custom_data) {
		pr_err("%s: failed to request pads!\n", __func__);
		return -ENODEV;
	}
	ret = setup_fp_clocks();
	if (ret) {
		pr_err("%s: failed to setup fastpath clocks\n", __func__);
		return ret;
	}
	return ret;
}

void stid127_fp_release_resources(void *plat)
{

	struct platform_device *pdev = plat;
	struct plat_stmfp_data *plat_dat = pdev->dev.platform_data;

	if (!plat_dat->custom_data)
		return;
	devm_stm_device_exit(&pdev->dev, plat_dat->custom_data);
	plat_dat->custom_data = NULL;
}
