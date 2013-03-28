/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Srinivas Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/stm/device.h>
#include <linux/stm/platform.h>

#define ST40_BOOT_ADDR_SHIFT		1
static int mpe41_st40_boot(struct stm_device_state *dev_state,
					unsigned long boot_addr,
					unsigned long phy_addr)
{

	stm_device_sysconf_write(dev_state, "MASK_RESET", 1);
	stm_device_sysconf_write(dev_state, "RESET" , 0);
	stm_device_sysconf_write(dev_state, "BOOT_ADDR",
					 boot_addr >> ST40_BOOT_ADDR_SHIFT);

	stm_device_sysconf_write(dev_state, "LMI0_NOT_LMI1",
				(phy_addr & 0x40000000) ? 1 : 0);
	stm_device_sysconf_write(dev_state, "LMI_SYS_BASE",
				((phy_addr & 0x3C000000) >> 26));

	stm_device_sysconf_write(dev_state, "RESET", 1);


	return 0;
}
struct plat_stm_st40_coproc_data mpe41_st40_coproc_data = {
	.cpu_boot = mpe41_st40_boot,
};
