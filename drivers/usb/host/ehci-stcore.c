/*
 * EHCI HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include "./hcd-stm.h"

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET       (0x10 + 0x84) /* From EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD       0x00800080

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		pr_debug("%s: " fmt, __func__, ## args)
#else
#define dgb_print(fmt, args...)
#endif

static int ehci_st40_reset(struct usb_hcd *hcd)
{
	writel(AHB2STBUS_INOUT_THRESHOLD,
	       hcd->regs + AHB2STBUS_INSREG01_OFFSET);
	return ehci_init(hcd);
}

#ifdef CONFIG_PM
static int
stm_ehci_bus_resume(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci;

	ehci = hcd_to_ehci(hcd);
	ehci_writel(ehci, FLAG_CF, &ehci->regs->configured_flag);

	return ehci_bus_resume(hcd);
}
#else
#define stm_ehci_bus_resume		NULL
#endif

static const struct hc_driver ehci_stm_hc_driver = {
	.description = hcd_name,
	.product_desc = "STMicroelectronics EHCI Host Controller",
	.hcd_priv_size = sizeof(struct ehci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq = ehci_irq,
	.flags = HCD_MEMORY | HCD_USB2,

	/*
	 * basic lifecycle operations
	 */
	.reset = ehci_st40_reset,
	.start = ehci_run,
	.stop = ehci_stop,
	.shutdown = ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue = ehci_urb_enqueue,
	.urb_dequeue = ehci_urb_dequeue,
	.endpoint_disable = ehci_endpoint_disable,
	.endpoint_reset	= ehci_endpoint_reset,

	/*
	 * scheduling support
	 */
	.get_frame_number = ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data = ehci_hub_status_data,
	.hub_control = ehci_hub_control,
/*
 * The ehci_bus_suspend suspends all the root hub ports but
 * it leaves all the interrupts enabled on insert/remove devices
 */
	.bus_suspend = ehci_bus_suspend,
	.bus_resume = stm_ehci_bus_resume,
	.relinquish_port = ehci_relinquish_port,
	.port_handed_over = ehci_port_handed_over,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static int ehci_hcd_stm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	if (!hcd)
		return 0;

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int ehci_hcd_stm_init(struct platform_device *pdev)
{
	int retval = 0;
	struct usb_hcd *hcd;
        struct ehci_hcd *ehci;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct platform_device *stm_usb_pdev;

	dgb_print("\n");
	hcd = usb_create_hcd(&ehci_stm_hc_driver, dev, dev_name(dev));
	if (!hcd) {
		retval = -ENOMEM;
		goto err0;
	}

	stm_usb_pdev = to_platform_device(pdev->dev.parent);

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_MEM, "ehci");
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len, hcd_name)) {
		pr_debug("request_mem_region failed");
		retval = -EBUSY;
		goto err1;
	}

	hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);
	if (!hcd->regs) {
		pr_debug("ioremap failed");
		retval = -ENOMEM;
		goto err2;
	}

	ehci = hcd_to_ehci(hcd);
	ehci->caps = hcd->regs;
	ehci->regs = hcd->regs +
		HC_LENGTH(ehci, ehci_readl(ehci, &ehci->caps->hc_capbase));

	/* cache this readonly data; minimize device reads */
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	/*
	 * Fix the reset port issue on a load-unload-load sequence
	 */
	ehci->has_reset_port_bug = 1,
	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_IRQ, "ehci");

	retval = usb_add_hcd(hcd, res->start, 0);
	if (unlikely(retval))
		goto err3;

#ifdef CONFIG_PM
	hcd->self.root_hub->do_remote_wakeup = 0;
	hcd->self.root_hub->persist_enabled = 0;
	usb_disable_autosuspend(hcd->self.root_hub);
#endif
	return 0;
err3:
	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

static int ehci_hcd_stm_probe(struct platform_device *pdev)
{
	int ret;
	ret = ehci_hcd_stm_init(pdev);
	if (ret)
		return ret;
	/* by default the device is on */
	pm_runtime_set_active(&pdev->dev);
	pm_suspend_ignore_children(&pdev->dev, true);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_get(&pdev->dev);	/* notify the ehci is used
					 * the pm_runtime will take
					 * care to resume the hcd
					 */
	return 0;
}

#ifdef CONFIG_PM
static DEFINE_MUTEX(stm_ehci_usb_mutex); /* to serialize the operations.. */

static int stm_ehci_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pm_runtime_status_suspended(dev))
		/* want resume using pm_runtime */
		return 0;
	mutex_lock(&stm_ehci_usb_mutex);
	ehci_hcd_stm_remove(pdev);
	mutex_unlock(&stm_ehci_usb_mutex);
	return 0;

}

static int stm_ehci_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pm_runtime_status_suspended(dev))
		/* want resume using pm_runtime */
		return 0;
	mutex_lock(&stm_ehci_usb_mutex);
	ehci_hcd_stm_init(pdev);
	mutex_unlock(&stm_ehci_usb_mutex);

	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int stm_ehci_runtime_suspend(struct device *dev)
{
	if (pm_runtime_status_suspended(dev))
		return 0;
	return stm_ehci_freeze(dev);
}
static int stm_ehci_runtime_resume(struct device *dev)
{
	if (pm_runtime_status_suspended(dev))
		return 0;
	return stm_ehci_restore(dev);
}
#else
#define stm_ehci_runtime_suspend	NULL
#define stm_ehci_runtime_resume		NULL
#endif

static const struct dev_pm_ops stm_ehci_pm = {
	.freeze = stm_ehci_freeze,
	.thaw = stm_ehci_restore,
	.restore = stm_ehci_restore,
	.runtime_suspend = stm_ehci_runtime_suspend,
	.runtime_resume = stm_ehci_runtime_resume,
};
#else
static const struct dev_pm_ops stm_ehci_pm;
#endif

static struct platform_driver ehci_hcd_stm_driver = {
	.probe = ehci_hcd_stm_probe,
	.remove = ehci_hcd_stm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "stm-ehci",
		.owner = THIS_MODULE,
		.pm = &stm_ehci_pm,
	},
};
