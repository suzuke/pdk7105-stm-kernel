#ifndef _LINUX_KPTRACE_ARM_H
#define _LINUX_KPTRACE_ARM_H
/*
 *  KPTrace - KProbes-based tracing
 *  include/linux/kptrace_arm.h
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

#define PC		ARM_pc
#define SP		ARM_sp
#define RET		uregs[0]
#define ARG0		uregs[0]
#define ARG1		uregs[1]
#define ARG2		uregs[2]
#define ARG3		uregs[3]

extern struct kp_target_t kp_target_arm ;

#ifdef __DEFINE_KP_TARGET__
static struct kp_target_t *kp_target = &kp_target_arm;
#endif /*__DEFINE_KP_TARGET__*/

#endif /* _LINUX_KPTRACE_ARM_H */
