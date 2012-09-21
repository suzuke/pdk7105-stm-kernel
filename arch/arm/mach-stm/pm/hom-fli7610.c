/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#include "../hom.h"
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/irqflags.h>
#include <linux/io.h>

#include <linux/stm/stih415.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/gpio.h>
#include <linux/stm/fli7610-periphs.h>
#include <linux/stm/mpe41-periphs.h>

#include <asm/hardware/gic.h>

#include <linux/stm/hom.h>
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include <mach/soc-fli7610.h>

#define SBC_MBX			0xfe4b4000
#define SBC_MBX_WRITE_STATUS(x)	(SBC_MBX + 0x4 + 0x4 * (x))
#define SBC_GPIO_PORT(_nr)	(0xfe610000 + (_nr) * 0x1000)
#define LMI_RET_GPIO_PORT		4
#define LMI_RET_GPIO_PIN		4
#define LMI_RETENTION_PIN	stm_gpio(LMI_RET_GPIO_PORT, LMI_RET_GPIO_PIN)


#define SYSCONF_SYSTEM(x)	(MPE41_SYSTEM_SYSCONF_BASE + \
				 ((x) - 600) * 0x4)
#define SYSCONF_DDR0_PWR_DWN	SYSCONF_SYSTEM(608)
#define SYSCONF_DDR0_PWR_ACK	SYSCONF_SYSTEM(670)
#define SYSCONF_DDR1_PWR_DWN	SYSCONF_SYSTEM(613)
#define SYSCONF_DDR1_PWR_ACK	SYSCONF_SYSTEM(672)


static const unsigned long __fli7610_hom_ddr_0[] = {
OR32(MPE41_DDR0_PCTL_BASE + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),
synopsys_ddr32_in_hom(MPE41_DDR0_PCTL_BASE),
};

static const unsigned long __fli7610_hom_ddr_1[] = {
OR32(MPE41_DDR1_PCTL_BASE + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),
synopsys_ddr32_in_hom(MPE41_DDR1_PCTL_BASE),
};

static const unsigned long __fli7610_hom_lmi_retention[] = {
/*
 * Enable retention mode gpio
 */
POKE32(SBC_GPIO_PORT(LMI_RET_GPIO_PORT) + STM_GPIO_REG_CLR_POUT,
	1 << LMI_RET_GPIO_PIN),
};

static const unsigned long __fli7610_hom_enter_passive[] = {
/*
 * Send message 'ENTER_PASSIVE' (0x5)
 */
POKE32(SBC_MBX_WRITE_STATUS(0), 0x5),
};

#define HOM_TBL(name) {					\
		.addr = name,				\
		.size = ARRAY_SIZE(name) * sizeof(long),\
	}

static struct hom_table fli7610_hom_table[] = {
	HOM_TBL(__fli7610_hom_ddr_0),
	HOM_TBL(__fli7610_hom_ddr_1),
	HOM_TBL(__fli7610_hom_lmi_retention),
	HOM_TBL(__fli7610_hom_enter_passive),
};

static int fli7610_hom_prepare(void)
{
	stm_freeze_board();

	return 0;
}

static int fli7610_hom_complete(void)
{
	stm_restore_board();

	return 0;
}

static struct stm_mem_hibernation fli7610_hom = {
	.eram_iomem = (void *)0xc0080000,

	.early_console_rate = 100000000,

	.ops.prepare = fli7610_hom_prepare,
	.ops.complete = fli7610_hom_complete,
};

static int __init hom_fli7610_setup(void)
{
	int ret;
	int i;

	ret = gpio_request(LMI_RETENTION_PIN, "LMI retention mode");
	if (ret) {
		pr_err("stm pm hom: GPIO for retention mode"
			"not acquired\n");
		return ret;
	};

	gpio_direction_output(LMI_RETENTION_PIN, 1);

	INIT_LIST_HEAD(&fli7610_hom.table);

	fli7610_hom.early_console_base = (void *)ioremap(
		stm_asc_console_device->resource[0].start, 0x1000);

	pr_info("stm pm hom: Early console [%d] @ 0x%x\n",
			stm_asc_console_device->id,
			(unsigned int) fli7610_hom.early_console_base);

	for (i = 0; i < ARRAY_SIZE(fli7610_hom_table); ++i)
		list_add_tail(&fli7610_hom_table[i].node, &fli7610_hom.table);

	ret =  stm_hom_register(&fli7610_hom);
	if (ret) {
		gpio_free(LMI_RETENTION_PIN);
		pr_err("stm pm hom: Error: on stm_hom_register\n");
	}
	return ret;
}

module_init(hom_fli7610_setup);
