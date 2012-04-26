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

#include <asm/hardware/gic.h>

#include <linux/stm/hom.h>
#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

#include <mach/soc-stih415.h>

#define SBC_MBX

static unsigned long stxh415_hom_table[] __cacheline_aligned = {
/*
 * Now execute the real operations
 */
synopsys_ddr32_in_hom(STIH415_MPE_DDR0_PCTL_BASE),
synopsys_ddr32_in_hom(STIH415_MPE_DDR1_PCTL_BASE),

/*
 * TO BE DONE:
 * Notify 'Ready for Power-off' via MBX
 */
#warning "Ready for Power-off __not__ implemented"

/* END. */
END_MARKER,

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

	.tbl_addr = (unsigned long)stxh415_hom_table,
	.tbl_size = ARRAY_SIZE(stxh415_hom_table) * sizeof(long),

	.ops.prepare = stxh415_hom_prepare,
	.ops.complete = stxh415_hom_complete,
};

static int __init hom_stxh415_setup(void)
{
	stxh415_hom.early_console_base = (void *)ioremap(
		stm_asc_configured_devices[stm_asc_console_device]
			->resource[0].start, 0x1000);

	pr_info("[STM]: [PM]: [HoM]: Early console [%d] @ 0x%x\n",
			stm_asc_console_device,
			stxh415_hom.early_console_base);

	return stm_hom_register(&stxh415_hom);
}

module_init(hom_stxh415_setup);
