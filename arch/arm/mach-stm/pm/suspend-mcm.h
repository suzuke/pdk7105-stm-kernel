/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/irqflags.h>
#include <linux/irq.h>
#include <linux/io.h>

#include <linux/stm/sysconf.h>
#include <linux/stm/clk.h>
#include <linux/stm/wakeup_devices.h>

#include "../suspend.h"
#include <mach/hardware.h>
#include <asm/hardware/gic.h>	/* gic offset and struct gic_chip_data */

#include <linux/stm/poke_table.h>
#include <linux/stm/synopsys_dwc_ddr32.h>

extern struct stm_mcm_suspend *main_mcm;
extern struct stm_mcm_suspend *peripheral_mcm;

int stm_dual_mcm_suspend_begin(suspend_state_t state);

int stm_dual_mcm_suspend_pre_enter(suspend_state_t state);

void stm_dual_mcm_suspend_post_enter(suspend_state_t state);

void stm_dual_mcm_suspend_end(void);

int stm_get_wake_irq(void *gic_cpu);

