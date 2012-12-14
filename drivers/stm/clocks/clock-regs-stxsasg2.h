/*****************************************************************************
 *
 * File name   : clock-regs-stxsasg2.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_SASG2_H
#define __CLOCK_LLA_REGS_SASG2_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- Base addresses --------------------------------------- */
#define CKGA0_BASE_ADDRESS	0xFEE62000
#define CKGA1_BASE_ADDRESS	0xFEE81000
#define CKGB_BASE_ADDRESS	0xfe81c000
#define CKGC_BASE_ADDRESS	0xFE830000
#define CKGD_BASE_ADDRESS	0xFEE10000

#define SYS_0_BASE_ADDRESS	0xFE600000 /* SBC sysconf */
#define SYS_1000_BASE_ADDRESS	0xFEE10000
#define SYS_2000_BASE_ADDRESS	0xFE830000

/* --- CKGA registers --- */
#define CKGA_PLL0_CFG                   0x000
#define CKGA_PLL1_CFG                   0x004
#define CKGA_POWER_CFG	                0x010
#define CKGA_CLKOPSRC_SWITCH_CFG        0x014
#define CKGA_OSC_ENABLE_FB              0x018
#define CKGA_PLL0_ENABLE_FB             0x01c
#define CKGA_PLL1_ENABLE_FB             0x020
#define CKGA_CLKOPSRC_SWITCH_CFG2       0x024

#define CKGA_CLKOBS_MUX1_CFG            0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT     0x034
#define CKGA_CLKOBS_CMD                 0x038
#define CKGA_CLKOBS_STATUS              0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT        0x040
#define CKGA_OSCMUX_DEBUG               0x044
#define CKGA_CLKOBS_MUX2_CFG            0x048
#define CKGA_LOW_POWER_CTRL             0x04C

/*
 * The CKGA_SOURCE_CFG(..) replaces the
 * - CKGA_OSC_DIV0_CFG
 * - CKGA_PLL0HS_DIV0_CFG
 * - CKGA_PLL0LS_DIV0_CFG
 * - CKGA_PLL1_DIV0_CFG
 * macros.
 * The _parent_id identifies the parent as:
 * - 0: OSC
 * - 1: PLL0_HS
 * - 2: PLL0_LS
 * - 3: PLL1
 */
#define CKGA_SOURCE_CFG(_parent_id)	(0x800 + (_parent_id) * 0x100)

/* Clock recovery registers */
#define CKGB_RECOV_REF_MAX              0x000
#define CKGB_RECOV_CMD                  0x004
#define CKGB_RECOV_CPT_PCM              0x008
#define CKGB_RECOV_CPT_HD               0x00c

#ifdef __cplusplus
}
#endif

#endif  /* End __CLOCK_LLA_REGS_SASG2_H */

