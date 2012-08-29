/*
 * (c) 2012 STMicroelectronics Limited
 *
 * Author: David Mckay <david.mckay@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * ARM specific glue to join up the stm pci driver in drivers/stm/pcie.c
 * to the main arm pci code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/stm/platform.h>
#include <linux/stm/pci-glue.h>
#include <linux/gpio.h>
#include <linux/cache.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/list.h>

struct stm_pci_info {
	/* ARM specific data structure */
	struct hw_pci hw_pci;
	enum stm_pci_type type;
	int legacy_irq;
	struct platform_device *pdev;
	struct pci_ops *ops;
	struct resource res[3];
};

static struct stm_pci_info *sysdata_to_info(struct pci_sys_data *sys)
{
	struct hw_pci *hw_pci = sys->hw;

	return container_of(hw_pci, struct stm_pci_info, hw_pci);
}

static struct stm_pci_info *bus_to_info(struct pci_bus *bus)
{
	struct pci_sys_data *sys = bus->sysdata;

	return sysdata_to_info(sys);
}

/* Given a pci_bus, return the corresponding platform data */
struct platform_device *stm_pci_bus_to_platform(struct pci_bus *bus)
{
	struct stm_pci_info *info;

	info = bus_to_info(bus);
	if (!info)
		return NULL;

	return info->pdev;
}

/* Used to build a sensible name for the resource */
static char *stm_pci_res_name(struct platform_device *pdev,
			      enum stm_pci_type type,
			      const char *sub)
{
	char res_name[32];
	char res_id[8];
	char *name;
	int len;

	scnprintf(res_id, sizeof(res_id), ".%d", pdev->id);
	len = scnprintf(res_name, sizeof(res_name), "pci%s%s %s",
			(type == STM_PCI_EXPRESS) ? "e" : "",
			(pdev->id >= 0) ? res_id : "", sub);

	name = devm_kzalloc(&pdev->dev, len + 1, GFP_KERNEL);

	return strcpy(name, res_name);
}

/* It would be nice if we could use the private_data field
 * of the pci_sys_data to give us the handle, but we can't
 * because it is allocated for us
 */
static int stm_pci_setup(int nr, struct pci_sys_data *sys)
{
	struct stm_pci_info *info = sysdata_to_info(sys);
	struct platform_device *pdev = info->pdev;
	void *platdata = dev_get_platdata(&pdev->dev);
	struct stm_pci_window_info *window;
	phys_addr_t pref_window_start;
	int i;
	int res;

	if (nr != 0)
		return -EINVAL;

	if (info->type == STM_PCI_EXPRESS) {
		struct stm_plat_pcie_config *config = platdata;
		window = &config->pcie_window;
	} else {
		struct stm_plat_pci_config *config = platdata;
		window = &config->pci_window;
	}

	if (window->io_size != 0) {
		info->res[0].flags = IORESOURCE_IO;
		info->res[0].start = window->io_start;
		info->res[0].end = window->io_start + window->io_size - 1;
		info->res[0].name = stm_pci_res_name(pdev, info->type, "io");

		res = request_resource(&ioport_resource, info->res);
		if (res < 0)
			return res;
	}

	/* We split the available memory window 1/3 between non-prefetchable
	 * and prefetachable memory. This seems like a reasonable compromise
	 * We align to a 64K boundary to make the numbers look less wierd.
	 */
	pref_window_start = window->start + ALIGN(window->size/3, 64*1024);

	info->res[1].flags = IORESOURCE_MEM;
	info->res[1].start = window->start;
	info->res[1].end = pref_window_start - 1;
	info->res[1].name = stm_pci_res_name(pdev, info->type, "mem");

	res = request_resource(&iomem_resource, info->res + 1);
	if (res < 0) {
		release_resource(info->res);
		return res;
	}

	/* This is the prefetchable window */
	info->res[2].flags = IORESOURCE_MEM | IORESOURCE_PREFETCH;
	info->res[2].start = pref_window_start;
	info->res[2].end = window->start + window->size - 1 ;
	info->res[2].name = stm_pci_res_name(pdev, info->type, "pref mem");

	res = request_resource(&iomem_resource, info->res + 2);
	if (res < 0) {
		release_resource(info->res);
		release_resource(info->res + 1);
		return res;
	}

	/* Add the PCI resources, we skip the first one if we have no IO */
	for (i = (window->io_size == 0) ; i < 3 ; i++)
		pci_add_resource(&sys->resources, info->res + i);

	/* Anybody who does any IO will blow up */
	sys->io_offset = 0;
	/* The offset is already in the resource, so we have zero here */
	sys->mem_offset = 0;

	return 1;
}

static struct pci_bus __devinit *stm_pci_scan(int nr, struct pci_sys_data *sys)
{
	struct stm_pci_info *info = sysdata_to_info(sys);

	return pci_scan_root_bus(NULL, sys->busnr, info->ops,
				 sys, &sys->resources);
}

/* This is only for PCI express, for PCI we have to call through to the BSP
 * layer as virtually any interrupt
 */
static int __devinit stm_map_pcie_irq(const struct pci_dev *dev,
				      u8 slot, u8 pin)
{
	struct stm_pci_info *info;

	info = bus_to_info(dev->bus);
	if (info == NULL)
		return -EINVAL;

	return info->legacy_irq;
}

int __devinit stm_pci_register_controller(struct platform_device *pdev,
					  struct pci_ops *config_ops,
					  enum stm_pci_type type)
{
	struct stm_pci_info *info;
	struct resource *res;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	res = devm_kzalloc(&pdev->dev, sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;

	if (type == STM_PCI_EXPRESS) {
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						   "pcie inta");
		if (!res)
			return -ENXIO;
		info->legacy_irq = res->start;
		info->hw_pci.map_irq = stm_map_pcie_irq;
		/* PCI express bridges do standard swizzling */
		info->hw_pci.swizzle = pci_std_swizzle;
	} else {
		struct stm_plat_pci_config *cnf = dev_get_platdata(&pdev->dev);
		/* The irq map function comes from the BSP for PCI */
		info->hw_pci.map_irq = cnf->pci_map_irq;
		/* The map function will have to the swizzling */
		info->hw_pci.swizzle = NULL;
		info->legacy_irq = -EINVAL;
	}

	info->type = type;
	info->pdev = pdev;
	info->ops = config_ops;

	/* We cannot use the nr_controllers feature, instead
	 * we instantiate a new controller every time. This is because
	 * the SH PCI layer doesn't support this, and we need to support
	 * both. There is no functionality loss anyway.
	 */
	info->hw_pci.nr_controllers = 1;
	info->hw_pci.setup = stm_pci_setup;
	info->hw_pci.scan = stm_pci_scan;

	pci_common_init(&(info->hw_pci));

	return 0;
}

enum stm_pci_type stm_pci_controller_type(struct pci_bus *bus)
{
	struct stm_pci_info *info;

	info = bus_to_info(bus);

	BUG_ON(!info);

	return info->type;
}

