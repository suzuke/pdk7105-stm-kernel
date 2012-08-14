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
#include <linux/stm/stih415-periphs.h>

#include <asm/hardware/gic.h>

#include <linux/stm/hom.h>
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include <mach/soc-stih415.h>

#define SBC_MBX

#define SBC_GPIO_PORT(_nr)	(0xfe610000 + (_nr) * 0x1000)
#define LMI_RET_GPIO_PORT		4
#define LMI_RET_GPIO_PIN		4
#define LMI_RETENTION_PIN	stm_gpio(LMI_RET_GPIO_PORT, LMI_RET_GPIO_PIN)

static const unsigned long __stxh415_hom_ddr_0[] = {
synopsys_ddr32_in_hom(STIH415_MPE_DDR0_PCTL_BASE),
};

static const unsigned long __stxh415_hom_ddr_1[] = {
synopsys_ddr32_in_hom(STIH415_MPE_DDR1_PCTL_BASE),
/*
 * Enable retention mode gpio
 *
 */
POKE32(SBC_GPIO_PORT(LMI_RET_GPIO_PORT) + STM_GPIO_REG_CLR_POUT,
	1 << LMI_RET_GPIO_PIN),
};

#define HOM_TBL(name) {					\
		.addr = name,				\
		.size = ARRAY_SIZE(name) * sizeof(long),\
	}

static struct hom_table stxh415_hom_table[] = {
	HOM_TBL(__stxh415_hom_ddr_0),
	HOM_TBL(__stxh415_hom_ddr_1),
};

static int stxh415_hom_prepare(void)
{
	stm_freeze_board();

	return 0;
}

static int stxh415_hom_complete(void)
{
	stm_restore_board();

	return 0;
}

static struct stm_mem_hibernation stxh415_hom = {
	.eram_iomem = (void *)0xc00a0000,

	.early_console_rate = 100000000,

	.ops.prepare = stxh415_hom_prepare,
	.ops.complete = stxh415_hom_complete,
};

static int __init hom_stxh415_setup(void)
{
	int ret;
	int i;

	ret = gpio_request(LMI_RETENTION_PIN, "LMI retention mode");
	if (ret) {
		pr_err("[STM]: [PM]: [HoM]: GPIO for retention mode"
			"not acquired\n");
		return ret;
	};

	gpio_direction_output(LMI_RETENTION_PIN, 1);

	INIT_LIST_HEAD(&stxh415_hom.table);

	stxh415_hom.early_console_base = (void *)ioremap(
		stm_asc_console_device->resource[0].start, 0x1000);

	pr_info("stm pm hom: Early console [%d] @ 0x%x\n",
			stm_asc_console_device->id,
			(unsigned int) stxh415_hom.early_console_base);

	for (i = 0; i < ARRAY_SIZE(stxh415_hom_table); ++i)
		list_add_tail(&stxh415_hom_table[i].node, &stxh415_hom.table);

	ret =  stm_hom_register(&stxh415_hom);
	if (ret) {
		gpio_free(LMI_RETENTION_PIN);
		pr_err("[STM][HoM]: Error: on stm_hom_register\n");
	}
	return ret;
}

module_init(hom_stxh415_setup);
