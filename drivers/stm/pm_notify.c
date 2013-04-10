/*
 * -------------------------------------------------------------------------
 * <linux_root>/drivers/stm/pm_notify.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/stm/pm_notify.h>

static DEFINE_MUTEX(pm_notify_mutex);
static LIST_HEAD(pm_notify_list);

enum stm_pm_notify_ret stm_pm_early_check(int wkirq)
{
	struct stm_pm_notify *handler;

	list_for_each_entry(handler, &pm_notify_list, list)
		if (handler->irq == wkirq)
			return handler->notify();

	return STM_PM_RET_OK;
}

static struct stm_pm_notify *__look_for(struct stm_pm_notify *handler)
{
	struct stm_pm_notify *p;

	list_for_each_entry(p,  &pm_notify_list, list)
		if (p == handler)
			return p;
	return NULL;
}

int stm_register_pm_notify(struct stm_pm_notify *handler)
{
	if (!handler || !handler->notify)
		return -EINVAL;

	mutex_lock(&pm_notify_mutex);
	if (__look_for(handler)) {
		mutex_unlock(&pm_notify_mutex);
		return -EEXIST;
	}

	list_add(&handler->list, &pm_notify_list);
	mutex_unlock(&pm_notify_mutex);

	return 0;
}

EXPORT_SYMBOL(stm_register_pm_notify);

int stm_unregister_pm_notify(struct stm_pm_notify *handler)
{
	if (!handler)
		return -EINVAL;

	mutex_lock(&pm_notify_mutex);
	if (!__look_for(handler)) {
		mutex_unlock(&pm_notify_mutex);
		return -EINVAL;
	}

	list_del(&handler->list);
	mutex_unlock(&pm_notify_mutex);

	return 0;
}
EXPORT_SYMBOL(stm_unregister_pm_notify);
