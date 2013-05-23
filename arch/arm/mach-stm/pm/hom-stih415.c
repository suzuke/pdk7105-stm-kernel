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

#define SBC_GPIO_PORT_BASE	(0xfe610000)

#define SBC_MBX			0xfe4b4000
#define SBC_MBX_WRITE_STATUS(x)	(SBC_MBX + 0x4 + 0x4 * (x))

static const unsigned long __stxh415_hom_ddr_0[] = {
synopsys_ddr32_in_hom(MPE41_DDR0_PCTL_BASE),
};

static const unsigned long __stxh415_hom_ddr_1[] = {
synopsys_ddr32_in_hom(MPE41_DDR1_PCTL_BASE),
};

static unsigned long __stxh415_hom_lmi_retention[] = {
/*
 * Enable retention mode gpio
 * Address and value set in stm_setup_lmi_retention_gpio.
 */
POKE32(0x0, 0x0), /* dummy value just to guarantee the required space */
};

static const unsigned long __stxh415_hom_enter_passive[] = {
/*
 * Send message 'ENTER_PASSIVE' (0x5)
 */
POKE32(SBC_MBX_WRITE_STATUS(0), 0x5),
};

#ifdef CONFIG_HOM_SELF_RESET
static const unsigned long __stxh415_hom_reset[] = {
/*
 * CPU Self-Reset
 */
POKE32(STIH415_SBC_SYSCONF_BASE + (11) * 0x4, 0),
};
#endif

#define HOM_TBL(name) {					\
		.addr = name,				\
		.size = ARRAY_SIZE(name) * sizeof(long),\
	}

static struct hom_table stxh415_hom_table[] = {
	HOM_TBL(__stxh415_hom_ddr_0),
	HOM_TBL(__stxh415_hom_ddr_1),
	HOM_TBL(__stxh415_hom_lmi_retention),
	HOM_TBL(__stxh415_hom_enter_passive),
#ifdef CONFIG_HOM_SELF_RESET
	HOM_TBL(__stxh415_hom_reset),
#endif
};

static struct stm_wakeup_devices stxh415_wkd;

static int stxh415_hom_prepare(void)
{
	stm_check_wakeup_devices(&stxh415_wkd);
	stm_freeze_board(&stxh415_wkd);

	return 0;
}

static int stxh415_hom_complete(void)
{
	stm_restore_board(&stxh415_wkd);

	return 0;
}

static struct stm_mem_hibernation stxh415_hom = {
	.eram_iomem = (void *)0xc00a0000,
	.gpio_iomem = (void *)SBC_GPIO_PORT_BASE,

	.ops.prepare = stxh415_hom_prepare,
	.ops.complete = stxh415_hom_complete,
};

int __init stm_hom_stxh415_setup(struct stm_hom_board *hom_board)
{
	int ret, i;
	int lmi_gpio_port, lmi_gpio_pin;

	stxh415_hom.board = hom_board;

	ret = stm_setup_lmi_retention_gpio(&stxh415_hom,
		__stxh415_hom_lmi_retention);

	if (ret)
		return ret;

	INIT_LIST_HEAD(&stxh415_hom.table);

	for (i = 0; i < ARRAY_SIZE(stxh415_hom_table); ++i)
		list_add_tail(&stxh415_hom_table[i].node, &stxh415_hom.table);

	ret =  stm_hom_register(&stxh415_hom);
	if (ret) {
		gpio_free(hom_board->lmi_retention_gpio);
		pr_err("stm pm hom: Error: on stm_hom_register\n");
	}
	return ret;
}
