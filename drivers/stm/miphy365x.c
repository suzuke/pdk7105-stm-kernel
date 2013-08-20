/*
 * STMicroelectronics MiPHY 3-65 style code
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * Orignially copied from miphy.c
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

#define DRIVER_VERSION "MiPHY3-65x Driver Version 1.00"

#define MIPHY_RESET			0x00
#define RST_RX				(1<<4)

#define MIPHY_STATUS			0x01
#define MIPHY_CONTROL			0x02
#define DIS_LINK_RST			(1<<4)

#define MIPHY_INT_STATUS		0x04
#define BITUNLOCK_INT			(1<<1)
#define SYMBUNLOCK_INT			(1<<2)
#define FIFOOVERLAP_INT			(1<<3)


#define MIPHY_BOUNDARY_1		0x10
#define POWERSEL_SEL			(1<<2)
#define SPDSEL_SEL			(1<<0)

#define MIPHY_BOUNDARY_3		0x12
#define RX_LSPD				(1<<5)

#define MIPHY_COMPENS_CONTROL_1		0x40

#define MIPHY_IDLL_TEST			0x72
#define START_CLK_HF			(1<<6)
#define STOP_CLK_HF			(1<<7)

#define MIPHY_DES_BITLOCK_CFG		0x85
#define CLEAR_BIT_UNLOCK_FLAG		(1<<0)
#define UPDATE_TRANS_DENSITY		(1<<1)

#define MIPHY_DES_BITLOCK		0x86

#define MIPHY_DES_BITLOCK_STATUS	0x88
#define BIT_LOCK			(1<<0)
#define BIT_LOCK_FAILED			(1<<1)
#define BIT_UNLOCK			(1<<2)
#define TRANS_DENSITY_UPDATED		(1<<3)

#define MIPHY_VERSION			0xfb
#define MIPHY_REVISION			0xfc

/***************************** MiPHY3-65 ops ********************************/

