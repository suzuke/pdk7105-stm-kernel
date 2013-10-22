/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CONFIG_OF
/* All the Drivers are now configured using device trees so,
 * Please start using device trees */
#warning  "This code will disappear soon, you should use device trees"


#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stih415.h>


#define PCIE_SOFT_RST_N_PCIE	(1<<6) /* Active low !! */
#define PCIE_SYS_INT		(1<<5)
#define PCIE_P1_SSC_EN		(1<<4)
#define PCIE_APP_REQ_RETRY_EN	(1<<3)
#define PCIE_APP_LTSSM_ENABLE	(1<<2)
#define PCIE_APP_INIT_RST	(1<<1)
#define PCIE_DEVICE_TYPE	(1<<0)

#define PCIE_DEFAULT_VAL	(PCIE_SOFT_RST_N_PCIE | PCIE_DEVICE_TYPE)

static void *stih415_pcie_init(struct platform_device *pdev)
{
	static struct sysconf_field *sc;

	if (!sc)
		sc = sysconf_claim(SYSCONF(334), 0, 6, "pcie");

	BUG_ON(!sc);

	/* Drive RST_N low, set device type */
	sysconf_write(sc, PCIE_DEVICE_TYPE);

	sysconf_write(sc, PCIE_DEFAULT_VAL);

	mdelay(1);
	return sc;
}

static void stih415_pcie_enable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *) handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL | PCIE_APP_LTSSM_ENABLE);
}

static void stih415_pcie_disable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *) handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL);
}

/* Ops to drive the platform specific bits of the interface */
static struct stm_plat_pcie_ops stih415_pcie_ops = {
	.init          = stih415_pcie_init,
	.enable_ltssm  = stih415_pcie_enable_ltssm,
	.disable_ltssm = stih415_pcie_disable_ltssm,
};

/* PCI express support */
#define PCIE_MEM_START 0x20000000
#define PCIE_MEM_SIZE  0x20000000
#define PCIE_CONFIG_SIZE (64*1024)

#define LMI_START 0x40000000
#define LMI_SIZE  0x80000000

static struct stm_plat_pcie_config stih415_plat_pcie_config = {
	.ahb_val = 0x264207,
	.ops = &stih415_pcie_ops,
	.pcie_window.start = PCIE_MEM_START,
	.pcie_window.size = PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,
	.pcie_window.lmi_start = LMI_START,
	.pcie_window.lmi_size = LMI_SIZE,
};

static struct platform_device stih415_pcie_device = {
	.name = "pcie_stm",
	.id = -1,
	.num_resources = 6,
	.resource = (struct resource[]) {
		/* Place 64K Config window at end of memory block */
		STM_PLAT_RESOURCE_MEM_NAMED("pcie config",
			PCIE_MEM_START + PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,
			PCIE_CONFIG_SIZE),
		STM_PLAT_RESOURCE_MEM_NAMED("pcie cntrl", 0xfe800000, 0x1000),
		STM_PLAT_RESOURCE_MEM_NAMED("pcie ahb", 0xfe808000, 0x8),
		STIH415_RESOURCE_IRQ_NAMED("pcie inta", 166),
		STIH415_RESOURCE_IRQ_NAMED("pcie syserr", 171),
		STIH415_RESOURCE_IRQ_NAMED("msi mux", 167),
	},
	.dev.platform_data = &stih415_plat_pcie_config,
};


void __init stih415_configure_pcie(struct stih415_pcie_config *config)
{
	stih415_plat_pcie_config.reset_gpio = config->reset_gpio;
	stih415_plat_pcie_config.reset = config->reset;
	/* There is only one PCIe controller on the orly and it is hardwired
	 * to use lane1
	 */
	stih415_plat_pcie_config.miphy_num = 1;

	platform_device_register(&stih415_pcie_device);
}

#endif /* CONFIG_OF */
