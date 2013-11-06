/*
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 */

#include <linux/stm/wakeup_devices.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/export.h>
#include <linux/phy.h>

static int wokenup_by;

int stm_get_wakeup_reason(void)
{
	return wokenup_by;
}

void stm_set_wakeup_reason(int irq)
{
	wokenup_by = irq;
}

static void stm_wake_init(struct stm_wakeup_devices *wkd)
{
	memset(wkd, 0, sizeof(*wkd));
}

static int __check_wakeup_device(struct device *dev, void *data)
{
	struct stm_wakeup_devices *wkd = (struct stm_wakeup_devices *)data;

	if (device_may_wakeup(dev)) {
		pr_info("stm pm: -> device %s can wakeup\n", dev_name(dev));
		if (strstr(dev_name(dev), "lirc"))
			wkd->lirc_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "hdmi"))
			wkd->hdmi_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "snps,dwmac"))
			wkd->stm_mac0_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "snps,dwmac.0"))
			wkd->stm_mac0_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "snps,dwmac.1"))
			wkd->stm_mac1_can_wakeup = 1;
		else if (!strcmp(dev_name(dev), "stm-hdmi-cec"))
			wkd->hdmi_cec = 1;
		else if (!strcmp(dev_name(dev), "stm-hdmi-hot"))
			wkd->hdmi_hotplug = 1;
		else if (!strcmp(dev_name(dev), "stm-kscan"))
			wkd->kscan = 1;
		else if (!strcmp(dev_name(dev), "stm-rtc"))
			wkd->rtc = 1;
		else if (strstr(dev_name(dev), "uart"))
			wkd->asc = 1;
		else if (strstr(dev_name(dev), "rtc_sbc"))
			wkd->rtc_sbc = 1;
	}
	return 0;
}

#ifdef CONFIG_PHYLIB
static int __check_mdio_wakeup_device(struct device *dev, void *data)
{
	struct stm_wakeup_devices *wkd = (struct stm_wakeup_devices *)data;

	if (device_may_wakeup(dev)) {
		if (!strncmp(dev_name(dev), "0:", 2))
			wkd->stm_phy_can_wakeup = 1;
		else if (!strncmp(dev_name(dev), "1:", 2))
			wkd->stm_phy_can_wakeup = 1;
	}

	return 0;
}
#endif

int stm_check_wakeup_devices(struct stm_wakeup_devices *wkd)
{
	stm_wake_init(wkd);
	bus_for_each_dev(&platform_bus_type, NULL, wkd, __check_wakeup_device);
#ifdef CONFIG_PHYLIB
	bus_for_each_dev(&mdio_bus_type, NULL, wkd, __check_mdio_wakeup_device);
#endif
	return 0;
}

EXPORT_SYMBOL(stm_check_wakeup_devices);
