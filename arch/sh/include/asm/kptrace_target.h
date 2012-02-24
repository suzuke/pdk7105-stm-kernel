#ifndef _LINUX_KPTRACE_SH_H
#define _LINUX_KPTRACE_SH_H
/*
 *  KPTrace - KProbes-based tracing
 *  include/linux/kptrace_sh.h
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

#define PC		pc
#define SP		regs[15]
#define RET		regs[0]
#define ARG0		regs[4]
#define ARG1		regs[5]
#define ARG2		regs[6]
#define ARG3		regs[7]

extern struct kp_target_t kp_target_sh ;

#ifdef __DEFINE_KP_TARGET__
static struct kp_target_t *kp_target = &kp_target_sh;
#endif /*__DEFINE_KP_TARGET__*/

#endif /* _LINUX_KPTRACE_SH_H */


