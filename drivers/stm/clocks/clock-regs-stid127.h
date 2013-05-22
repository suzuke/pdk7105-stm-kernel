/*****************************************************************************
 *
 * File name   : clock-regs-stid127.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_D127_H
#define __CLOCK_LLA_REGS_D127_H

/* --- Base addresses --------------------------------------- */
#define CKGA0_BASE_ADDRESS		0xFEA10000
#define CKGA1_BASE_ADDRESS		0xFEA20000
#define QFS_TEL_ADDRESS			0xFE910000
#define QFS_DOC_ADDRESS			0xFEF62000

#define SYSCFG_HD			0xFE930000	/* 9xx */
#define SYSCFG_CPU			0xFE9A0000	/* 7xx */
#define SYSCFG_PEAST			0xFEBD0000	/* 14xx */
#define SYSCFG_WEST			0xFEBF0000	/* 4x .. 6x */
#define SYSCFG_PWEST			0xFEC00000	/* 10xx */
#define SYSCFG_DOCSIS			0xFEF90000	/* 6xx */
#define SYSCFG_SOUTH			0xFEFA0000	/* 2xx */
#define SYSCFG_PSOUTH			0xFEFD0000	/* 12xx */

/* --- CKGA registers --- */
#define CKGA_PLL_CFG(_pll_id, _reg_id)	((_reg_id) * 4 + (_pll_id) * 0xc)
#define CKGA_POWER_CFG			0x018
#define CKGA_CLKOPSRC_SWITCH_CFG	0x01c
#define CKGA_CLKOPSRC_SWITCH_CFG2	0x020

#define CKGA_CLKOBS_MUX0_CFG		0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT	0x034
#define CKGA_CLKOBS_CMD			0x038
#define CKGA_CLKOBS_STATUS		0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT	0x040
#define CKGA_OSCMUX_DEBUG		0x044
#define CKGA_CLKOBS_MUX1_CFG		0x048
#define CKGA_LOW_POWER_CTRL		0x04C
#define CKGA_LOW_POWER_CFG		0x050

#define CKGA_PLL0_REG3_CFG		0x054
#define CKGA_PLL1_REG3_CFG		0x058

/*
 * The CKGA_SOURCE_CFG(..) replaces the
 * - CKGA_OSC_DIV0_CFG
 * - CKGA_PLL0HS_DIV0_CFG
 * - CKGA_PLL1HS_DIV0_CFG
 * - CKGA_PLL0LS_DIV0_CFG
 * - CKGA_PLL1LS_DIV0_CFG
 * macros.
 * The _parent_id identifies the parent as:
 * - 0: OSC
 * - 1: PLL0_HS
 * - 2: PLL0_LS
 * - 3: PLL1_HS
 * - 4: PLL1_LS
 */
#define CKGA_SOURCE_CFG(_parent_id)	(0x800 + (_parent_id) * 0x100 +  \
					((_parent_id) == 2 ? 0x80 : 0) - \
					((_parent_id) >= 2 ? 0x100 : 0))

#define CKGA_OSC_DIV0_CFG		0x800
#define CKGA_PLL0HS_DIV0_CFG		0x900
#define CKGA_PLL0LS_DIV0_CFG		0xA08
#define CKGA_PLL1HS_DIV0_CFG		0x980
#define CKGA_PLL1LS_DIV0_CFG		0xAD8

/* ---  QuadFS registers --- */
#define QFS_SETUP			0x000
#define QFS_FSX_CFG(_id)		(0x04 + (0x4 * _id))
#define QFS_ENPRGMSK			0x014
#define QFS_EXCLKCMN			0x018
#define QFS_OPCLKCMN			0x01C
#define QFS_PWR				0x020
#define QFS_OPCLKSTP			0x024
#define QFS_REFCLKSEL			0x028
#define QFS_JE_CTRL0			0x040
#define QFS_GPOUTX(_id)			(0x060 + (0x4 * _id))
#define QFS_GPOUTT			0x074
#define QFS_GPIN			0x078

#endif  /* End __CLOCK_LLA_REGS_D127_H */
