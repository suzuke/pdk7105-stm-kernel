/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/device.h>
#include <linux/stm/sysconf.h>

#include <linux/stm/stih416.h>

/* St231 coproc fw loading resources */
static struct platform_device mpe42_st231_coprocessor_devices[5] = {
	{
		.name = "stm-coproc-st200",
		.id = 0,
		.dev.platform_data = &(struct plat_stm_st231_coproc_data) {
			.name = "audio",
			.id = 0,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYSCONF(SYSCONF(7551),
							6, 31, "BOOT_ADDR"),
					STM_DEVICE_SYSCONF(SYSCONF(7563),
							26, 26, "RESET"),
				},
			},
			.boot_shift = 6,
			.not_reset = 1,
		},
	}, {
		.name = "stm-coproc-st200",
		.id = 1,
		.dev.platform_data = &(struct plat_stm_st231_coproc_data) {
			.name = "dmu",
			.id = 0,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYSCONF(SYSCONF(7550),
							6, 31, "BOOT_ADDR"),
					STM_DEVICE_SYSCONF(SYSCONF(7563),
							27, 27, "RESET"),
				},
			},
			.boot_shift = 6,
			.not_reset = 1,
		},
	}, {
		.name = "stm-coproc-st200",
		.id = 2,
		.dev.platform_data = &(struct plat_stm_st231_coproc_data) {
			.name = "audio",
			.id = 1,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYSCONF(SYSCONF(7552),
							6, 31, "BOOT_ADDR"),
					STM_DEVICE_SYSCONF(SYSCONF(7563),
							28, 28, "RESET"),
				},
			},
			.boot_shift = 6,
			.not_reset = 1,
		},
	}, {
		.name = "stm-coproc-st200",
		.id = 3,
		.dev.platform_data = &(struct plat_stm_st231_coproc_data) {
			.name = "dmu",
			.id = 1,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYSCONF(SYSCONF(7553),
							6, 31, "BOOT_ADDR"),
					STM_DEVICE_SYSCONF(SYSCONF(7563),
							29, 29, "RESET"),
				},
			},
			.boot_shift = 6,
			.not_reset = 1,
		},
	}, {
		.name = "stm-coproc-st200",
		.id = 4,
		.dev.platform_data = &(struct plat_stm_st231_coproc_data) {
			.name = "gp",
			.id = 0,
			.device_config = &(struct stm_device_config) {
				.sysconfs_num = 2,
				.sysconfs = (struct stm_device_sysconf []) {
					STM_DEVICE_SYSCONF(SYSCONF(7554),
							6, 31, "BOOT_ADDR"),
					STM_DEVICE_SYSCONF(SYSCONF(7563),
							30, 30, "RESET"),
				},
			},
			.boot_shift = 6,
			.not_reset = 1,
		},
	},
};

#define ST40_BOOT_ADDR_SHIFT            1

static int mpe42_st40_boot(struct stm_device_state *dev_state,
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

static struct platform_device mpe42_st40_coproc_device = {
	.name = "stm-coproc-st40",
	.id = 0,
	.dev.platform_data  = &(struct plat_stm_st40_coproc_data) {
		.name = "st40",
		.id = 0,
		.cpu_boot = mpe42_st40_boot,
		.device_config = &(struct stm_device_config) {
		.sysconfs_num = 9,
		.sysconfs = (struct stm_device_sysconf []){
			STM_DEVICE_SYSCONF(SYSCONF(7549), 1, 28,
				"BOOT_ADDR"),
			STM_DEVICE_SYSCONF(SYSCONF(7545), 2, 2,
				"RESET"),
			STM_DEVICE_SYSCONF(SYSCONF(7530), 0, 3,
				"LMI_SYS_BASE"),
			STM_DEVICE_SYSCONF(SYSCONF(7530), 4, 4,
				"LMI0_NOT_LMI1"),
			STM_DEVICE_SYSCONF(SYSCONF(7530), 5, 5,
				"BART_LOCK_ENABLE"),
			STM_DEVICE_SYSCONF(SYSCONF(7546), 2, 2,
				"MASK_RESET"),
			/* Bart Status */
			STM_DEVICE_SYSCONF(SYSCONF(7566), 5, 5,
				"BART_LOCK_ENABLE_STATUS"),
			STM_DEVICE_SYSCONF(SYSCONF(7566), 4, 4,
				"BART_LMI0_NOT_LMI1_SEL_STATUS"),
			STM_DEVICE_SYSCONF(SYSCONF(7566), 0, 3,
				"BART_LMI_OFFSET_BASEADDR_STATUS"),
			/* Masks soft powerreset on st40 to
			 * peripherial resets
			 */
#if 0
			STM_DEVICE_SYSCONF(SYSCONF(7546), 8, 8,
				"SW_PWR_RESET_MASK"),
#endif
			},
		},
	},
};

/* Late initialisation ---------------------------------------------------- */

static struct platform_device *mpe42_devices[] __initdata = {
	&mpe42_st231_coprocessor_devices[0],
	&mpe42_st231_coprocessor_devices[1],
	&mpe42_st231_coprocessor_devices[2],
	&mpe42_st231_coprocessor_devices[3],
	&mpe42_st231_coprocessor_devices[4],
	&mpe42_st40_coproc_device
};

static int __init mpe42_devices_setup(void)
{
	return platform_add_devices(mpe42_devices,
			ARRAY_SIZE(mpe42_devices));
}
device_initcall(mpe42_devices_setup);
