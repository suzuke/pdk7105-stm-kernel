/*
 * STMicroelectronics MiPHY A-40x style code
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */

#include <linux/module.h>
#include <asm/processor.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stm/platform.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/miphy.h>
#include "miphy.h"

#define DRIVER_VERSION "MiPHYA-40x Driver Version 1.00"

#define MIPHY_INT_STATUS		0x04
#define MIPHY_VERSION			0xfb
#define MIPHY_REVISION			0xfc

static int miphya40x_sata_start(struct stm_miphy *miphy)
{
	/* MIPHYA-40LP series */

	/*
	 * Force macro1 to use rx_lspd, tx_lspd
	 * (by default rx_lspd and tx_lspd set for Gen2)
	 */
	stm_miphy_write(miphy, 0x10, 0x1);
	stm_miphy_write(miphy, 0x12, 0xE9);
	stm_miphy_write(miphy, 0x53, 0x64);
	/*
	 * Force PLL calibration reset, PLL reset and
	 * assert Deserializer Reset
	 */
	stm_miphy_write(miphy, 0x00, 0x86);
	/* Waiting for PLL to shut down*/
	if (miphy->dev->tx_pol_inv)
		/*For TXP and TXN swap */
		stm_miphy_write(miphy, 0x02, 0x10);

	while ((stm_miphy_read(miphy, 0x1) & 0x3) != 0x0)
		cpu_relax();
	/* Compensation Recalibration*/
	stm_miphy_write(miphy, 0x3E, 0x0F);
	stm_miphy_write(miphy, 0x3D, 0x61);
	mdelay(100);
	stm_miphy_write(miphy, 0x00, 0x00);
	/* Configure PLL */
	stm_miphy_write(miphy, 0x50, 0x9D);
	/*
	 * Configure PLL x2 ( this one allows to read
	 * correct value in feedback register)
	 */
	stm_miphy_write(miphy, 0x50, 0x9D);
	stm_miphy_write(miphy, 0x51, 0x2);
	/* Unbanked settings */
	/* lock onpattern complete */
	stm_miphy_write(miphy, 0xAA, 0xE);
	/* vertical bitlock enabled */
	stm_miphy_write(miphy, 0x86, 0x81);

	if (miphy->dev->tx_pol_inv)
		stm_miphy_write(miphy, 0x02, 0x14);
	else
		stm_miphy_write(miphy, 0x02, 0x4);

	/* IDLL Setup */
	stm_miphy_write(miphy, 0x77, 0xf);
	/* IDLL_DC_THRESHOLD */
	stm_miphy_write(miphy, 0x77, 0x8);
	stm_miphy_write(miphy, 0x70, 0xF0);
	udelay(100);
	/* IDLL_MODE */
	stm_miphy_write(miphy, 0x70, 0x70);

	/* VGA Settings */
	/* VGA CONTROL */
	stm_miphy_write(miphy, 0x95, 0x01);
	/* VGA GAIN SET TO GAIN 6 */
	stm_miphy_write(miphy, 0x96, 0x1F);
	/* VGA OFFSET SET TO 0 */
	stm_miphy_write(miphy, 0x97, 0x00);
	/* Rx Buffer Setup */
	/* Bypass threshold optimization */
	stm_miphy_write(miphy, 0xAA, 0x6);
	/* Put threshold in zero volt crossing */
	stm_miphy_write(miphy, 0xAE, 0x0);
	/* Put threshold in zero volt crossing */
	stm_miphy_write(miphy, 0xAF, 0x0);
	/* Force Macro1 in partial mode & release pll cal reset */
	mdelay(100);
	/* Banked Settingsfor GEN-1 and 2*/

	stm_miphy_write(miphy, 0x10, 0x1);
	stm_miphy_write(miphy, 0x30, 0x10);
	/* Tx Buffer Settings */
	/* Slew*/
	stm_miphy_write(miphy, 0x32, 0x1);
	/* Swing*/
	stm_miphy_write(miphy, 0x31, 0x06);
	/* Preempahsis*/
	stm_miphy_write(miphy, 0x32, 0x21);
	/* Sigdet Settings */

	/* MAN_SIGDET_CTRL=1 */
	stm_miphy_write(miphy, 0x80, 0xD);
	/* SIGDET_MODE_LP=0 */
	stm_miphy_write(miphy, 0x85, 0x01);
	/* EN_SIGDET_EQ=1 */
	stm_miphy_write(miphy, 0x85, 0x11);
	stm_miphy_write(miphy, 0xD9, 0x24);
	stm_miphy_write(miphy, 0xDA, 0x24);
	stm_miphy_write(miphy, 0xDB, 0x64);
	stm_miphy_write(miphy, 0x84, 0xE0);

	/* Rx Buffer Settings */
	stm_miphy_write(miphy, 0x85, 0x51);
	stm_miphy_write(miphy, 0x85, 0x51);
	stm_miphy_write(miphy, 0x83, 0x10);
	/* For Equa-7 RX =Equalization */
	stm_miphy_write(miphy, 0x84, 0xE0);
	stm_miphy_write(miphy, 0xD0, 0x01);

	while (stm_miphy_read(miphy, 0x69) != 0x64)
		cpu_relax();

	while ((stm_miphy_read(miphy, 0x1) & 0x03) != 0x03)
		cpu_relax();
	stm_miphy_write(miphy, 0x10, 0x00);
	return 0;
}

