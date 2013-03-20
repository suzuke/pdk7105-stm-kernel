/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __STM_ARM_SUSPEND_h__
#define __STM_ARM_SUSPEND_h__

#include <linux/suspend.h>
#include <linux/stm/wakeup_devices.h>
#include <linux/list.h>

#include "pokeloop.h"

struct stm_suspend_table {
	long const *enter;
	unsigned long enter_size;
	long const *exit;
	unsigned long exit_size;
	struct list_head node;
};

struct stm_platform_suspend {
	void __iomem *eram_iomem;
	struct list_head mem_tables;
	int (*pre_enter)(suspend_state_t state);
	void (*post_enter)(suspend_state_t state);
	int (*get_wake_irq)(void);
	struct platform_suspend_ops ops;
};

struct stm_mcm_suspend {
	struct stm_suspend_table *tables;
	unsigned long nr_tables;
	int (*begin)(suspend_state_t state,
		struct stm_wakeup_devices *wkd);
	int (*pre_enter)(suspend_state_t state,
		struct stm_wakeup_devices *wkd);
	void (*post_enter)(suspend_state_t state);
	void (*end)(suspend_state_t state);
};

static inline int
stm_suspend_mcm_begin(struct stm_mcm_suspend *mcm,
	suspend_state_t state, struct stm_wakeup_devices *wkd)
{
	if (mcm && mcm->begin)
		return mcm->begin(state, wkd);
	return 0;
}

static inline int
stm_suspend_mcm_pre_enter(struct stm_mcm_suspend *mcm,
	suspend_state_t state, struct stm_wakeup_devices *wkd)
{
	if (mcm && mcm->pre_enter)
		return mcm->pre_enter(state, wkd);
	return 0;
}

static inline void
stm_suspend_mcm_post_enter(struct stm_mcm_suspend *mcm,
	suspend_state_t state)
{
	if (mcm && mcm->post_enter)
		mcm->post_enter(state);
}

static inline void
stm_suspend_mcm_end(struct stm_mcm_suspend *mcm,
	suspend_state_t state)
{
	if (mcm && mcm->end)
		mcm->end(state);
}



int stm_suspend_register(struct stm_platform_suspend *platform);

struct stm_suspend_eram_data {
	void *pa_table_enter;
	void *pa_table_exit;
	void *pa_stm_eram_code;
	void *pa_pokeloop;
};

void stm_suspend_exec_table(struct stm_suspend_eram_data *pa_eram_data,
		unsigned long va_2_pa);

int stm_suspend_on_eram(void);
extern unsigned long stm_suspend_on_eram_sz;

#endif
