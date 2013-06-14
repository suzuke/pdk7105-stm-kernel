/*
 * OHCI HCD (Host Controller Driver) for USB
 *
 * (C) copyright STMicroelectronics 2005
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * STMicroelectronics on-chip USB host controller Bus Glue.
 * Based on the StrongArm ohci-sa1111.c file
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include "./hcd-stm.h"

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)				\
		pr_debug("%s: " fmt, __func__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif


static int
stm_ohci_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret = 0;

	dgb_print("\n");
	if ((ret = ohci_init(ohci)) < 0)
		return ret;

	if ((ret = ohci_run(ohci)) < 0) {
		err("can't start %s", hcd->self.bus_name);
		ohci_stop(hcd);
		return ret;
	}

	return 0;
}

static const struct hc_driver ohci_st40_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"STMicroelectronics OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/* generic hardware linkage */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/* basic lifecycle operations */
	.start =		stm_ohci_start,
	.stop =			ohci_stop,
	.shutdown = ohci_shutdown,

	/* managing i/o requests and associated device resources */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/* scheduling support */
	.get_frame_number =	ohci_get_frame,

	/* root hub support */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef CONFIG_PM
	.bus_suspend =		ohci_bus_suspend,
	.bus_resume =		ohci_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

static int ohci_hcd_stm_init(struct platform_device *pdev)
{
	struct usb_hcd *hcd = NULL;
	int retval;
	struct resource *res;
	struct platform_device *stm_usb_pdev;

	dgb_print("\n");
	hcd = usb_create_hcd(&ohci_st40_hc_driver, &pdev->dev,
			     dev_name(&pdev->dev));

	if (!hcd) {
		pr_debug("hcd_create_hcd failed");
		retval = -ENOMEM;
		goto err0;
	}

	stm_usb_pdev = to_platform_device(pdev->dev.parent);

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_MEM, "ohci");
	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	if (!request_mem_region(hcd->rsrc_start, hcd->rsrc_len,	hcd_name)) {
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

	ohci_hcd_init(hcd_to_ohci(hcd));

	res = platform_get_resource_byname(stm_usb_pdev,
			IORESOURCE_IRQ, "ohci");
	retval = usb_add_hcd(hcd, res->start, 0);
	if (retval == 0) {
#ifdef CONFIG_PM
		hcd->self.root_hub->do_remote_wakeup = 0;
		hcd->self.root_hub->persist_enabled = 0;
		usb_disable_autosuspend(hcd->self.root_hub);
#endif
		return retval;
	}

	iounmap(hcd->regs);
err2:
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
err1:
	usb_put_hcd(hcd);
err0:
	return retval;
}

static int ohci_hcd_stm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);

	usb_remove_hcd(hcd);
	iounmap(hcd->regs);
	release_mem_region(hcd->rsrc_start, hcd->rsrc_len);
	usb_put_hcd(hcd);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int ohci_hcd_stm_probe(struct platform_device *pdev)
{
	int ret;
	ret = ohci_hcd_stm_init(pdev);
	if (ret)
		return ret;
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
static DEFINE_MUTEX(stm_ohci_usb_mutex); /* to serialize the operations.. */

static int stm_ohci_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pm_runtime_status_suspended(dev))
		/* already suspended using pm_runtime */
		return 0;
	mutex_lock(&stm_ohci_usb_mutex);
	ohci_hcd_stm_remove(pdev);
	mutex_unlock(&stm_ohci_usb_mutex);
	return 0;
}

static int stm_ohci_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	if (pm_runtime_status_suspended(dev))
		/* want resume using pm_runtime */
		return 0;
	mutex_lock(&stm_ohci_usb_mutex);
	ohci_hcd_stm_init(pdev);
	mutex_unlock(&stm_ohci_usb_mutex);
	return 0;
}

#ifdef CONFIG_PM_RUNTIME
static int stm_ohci_runtime_suspend(struct device *dev)
{
	if (pm_runtime_status_suspended(dev))
		return 0;
	return stm_ohci_freeze(dev);
}

static int stm_ohci_runtime_resume(struct device *dev)
{
	if (pm_runtime_status_suspended(dev))
		return 0;
	return stm_ohci_restore(dev);
}
#else
#define stm_ohci_runtime_suspend	NULL
#define stm_ohci_runtime_resume		NULL
#endif

static const struct dev_pm_ops stm_ohci_pm = {
	.freeze = stm_ohci_freeze,
	.thaw = stm_ohci_restore,
	.restore = stm_ohci_restore,
	.runtime_suspend = stm_ohci_runtime_suspend,
	.runtime_resume = stm_ohci_runtime_resume,
};
#endif

static struct platform_driver ohci_hcd_stm_driver = {
	.probe = ohci_hcd_stm_probe,
	.remove = ohci_hcd_stm_remove,
	.shutdown = usb_hcd_platform_shutdown,
	.driver = {
		.name = "stm-ohci",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &stm_ohci_pm,
#endif
	},
};
