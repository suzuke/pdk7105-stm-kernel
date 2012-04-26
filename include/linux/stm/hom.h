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
#include <linux/compiler.h>

struct stm_mem_hibernation {
	void __iomem *eram_iomem; /* on ARM */
	long flags;
	long tbl_addr;
	long tbl_size;
	void __iomem *early_console_base;
	unsigned long early_console_rate;
	struct platform_hom_ops ops;
};

int stm_hom_register(struct stm_mem_hibernation *platform);

void stm_hom_exec_table(unsigned int tbl, unsigned int tbl_end,
		unsigned long lpj);


void stm_defrost_kernel(void);

int stm_freeze_board(void);
int stm_restore_board(void);

struct stm_hom_board {
	int (*freeze)(void);
	int (*restore)(void);
};

int stm_hom_board_register(struct stm_hom_board *board);

void hom_printk(char *buf, ...);

#endif
