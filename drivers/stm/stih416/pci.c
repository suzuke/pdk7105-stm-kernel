/*
 * (c) 2012 STMicroelectronics Limited
 *
 * Author: Divya Pathak <divya.pathak@st.com>
 * Author: David McKay <david.mckay@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stih416.h>
#include <linux/stm/amba_bridge.h>

#define PCIE_SYS_INT		(1<<5)
#define PCIE_APP_REQ_RETRY_EN	(1<<3)
#define PCIE_APP_LTSSM_ENABLE	(1<<2)
#define PCIE_APP_INIT_RST	(1<<1)
#define PCIE_DEVICE_TYPE	(1<<0)
#define PCIE_DEFAULT_VAL        (PCIE_DEVICE_TYPE)

static void *stih416_pcie_init(struct platform_device *pdev)
{
	struct stm_plat_pcie_config *config = pdev->dev.platform_data;
	int port = config->miphy_num;
	struct sysconf_field *sc, *pdown;

	switch (port) {
	case 0:
		sc = sysconf_claim(SYSCONF(2524), 0, 5, "pcie");
		BUG_ON(!sc);
		/* Power up PCIe0 Controller */
		pdown = sysconf_claim(SYSCONF(2525), 7, 7, "pcie");
		BUG_ON(!pdown);
		sysconf_write(pdown, 0);
		break;
	case 1:
		sc = sysconf_claim(SYSCONF(2523), 0, 5, "pcie");
		BUG_ON(!sc);
		/* Power up PCIe1 Controller */
		pdown = sysconf_claim(SYSCONF(2525), 5, 5, "pcie");
		BUG_ON(!pdown);
		sysconf_write(pdown, 0);
		break;
	default:
		BUG();
	}

	/* Drive RST_N low, set device type */
	sysconf_write(sc, PCIE_DEVICE_TYPE);

	sysconf_write(sc, PCIE_DEFAULT_VAL);

	mdelay(1);
	return sc;
}

static void stih416_pcie_enable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *)handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL | PCIE_APP_LTSSM_ENABLE);
}

static void stih416_pcie_disable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *)handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL);
}

/* Ops to drive the platform specific bits of the interface */
static struct stm_plat_pcie_ops stih416_pcie_ops = {
	.init = stih416_pcie_init,
	.enable_ltssm = stih416_pcie_enable_ltssm,
	.disable_ltssm = stih416_pcie_disable_ltssm,
};

/* 256 megs dedicated to each pcie controller */
#define PCIE_MEM_SIZE  0x10000000
#define PCIE_CONFIG_SIZE (64*1024)

#define LMI_START 0x40000000
#define LMI_SIZE  0x80000000

static struct stm_plat_pcie_config stih416_plat_pcie_config[2] = {
	[0] = {
	       .pcie_window.start = 0x30000000,
	       .pcie_window.size = PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,
	       .pcie_window.lmi_start = 0x40000000,
	       .pcie_window.lmi_size = LMI_SIZE,

	       },
	[1] = {
	       .pcie_window.start = 0x20000000,
	       .pcie_window.size = PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,
	       .pcie_window.lmi_start = 0x40000000,
	       .pcie_window.lmi_size = LMI_SIZE,
	       },
};

#define PCIE_STM(_id, _base, _mem_start, _inta_irq, _syserr_irq, _msi_irq) \
[_id] = {								\
	.name = "pcie_stm",						\
	.id = _id,							\
	.num_resources  = 6,						\
	.resource = (struct resource[]) {				\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie config",		\
			_mem_start + PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,	\
			PCIE_CONFIG_SIZE),				\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie cntrl", _base, 0x1000),\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie ahb", _base + 0x8000, 0x8),\
		STIH416_RESOURCE_IRQ_NAMED("pcie inta", _inta_irq),	\
		STIH416_RESOURCE_IRQ_NAMED("pcie syserr", _syserr_irq),\
		STIH416_RESOURCE_IRQ_NAMED("msi mux", _msi_irq),	\
	},								\
	.dev.platform_data = &stih416_plat_pcie_config[_id],		\
}

static struct platform_device stih416_pcie_devices[] = {
	PCIE_STM(0, 0xfe390000, 0x30000000, 158, 161, 159),
	PCIE_STM(1, 0xfe800000, 0x20000000, 166, 169, 167),
};

void __init stih416_configure_pcie(struct stih416_pcie_config *config)
{
	int port = config->port;

	BUG_ON((port < 0) || (port > 1));

	stih416_plat_pcie_config[port].ops = &stih416_pcie_ops;
	stih416_plat_pcie_config[port].ahb_val = 0x26C208,
	    stih416_plat_pcie_config[port].reset_gpio = config->reset_gpio;
	stih416_plat_pcie_config[port].reset = config->reset;
	stih416_plat_pcie_config[port].miphy_num = port;
	platform_device_register(&stih416_pcie_devices[port]);
}
