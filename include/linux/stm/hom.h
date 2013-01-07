/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2010  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#ifndef __STM_MEM_HIBERNATION_H__
#define __STM_MEM_HIBERNATION_H__

#include <linux/hom.h>
#include <linux/list.h>
#include <linux/compiler.h>

#include <linux/stm/wakeup_devices.h>

struct stm_hom_board {
	int lmi_retention_gpio;
	int (*freeze)(struct stm_wakeup_devices *dev_wk);
	int (*restore)(struct stm_wakeup_devices *dev_wk);
};

struct hom_table {
	long const *addr;
	unsigned long size;
	struct list_head node;
};

struct stm_mem_hibernation {
	struct stm_hom_board *board;
	void __iomem *eram_iomem; /* on ARM */
	void __iomem *gpio_iomem; /* on ARM */
	long flags;
	struct list_head table;
	void __iomem *early_console_base;
	unsigned long early_console_rate;
	struct platform_hom_ops ops;
};

int stm_hom_register(struct stm_mem_hibernation *platform);

void stm_hom_exec_table(unsigned int tbl, unsigned int tbl_end,
		unsigned long lpj);


int stm_setup_lmi_retention_gpio(struct stm_mem_hibernation *platform,
	unsigned long lmi_retention_table[]);

void stm_defrost_kernel(void);

int stm_freeze_board(struct stm_wakeup_devices *dev_wk);
int stm_restore_board(struct stm_wakeup_devices *dev_wk);

void hom_printk(char *buf, ...);

/*
 * Chip specific registration function
 */
int stm_hom_stxh415_setup(struct stm_hom_board *hom_board);
int stm_hom_stxh416_setup(struct stm_hom_board *hom_board);
int stm_hom_fli7610_setup(struct stm_hom_board *hom_board);

#endif
