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
#ifndef __STM_ARM_HIBERNATION_ON_MEMORY__
#define __STM_ARM_HIBERNATION_ON_MEMORY__

/*
 * the HFD (HoM frozen data) have to follow the
 * ordering in the struct hom_frozen_data
 */
#define HFD_ID_OFF(x)			(4 * (x))
#define HFD_MMU_PG_DIR_OFF		HFD_ID_OFF(0)
#define HFD_SP_OFF			HFD_ID_OFF(1)
#define HFD_MMU_TTBR0_OFF		HFD_ID_OFF(2)
#define HFD_MMU_TTBR1_OFF		HFD_ID_OFF(3)
#define HFD_MMU_TTBCR_OFF		HFD_ID_OFF(4)

#ifdef CONFIG_HOM_DEBUG
#define HFD_DEBUG_OFF			HFD_ID_OFF(5)
#define HFD_DEBUG_DATA_OFF		HFD_ID_OFF(6)
#endif

#define HFD_END				HFD_ID_OFF(8)

#ifndef __ASSEMBLER__

#include <linux/hom.h>
#include <linux/compiler.h>

void write_pen_release(int val);

struct hom_frozen_data {
	long pg_dir;
	long sp;
	long ttbr0;
	long ttbr1;
	long ttbcr;
#ifdef CONFIG_HOM_DEBUG
	long debug;
	long debug_data;
#endif
};

extern struct hom_frozen_data hom_frozen_data;

struct stm_hom_eram_data {
	void *pa_table;
	void *pa_stm_eram_code;
	void *pa_pokeloop;
};

void stm_hom_exec_on_eram(struct stm_hom_eram_data *pa_eram_data,
			  void *hom_pgd, unsigned long va_2_pa);

int stm_hom_on_eram(void);
extern unsigned long stm_hom_on_eram_sz;

#ifdef CONFIG_HOM_DEBUG
static inline void hom_mark_step(int x)
{
	hom_frozen_data.debug = x;
	__asm__ __volatile__(
		/* clean & invalidate by MVA */
		"mcr	p15, 0, %0, c7, c10, 1\n"
		: : "r" (&hom_frozen_data.debug)
		: "memory");

}
#else
static inline void hom_mark_step(int x) {	}
#endif
#endif /* __ASSEMBLER__ */

#endif
