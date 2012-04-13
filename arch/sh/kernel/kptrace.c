/*
 *  KPTrace - Kprobes-based tracing
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
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
 * Copyright (C) STMicroelectronics, 2008, 2012
 *
 * 2007-Jul	Created by Chris Smith <chris.smith@st.com>
 * 2008-Aug     Chris Smith <chris.smith@st.com> added a sysfs interface for
 *              user space tracing.
 */
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/prctl.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/futex.h>
#include <linux/version.h>
#include <trace/kptrace.h>
#include <net/sock.h>
#include <asm/sections.h>

#include <asm/kptrace_target.h>

/*
 * target specific context switch handler
 * */
static int context_switch_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	int prev, next;
	prev = ((struct task_struct *)regs->ARG0)->pid;
	next = ((struct task_struct *)regs->ARG1)->pid;

	snprintf(tbuf, KPTRACE_SMALL_BUF, "C %d %d", prev, next);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/*
 * target specific core events
 * */
void init_core_event_logging(struct kp_tracepoint_set *set)
{
	kptrace_create_tracepoint(set, "__switch_to",
			context_switch_pre_handler, NULL);
}

 /*
  * target specific syscalls
  * */
void init_syscall_logging(struct kp_tracepoint_set *set)
{
	CALL_CUSTOM_PRE(sys_waitpid, syscall_ihhh_pre_handler)
	CALL(sys_alarm)
	CALL(sys_oldumount)
	CALL(sys_stime)
	CALL(sys_fstat)
	CALL(sys_utime)
	CALL(sys_signal)
	CALL(sys_time)
	CALL(sys_stat)
	CALL(sys_sgetmask)
	CALL(sys_ssetmask)
	CALL(sys_old_getrlimit)
	CALL(sys_lstat)
	CALL(sys_old_readdir)
	CALL(old_mmap)
	CALL(sys_socketcall)
	CALL(sys_uname)
	CALL(sys_ipc)
	CALL(sys_cacheflush)
	CALL(sys_mremap)
	CALL(sys_pread_wrapper)
	CALL(sys_pwrite_wrapper)
	CALL(sys_sigaltstack)
	CALL(sys_truncate64)
	CALL(sys_ftruncate64)
	CALL(sys_stat64)
	CALL(sys_lstat64)
	CALL(sys_fstat64)
	CALL(sys_fcntl64)
	CALL(sys_readahead)
	CALL(sys_fadvise64)
	CALL(sys_epoll_ctl)
	CALL(sys_epoll_wait)
	CALL(sys_fadvise64_64_wrapper)
	CALL(sys_migrate_pages)
	CALL(sys_fstatat64)
	CALL(sys_sync_file_range)
}

/**/
struct kp_target_t kp_target_sh = {
	.init_core_event_logging = init_core_event_logging,
	.init_syscall_logging = init_syscall_logging,
	.init_memory_logging = NULL
};

