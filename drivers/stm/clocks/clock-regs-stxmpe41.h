/*****************************************************************************
 *
 * File name   : clock-regs-stxMPE41.h
 * Description : Low Level API - Base addresses & register definitions.
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License V2 _ONLY_.  See linux/COPYING for more information.
 *
 *****************************************************************************/

#ifndef __CLOCK_LLA_REGS_MPE41_H
#define __CLOCK_LLA_REGS_MPE41_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- Base addresses --------------------------------------- */
#define CKGA0_BASE_ADDRESS	0xfde12000
#define CKGA1_BASE_ADDRESS	0xfd6db000
#define CKGA2_BASE_ADDRESS	0xfd345000
#define CKGB_BASE_ADDRESS	0xfd546000
#define CKGD_BASE_ADDRESS	0xfe700000

#define SYS_MALI_BASE_ADDRESS	0xFD68FF00

#define SYS_VIDEO_BASE_ADDRESS	0xFD320000
#define SYS_TRANSPORT_BASE_ADDRESS	0xFD690000
#define SYS_CPU_BASE_ADDRESS	0xFDDE0000

#define PIO5_BASE_ADDRESS               0xfd025000
#define PIO_BASE_ADDRESS(bank)          PIO5_BASE_ADDRESS

/* --- CKGA registers --- */
#define CKGA_PLL0_REG0_CFG              0x000
#define CKGA_PLL0_REG1_CFG              0x004
#define CKGA_PLL0_REG2_CFG              0x008
#define CKGA_PLL1_REG0_CFG              0x00c
#define CKGA_PLL1_REG1_CFG              0x010
#define CKGA_PLL1_REG2_CFG              0x014
#define CKGA_POWER_CFG	                0x018
#define CKGA_CLKOPSRC_SWITCH_CFG        0x01c
#define CKGA_CLKOPSRC_SWITCH_CFG2       0x020
#define CKGA_PLL0_ENABLE_FB             0x024
#define CKGA_PLL1_ENABLE_FB             0x028
#define CKGA_OSC_ENABLE_FB              0x02c

#define CKGA_CLKOBS_MUX0_CFG            0x030
#define CKGA_CLKOBS_MASTER_MAXCOUNT     0x034
#define CKGA_CLKOBS_CMD                 0x038
#define CKGA_CLKOBS_STATUS              0x03c
#define CKGA_CLKOBS_SLAVE0_COUNT        0x040
#define CKGA_OSCMUX_DEBUG               0x044
#define CKGA_CLKOBS_MUX1_CFG            0x048
#define CKGA_LOW_POWER_CTRL             0x04C

#define CKGA_PLL0_REG3_CFG              0x054
#define CKGA_PLL1_REG3_CFG              0x058

#define CKGA_OSC_DIV0_CFG				0x800
#define CKGA_PLL0_ODF0_DIV0_CFG			0x900
#define CKGA_PLL0_ODF1_DIV0_CFG			0x980
#define CKGA_PLL0_ODF2_DIV0_CFG			0xa20
#define CKGA_PLL0_ODF3_DIV0_CFG			0xa40
#define CKGA_PLL1_ODF0_DIV0_CFG			0xa60
#define CKGA_PLL1_ODF1_DIV0_CFG			0xa80
#define CKGA_PLL1_ODF2_DIV0_CFG			0xb10
#define CKGA_PLL1_ODF3_DIV0_CFG			0xb30

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

/* --- Audio config registers --- */
/* Note:
 * Channel 0 => PCM0 (tvout ss)
 * Channel 1 => PCM1 (analog out)
 * Channel 2 => SPDIF0
 * Channel 3 => PCM2 (digital out)
 */

#define CKGC_FS0_CFG	   0x060
#define CKGC_FS_MD(_bk, _chan)		\
		(0x64 + 0x100 * (_bk) + 0x10 * (_chan))
#define CKGC_FS_PE(_bk, _chan)	(0x4 + CKGC_FS_MD(_bk, _chan))
#define CKGC_FS_SDIV(_bk, _chan)	(0x8 + CKGC_FS_MD(_bk, _chan))
#define CKGC_FS_EN_PRG(_bk, _chan)	(0xc + CKGC_FS_MD(_bk, _chan))

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

#endif  /* End __CLOCK_LLA_REGS_MPE41_H */
