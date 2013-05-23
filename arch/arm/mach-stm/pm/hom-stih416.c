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

#define SBC_GPIO_PORT_BASE	(SASG2_SBC_PIO_BASE)

#define SBC_MBX			(SASG2_SBC_LPM_BASE + 0xb4000)
#define SBC_MBX_WRITE_STATUS(x)	(SBC_MBX + 0x4 + 0x4 * (x))

#define SELF_REFRESH_ON_PCTL	1

static const unsigned long __stxh416_hom_ddr_0[] = {
OR32(MPE42_DDR_PCTL_BASE(0) + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_in_self_refresh(MPE42_DDR_PCTL_BASE(0)),
#else
UPDATE32(MPE42_DDR_PWR_DWN(0), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(0), MPE42_DDR_PWR_STATUS_ACK, 0),
#endif

synopsys_ddr32_phy_hom_enter(MPE42_DDR_PCTL_BASE(0)),
};

static const unsigned long __stxh416_hom_ddr_1[] = {
OR32(MPE42_DDR_PCTL_BASE(1) + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),

#ifdef SELF_REFRESH_ON_PCTL
synopsys_ddr32_in_self_refresh(MPE42_DDR_PCTL_BASE(1)),
#else
UPDATE32(MPE42_DDR_PWR_DWN(1), ~MPE42_DDR_PWR_DWN_REQ, 0),
WHILE_NE32(MPE42_DDR_PWR_STATUS(1), MPE42_DDR_PWR_STATUS_ACK, 0),
#endif

synopsys_ddr32_phy_hom_enter(MPE42_DDR_PCTL_BASE(1)),
};

static unsigned long __stxh416_hom_lmi_retention[] = {
/*
 * Enable retention mode gpio
 * Address and value set in stm_setup_lmi_retention_gpio.
 */
POKE32(0x0, 0x0),
};

static const unsigned long __stxh416_hom_enter_passive[] = {
/*
 * Send message 'ENTER_PASSIVE' (0x5)
 */
POKE32(SBC_MBX_WRITE_STATUS(0), 0x5),
};

#ifdef CONFIG_HOM_SELF_RESET
static const unsigned long __stxh416_hom_reset[] = {
/*
 * CPU Self-Reset
 */
POKE32(SASG2_SBC_SYSCONF_BASE + 0x7d0, 0),
};

#endif
#define HOM_TBL(name) {					\
		.addr = name,				\
		.size = ARRAY_SIZE(name) * sizeof(long),\
	}

static struct hom_table stxh416_hom_table[] = {
	HOM_TBL(__stxh416_hom_ddr_0),
	HOM_TBL(__stxh416_hom_ddr_1),
	HOM_TBL(__stxh416_hom_lmi_retention),
	HOM_TBL(__stxh416_hom_enter_passive),
#ifdef CONFIG_HOM_SELF_RESET
	HOM_TBL(__stxh416_hom_reset),
#endif
};

static struct stm_wakeup_devices stxh416_wkd;

static int stxh416_hom_prepare(void)
{
	stm_check_wakeup_devices(&stxh416_wkd);
	stm_freeze_board(&stxh416_wkd);

	return 0;
}

static int stxh416_hom_complete(void)
{
	stm_restore_board(&stxh416_wkd);

	return 0;
}

static struct stm_mem_hibernation stxh416_hom = {
	.eram_iomem = (void *)0xc00a0000,
	.gpio_iomem = (void *)SBC_GPIO_PORT_BASE,

	.ops.prepare = stxh416_hom_prepare,
	.ops.complete = stxh416_hom_complete,
};

int __init stm_hom_stxh416_setup(struct stm_hom_board *hom_board)
{
	int ret, i;

	stxh416_hom.board = hom_board;

	ret = stm_setup_lmi_retention_gpio(&stxh416_hom,
		 __stxh416_hom_lmi_retention);

	if (ret)
		return ret;

	INIT_LIST_HEAD(&stxh416_hom.table);

	for (i = 0; i < ARRAY_SIZE(stxh416_hom_table); ++i)
		list_add_tail(&stxh416_hom_table[i].node, &stxh416_hom.table);

	ret =  stm_hom_register(&stxh416_hom);
	if (ret) {
		gpio_free(hom_board->lmi_retention_gpio);
		pr_err("stm pm hom: Error: on stm_hom_register\n");
	}
	return ret;
}
