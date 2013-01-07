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
#define SBC_GPIO_PORT_BASE	(0xfe610000)


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

static unsigned long __fli7610_hom_lmi_retention[] = {
/*
 * Enable retention mode gpio
 * Address and value set in stm_setup_lmi_retention_gpio.
 */
POKE32(0x0, 0x0),
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

static struct stm_wakeup_devices fli7610_wkd;

static int fli7610_hom_prepare(void)
{
	stm_check_wakeup_devices(&fli7610_wkd);
	stm_freeze_board(&fli7610_wkd);

	return 0;
}

static int fli7610_hom_complete(void)
{
	stm_restore_board(&fli7610_wkd);

	return 0;
}

static struct stm_mem_hibernation fli7610_hom = {
	.eram_iomem = (void *)0xc0080000,
	.gpio_iomem = (void *)SBC_GPIO_PORT_BASE,

	.ops.prepare = fli7610_hom_prepare,
	.ops.complete = fli7610_hom_complete,
};

int __init stm_hom_fli7610_setup(struct stm_hom_board *hom_board)
{
	int ret, i;

	fli7610_hom.board = hom_board;

	ret = stm_setup_lmi_retention_gpio(&fli7610_hom,
		__fli7610_hom_lmi_retention);

	if (ret)
		return ret;

	INIT_LIST_HEAD(&fli7610_hom.table);

	for (i = 0; i < ARRAY_SIZE(fli7610_hom_table); ++i)
		list_add_tail(&fli7610_hom_table[i].node, &fli7610_hom.table);

	ret =  stm_hom_register(&fli7610_hom);

	return ret;
}

