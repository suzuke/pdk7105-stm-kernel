/*
 * (c) 2011 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/stm/device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih415.h>
#include <linux/stm/amba_bridge.h>
#include <linux/stm/stih415-periphs.h>
#include <linux/delay.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#include <mach/soc-stih415.h>
#include <mach/hardware.h>
#endif

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#include "pio-control.h"



/* USB resources ---------------------------------------------------------- */
static u64 stih415_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_HOST_PWR		"USB_HOST_PWR"
#define USB_PWR_ACK		"USB_PWR_ACK"
#define USB_IN_DC_SHIFT		"USB_IN_DC_SHIFT"
#define USB_IN_EDGE_CTRL	"USB_IN_EDGE_CTRL"

static int stih415_usb_init(struct stm_device_state *device_state)
{
	stm_device_sysconf_write(device_state, USB_IN_DC_SHIFT, 0);
	stm_device_sysconf_write(device_state, USB_IN_EDGE_CTRL, 1);

	return 0;
}

static void stih415_usb_power(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_PWR_ACK)
			== value)
			break;
		mdelay(10);
	}
}

/* STBus Convertor config */
static struct stm_amba_bridge_config stih415_amba_usb_config = {
	STM_DEFAULT_USB_AMBA_PLUG_CONFIG(128),
	.type2.sd_config_missing = 1,
};

static struct stm_plat_usb_data stih415_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih415_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih415_usb_init,
			.power = stih415_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(336), 0, 0,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(384), 0, 0,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(332), 0, 0,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(332), 3, 3,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(9, 4, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(9, 5, 1),
				},
			},
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih415_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih415_usb_init,
			.power = stih415_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(336), 1, 1,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(384), 1, 1,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(332), 1, 1,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(332), 4, 4,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(18, 0, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(18, 1, 1),
				},
			},
		},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stih415_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.init = stih415_usb_init,
			.power = stih415_usb_power,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(336), 2, 2,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(384), 2, 2,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(332), 2, 2,
					USB_IN_DC_SHIFT),
				STM_DEVICE_SYSCONF(SYSCONF(332), 5, 5,
					USB_IN_EDGE_CTRL),
			},
			.pad_config = &(struct stm_pad_config) {
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(18, 2, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(18, 3, 1),
				},
			},
		},
	},
};

static struct platform_device stih415_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stih415_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stih415_usb_platform_data[0],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe100000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe1ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe1ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe1fff00, 0x100),
			STIH415_RESOURCE_IRQ_NAMED("ehci", 155),
			STIH415_RESOURCE_IRQ_NAMED("ohci", 156),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stih415_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stih415_usb_platform_data[1],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe200000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe2ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe2ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe2fff00, 0x100),
			STIH415_RESOURCE_IRQ_NAMED("ehci", 157),
			STIH415_RESOURCE_IRQ_NAMED("ohci", 158),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stih415_usb_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stih415_usb_platform_data[2],
		},
		.num_resources = 6,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",
					0xfe300000, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",
					0xfe3ffc00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",
					0xfe3ffe00, 0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",
					0xfe3fff00, 0x100),
			STIH415_RESOURCE_IRQ_NAMED("ehci", 159),
			STIH415_RESOURCE_IRQ_NAMED("ohci", 160),
		},
	},
};

void __init stih415_configure_usb(int port)
{
	static int osc_initialized;
	static int configured[ARRAY_SIZE(stih415_usb_devices)];
	struct sysconf_field *sc;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stih415_usb_devices));

	BUG_ON(configured[port]++);

	if (!osc_initialized++) {
		/* USB2TRIPPHY_OSCIOK */
		sc = sysconf_claim(SYSCONF(332), 6, 6, "USB");
		sysconf_write(sc, 1);
	}

	platform_device_register(&stih415_usb_devices[port]);
}
