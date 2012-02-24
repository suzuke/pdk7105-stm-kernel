#ifndef _TRACE_KPTRACE_H
#define _TRACE_KPTRACE_H
/*
 *  KPTrace - KProbes-based tracing
 *  include/trace/kptrace.h
 *
 * GENERIC KPTRACE HEADER FILE
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
 * Copyright (C) STMicroelectronics, 2007, 2012
 *
 * 2007-July    Created by Chris Smith <chris.smith@st.com>
 * 2008-August  kpprintf added by Chris Smith <chris.smith@st.com>
 */

#ifdef CONFIG_KPTRACE

#define KPTRACE_BUF_SIZE 1024
#define KPTRACE_SMALL_BUF 128

#include <linux/kprobes.h>

/* from trap.c, put here to avoid complaints from checkpatch */
extern int get_stack(char *buf, unsigned long *sp, size_t size, size_t depth);


/* Mark a particular point in the code as "interesting" in the kptrace log */
void kptrace_mark(void);

/* Write a string to the kptrace log */
void kptrace_write_record(const char *buf);

/* Stop logging trace records until kptrace_restart() is called */
void kptrace_pause(void);

/* Restart logging of trace records after a kptrace_pause() */
void kptrace_restart(void);

/* Allow printf-style records to be added. Note that kptrace_write_record
 * is a lighter alternative when no formatting is required. */
void kpprintf(char *fmt, ...);


struct kp_tracepoint {
	struct kprobe kp;
	struct kretprobe rp;
	struct kobject kobj;
	int enabled;
	int callstack;
	int inserted;
	int stopon;
	int starton;
	int user_tracepoint;
	int late_tracepoint;
	const struct file_operations *ops;
	struct list_head list;
};

struct kp_tracepoint_set {
	int enabled;
	struct kobject kobj;
	const struct file_operations *ops;
	struct list_head list;
};

struct kp_tracepoint *kptrace_create_tracepoint(
	struct kp_tracepoint_set *set,
	const char *name,
	int (*entry_handler)(struct kprobe *, struct pt_regs *),
	int (*return_handler)(struct kretprobe_instance *, struct pt_regs *)
	);

void kptrace_write_trace_record(
		struct kprobe *kp,
		struct pt_regs *regs,
		const char *rec);

void kptrace_write_trace_record_no_callstack(const char *rec);

/*
 * KPTrace can write the trace data out through any driver that provides
 * a function with the prototype write_func below.
 *
 * The default implementation is relay, which provides efficient output
 * to a file.
 */
struct kp_output_driver {
	char *name;
	void (*write_func)(struct rchan *chan, const void *str, size_t len);
	struct list_head list;
};

/* Register/unregister alternative drivers here. The driver to use is
 * selected via sysfs */
void kptrace_register_output_driver(struct kp_output_driver *driver);
void kptrace_unregister_output_driver(struct kp_output_driver *driver);


/* Tracepoint handlers available to arch-specific tracepoints */
int irq_pre_handler(struct kprobe *p, struct pt_regs *regs);
int irq_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int syscall_ihhh_pre_handler(struct kprobe *p, struct pt_regs *regs);
int syscall_iihh_pre_handler(struct kprobe *p, struct pt_regs *regs);
int syscall_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);
int syscall_pre_handler(struct kprobe *p, struct pt_regs *regs);

int alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs);
int alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);


/* Per-architecture hooks. */
struct kp_target_t {
	void (*init_syscall_logging)(struct kp_tracepoint_set *);
	void (*init_core_event_logging)(struct kp_tracepoint_set *);
	void (*init_memory_logging)(struct kp_tracepoint_set *);
};

#ifdef KPTRACE_DEBUG
#define kptrace_printk(format, args...) printk(format, ##args)
#else
#define kptrace_printk(format, args...) /* nothing */
#endif


/*
 * Macro set to install syscalls for various platforms.
 * */

#define CALL(x) kptrace_create_tracepoint(set, #x, syscall_pre_handler, \
				  syscall_rp_handler);
#define CALL_ABI(native, compat) kptrace_create_tracepoint(set, #native, \
						   syscall_pre_handler, \
						   syscall_rp_handler);
#define CALL_CUSTOM_PRE(x, eh) kptrace_create_tracepoint(set, #x, eh, \
						syscall_rp_handler);
#define CALL_CUSTOM_PRE_ABI(native, compat, eh) \
		kptrace_create_tracepoint(set, #native, eh, syscall_rp_handler);

#endif /* CONFIG_KPTRACE */
#endif /* _TRACE_KPTRACE_H */
