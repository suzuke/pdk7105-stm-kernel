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

#include <linux/stm/stih416.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/platform.h>
#include <linux/stm/gpio.h>
#include <linux/stm/mpe42-periphs.h>
#include <linux/stm/sasg2-periphs.h>

#include <asm/hardware/gic.h>

#include <linux/stm/hom.h>
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include <mach/soc-stih416.h>

#define SBC_GPIO_PORT(_nr)	(SASG2_SBC_PIO_BASE + (_nr) * 0x1000)
#define LMI_RET_GPIO_PORT		4
#define LMI_RET_GPIO_PIN		4
#define LMI_RETENTION_PIN	stih416_gpio(LMI_RET_GPIO_PORT,	\
					LMI_RET_GPIO_PIN)

#define SBC_MBX			(SASG2_SBC_LPM_BASE + 0xb4000)
#define SBC_MBX_WRITE_STATUS(x)	(SBC_MBX + 0x4 + 0x4 * (x))

static const unsigned long __stxh416_hom_ddr_0[] = {
OR32(MPE42_DDR_PCTL_BASE(0) + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),

UPDATE32(MPE42_DDR_PWR_DWN(0), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(0), MPE42_DDR_PWR_STATUS_ACK, 0),

synopsys_ddr32_phy_hom_enter(MPE42_DDR_PCTL_BASE(0)),
};

static const unsigned long __stxh416_hom_ddr_1[] = {
OR32(MPE42_DDR_PCTL_BASE(1) + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),

UPDATE32(MPE42_DDR_PWR_DWN(1), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(1), MPE42_DDR_PWR_STATUS_ACK, 0),

synopsys_ddr32_phy_hom_enter(MPE42_DDR_PCTL_BASE(1)),
};

static const unsigned long __stxh416_hom_lmi_retention[] = {
/*
 * Enable retention mode gpio
 */
POKE32(SBC_GPIO_PORT(LMI_RET_GPIO_PORT) + STM_GPIO_REG_CLR_POUT,
	1 << LMI_RET_GPIO_PIN),
};

static const unsigned long __stxh416_hom_enter_passive[] = {
/*
 * Send message 'ENTER_PASSIVE' (0x5)
 */
POKE32(SBC_MBX_WRITE_STATUS(0), 0x5),
};

#define HOM_TBL(name) {					\
		.addr = name,				\
		.size = ARRAY_SIZE(name) * sizeof(long),\
	}

static struct hom_table stxh416_hom_table[] = {
	HOM_TBL(__stxh416_hom_ddr_0),
	HOM_TBL(__stxh416_hom_ddr_1),
	HOM_TBL(__stxh416_hom_lmi_retention),
	HOM_TBL(__stxh416_hom_enter_passive),
};

static int stxh416_hom_prepare(void)
{
	stm_freeze_board();

	return 0;
}

static int stxh416_hom_complete(void)
{
	stm_restore_board();

	return 0;
}

static struct stm_mem_hibernation stxh416_hom = {
	.eram_iomem = (void *)0xc00a0000,

	.ops.prepare = stxh416_hom_prepare,
	.ops.complete = stxh416_hom_complete,
};

static int __init hom_stxh416_setup(void)
{
	int ret;
	int i;

	ret = gpio_request(LMI_RETENTION_PIN, "LMI retention mode");
	if (ret) {
		pr_err("stm pm hom: GPIO for LMI retention mode"
			" not acquired\n");
		return ret;
	};

	gpio_direction_output(LMI_RETENTION_PIN, 1);

	INIT_LIST_HEAD(&stxh416_hom.table);

	for (i = 0; i < ARRAY_SIZE(stxh416_hom_table); ++i)
		list_add_tail(&stxh416_hom_table[i].node, &stxh416_hom.table);

	ret =  stm_hom_register(&stxh416_hom);
	if (ret) {
		gpio_free(LMI_RETENTION_PIN);
		pr_err("stm pm hom: Error: on stm_hom_register\n");
	}
	return ret;
}

module_init(hom_stxh416_setup);