static int miphya40x_pcie_start(struct stm_miphy *miphy)
{
	/*
	 * So far only thing we have to do for PCIe is apply the
	 * 10 bit symbols settings for the pipe
	 */
	if (!miphy->dev->ten_bit_symbols ||
	    miphy->dev->ten_bit_done)
		return 0;

	/* Change PIPE reset sequence length from 13 to 14 */
	stm_miphy_pipe_write(miphy, 0x20, 0Xe);
	/*
	 * Make Rx symbol lock on 10-bit symbols, not 20-bit as default
	 * Broadcast to all MiPHY lanes
	 */
	stm_miphy_pipe_write(miphy, 0x1034, 0x20200b);
	/* Pulse soft reset of PCS */
	stm_miphy_pipe_write(miphy, 0x00, 0x01);
	stm_miphy_pipe_write(miphy, 0x00, 0x00);

	/*
	 * We only apply this 10 bit sequence to the first port to be brought
	 * up on the miphy. The Pipe settings are broadcast to the other lanes,
	 * so we should only really do it once, although repeating it seems to
	 * do no harm.
	 */
	miphy->dev->ten_bit_done = 1;

	return 0;
}

static int miphya40x_start(struct stm_miphy *miphy)
{
	int rval = -ENODEV;

	switch (miphy->mode) {
	case SATA_MODE:
		rval = miphya40x_sata_start(miphy);
		break;
	case PCIE_MODE:
		rval = miphya40x_pcie_start(miphy);
		break;
	default:
		BUG();
	}

	miphy->miphy_version = stm_miphy_read(miphy, MIPHY_VERSION);
	miphy->miphy_revision = stm_miphy_read(miphy, MIPHY_REVISION);

	return rval;
}

/* MiPHYA40x Ops */
static struct stm_miphy_style_ops miphya40x_ops = {
	.miphy_start = miphya40x_start,
};

static int miphya40x_probe(struct stm_miphy *miphy)
{
	pr_info("MiPHY driver style %s probed successfully\n",
		ID_MIPHYA40X);
	return 0;
}

static int miphya40x_remove(void)
{
	return 0;
}

static struct stm_miphy_style miphya40x_style = {
	.style_id	= ID_MIPHYA40X,
	.probe		= miphya40x_probe,
	.remove		= miphya40x_remove,
	.miphy_ops	= &miphya40x_ops,
};

static int __init miphya40x_init(void)
{
	int result;

	result = miphy_register_style(&miphya40x_style);
	if (result)
		pr_err("MiPHY driver style %s register failed (%d)",
				ID_MIPHYA40X, result);

	return result;
}
postcore_initcall(miphya40x_init);

MODULE_AUTHOR("Srinivas Kandagatla <srinivas.kandagatla@st.com>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
