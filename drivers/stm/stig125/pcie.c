/*
 * (c) 2013 STMicroelectronics Limited
 *
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
#include <linux/stm/stig125.h>
#include <linux/stm/amba_bridge.h>

#define PCIE_SYS_INT		(1<<5)
#define PCIE_APP_REQ_RETRY_EN	(1<<3)
#define PCIE_APP_LTSSM_ENABLE	(1<<2)
#define PCIE_APP_INIT_RST	(1<<1)
#define PCIE_DEVICE_TYPE	(1<<0)

#define PCIE_DEFAULT_VAL        (PCIE_DEVICE_TYPE)

static void stig125_pcie_init(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *)handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL);

	mdelay(1);
}

static void stig125_pcie_enable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *)handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL | PCIE_APP_LTSSM_ENABLE);
}

static void stig125_pcie_disable_ltssm(void *handle)
{
	struct sysconf_field *sc = (struct sysconf_field *)handle;

	sysconf_write(sc, PCIE_DEFAULT_VAL);
}

/* Ops to drive the platform specific bits of the interface */
static struct stm_plat_pcie_ops stig125_pcie_ops = {
	.init          = stig125_pcie_init,
	.enable_ltssm  = stig125_pcie_enable_ltssm,
	.disable_ltssm = stig125_pcie_disable_ltssm,
};

/* PCI express support */
#define PCIE_MEM_SIZE  0x8000000
#define PCIE_CONFIG_SIZE (64*1024)

#define LMI_SIZE  0xbc000000

/* We give 8 MSI irqs to each controller, should be plenty */
#define MSI_IRQS_PER_CONTROLLER	8
#define PCIE_MSI_FIRST_IRQ(x) (NR_IRQS - (MSI_IRQS_PER_CONTROLLER * ((3-(x)))))
#define PCIE_MSI_LAST_IRQ(x) (PCIE_MSI_FIRST_IRQ(x)+(MSI_IRQS_PER_CONTROLLER-1))

#define PCIE_PLAT_CONFIG(id, pci_window)			\
	[(id)] = {						\
	.pcie_window.start = pci_window,			\
	.pcie_window.size = PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,	\
	.pcie_window.lmi_start = 0x40000000,			\
	.pcie_window.lmi_size = LMI_SIZE,			\
	.ahb_val = 0x26c028,					\
	.ops = &stig125_pcie_ops,				\
	.miphy_num = id + 1, /* miphy 0 is pure SATA */		\
	.reset_gpio = -EINVAL,					\
	.reset = NULL,						\
	}

static struct stm_plat_pcie_config stig125_plat_pcie_config[3] = {
	PCIE_PLAT_CONFIG(0, 0x20000000),
	PCIE_PLAT_CONFIG(1, 0x28000000),
	PCIE_PLAT_CONFIG(2, 0x30000000)
};

#define PCIE_STM(_id, _base, _mem_start, _inta_irq)			\
	[_id] = {							\
		.name = "pcie_stm",					\
		.id = _id,						\
		.num_resources  = 7,					\
		.resource = (struct resource[]) {			\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie config",		\
			_mem_start + PCIE_MEM_SIZE - PCIE_CONFIG_SIZE,	\
		PCIE_CONFIG_SIZE),				\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie cntrl", _base, 0x1000),\
		STM_PLAT_RESOURCE_MEM_NAMED("pcie ahb", _base + 0x8000, 0x8),\
		STIG125_RESOURCE_IRQ_NAMED("pcie inta", _inta_irq),	\
		STIG125_RESOURCE_IRQ_NAMED("pcie syserr", _inta_irq - 1),\
		STIG125_RESOURCE_IRQ_NAMED("msi mux", _inta_irq + 2),	\
		{							\
			.start = PCIE_MSI_FIRST_IRQ(_id),		\
			.end  = PCIE_MSI_LAST_IRQ(_id),			\
			.name = "msi range",				\
			.flags = IORESOURCE_IRQ,			\
		}							\
		},							\
	.dev.platform_data = &stig125_plat_pcie_config[_id],		\
}

static struct platform_device stig125_pcie_devices[] = {
	PCIE_STM(0, 0xfef20000, 0x20000000, 87),
	PCIE_STM(1, 0xfef30000, 0x28000000, 95),
	PCIE_STM(2, 0xfef40000, 0x30000000, 101),
};

void __init stig125_configure_pcie(int controller)
{
	struct sysconf_field *sc;

	switch (controller) {
	case 0:
		sc = sysconf_claim(SYSCONF(267), 0, 5, "pcie");
		BUG_ON(!sc);
		break;
	case 1:
		sc = sysconf_claim(SYSCONF(263), 0, 5, "pcie");
		BUG_ON(!sc);
		break;
	case 2:
		sc = sysconf_claim(SYSCONF(264), 0, 5, "pcie");
		BUG_ON(!sc);
		break;
	default:
		BUG();
	}

	stig125_plat_pcie_config[controller].ops_handle = sc;
	platform_device_register(&stig125_pcie_devices[controller]);
}
