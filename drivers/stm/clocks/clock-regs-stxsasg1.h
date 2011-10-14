/*****************************************************************************
 *
 * File name   : clock-regs-stxSASG1.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_SASG1_H
#define __CLOCK_LLA_REGS_SASG1_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- Base addresses --------------------------------------- */
#define CKGA0_BASE_ADDRESS	0xFEE62000 // ok
#define CKGA1_BASE_ADDRESS	0xFEE81000 // ok
#define CKGB_BASE_ADDRESS	0xfe81c000 // ok
#define CKGC_BASE_ADDRESS	0xFE830000 // ok
#define CKGD_BASE_ADDRESS	0xFEE10000 // ok

#define SYS_FRONT_BASE_ADDRESS	0xFEE10000 // ok
#define SYS_REAR_BASE_ADDRESS	0xFE830000 // ok
#define SYS_SBC_BASE_ADDRESS	0xFDDF0000 // ok

#define PIO5_BASE_ADDRESS               0xfd025000
#define PIO_BASE_ADDRESS(bank)          PIO5_BASE_ADDRESS

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

/* --- CKGB registers --- */
#define CKGB_LOCK                       0x010
#define CKGB_FS0_CTRL                   0x014
#define CKGB_FS1_CTRL                   0x05c
#define CKGB_FS0_CLKOUT_CTRL            0x058
#define CKGB_FS1_CLKOUT_CTRL            0x0a0
#define CKGB_DISPLAY_CFG                0x0a4
#define CKGB_FS_SELECT                  0x0a8
#define CKGB_POWER_DOWN                 0x0ac
#define CKGB_POWER_ENABLE               0x0b0
#define CKGB_OUT_CTRL                   0x0b4
#define CKGB_CRISTAL_SEL                0x0b8

/*
 * both bank and channel counts from _zero_
 */
#define CKGB_FS_MD(_bk, _chan)		\
	(0x18 + (_chan) * 0x10 + (_bk) * 0x48)
#define CKGB_FS_PE(_bk, _chan)	(0x4 + CKGB_FS_MD(_bk, _chan))
#define CKGB_FS_EN_PRG(_bk, _chan)	(0x4 + CKGB_FS_PE(_bk, _chan))
#define CKGB_FS_SDIV(_bk, _chan)	(0x4 + CKGB_FS_EN_PRG(_bk, _chan))

/* Clock recovery registers */
#define CKGB_RECOV_REF_MAX              0x000
#define CKGB_RECOV_CMD                  0x004
#define CKGB_RECOV_CPT_PCM              0x008
#define CKGB_RECOV_CPT_HD               0x00c

/* --- PIO registers (  ) ------------------------------- */
#define         PIO_CLEAR_PnC0                0x28
#define         PIO_CLEAR_PnC1                0x38
#define         PIO_CLEAR_PnC2                0x48
#define         PIO_PnC0                      0x20
#define         PIO_PnC1                      0x30
#define         PIO_PnC2                      0x40
#define         PIO_SET_PnC0                  0x24
#define         PIO_SET_PnC1                  0x34
#define         PIO_SET_PnC2                  0x44

#ifdef __cplusplus
}
#endif

#endif  /* End __CLOCK_LLA_REGS_SASG1_H */