#ifdef DEBUG
/* Pretty-ish print of Miphy registers, helpful for debugging */
void stm_miphy_dump_registers(struct stm_miphy *miphy)
{
	printk(KERN_INFO "MIPHY_RESET (0x0): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_RESET));
	printk(KERN_INFO "MIPHY_STATUS (0x1): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_STATUS));
	printk(KERN_INFO "MIPHY_CONTROL (0x1): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_CONTROL));
	printk(KERN_INFO "MIPHY_INT_STATUS (0x4): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_INT_STATUS));
	printk(KERN_INFO "MIPHY_BOUNDARY_1 (0x10): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_BOUNDARY_1));
	printk(KERN_INFO "MIPHY_BOUNDARY_3 (0x12): 0x%.2x\n",
			   stm_miphy_read(miphy, MIPHY_BOUNDARY_3));
	printk(KERN_INFO "MIPHY_COMPENS_CONTROL_1 (0x40): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_COMPENS_CONTROL_1));
	printk(KERN_INFO "MIPHY_IDLL_TEST (0x72): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_IDLL_TEST));
	printk(KERN_INFO "MIPHY_DES_BITLOCK_CFG (0x85): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_DES_BITLOCK_CFG));
	printk(KERN_INFO "MIPHY_DES_BITLOCK (0x86): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_DES_BITLOCK));
	printk(KERN_INFO "MIPHY_DES_BITLOCK_STATUS (0x88): 0x%.2x\n",
			  stm_miphy_read(miphy, MIPHY_DES_BITLOCK_STATUS));
}
#endif /* DEBUG */

static void miphy365x_tap_start_port0(struct stm_miphy *miphy0)
{
	int timeout;
	struct stm_miphy *miphy1;

	miphy1 = stm_miphy_find_port(1);

	BUG_ON(!miphy1);

#ifndef CONFIG_ARM
	/* TODO: Get rid of this */
	if (cpu_data->type == CPU_STX7108) {
		/*Force SATA port 1 in Slumber Mode */
		stm_miphy_write(miphy1, 0x11, 0x8);
		/*Force Power Mode selection from MiPHY soft register 0x11 */
		stm_miphy_write(miphy1, 0x10, 0x4);
	}
#endif

	/* Force Macro1 in reset and request PLL calibration reset */

	/* Force PLL calibration reset, PLL reset and assert
	 * Deserializer Reset */
	stm_miphy_write(miphy0, 0x00, 0x16);
	stm_miphy_write(miphy0, 0x11, 0x0);
	/* Force macro1 to use rx_lspd, tx_lspd (by default rx_lspd
	 * and tx_lspd set for Gen1)  */
	stm_miphy_write(miphy0, 0x10, 0x1);

	/* Force Recovered clock on first I-DLL phase & all
	 * Deserializers in HP mode */

	/* Force Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(miphy0, 0x72, 0x40);
	/* Force Des in HP mode on macro1 */
	stm_miphy_write(miphy0, 0x12, 0x00);

	/* Wait for HFC_READY = 0 */
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (stm_miphy_read(miphy0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	/* Restart properly Process compensation & PLL Calibration */

	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(miphy0, 0x41, 0x1E);
	/* comsr compensation reference */
	stm_miphy_write(miphy0, 0x42, 0x28);
	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(miphy0, 0x41, 0x1E);
	/* comsr cal gives more suitable results in fast PVT for comsr
	   used by TX buffer to build slopes making TX rise/fall fall
	   times. */
	stm_miphy_write(miphy0, 0x42, 0x33);
	/* Force VCO current to value defined by address 0x5A */
	stm_miphy_write(miphy0, 0x51, 0x2);
	/* Force VCO current to value defined by address 0x5A */
	stm_miphy_write(miphy0, 0x5A, 0xF);
	/* Enable auto load compensation for pll_i_bias */
	stm_miphy_write(miphy0, 0x47, 0x2A);
	/* Force restart compensation and enable auto load for
	 * Comzc_Tx, Comzc_Rx & Comsr on macro1 */
	stm_miphy_write(miphy0, 0x40, 0x13);

	/* Wait for comzc & comsr done */
	while ((stm_miphy_read(miphy0, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/* Recommended settings for swing & slew rate FOR SATA GEN 1
	 * from CPG */
	stm_miphy_write(miphy0, 0x20, 0x00);
	/* (Tx Swing target 500-550mV peak-to-peak diff) */
	stm_miphy_write(miphy0, 0x21, 0x2);
	/* (Tx Slew target120-140 ps rising/falling time) */
	stm_miphy_write(miphy0, 0x22, 0x4);

	/* Force Macro1 in partial mode & release pll cal reset */
	stm_miphy_write(miphy0, 0x00, 0x10);
	udelay(10);

#if 0
	/* SSC Settings. SSC will be enabled through Link */
	stm_miphy_write(miphy, 0x53, 0x00); /* pll_offset */
	stm_miphy_write(miphy, 0x54, 0x00); /* pll_offset */
	stm_miphy_write(miphy, 0x55, 0x00); /* pll_offset */
	stm_miphy_write(miphy, 0x56, 0x04); /* SSC Ampl=0.48% */
	stm_miphy_write(miphy, 0x57, 0x11); /* SSC Ampl=0.48% */
	stm_miphy_write(miphy, 0x58, 0x00); /* SSC Freq=31KHz */
	stm_miphy_write(miphy, 0x59, 0xF1); /* SSC Freq=31KHz */
	/*SSC Settings complete*/
#endif

	stm_miphy_write(miphy0, 0x50, 0x8D);
	stm_miphy_write(miphy0, 0x50, 0x8D);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13) */

	while ((stm_miphy_read(miphy0, 0x01) & 0x03) != 0x03)
		cpu_relax();

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	stm_miphy_write(miphy0, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(miphy0, 0x72, 0x00);

	/* Deassert deserializer reset */
	stm_miphy_write(miphy0, 0x00, 0x00);
	/* des_bit_lock_en is set */
	stm_miphy_write(miphy0, 0x02, 0x08);

	/* bit lock detection strength */
	stm_miphy_write(miphy0, 0x86, 0x61);
}

/*
 * MiPhy Port 1 Start function for SATA
 */
static void miphy365x_tap_start_port1(struct stm_miphy *miphy1)
{
	int timeout;
	struct stm_miphy *miphy0;

	miphy0 = stm_miphy_find_port(0);

	BUG_ON(!miphy0);

	/* Force PLL calibration reset, PLL reset and assert Deserializer
	 * Reset */
	stm_miphy_write(miphy1, 0x00, 0x2);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro2 */
	stm_miphy_write(miphy1, 0x40, 0x13);

	/* Force PLL reset  */
	stm_miphy_write(miphy1, 0x00, 0x2);
	/* Set properly comsr definition for 30 MHz ref clock */
	stm_miphy_write(miphy1, 0x41, 0x1E);
	/* to get more optimum result on comsr calibration giving faster
	 * rise/fall time in SATA spec Gen1 useful for some corner case.*/
	stm_miphy_write(miphy1, 0x42, 0x33);
	/* Force restart compensation and enable auto load for Comzc_Tx,
	 * Comzc_Rx & Comsr on macro1 */
	stm_miphy_write(miphy1, 0x40, 0x13);

	/*Wait for HFC_READY = 0*/
	timeout = 50; /* Jeeeezzzzz.... */
	while (timeout-- && (stm_miphy_read(miphy0, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	stm_miphy_write(miphy1, 0x11, 0x0);
	/* Force macro2 to use rx_lspd, tx_lspd  (by default rx_lspd and
	 * tx_lspd set for Gen1) */
	stm_miphy_write(miphy1, 0x10, 0x1);
	/* Force Rx_Clock on first I-DLL phase on macro2*/
	stm_miphy_write(miphy1, 0x72, 0x40);
	/* Force Des in HP mode on macro2 */
	stm_miphy_write(miphy1, 0x12, 0x00);

	while ((stm_miphy_read(miphy1, 0x40) & 0xC) != 0xC)
		cpu_relax();

	/*RECOMMENDED SETTINGS for Swing & slew rate FOR SATA GEN 1 from CPG*/
	stm_miphy_write(miphy1, 0x20, 0x00);
	/*(Tx Swing target 500-550mV peak-to-peak diff) */
	stm_miphy_write(miphy1, 0x21, 0x2);
	/*(Tx Slew target120-140 ps rising/falling time) */
	stm_miphy_write(miphy1, 0x22, 0x4);
	/*Force Macr21 in partial mode & release pll cal reset */
	stm_miphy_write(miphy1, 0x00, 0x10);
	udelay(10);
	/* Release PLL reset  */
	stm_miphy_write(miphy1, 0x00, 0x0);

	/*  Wait for phy_ready */
	/*  When phy is in ready state ( register 0x01 of macro1 to 0x13)*/
	while ((stm_miphy_read(miphy1, 0x01) & 0x03) != 0x03)
		cpu_relax();

	/* Enable macro1 to use rx_lspd  & tx_lspd from link interface */
	stm_miphy_write(miphy1, 0x10, 0x00);
	/* Release Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(miphy1, 0x72, 0x00);

	/* Deassert deserializer reset */
	stm_miphy_write(miphy1, 0x00, 0x00);
	/*des_bit_lock_en is set */
	stm_miphy_write(miphy1, 0x02, 0x08);

	/*bit lock detection strength */
	stm_miphy_write(miphy1, 0x86, 0x61);
}

static int miphy365x_tap_sata_start(struct stm_miphy *miphy)
{
	int rval = 0;
	switch (miphy->port) {
	case 0:
		miphy365x_tap_start_port0(miphy);
		break;
	case 1:
		miphy365x_tap_start_port1(miphy);
		break;
	default:
		rval = -EINVAL;
	}
	return rval;
}

static int miphy365x_tap_pcie_start(struct stm_miphy *miphy)
{
	/* The tap interface versions do not support PCIE */
	return -1;
}

/*
 * MiPhy Port 0 & 1 Start function for SATA
 */

static int miphy365x_uport_sata_start(struct stm_miphy *miphy)
{
	unsigned int regvalue;
	int timeout;
	struct stm_miphy_device *miphy_dev;
	enum miphy_sata_gen sata_gen;
	static u8 des_mode[] = {0x00, 0x00, 0x09, 0x12};

	miphy_dev = miphy->dev;

	/* Later revision can do GEN2 and GEN3 */
	sata_gen = (miphy->miphy_revision == 9) ? SATA_GEN2 : SATA_GEN1;

	/* Override from soc or board layer only way to get GEN3 */
	if (miphy->dev->sata_gen != SATA_GEN_DEFAULT)
		sata_gen = miphy->dev->sata_gen;

	/*
	 * Force PHY macro reset,PLL calibration reset, PLL reset
	 * and assert Deserializer Reset
	 */
	stm_miphy_write(miphy, 0x00, 0x96);

	if (miphy_dev->tx_pol_inv)
		stm_miphy_write(miphy, 0x2, 0x20);

	/*
	 * Force macro1 to use rx_lspd, tx_lspd
	 * (by default rx_lspd and tx_lspd set for Gen1 and 2)
	 */
	stm_miphy_write(miphy, 0x10, 0x1);
	/* Force Rx_Clock on first I-DLL phase on macro1 */
	stm_miphy_write(miphy, 0x72, 0x40);

	/* Force Des in HP mode on macro, rx_lspd, tx_lspd for Gen2/3 */
	stm_miphy_write(miphy, 0x12, des_mode[sata_gen]);

	/* Wait for HFC_READY = 0*/
	timeout = 50;
	while (timeout-- && (stm_miphy_read(miphy, 0x01) & 0x3))
		udelay(2000);
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);

	/*--------Compensation Recaliberation--------------*/
	if (sata_gen == SATA_GEN1)
		/* Set properly comp_1mhz_ratio for 30 MHz ref clock */
		stm_miphy_write(miphy, 0x41, 0x1E);
	else
		/* Set properly comp_2mhz_ratio for 30 MHz ref clock */
		stm_miphy_write(miphy, 0x41, 0xF);

	if (sata_gen != SATA_GEN3) {
		stm_miphy_write(miphy, 0x42, 0x33);
		/*
		 * Force VCO current to value defined by address 0x5A
		 * and disable PCIe100Mref bit
		 */
		stm_miphy_write(miphy, 0x51, 0x2);
		/* Enable auto load compensation for pll_i_bias */
		stm_miphy_write(miphy, 0x47, 0x2A);
	}

	/*
	 * Force restart compensation and enable auto load
	 * for Comzc_Tx, Comzc_Rx and Comsr on macro
	 * */
	stm_miphy_write(miphy, 0x40, 0x13);
	while ((stm_miphy_read(miphy, 0x40) & 0xC) != 0xC)
		cpu_relax();

	switch (sata_gen) {
	case SATA_GEN3:
		stm_miphy_write(miphy, 0x20, 0x12);
		/* TX Swing target 550-600mv peak to peak diff */
		stm_miphy_write(miphy, 0x21, 0x64);
		/* Tx Slew target 90-110ps rising/falling time */
		stm_miphy_write(miphy, 0x22, 0x2);
		stm_miphy_write(miphy, 0x23, 0x0);
		/* Rx Eq ON3, Sigdet threshold SDTH1 */
		stm_miphy_write(miphy, 0x25, 0x31);
		break;
	case SATA_GEN2:
		/* Recommended Settings for Swing & slew rate
		* for SATA GEN 2 from CCI:(c1.9)
		* conf gen sel=0x1 to program Gen2 banked registers
		* and VDDT filter ON
		*/
		stm_miphy_write(miphy, 0x20, 0x01);
		/*(Tx Swing target 550-600mV peak-to-peak diff) */
		stm_miphy_write(miphy, 0x21, 0x04);
		/*(Tx Slew target 90-110 ps rising/falling time) */
		stm_miphy_write(miphy, 0x22, 0x2);
		/*RX Equalization ON1, Sigdet threshold SDTH1*/
		stm_miphy_write(miphy, 0x25, 0x11);
		break;
	case SATA_GEN1:
		/* Recommended Settings for Swing & slew rate
		* for SATA GEN 1 from CCI:(c1.51)
		* conf gen sel = 00b to program Gen1 banked registers &
		* VDDT filter ON
		*/
		stm_miphy_write(miphy, 0x20, 0x10);
		/*(Tx Swing target 500-550mV peak-to-peak diff) */
		stm_miphy_write(miphy, 0x21, 0x3);
		/*(Tx Slew target120-140 ps rising/falling time) */
		stm_miphy_write(miphy, 0x22, 0x4);
		break;
	default:
		BUG();
	}

	/* Force Macro1 in partial mode & release pll cal reset */
	stm_miphy_write(miphy, 0x00, 0x10);
	udelay(100);
	/* SSC Settings. SSC will be enabled through Link */
	/*  SSC Ampl.=0.4% */
	stm_miphy_write(miphy, 0x56, 0x03);
	/*  SSC Ampl.=0.4% */
	stm_miphy_write(miphy, 0x57, 0x63);
	/*  SSC Freq=31KHz */
	stm_miphy_write(miphy, 0x58, 0x00);
	/*  SSC Freq=31KHz   */
	stm_miphy_write(miphy, 0x59, 0xF1);
	/* SSC Settings complete*/
	if (sata_gen == SATA_GEN1)
		stm_miphy_write(miphy, 0x50, 0x8D);
	else
		stm_miphy_write(miphy, 0x50, 0xCD);

	/* MIPHY PLL ratio */
	stm_miphy_read(miphy, 0x52);
	/* Wait for phy_ready */
	/* When phy is in ready state ( register 0x01 reads 0x13) */
	regvalue = stm_miphy_read(miphy, 0x01);
	timeout = 50;
	while (timeout-- && ((regvalue & 0x03) != 0x03)) {
		regvalue = stm_miphy_read(miphy, 0x01);
		udelay(2000);
	}
	if (timeout < 0)
		pr_err("%s(): HFC_READY timeout!\n", __func__);
	if ((regvalue & 0x03) == 0x03) {
		/* Enable macro1 to use rx_lspd  &
		 * tx_lspd from link interface */
		stm_miphy_write(miphy, 0x10, 0x00);
		/* Release Rx_Clock on first I-DLL phase on macro1 */
		stm_miphy_write(miphy, 0x72, 0x00);
		/* Assert deserializer reset */
		stm_miphy_write(miphy, 0x00, 0x10);
		/* des_bit_lock_en is set */
		stm_miphy_write(miphy, 0x02,
				miphy_dev->tx_pol_inv ? 0x28 : 0x08);
		/* bit lock detection strength */
		stm_miphy_write(miphy, 0x86, 0x61);
		/* Deassert deserializer reset */
		stm_miphy_write(miphy, 0x00, 0x00);
	}
	return 0;
}


static int miphy365x_uport_pcie_start(struct stm_miphy *miphy)
{
        if (miphy->dev->tx_pol_inv) {
        	/* Invert Tx polarity for Orly-2 */
                stm_miphy_write(miphy, 0x2, 0x2d);
		/* Set pci_tx_detect_pol to 0 for Orly-2 */
		stm_miphy_write(miphy, 0x16, 0x0);
	}

	return 0;
}


static int miphy365x_start(struct stm_miphy *miphy)
{
	int rval = -ENODEV;

	switch (miphy->mode) {
	case SATA_MODE:
		switch (miphy->dev->type) {
		case TAP_IF:
			rval = miphy365x_tap_sata_start(miphy);
			break;
		case UPORT_IF:
			rval = miphy365x_uport_sata_start(miphy);
			break;
		default:
			BUG();
		}
		break;
	case PCIE_MODE:
		switch (miphy->dev->type) {
		case TAP_IF:
			rval = miphy365x_tap_pcie_start(miphy);
			break;
		case UPORT_IF:
			rval = miphy365x_uport_pcie_start(miphy);
			break;
		default:
			BUG();
		}
		break;
	default:
		BUG();
	}

	miphy->miphy_version = stm_miphy_read(miphy, MIPHY_VERSION);
	miphy->miphy_revision = stm_miphy_read(miphy, MIPHY_REVISION);

	/* Clear the contents of interrupt control register,
	   excluding fifooverlap_int */
	stm_miphy_write(miphy, MIPHY_INT_STATUS, 0x77);

	return rval;
}

static void miphy365x_assert_deserializer(struct stm_miphy *miphy, int assert)
{
	stm_miphy_write(miphy, 0x00, assert ? 0x10 : 0x00);
}

static int miphy365x_sata_status(struct stm_miphy *miphy)
{
	u8 miphy_int_status;
	miphy_int_status = stm_miphy_read(miphy, 0x4);
	return (miphy_int_status & 0x8) || (miphy_int_status & 0x2);
}


/*****************************End of MiPHY3-65 ops****************************/

/* MiPHY3-65 Ops */
static struct stm_miphy_style_ops miphy365x_ops = {
	.miphy_start			= miphy365x_start,
	.miphy_assert_deserializer	= miphy365x_assert_deserializer,
	.miphy_sata_status		= miphy365x_sata_status,
};

static int miphy365x_probe(struct stm_miphy *miphy)
{
	pr_info("MiPHY driver style %s probed successfully\n",
		ID_MIPHY365X);
	return 0;
}

static int miphy365x_remove(void)
{
	return 0;
}

static struct stm_miphy_style miphy365x_style = {
	.style_id	= ID_MIPHY365X,
	.probe		= miphy365x_probe,
	.remove		= miphy365x_remove,
	.miphy_ops	= &miphy365x_ops,
};

static int __init miphy365x_init(void)
{
	int result;

	result = miphy_register_style(&miphy365x_style);
	if (result)
		pr_err("MiPHY driver style %s register failed (%d)",
				ID_MIPHY365X, result);

	return result;
}
postcore_initcall(miphy365x_init);

MODULE_AUTHOR("Srinivas Kandagatla <srinivas.kandagatla@st.com>");
MODULE_DESCRIPTION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
