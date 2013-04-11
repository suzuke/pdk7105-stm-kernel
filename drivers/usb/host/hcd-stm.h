/*
 * HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2009 STMicroelectronics Limited
 * Author: Francesco Virlinzi
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */
#ifndef __ST_USB_HCD__
#define __ST_USB_HCD__

/* Wrapper Glue registers */

#define AHB2STBUS_STRAP_OFFSET          0x14    /* From WRAPPER_GLUE_BASE */
#define AHB2STBUS_STRAP_PLL             0x08    /* undocumented */
#define AHB2STBUS_STRAP_8_BIT           0x00    /* ss_word_if */
#define AHB2STBUS_STRAP_16_BIT          0x04    /* ss_word_if */


/* Extensions to the standard USB register set */

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET       (0x10 + 0x84) /* From EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD       0x00800080

#define NR_USB_CLKS			(3)
struct drv_usb_data {
	void *ahb2stbus_wrapper_glue_base;
	struct platform_device *ehci_device;
	struct platform_device *ohci_device;
	struct stm_device_state *device_state;
	struct stm_amba_bridge *amba_bridge;
	struct clk *clks[NR_USB_CLKS];
	struct stm_plat_usb_data *plat_data;
};

#endif
