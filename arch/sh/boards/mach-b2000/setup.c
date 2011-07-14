/*
 * arch/sh/boards/mach-b2000/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STiH415 processor module board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>
#include <asm/irq.h>



static void __init b2000_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STiH415-MBOARD (b2000) "
			"initialisation\n");

	stih415_early_device_init();

	stih415_configure_asc(2, &(struct stih415_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
}

static int __init b2000_devices_init(void)
{
	stih415_configure_usb(0);
	stih415_configure_usb(1);
	stih415_configure_usb(2);

	return 0;
}
arch_initcall(b2000_devices_init);

struct sh_machine_vector mv_b2000 __initmv = {
	.mv_name		= "b2000",
	.mv_setup		= b2000_setup,
	.mv_nr_irqs		= NR_IRQS,
};
