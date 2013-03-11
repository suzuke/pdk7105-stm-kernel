/*
 *  KPTrace - KProbes-based tracing
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2010
 */
#ifndef _MTT_ARCH_SH_H_
#define _MTT_ARCH_SH_H_

#define REG_EIP	pc
#define REG_SP	regs[15]
#define REG_RET	regs[0]
#define REG_ARG0	regs[4]
#define REG_ARG1	regs[5]
#define REG_ARG2	regs[6]
#define REG_ARG3	regs[7]

#endif /* _MTT_ARCH_SH_H_ */
