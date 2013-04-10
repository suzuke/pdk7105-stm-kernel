/*
 * -------------------------------------------------------------------------
 * <linux_root>/include/linux/stm/pm_notify.h
 * -------------------------------------------------------------------------
 * STMicroelectronics
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#ifndef __STM_LINUX_NOTIFY__
#define __STM_LINUX_NOTIFY__

#include <linux/list.h>

enum stm_pm_notify_ret {
	STM_PM_RET_OK,
	STM_PM_RET_AGAIN,
};

struct stm_pm_notify {
	struct list_head list;
	int irq;
	enum stm_pm_notify_ret (*notify)(void);
};

enum stm_pm_notify_ret stm_pm_early_check(int irq_reason);

int stm_register_pm_notify(struct stm_pm_notify *notify);

int stm_unregister_pm_notify(struct stm_pm_notify *notify);

#endif
