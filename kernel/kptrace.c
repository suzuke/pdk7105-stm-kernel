/*
 * KPTrace - Kprobes-based tracing
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
#include <linux/kprobes.h>  /* CONFIG_MODULES required */
#include <linux/kallsyms.h> /* CONFIG_KALLSYMS required */
#include <linux/prctl.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/futex.h>
#include <linux/version.h>
#include <trace/kptrace.h>
#include <net/sock.h>
#include <asm/sections.h>

#define __DEFINE_KP_TARGET__
#include <asm/kptrace_target.h>
#undef __DEFINE_KP_TARGET__

enum kptrace_tracepoint_states {
	TP_UNUSED,
	TP_USED,
	TP_INUSE
};

static LIST_HEAD(tracepoint_sets);
static LIST_HEAD(tracepoints);
static LIST_HEAD(output_drivers);

/* exported buffer for kpprintf */
static char kpprintf_buf[KPTRACE_BUF_SIZE];
static char kpprintf_buf_irq[KPTRACE_BUF_SIZE];
static struct mutex kpprintf_mutex = __MUTEX_INITIALIZER(kpprintf_mutex);
static DEFINE_SPINLOCK(kpprintf_lock);

static DEFINE_PER_CPU(char[KPTRACE_BUF_SIZE], stack_buf);
static DEFINE_PER_CPU(char[KPTRACE_BUF_SIZE], cpu_buf);
static char user_new_symbol[KPTRACE_BUF_SIZE];
static struct kp_tracepoint_set *user_set;
static struct kobject userspace;
static int user_stopon;
static int user_starton;
static int timestamping_enabled = 1;
static int stackdepth = 16;

/* This value, exposed via sysfs, allows userspace to
 * know what interface to expect */
#define KPTRACE_VERSION(major, minor, patch) ((major<<16)|(minor<<8)|(patch<<0))
static const int interface_version = KPTRACE_VERSION(3, 0, 0);

/* relay data */
static struct rchan *chan;
static struct dentry *dir;
static int logging;
static int mappings;
static int suspended;
static size_t dropped;
static size_t subbuf_size = 262144;
static size_t n_subbufs = 4;
static size_t overwrite_subbufs;
#define KPTRACE_MAXSUBBUFSIZE 16777216
#define KPTRACE_MAXSUBBUFS 256
#define MAX_BUFFER_FULL_WARNINGS 10
static int buffer_full_warning_ratelimit = MAX_BUFFER_FULL_WARNINGS;

/* channel-management control files */
static struct dentry *enabled_control;
static struct dentry *create_control;
static struct dentry *subbuf_size_control;
static struct dentry *n_subbufs_control;
static struct dentry *dropped_control;
static struct dentry *overwrite_control;

/* produced/consumed control files */
static struct dentry *produced_control[NR_CPUS];
static struct dentry *consumed_control[NR_CPUS];

/* control file fileop declarations */
static const struct file_operations enabled_fops;
static const struct file_operations create_fops;
static const struct file_operations subbuf_size_fops;
static const struct file_operations n_subbufs_fops;
static const struct file_operations dropped_fops;
static const struct file_operations overwrite_fops;
static const struct file_operations produced_fops;
static const struct file_operations consumed_fops;

/* which driver to write the trace out through */
static struct kp_output_driver *current_output_driver;

/* forward declarations */
static int create_controls(void);
static void destroy_channel(void);
static void remove_controls(void);
static void start_tracing(void);
static void stop_tracing(void);
static int user_pre_handler(struct kprobe *p, struct pt_regs *regs);
static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs);

/* protection for the formatting temporary buffer */
static DEFINE_SPINLOCK(tmpbuf_lock);

static struct attribute *tracepoint_attribs[] = {
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	&(struct attribute){
			    .name = "callstack",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	NULL
};

static struct attribute *tracepoint_set_attribs[] = {
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	NULL
};

static struct attribute *user_tp_attribs[] = {
	&(struct attribute){
			    .name = "new_symbol",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	&(struct attribute){
			    .name = "add",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	&(struct attribute){
			    .name = "enabled",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	&(struct attribute){
			    .name = "stopon",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	&(struct attribute){
			    .name = "starton",
			    .mode = S_IRUGO|S_IWUSR,
			    },
	NULL
};

static struct attribute *userspace_attribs[] = {
		&(struct attribute){
		.name = "new_record",
		.mode = S_IRUGO|S_IWUSR,
		},
		NULL
};

static ssize_t
tracepoint_set_show_attrs(struct kobject *kobj,
			  struct attribute *attr, char *buffer)
{
	struct kp_tracepoint_set *set = container_of(kobj,
						    struct kp_tracepoint_set,
						    kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (set->enabled) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint set \"%s\" is disabled\n",
				 kobj->name);
		}
	}
	return strlen(buffer);
}

static ssize_t
tracepoint_set_store_attrs(struct kobject *kobj,
			   struct attribute *attr, const char *buffer,
			   size_t size)
{
	struct kp_tracepoint_set *set = container_of(kobj,
						    struct kp_tracepoint_set,
						    kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			set->enabled = 1;
		else
			set->enabled = 0;
	}
	return size;
}

static ssize_t
tracepoint_show_attrs(struct kobject *kobj,
		      struct attribute *attr, char *buffer)
{
	struct kp_tracepoint *tp = container_of(kobj,
						struct kp_tracepoint, kobj);
	if (strcmp(attr->name, "enabled") == 0) {
		if (tp->enabled == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is enabled\n", kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Tracepoint on %s is disabled\n", kobj->name);
		}
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (tp->callstack == 1) {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is enabled\n",
				 kobj->name);
		} else {
			snprintf(buffer, PAGE_SIZE,
				 "Callstack gathering on %s is disabled\n",
				 kobj->name);
		}
	}

	return strlen(buffer);
}

static ssize_t
tracepoint_store_attrs(struct kobject *kobj,
		       struct attribute *attr, const char *buffer, size_t size)
{
	struct kp_tracepoint *tp = container_of(kobj,
						struct kp_tracepoint, kobj);

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			tp->enabled = 1;
		else
			tp->enabled = 0;
	}

	if (strcmp(attr->name, "callstack") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			tp->callstack = 1;
		else
			tp->callstack = 0;
	}

	return size;
}

/* call stack depth parameter */
static ssize_t
kptrace_stackdepth_show_attrs(struct device *device,
			      struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "Callstack depth is %d\n",
			stackdepth);
}

static ssize_t
kptrace_stackdepth_store_attrs(struct device *device,
			       struct device_attribute *attr,
			       const char *buffer, size_t size)
{
	unsigned long tmp;

	if (kstrtoul(buffer, 10, &tmp) == 0)
		stackdepth = tmp;

	return size;
}

static ssize_t
kptrace_configured_show_attrs(struct device *device,
			      struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "Used to start/stop tracing\n");
}

static ssize_t
kptrace_configured_store_attrs(struct device *device,
			       struct device_attribute *attr,
			       const char *buffer, size_t size)
{
	if (*buffer == '1')
		start_tracing();
	else
		stop_tracing();
	return size;
}

static ssize_t
kptrace_version_show_attrs(struct device *device,
			   struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE, "%d\n", interface_version);
}

static ssize_t
kptrace_version_store_attrs(struct device *device,
			    struct device_attribute *attr,
			    const char *buffer, size_t size)
{
	/* Nothing happens */
	return size;
}

static ssize_t
kptrace_pause_show_attrs(struct device *device,
			 struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE,
			"Write to this file to pause tracing\n");
}

static ssize_t
kptrace_pause_store_attrs(struct device *device,
			  struct device_attribute *attr, const char *buffer,
			  size_t size)
{
	kptrace_pause();
	return size;
}

static ssize_t
kptrace_restart_show_attrs(struct device *device,
			   struct device_attribute *attr, char *buffer)
{
	return snprintf(buffer, PAGE_SIZE,
			"Write to this file to restart tracing\n");
}

static ssize_t
kptrace_restart_store_attrs(struct device *device,
			    struct device_attribute *attr,
			    const char *buffer, size_t size)
{
	kptrace_restart();
	return size;
}

static ssize_t
user_show_attrs(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	if (strcmp(attr->name, "new_symbol") == 0) {
		return snprintf(buffer, PAGE_SIZE, "new_symbol = %s\n",
				user_new_symbol);
	}

	if (strcmp(attr->name, "add") == 0) {
		return snprintf(buffer, PAGE_SIZE, "Adding new tracepoint %s\n",
				user_new_symbol);
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (user_set->enabled) {
			return snprintf(buffer, PAGE_SIZE,
			"User-defined tracepoints are enabled");
		} else {
			return snprintf(buffer, PAGE_SIZE,
			"User-defined tracepoints are disabled");
		}
	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
			"Stop logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
			"Stop logging on this tracepoint: off");
		}
	}

	if (strcmp(attr->name, "starton") == 0) {
		if (user_stopon) {
			return snprintf(buffer, PAGE_SIZE,
			"Start logging on this tracepoint: on");
		} else {
			return snprintf(buffer, PAGE_SIZE,
			"Start logging on this tracepoint: off");
		}
	}

	return snprintf(buffer, PAGE_SIZE, "Unknown attribute\n");
}

ssize_t
user_store_attrs(struct kobject *kobj, struct attribute *attr,
		 const char *buffer, size_t size)
{
	struct list_head *p;
	struct kp_tracepoint *tp, *new_tp = NULL;

	if (strcmp(attr->name, "new_symbol") == 0)
		strncpy(user_new_symbol, buffer, KPTRACE_BUF_SIZE);

	if (strcmp(attr->name, "add") == 0) {
		/* Check it doesn't already exist, to avoid duplicates */
		list_for_each(p, &tracepoints) {
			tp = list_entry(p, struct kp_tracepoint, list);
			if (tp != NULL && tp->user_tracepoint == 1) {
				if (strncmp
				    (kobject_name(&tp->kobj), user_new_symbol,
				     KPTRACE_BUF_SIZE) == 0)
					return size;
			}
		}

		new_tp = kptrace_create_tracepoint(user_set, user_new_symbol,
						   &user_pre_handler,
						   &user_rp_handler);

		if (!new_tp) {
			printk(KERN_ERR "kptrace: Cannot create tracepoint\n");
			return -ENOSYS;
		} else {
			new_tp->stopon = user_stopon;
			new_tp->starton = user_starton;
			new_tp->user_tracepoint = 1;
			return size;
		}
	}

	if (strcmp(attr->name, "enabled") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_set->enabled = 1;
		else
			user_set->enabled = 0;

	}

	if (strcmp(attr->name, "stopon") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_stopon = 1;
		else
			user_stopon = 0;

	}

	if (strcmp(attr->name, "starton") == 0) {
		if (strncmp(buffer, "1", 1) == 0)
			user_starton = 1;
		else
			user_starton = 0;

	}

	return size;
}

static ssize_t
userspace_show_attrs(struct kobject *kobj, struct attribute *attr, char *buffer)
{
	if (strcmp(attr->name, "new_record") == 0)
		return snprintf(buffer, PAGE_SIZE,
				"Used to add records from user space\n");

	return snprintf(buffer, PAGE_SIZE, "Unknown attribute\n");
}

static ssize_t
userspace_store_attrs(struct kobject *kobj,
		      struct attribute *attr, const char *buffer, size_t size)
{
	if (strcmp(attr->name, "new_record") == 0)
		kptrace_write_trace_record_no_callstack(buffer);

	return size;
}

/* 'dfs' (short for debugfs) isn't a great name, but it's what the userspace
 * tools currently expect, so avoid breaking compatibility. */
static struct kp_output_driver relay_output_driver = {
	.name = "dfs", .write_func = relay_write
};

void kptrace_register_output_driver(struct kp_output_driver *driver)
{
	printk(KERN_INFO "kptrace: registering output driver %s\n",
			driver->name);
	list_add(&driver->list, &output_drivers);
}
EXPORT_SYMBOL(kptrace_register_output_driver);

void kptrace_unregister_output_driver(struct kp_output_driver *driver)
{
	struct list_head *p, *tmp;
	struct kp_output_driver *tmp_driver;

	list_for_each_safe(p, tmp, &output_drivers) {
		tmp_driver = list_entry(p, struct kp_output_driver, list);

		if (tmp_driver == driver) {
			printk(KERN_INFO "kptrace: unregistering output driver %s\n",
					driver->name);
			if (tmp_driver == current_output_driver) {
				if (tmp_driver != &relay_output_driver) {
					printk(KERN_INFO "kptrace: unregistering "
							"selected driver, falling back "
							"on relay\n");
					current_output_driver =
							&relay_output_driver;
				} else
					current_output_driver = NULL;
			}
			list_del(p);
		}
	}
}
EXPORT_SYMBOL(kptrace_unregister_output_driver);


/*
 * Display the currently selected output driver
 */
static ssize_t output_driver_show_attrs(struct device *device,
			struct device_attribute *attr, char *buffer)
{
	int ret = 0;

	if (current_output_driver)
		ret = snprintf(buffer, PAGE_SIZE, "%s\n",
				current_output_driver->name);
	else
		ret = snprintf(buffer, PAGE_SIZE, "unknown\n");

	return ret;
}


/*
 * Select the output driver according to the name passed in
 */
static ssize_t output_driver_store_attrs(struct device *device,
			struct device_attribute *attr,
			const char *buffer, size_t size)
{
	struct list_head *p;
	struct kp_output_driver *driver;
	int success = 0;

	list_for_each(p, &output_drivers) {
		driver = list_entry(p, struct kp_output_driver, list);
		if (strncmp(driver->name, buffer, strlen(driver->name)) == 0) {
			current_output_driver = driver;
			success = 1;
		}
	}

	if (!success) {
		printk(KERN_INFO
				"kptrace: Unrecognized kptrace output driver:"
				" %skptrace: Falling back on relay output "
				"driver\n", buffer);
		current_output_driver = &relay_output_driver;
	}

	return size;
}

/* Main control is a subsys */
struct bus_type kptrace_subsys = {
	.name = "kptrace",
	.dev_name = "kptrace",
};

DEVICE_ATTR(configured, S_IRUGO|S_IWUSR, kptrace_configured_show_attrs,
		kptrace_configured_store_attrs);
DEVICE_ATTR(stackdepth, S_IRUGO|S_IWUSR, kptrace_stackdepth_show_attrs,
		kptrace_stackdepth_store_attrs);
DEVICE_ATTR(version, S_IRUGO|S_IWUSR, kptrace_version_show_attrs,
		kptrace_version_store_attrs);
DEVICE_ATTR(pause, S_IRUGO|S_IWUSR, kptrace_pause_show_attrs,
		kptrace_pause_store_attrs);
DEVICE_ATTR(restart, S_IRUGO|S_IWUSR, kptrace_restart_show_attrs,
		kptrace_restart_store_attrs);
DEVICE_ATTR(output_driver, S_IRUGO|S_IWUSR, output_driver_show_attrs,
		output_driver_store_attrs);

static struct device kptrace_device = {
	.id = 0,
	.bus = &kptrace_subsys,
};

/* Operations for the three kobj types */
static const struct sysfs_ops tracepoint_sysfs_ops = {
	&tracepoint_show_attrs, &tracepoint_store_attrs
};

static const struct sysfs_ops tracepoint_set_sysfs_ops = {
	&tracepoint_set_show_attrs, &tracepoint_set_store_attrs
};
static const struct sysfs_ops user_sysfs_ops = {
	&user_show_attrs, &user_store_attrs };

static const struct sysfs_ops userspace_sysfs_ops = { &userspace_show_attrs,
	&userspace_store_attrs
};

/* Three kobj types: tracepoints, tracepoint sets,
 * the special "user" tracepoint set
 **/
struct kobj_type kp_tracepointype = { NULL, &tracepoint_sysfs_ops,
	tracepoint_attribs
};

struct kobj_type kp_tracepoint_setype = { NULL, &tracepoint_set_sysfs_ops,
	tracepoint_set_attribs
};
struct kobj_type user_type = { NULL, &user_sysfs_ops, user_tp_attribs };

struct kobj_type userspace_type = { NULL, &userspace_sysfs_ops,
	userspace_attribs
};

static struct kp_tracepoint *__create_tracepoint(struct kp_tracepoint_set *set,
		const char *name,
		int (*entry_handler) (struct kprobe *, struct pt_regs *),
		int (*return_handler) (struct kretprobe_instance *,
		struct pt_regs *),
		int late_tracepoint,
		const char *alias)
{
	struct kp_tracepoint *tp;
	tp = kzalloc(sizeof(*tp), GFP_KERNEL);
	if (!tp) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for tracepoint %s\n",
		       name);
		return NULL;
	}

	/* The 'alias' is the tracepoint name exposed via sysfs. By default, it
	 * is the symbol name */
	if (!alias)
		alias = name;

	tp->enabled = 0;
	tp->callstack = 0;
	tp->stopon = 0;
	tp->starton = 0;
	tp->user_tracepoint = 0;
	tp->late_tracepoint = late_tracepoint;
	tp->inserted = TP_UNUSED;

	if (entry_handler != NULL) {
		tp->kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name(name);

		if (tp->late_tracepoint == 1)
			tp->kp.flags |= KPROBE_FLAG_DISABLED;

		if (!tp->kp.addr) {
			printk(KERN_WARNING "kptrace: Symbol %s not found\n",
			       name);
			kfree(tp);
			return NULL;
		}
		tp->kp.pre_handler = entry_handler;
	}

	if (return_handler != NULL) {
		if (entry_handler != NULL)
			tp->rp.kp.addr = tp->kp.addr;
		else
			tp->rp.kp.addr =
			    (kprobe_opcode_t *) kallsyms_lookup_name(name);

		tp->rp.handler = return_handler;
		tp->rp.maxactive = 128;
	}

	list_add(&tp->list, &tracepoints);

	if (kobject_init_and_add(&tp->kobj, &kp_tracepointype, &set->kobj,
			alias) < 0) {
		printk(KERN_WARNING "kptrace: Failed add to add kobject %s\n",
		       name);
		return NULL;
	}

	return tp;
}

/*
 * Creates a tracepoint in the given set. Pointers to entry and/or return
 * handlers can be NULL if it is not necessary to track those events.
 *
 * This function only initializes the data structures and adds the sysfs node.
 * To actually add the kprobes and start tracing, use insert_tracepoint().
 */
struct kp_tracepoint *kptrace_create_tracepoint(
		struct kp_tracepoint_set *set,
		const char *name,
		int (*entry_handler) (struct kprobe *, struct pt_regs *),
		int (*return_handler) (struct kretprobe_instance *,
		struct pt_regs *))
{
	return __create_tracepoint(set, name, entry_handler,
					return_handler, 0, NULL);
}

/*
 * As kptrace_create_tracepoint(), except that is exposed in sysfs with the
 * name "alias", rather than its symbol name. This is useful to allow a common
 * sysfs entry between kernel versions where the actual symbols have changed,
 * for example kthread_create() changed from a function to a macro.
 */
struct kp_tracepoint *kptrace_create_aliased_tracepoint(
		struct kp_tracepoint_set *set,
		const char *name,
		int (*entry_handler) (struct kprobe *, struct pt_regs *),
		int (*return_handler) (struct kretprobe_instance *,
		struct pt_regs *), const char *alias)
{
	return __create_tracepoint(set, name, entry_handler,
					return_handler, 0, alias);
}

/*
 * As kptrace_create_tracepoint(), except that the tracepoint is not armed
 * until all tracepoints have been added. This is useful when tracing a
 * function used in the kprobe code, such as mutex_lock().
 */
#ifdef CONFIG_KPTRACE_SYNC
static struct kp_tracepoint*
create_late_tracepoint(struct kp_tracepoint_set *set,
	const char *name,
	int (*entry_handler) (struct kprobe *, struct pt_regs *),
	int (*return_handler) (struct kretprobe_instance *, struct pt_regs *))
{
	return __create_tracepoint(set, name, entry_handler,
					return_handler, 1, NULL);
}
#endif

/*
 * Registers the kprobes for the tracepoint, so that it will start to
 * be logged.
 *
 * kretprobes are only registered the first time. After that, we only
 * register and unregister the initial kprobe. This prevents race
 * conditions where a function is halfway through execution when the
 * probe is removed.
 */
static void insert_tracepoint(struct kp_tracepoint *tp)
{
	if (tp->inserted != TP_INUSE) {
		if (tp->kp.addr != NULL)
			register_kprobe(&tp->kp);

		if (tp->rp.kp.addr != NULL) {
			if (tp->inserted == TP_UNUSED)
				register_kretprobe(&tp->rp);
			else if (tp->inserted == TP_USED)
				register_kprobe(&tp->rp.kp);

		}

		tp->inserted = TP_INUSE;
	}
}

/* Insert all enabled tracepoints in this set */
static void insert_tracepoints_in_set(struct kp_tracepoint_set *set)
{
	struct list_head *p;
	struct kp_tracepoint *tp;

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp->kobj.parent) {
			if ((strcmp(tp->kobj.parent->name, set->kobj.name) == 0)
			    && (tp->enabled == 1))
				insert_tracepoint(tp);
		}
	}
}

/*
 * Unregister the kprobes for the tracepoint. From kretprobes,
 * only unregister the initial kprobe to prevent race condition
 * when function is halfway through execution when the probe is
 * removed.
 *
 * Note that unregister_kprobe set the "disabled" flag, which would
 * prevent the probe being re-inserted. We remove that.
 */
int unregister_tracepoint(struct kp_tracepoint *tp)
{
	if (tp->kp.addr != NULL) {
		if (tp->late_tracepoint)
			arch_disarm_kprobe(&tp->kp);
		unregister_kprobe(&tp->kp);
		tp->kp.flags &= ~KPROBE_FLAG_DISABLED;
	}

	if (tp->rp.kp.addr != NULL) {
		unregister_kprobe(&tp->rp.kp);
		tp->rp.kp.flags &= ~KPROBE_FLAG_DISABLED;
	}

	tp->inserted = TP_USED;

	return 0;
}

/*
 * Allocates the data structures for a new tracepoint set and
 * creates a sysfs node for it.
 */
static struct kp_tracepoint_set *create_tracepoint_set(const char *name)
{
	struct kp_tracepoint_set *set;
	set = kzalloc(sizeof(*set), GFP_KERNEL);
	if (!set)
		return NULL;

	list_add(&set->list, &tracepoint_sets);
	if (kobject_init_and_add(&set->kobj, &kp_tracepoint_setype,
				 &kptrace_subsys.dev_root->kobj, name) < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject %s\n",
		       name);
	set->enabled = 0;

	return set;
}

/* Inserts all the tracepoints in each enabled set */
static void start_tracing(void)
{
	struct list_head *p, *tmp;
	struct kp_tracepoint_set *set;
	struct kp_tracepoint *tp;

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, struct kp_tracepoint_set, list);
		if (set->enabled)
			insert_tracepoints_in_set(set);

	}

	/* Arm any "late" tracepoints */
	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp->late_tracepoint && tp->enabled) {
			if (kprobe_disabled(&tp->kp))
				arch_arm_kprobe(&tp->kp);
		}
	}

	buffer_full_warning_ratelimit = MAX_BUFFER_FULL_WARNINGS;

	logging = 1;
}

/* Remove all tracepoints */
static void stop_tracing(void)
{
	struct list_head *p, *tmp;
	struct kp_tracepoint *tp;

	list_for_each_safe(p, tmp, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);

		if (tp->inserted == TP_INUSE)
			unregister_tracepoint(tp);

		if (tp->user_tracepoint == 1) {
			kobject_put(&tp->kobj);
			tp->kp.addr = NULL;
			list_del(p);
		} else {
			tp->enabled = 0;
		}
	}
}

/*
 * Write a trace record to the relay buffer.
 *
 * Prepends a timestamp and the current PID, and puts a callstack
 * on the end where requested.
 */
void
kptrace_write_trace_record(struct kprobe *kp, struct pt_regs *regs,
			   const char *rec)
{
	unsigned tlen;
	struct timeval tv;
	unsigned long flags;
	struct kp_tracepoint *tp = NULL;
	char *cbuf, *sbuf;
	int current_pid;

	/* get the current pid in a reliable way */

	current_pid = current_thread_info()->task->pid;

	spin_lock_irqsave(&tmpbuf_lock, flags);

	if (timestamping_enabled) {
		do_gettimeofday(&tv);
	} else {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
	}

	tp = container_of(kp, struct kp_tracepoint, kp);

	if (kp && tp->starton == 1)
		logging = 1;

	if (!logging) {
		spin_unlock_irqrestore(&tmpbuf_lock, flags);
		return;
	}

	sbuf = __get_cpu_var(stack_buf);
	cbuf = __get_cpu_var(cpu_buf);

	if ((kp && tp->callstack == 1)
		&& get_stack(sbuf, (unsigned long *)regs->SP,
				KPTRACE_BUF_SIZE, stackdepth)
		)
		tlen = snprintf(cbuf, KPTRACE_BUF_SIZE, "%lu.%06lu %d %s\n%s\n",
				tv.tv_sec, tv.tv_usec, current_pid, rec,
				sbuf);
	else
		tlen = snprintf(cbuf, KPTRACE_BUF_SIZE, "%lu.%06lu %d %s\n",
				tv.tv_sec, tv.tv_usec, current_pid, rec);

	if (current_output_driver)
		current_output_driver->write_func(chan, cbuf, tlen);

	spin_unlock_irqrestore(&tmpbuf_lock, flags);

	if (kp && tp->stopon == 1)
		logging = 0;
}

/*
 * Write a trace record to the relay buffer.
 *
 * Because the current kprobe and regs are not provided,
 * no callstack can be added.
 */
void kptrace_write_trace_record_no_callstack(const char *rec)
{
	kptrace_write_trace_record(NULL, NULL, rec);
}

/*
 * Primary interface for user to new static tracepoints.
 */
void kptrace_write_record(const char *rec)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "K %s", rec);
	kptrace_write_trace_record_no_callstack(tbuf);
}
EXPORT_SYMBOL(kptrace_write_record);

/*
 * Provides a printf-style interface on the trace buffer.
 */
__printf(1, 2)
void kpprintf(char *fmt, ...)
{
	unsigned long flags;
	va_list ap;
	va_start(ap, fmt);

	/* Only spin in interrupt context, to reduce intrusion */
	if (in_interrupt()) {
		spin_lock_irqsave(&kpprintf_lock, flags);
		vsnprintf(kpprintf_buf_irq, KPTRACE_BUF_SIZE, fmt, ap);
		kptrace_write_record(kpprintf_buf_irq);
		spin_unlock_irqrestore(&kpprintf_lock, flags);
	} else {
		mutex_lock(&kpprintf_mutex);
		vsnprintf(kpprintf_buf, KPTRACE_BUF_SIZE, fmt, ap);
		kptrace_write_record(kpprintf_buf);
		mutex_unlock(&kpprintf_mutex);
	}
}
EXPORT_SYMBOL(kpprintf);

/*
 * Indicates that this is an interesting point in the code.
 * Intended to be highlighted prominantly in a GUI.
 */
void kptrace_mark(void)
{
	kptrace_write_trace_record_no_callstack("KM");
}
EXPORT_SYMBOL(kptrace_mark);

/*
 * Stops the logging of trace records until kptrace_restart()
 * is called.
 */
void kptrace_pause(void)
{
	kptrace_write_trace_record_no_callstack("KP");
	logging = 0;
}
EXPORT_SYMBOL(kptrace_pause);

/*
 * Restarts logging of trace after a kptrace_pause()
 */
void kptrace_restart(void)
{
	logging = 1;
	kptrace_write_trace_record_no_callstack("KR");
}
EXPORT_SYMBOL(kptrace_restart);

static int user_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF,
		 "U %.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x",
		 (int)regs->PC, (int)regs->ARG0, (int)regs->ARG1,
		 (int)regs->ARG2, (int)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int user_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	u32 probe_func_addr = (u32) ri->rp->kp.addr;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "u %d %.8x", (int)regs->RET,
		 probe_func_addr);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

int syscall_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "X %.8x %d",
		 (unsigned int)ri->rp->kp.addr, (int)regs->RET);

	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

int softirq_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "S %.8x",
		 (unsigned int)regs->PC);

	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
softirq_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	kptrace_write_trace_record_no_callstack("s");
	return 0;
}

static int wake_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "W %d",
		 ((struct task_struct *)regs->ARG0)->pid);

	/* If we try and put a timestamp on this, we'll cause a deadlock */
	timestamping_enabled = 0;
	kptrace_write_trace_record(p, regs, tbuf);
	timestamping_enabled = 1;

	return 0;
}

int irq_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	kptrace_write_trace_record_no_callstack("i");
	return 0;
}

static int irq_exit_rp_handler(struct kretprobe_instance *ri,
			       struct pt_regs *regs)
{
	kptrace_write_trace_record_no_callstack("Ix");
	return 0;
}

static int kthread_create_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "KC 0x%.8x\n",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int exit_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	kptrace_write_trace_record(p, regs, "KX");
	return 0;
}

static int daemonize_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "KD %s\n", (char *)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int kernel_thread_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "KT 0x%.8x\n",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
kthread_create_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct task_struct *new_task = (struct task_struct *)regs->RET;

	snprintf(tbuf, KPTRACE_SMALL_BUF, "Kc %d %s\n", new_task->pid,
							new_task->comm);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

int irq_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "I %d", (int)regs->ARG0);

	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

int syscall_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x 0x%.8x 0x%.8x 0x%.8x",
		 (unsigned)regs->PC, (unsigned)regs->ARG0,
		 (unsigned)regs->ARG1, (unsigned)regs->ARG2,
		 (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Special syscall handler for prctl, in order to get the process name
 out of prctl(PR_SET_NAME) calls. */
static int syscall_prctl_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	char static_buf[KPTRACE_SMALL_BUF];

	if ((unsigned)regs->ARG0 == PR_SET_NAME) {
		if (strncpy_from_user(static_buf, (char *)regs->ARG1,
				      KPTRACE_SMALL_BUF) < 0)
			snprintf(static_buf, KPTRACE_SMALL_BUF,
				 "<copy_from_user failed>");

		snprintf(tbuf, KPTRACE_SMALL_BUF,
			 "E %.8x %d \"%s\" %x %x",
			 (int)regs->PC, (unsigned)regs->ARG0,
			 static_buf, (unsigned)regs->ARG2,
			 (unsigned)regs->ARG3);
	} else {
		snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d %.8x %.8x %.8x",
			 (int)regs->PC,
			 (unsigned)regs->ARG0,
			 (unsigned)regs->ARG1, (unsigned)regs->ARG2,
			 (unsigned)regs->ARG3);
	}
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, hex, hex, hex format */
int syscall_ihhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d 0x%.8x 0x%.8x 0x%.8x",
		 (int)regs->PC, (unsigned)regs->ARG0,
		 (unsigned)regs->ARG1, (unsigned)regs->ARG2,
		 (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, hex, int, hex format */
int syscall_ihih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d 0x%.8x %d 0x%.8x",
		 (int)regs->PC, (int)regs->ARG0,
		 (unsigned)regs->ARG1, (int)regs->ARG2,
		 (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in int, int, hex, hex format */
int syscall_iihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x %d %d 0x%.8x 0x%.8x",
		 (int)regs->PC, (int)regs->ARG0, (int)regs->ARG1,
		 (unsigned)regs->ARG2, (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in hex, int, int, hex format */
int syscall_hiih_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x %d %d 0x%.8x",
		 (int)regs->PC, (unsigned)regs->ARG0,
		 (int)regs->ARG1, (int)regs->ARG2,
		 (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in hex, int, hex, hex format */
int syscall_hihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "E %.8x 0x%.8x %d 0x%.8x 0x%.8x",
		 (int)regs->PC, (unsigned)regs->ARG0,
		 (int)regs->ARG1, (unsigned)regs->ARG2,
		 (unsigned)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

/* Output syscall arguments in string, hex, hex, hex format */
int syscall_shhh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char *dyn_buf;
	char static_buf[KPTRACE_SMALL_BUF];
	char filename[KPTRACE_SMALL_BUF];
	int len = 0;
	int long sys_call_pc = regs->PC;

	if (sys_call_pc == (long)do_execve) {
		snprintf(filename, KPTRACE_SMALL_BUF, (char *)regs->ARG0);
	} else if (strncpy_from_user(filename, (char *)regs->ARG0,
				     KPTRACE_SMALL_BUF) < 0) {
		snprintf(filename, KPTRACE_SMALL_BUF,
			 "<copy_from_user failed>");
	}

	len = snprintf(static_buf, KPTRACE_SMALL_BUF,
			"E %.8x %s 0x%.8x 0x%.8x 0x%.8x",
			(int)sys_call_pc, filename, (unsigned)regs->ARG1,
			(unsigned)regs->ARG2, (unsigned)regs->ARG3);
	if (len < KPTRACE_SMALL_BUF) {
		kptrace_write_trace_record(p, regs, static_buf);
	} else {
		dyn_buf = kzalloc(len + 1, GFP_KERNEL);
		snprintf(dyn_buf, len, "E %.8x %s 0x%.8x 0x%.8x 0x%.8x",
			 (int)sys_call_pc, filename, (unsigned)regs->ARG1,
			 (unsigned)regs->ARG2, (unsigned)regs->ARG3);
		kptrace_write_trace_record(p, regs, dyn_buf);
		kfree(dyn_buf);
	}

	return 0;
}

/* Output syscall arguments in string, int, hex, hex format. */
int syscall_sihh_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char *dyn_buf;
	char static_buf[KPTRACE_SMALL_BUF];
	char filename[KPTRACE_SMALL_BUF];
	int len = 0;

	if (strncpy_from_user
	    (filename, (char *)regs->ARG0, KPTRACE_SMALL_BUF)
	    < 0)
		snprintf(filename, KPTRACE_SMALL_BUF,
			 "<copy_from_user failed>");

	len = snprintf(static_buf, KPTRACE_SMALL_BUF,
			"E %.8x %s %d 0x%.8x 0x%.8x",
			(int)regs->PC, filename, (int)regs->ARG1,
			(unsigned)regs->ARG2, (unsigned)regs->ARG3);

	if (len < KPTRACE_SMALL_BUF) {
		kptrace_write_trace_record_no_callstack(static_buf);
	} else {
		dyn_buf = kzalloc(len + 1, GFP_KERNEL);
		snprintf(dyn_buf, len, "E %.8x %s %d 0x%.8x 0x%.8x",
			 (int)regs->PC, filename, (int)regs->ARG1,
			 (unsigned)regs->ARG2, (unsigned)regs->ARG3);
		kptrace_write_trace_record_no_callstack(dyn_buf);
		kfree(dyn_buf);
	}

	return 0;
}

static int hash_futex_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	union futex_key *key = (union futex_key *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "HF 0x%.8lx %p 0x%.8x",
			key->both.word, key->both.ptr, key->both.offset);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int kmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MM %d %d", (int)regs->ARG0,
		 (int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
kmalloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mm 0x%.8x ", (int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int kfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MF 0x%.8x", (int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int do_page_fault_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MD 0x%.8x %d 0x%.8x",
		 ((unsigned int)((struct pt_regs *)regs->ARG0)->PC),
		 (int)regs->ARG1, (unsigned int)regs->ARG2);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int vmalloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MV %d", (int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
vmalloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mv 0x%.8x ", (int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int vfree_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MQ 0x%.8x", (int)regs->RET);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int get_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MG %d %d", (int)regs->ARG0,
		 (int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
get_free_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mg 0x%.8x ", (int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

int alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MA %d %d", (int)regs->ARG0,
		 (int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

int alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ma 0x%.8x", (int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MZ 0x%.8x %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int kmem_cache_alloc_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MS 0x%.8x %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
kmem_cache_alloc_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ms 0x%.8x ",
		 (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int kmem_cache_free_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MX 0x%.8x 0x%.8x",
		 (unsigned int)regs->ARG0, (unsigned int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

#if defined(CONFIG_BPA2) || defined(CONFIG_BIGPHYS_AREA)
static int
bigphysarea_alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MB 0x%.8x %d %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG1,
		 (int)regs->ARG2);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
bigphysarea_alloc_pages_rp_handler(struct kretprobe_instance *ri,
				   struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mb 0x%.8x",
		 (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int
bigphysarea_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MC 0x%.8x",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int bpa2_alloc_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MH 0x%.8x %d %d %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG1,
		 (int)regs->ARG2, (int)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
bpa2_alloc_pages_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Mh 0x%.8x",
		 (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);
	return 0;
}

static int bpa2_free_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "MI 0x%.8x 0x%.8x",
		 (unsigned int)regs->ARG0, (unsigned int)regs->ARG1);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}
#endif

static int netif_receive_skb_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	kptrace_write_trace_record(p, regs, "NR");
	return 0;
}

static int
netif_receive_skb_rp_handler(struct kretprobe_instance *ri,
			     struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Nr %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int dev_queue_xmit_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	kptrace_write_trace_record(p, regs, "NX");
	return 0;
}

static int
dev_queue_xmit_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Nx %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int sock_sendmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "SS 0x%.8x %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG2);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
sock_sendmsg_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ss %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int sock_recvmsg_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];

	snprintf(tbuf, KPTRACE_SMALL_BUF, "SR 0%.8x %d %d",
		 (unsigned int)regs->ARG0, (int)regs->ARG2,
		 (int)regs->ARG3);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
sock_recvmsg_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Sr %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int do_setitimer_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct itimerval *value = (struct itimerval *)regs->ARG1;

	snprintf(tbuf, KPTRACE_SMALL_BUF, "IS %d %li.%06li %li.%06li",
		 (int)regs->ARG0, value->it_interval.tv_sec,
		 value->it_interval.tv_usec, value->it_value.tv_sec,
		 value->it_value.tv_usec);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
do_setitimer_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Is %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int it_real_fn_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "IE 0x%.8x",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;
}

static int
it_real_fn_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Ie %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

#ifdef CONFIG_KPTRACE_SYNC
static int mutex_lock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZM 0x%.8x",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int mutex_unlock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zm 0x%.8x",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int lock_kernel_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	kptrace_write_trace_record(p, regs, "ZL");
	return 0;

}

static int unlock_kernel_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	kptrace_write_trace_record(p, regs, "Zl");
	return 0;

}

static int down_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZD 0x%.8x %d",
		 (unsigned int)regs->ARG0, (unsigned int)sem->count);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int
down_interruptible_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZI 0x%.8x %d",
		 (unsigned int)regs->ARG0, (unsigned int)sem->count);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZT 0x%.8x %d",
		 (unsigned int)regs->ARG0, (unsigned int)sem->count);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int
down_trylock_rp_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zt %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct semaphore *sem = (struct semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZU 0x%.8x %d",
		 (unsigned int)regs->ARG0, (unsigned int)sem->count);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int underscore_up_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zu 0x%.8x",
		 (unsigned int)regs->ARG0);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZR 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_read_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZA 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int
down_read_trylock_rp_handler(struct kretprobe_instance *ri,
			     struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Za %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_read_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zr 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int down_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZW 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int
down_write_trylock_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "ZB 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}

static int
down_write_trylock_rp_handler(struct kretprobe_instance *ri,
			      struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zb %d", (unsigned int)regs->RET);
	kptrace_write_trace_record_no_callstack(tbuf);

	return 0;
}

static int up_write_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	char tbuf[KPTRACE_SMALL_BUF];
	struct rw_semaphore *sem = (struct rw_semaphore *)regs->ARG0;
	snprintf(tbuf, KPTRACE_SMALL_BUF, "Zw 0x%.8x %d",
		 (unsigned int)regs->ARG0, sem->activity);
	kptrace_write_trace_record(p, regs, tbuf);
	return 0;

}
#endif				/* CONFIG_KPTRACE_SYNC */

/* Add the main sysdev and the "user" tracepoint set */
static int create_sysfs_tree(void)
{
	int ret;
	subsys_system_register(&kptrace_subsys, NULL);
	ret = device_register(&kptrace_device);
	if (ret)
		return ret;

	/* in /sys/devices/system/kptrace/kptrace0 */

	device_create_file(&kptrace_device, &dev_attr_configured);
	device_create_file(&kptrace_device, &dev_attr_stackdepth);
	device_create_file(&kptrace_device, &dev_attr_version);
	device_create_file(&kptrace_device, &dev_attr_pause);
	device_create_file(&kptrace_device, &dev_attr_restart);
	device_create_file(&kptrace_device, &dev_attr_output_driver);

	user_set = kzalloc(sizeof(*user_set), GFP_KERNEL);
	if (!user_set) {
		printk(KERN_WARNING
		       "kptrace: Failed to allocate memory for sysdev\n");
		return -ENOMEM;
	}
	list_add(&user_set->list, &tracepoint_sets);

	if (kobject_init_and_add(&user_set->kobj, &user_type,
				 &kptrace_subsys.dev_root->kobj, "user") < 0)
		printk(KERN_WARNING "kptrace: Failed to add kobject user\n");
	user_set->enabled = 0;

	if (kobject_init_and_add(&userspace, &userspace_type,
				&kptrace_subsys.dev_root->kobj,
				 "userspace") < 0)
		printk(KERN_WARNING
		       "kptrace: Failed to add kobject userspace\n");

	return 0;
}

static void remove_sysfs_tree(void)
{
	kobject_put(&userspace);

	/* in /sys/devices/system/kptrace/kptrace0 */

	device_remove_file(&kptrace_device, &dev_attr_configured);
	device_remove_file(&kptrace_device, &dev_attr_stackdepth);
	device_remove_file(&kptrace_device, &dev_attr_version);
	device_remove_file(&kptrace_device, &dev_attr_pause);
	device_remove_file(&kptrace_device, &dev_attr_restart);
	device_remove_file(&kptrace_device, &dev_attr_output_driver);
	device_unregister(&kptrace_device);

	kptrace_printk(KERN_DEBUG "bus_unregister: %8x\n",
		       (unsigned int)&kptrace_subsys);
	bus_unregister(&kptrace_subsys);
}

static void init_core_event_logging(void)
{
	struct kp_tracepoint_set *set =
			create_tracepoint_set("core_kernel_events");

	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create core tracepoint set.\n");
		return;
	}

	/*install ARCH specific probes */
	if (kp_target && kp_target->init_core_event_logging)
		kp_target->init_core_event_logging(set);

	kptrace_create_tracepoint(set, "handle_simple_irq", irq_pre_handler,
				  irq_rp_handler);
	kptrace_create_tracepoint(set, "handle_level_irq", irq_pre_handler,
				  irq_rp_handler);
	kptrace_create_tracepoint(set, "handle_fasteoi_irq", irq_pre_handler,
				  irq_rp_handler);
	kptrace_create_tracepoint(set, "handle_edge_irq", irq_pre_handler,
				  irq_rp_handler);
	kptrace_create_tracepoint(set, "irq_exit", NULL, irq_exit_rp_handler);
	kptrace_create_tracepoint(set, "tasklet_hi_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "net_tx_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "net_rx_action", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "blk_done_softirq", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "tasklet_action", softirq_pre_handler,
				  softirq_rp_handler);

	/* Since 2.6.38, kthread_create() is a macro. If the symbol exists,
	 * we set the tracepoint on that, if it doesn't then we use
	 * kthread_create_on_node with an alias so that it appears in sysfs
	 * as "kthread_create". This provides a common sysfs interface
	 * on each side of the change. */
	if  (kallsyms_lookup_name("kthread_create"))
		kptrace_create_tracepoint(set, "kthread_create",
					  kthread_create_pre_handler,
					  kthread_create_rp_handler);
	else
		kptrace_create_aliased_tracepoint(set, "kthread_create_on_node",
				kthread_create_pre_handler,
				kthread_create_rp_handler, "kthread_create");

	kptrace_create_tracepoint(set, "kernel_thread",
				  kernel_thread_pre_handler, NULL);
	kptrace_create_tracepoint(set, "daemonize", daemonize_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "exit_thread", exit_thread_pre_handler,
				  NULL);
}

static void init_syscall_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("syscalls");

	printk(KERN_DEBUG "init_syscall_logging\n");

	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create syscall tracepoint set.\n");
		return;
	}

	CALL(sys_restart_syscall)
	CALL(sys_exit)
	CALL(sys_fork)
	CALL_CUSTOM_PRE(sys_read, syscall_ihih_pre_handler);
	CALL_CUSTOM_PRE(sys_write, syscall_ihih_pre_handler);
	CALL_CUSTOM_PRE(sys_open, syscall_shhh_pre_handler);
	CALL_CUSTOM_PRE(sys_close, syscall_ihhh_pre_handler);
	CALL(sys_creat)
	CALL(sys_link)
	CALL(sys_unlink)
	CALL(sys_chdir)
	CALL(sys_mknod)
	CALL(sys_chmod)
	CALL(sys_lchown16)
	CALL(sys_lseek)
	CALL(sys_getpid)
	CALL(sys_mount)
	CALL(sys_setuid16)
	CALL(sys_getuid16)
	CALL(sys_ptrace)
	CALL(sys_pause)
	CALL_CUSTOM_PRE(sys_access,
					syscall_sihh_pre_handler);
	CALL(sys_nice)
	CALL(sys_sync)
	CALL(sys_kill)
	CALL(sys_rename)
	CALL(sys_mkdir)
	CALL(sys_rmdir)
	CALL_CUSTOM_PRE(sys_dup, syscall_ihhh_pre_handler);
	CALL(sys_pipe)
	CALL(sys_times)
	CALL(sys_brk)
	CALL(sys_setgid16)
	CALL(sys_getgid16)
	CALL(sys_geteuid16)
	CALL(sys_getegid16)
	CALL(sys_acct)
	CALL(sys_umount)
	CALL_CUSTOM_PRE(sys_ioctl,
					 syscall_iihh_pre_handler);
	CALL(sys_fcntl)
	CALL(sys_setpgid)
	CALL(sys_umask)
	CALL(sys_chroot)
	CALL(sys_ustat)
	CALL_CUSTOM_PRE(sys_dup2, syscall_iihh_pre_handler);
	CALL(sys_getppid)
	CALL(sys_getpgrp)
	CALL(sys_setsid)
	CALL(sys_sigaction)
	CALL(sys_setreuid16)
	CALL(sys_setregid16)
	CALL(sys_sigsuspend)
	CALL(sys_sigpending)
	CALL(sys_sethostname)
	CALL(sys_setrlimit)
	CALL(sys_getrusage)
	CALL(sys_gettimeofday)
	CALL(sys_settimeofday)
	CALL(sys_getgroups16)
	CALL(sys_setgroups16)
	CALL(sys_symlink)
	CALL(sys_readlink)
	CALL(sys_uselib)
	CALL(sys_swapon)
	CALL(sys_reboot)
	CALL_CUSTOM_PRE(sys_munmap,
					 syscall_hihh_pre_handler);
	CALL(sys_truncate)
	CALL(sys_ftruncate)
	CALL(sys_fchmod)
	CALL(sys_fchown16)
	CALL(sys_getpriority)
	CALL(sys_setpriority)
	CALL(sys_statfs)
	CALL(sys_fstatfs)
	CALL(sys_syslog)
	CALL(sys_setitimer)
	CALL(sys_getitimer)
	CALL(sys_newstat)
	CALL(sys_newlstat)
	CALL(sys_newfstat)
	CALL(sys_vhangup)
	CALL(sys_wait4)
	CALL(sys_swapoff)
	CALL(sys_sysinfo)
	CALL(sys_fsync)
	CALL(sys_sigreturn)
	CALL(sys_clone)
	CALL(sys_setdomainname)
	CALL(sys_newuname)
	CALL(sys_adjtimex)
	CALL(sys_mprotect)
	CALL(sys_sigprocmask)
	CALL(sys_init_module)
	CALL(sys_delete_module)
	CALL(sys_quotactl)
	CALL(sys_getpgid)
	CALL(sys_fchdir)
	CALL(sys_bdflush)
	CALL(sys_sysfs)
	CALL(sys_personality)
	CALL(sys_setfsuid16)
	CALL(sys_setfsgid16)
	CALL(sys_llseek)
	CALL(sys_getdents)
	CALL(sys_select)
	CALL(sys_flock)
	CALL(sys_msync)
	CALL(sys_readv)
	CALL(sys_writev)
	CALL(sys_getsid)
	CALL(sys_fdatasync)
	CALL(sys_sysctl)
	CALL(sys_mlock)
	CALL(sys_munlock)
	CALL(sys_mlockall)
	CALL(sys_munlockall)
	CALL(sys_sched_setparam)
	CALL(sys_sched_getparam)
	CALL(sys_sched_setscheduler)
	CALL(sys_sched_getscheduler)
	CALL(sys_sched_yield)
	CALL(sys_sched_get_priority_max)
	CALL(sys_sched_get_priority_min)
	CALL(sys_sched_rr_get_interval)
	CALL(sys_nanosleep)
	CALL(sys_setresuid16)
	CALL(sys_getresuid16)
	CALL(sys_poll)
	CALL(sys_nfsservctl)
	CALL(sys_setresgid16)
	CALL(sys_getresgid16)
	CALL_CUSTOM_PRE(sys_prctl, syscall_prctl_pre_handler);
	CALL(sys_rt_sigreturn)
	CALL(sys_rt_sigaction)
	CALL(sys_rt_sigprocmask)
	CALL(sys_rt_sigpending)
	CALL(sys_rt_sigtimedwait)
	CALL(sys_rt_sigqueueinfo)
	CALL(sys_rt_sigsuspend)
	CALL(sys_chown16)
	CALL(sys_getcwd)
	CALL(sys_capget)
	CALL(sys_capset)
	CALL(sys_sendfile)
	CALL(sys_vfork)
	CALL(sys_getrlimit)
	CALL_CUSTOM_PRE(sys_mmap2, syscall_hiih_pre_handler);
	CALL(sys_lchown)
	CALL(sys_getuid)
	CALL(sys_getgid)
	CALL(sys_geteuid)
	CALL(sys_getegid)
	CALL(sys_setreuid)
	CALL(sys_setregid)
	CALL(sys_getgroups)
	CALL(sys_setgroups)
	CALL(sys_fchown)
	CALL(sys_setresuid)
	CALL(sys_getresuid)
	CALL(sys_setresgid)
	CALL(sys_getresgid)
	CALL(sys_chown)
	CALL(sys_setuid)
	CALL(sys_setgid)
	CALL(sys_setfsuid)
	CALL(sys_setfsgid)
	CALL(sys_getdents64)
	CALL(sys_pivot_root)
	CALL(sys_mincore)
	CALL(sys_madvise)
	CALL(sys_gettid)
	CALL(sys_setxattr)
	CALL(sys_lsetxattr)
	CALL(sys_fsetxattr)
	CALL(sys_getxattr)
	CALL(sys_lgetxattr)
	CALL(sys_fgetxattr)
	CALL(sys_listxattr)
	CALL(sys_llistxattr)
	CALL(sys_flistxattr)
	CALL(sys_removexattr)
	CALL(sys_lremovexattr)
	CALL(sys_fremovexattr)
	CALL(sys_tkill)
	CALL(sys_sendfile64)
	CALL(sys_futex)
	CALL(sys_sched_setaffinity)
	CALL(sys_sched_getaffinity)
	CALL(sys_io_setup)
	CALL(sys_io_destroy)
	CALL(sys_io_getevents)
	CALL(sys_io_submit)
	CALL(sys_io_cancel)
	CALL(sys_exit_group)
	CALL(sys_lookup_dcookie)
	CALL(sys_epoll_create)
	CALL(sys_remap_file_pages)
	CALL(sys_set_tid_address)
	CALL(sys_timer_create)
	CALL(sys_timer_settime)
	CALL(sys_timer_gettime)
	CALL(sys_timer_getoverrun)
	CALL(sys_timer_delete)
	CALL(sys_clock_settime)
	CALL(sys_clock_gettime)
	CALL(sys_clock_getres)
	CALL(sys_clock_nanosleep)
	CALL(sys_statfs64)
	CALL(sys_fstatfs64)
	CALL(sys_tgkill)
	CALL(sys_utimes)
	CALL(sys_fadvise64_64)
	CALL(sys_mq_open)
	CALL(sys_mq_unlink)
	CALL(sys_mq_timedsend)
	CALL(sys_mq_timedreceive)
	CALL(sys_mq_notify)
	CALL(sys_mq_getsetattr)
	CALL(sys_waitid)
	CALL(sys_socket)
	CALL(sys_listen)
	CALL(sys_accept)
	CALL(sys_getsockname)
	CALL(sys_getpeername)
	CALL(sys_socketpair)
	CALL(sys_send)
	CALL(sys_recv)
	CALL(sys_recvfrom)
	CALL(sys_shutdown)
	CALL(sys_setsockopt)
	CALL(sys_getsockopt)
	CALL(sys_recvmsg)
	CALL(sys_semget)
	CALL(sys_semctl)
	CALL(sys_msgsnd)
	CALL(sys_msgrcv)
	CALL(sys_msgget)
	CALL(sys_msgctl)
	CALL(sys_shmat)
	CALL(sys_shmdt)
	CALL(sys_shmget)
	CALL(sys_shmctl)
	CALL(sys_add_key)
	CALL(sys_request_key)
	CALL(sys_keyctl)
	CALL(sys_ioprio_set)
	CALL(sys_ioprio_get)
	CALL(sys_inotify_init)
	CALL(sys_inotify_add_watch)
	CALL(sys_inotify_rm_watch)
	CALL(sys_mbind)
	CALL(sys_get_mempolicy)
	CALL(sys_set_mempolicy)
	CALL(sys_openat)
	CALL(sys_mkdirat)
	CALL(sys_mknodat)
	CALL(sys_fchownat)
	CALL(sys_futimesat)
	CALL(sys_unlinkat)
	CALL(sys_renameat)
	CALL(sys_linkat)
	CALL(sys_symlinkat)
	CALL(sys_readlinkat)
	CALL(sys_fchmodat)
	CALL(sys_faccessat)
	CALL(sys_unshare)
	CALL(sys_set_robust_list)
	CALL(sys_get_robust_list)
	CALL(sys_splice)
	CALL(sys_tee)
	CALL(sys_vmsplice)
	CALL(sys_move_pages)
	CALL(sys_kexec_load)
	kptrace_create_tracepoint(set, "do_execve",
				      syscall_shhh_pre_handler,
				      syscall_rp_handler);

	kptrace_create_tracepoint(set, "hash_futex", hash_futex_handler, NULL);

	/*install/override ARCH specific probes */
	if (kp_target && kp_target->init_syscall_logging)
		kp_target->init_syscall_logging(set);

}

static void init_memory_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("memory_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create memory tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "__kmalloc", kmalloc_pre_handler,
				  kmalloc_rp_handler);
	kptrace_create_tracepoint(set, "kfree", kfree_pre_handler, NULL);
	kptrace_create_tracepoint(set, "do_page_fault",
				  do_page_fault_pre_handler, NULL);
	kptrace_create_tracepoint(set, "vmalloc", vmalloc_pre_handler,
				  vmalloc_rp_handler);
	kptrace_create_tracepoint(set, "vfree", vfree_pre_handler, NULL);
	kptrace_create_tracepoint(set, "__get_free_pages",
				  get_free_pages_pre_handler,
				  get_free_pages_rp_handler);
	kptrace_create_tracepoint(set, "free_pages", free_pages_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "kmem_cache_alloc",
				  kmem_cache_alloc_pre_handler,
				  kmem_cache_alloc_rp_handler);
	kptrace_create_tracepoint(set, "kmem_cache_free",
				  kmem_cache_free_pre_handler, NULL);
#if defined(CONFIG_BPA2) || defined(CONFIG_BIGPHYS_AREA)
	kptrace_create_tracepoint(set, "__bigphysarea_alloc_pages",
				  bigphysarea_alloc_pages_pre_handler,
				  bigphysarea_alloc_pages_rp_handler);
	kptrace_create_tracepoint(set, "bigphysarea_free_pages",
				  bigphysarea_free_pages_pre_handler, NULL);
	kptrace_create_tracepoint(set, "__bpa2_alloc_pages",
				  bpa2_alloc_pages_pre_handler,
				  bpa2_alloc_pages_rp_handler);
	kptrace_create_tracepoint(set, "bpa2_free_pages",
				  bpa2_free_pages_pre_handler, NULL);
#endif

	/*install/override ARCH specific probes */
	if (kp_target && kp_target->init_memory_logging)
		kp_target->init_memory_logging(set);
}

static void init_network_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("network_events");
	if (set == NULL) {
		printk(KERN_WARNING
		       "kptrace: unable to create network tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "netif_receive_skb",
				  netif_receive_skb_pre_handler,
				  netif_receive_skb_rp_handler);
	kptrace_create_tracepoint(set, "dev_queue_xmit",
				  dev_queue_xmit_pre_handler,
				  dev_queue_xmit_rp_handler);
	kptrace_create_tracepoint(set, "sock_sendmsg", sock_sendmsg_pre_handler,
				  sock_sendmsg_rp_handler);
	kptrace_create_tracepoint(set, "sock_recvmsg", sock_recvmsg_pre_handler,
				  sock_recvmsg_rp_handler);
}

static void init_timer_logging(void)
{
	struct kp_tracepoint_set *set = create_tracepoint_set("timer_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create timer tracepoint set.\n");
		return;
	}

	kptrace_create_tracepoint(set, "do_setitimer", do_setitimer_pre_handler,
				  do_setitimer_rp_handler);
	kptrace_create_tracepoint(set, "it_real_fn", it_real_fn_pre_handler,
				  it_real_fn_rp_handler);
	kptrace_create_tracepoint(set, "run_timer_softirq", softirq_pre_handler,
				  softirq_rp_handler);
	kptrace_create_tracepoint(set, "try_to_wake_up", wake_pre_handler,
				  NULL);
}

#ifdef CONFIG_KPTRACE_SYNC
static void init_synchronization_logging(void)
{
	struct kp_tracepoint_set *set =
	    create_tracepoint_set("synchronization_events");
	if (!set) {
		printk(KERN_WARNING
		       "kptrace: unable to create synchronization tracepoint "
		       "set.\n");
		return;
	}

	create_late_tracepoint(set, "mutex_lock", mutex_lock_pre_handler, NULL);
	create_late_tracepoint(set, "mutex_unlock", mutex_unlock_pre_handler,
			       NULL);

	kptrace_create_tracepoint(set, "lock_kernel", lock_kernel_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "unlock_kernel",
				  unlock_kernel_pre_handler, NULL);

	kptrace_create_tracepoint(set, "down", down_pre_handler, NULL);
	kptrace_create_tracepoint(set, "down_interruptible",
				  down_interruptible_pre_handler, NULL);
	kptrace_create_tracepoint(set, "down_trylock", down_trylock_pre_handler,
				  down_trylock_rp_handler);
	kptrace_create_tracepoint(set, "up", up_pre_handler, NULL);
	kptrace_create_tracepoint(set, "__up", underscore_up_pre_handler, NULL);

	kptrace_create_tracepoint(set, "down_read", down_read_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "down_read_trylock",
				  down_read_trylock_pre_handler,
				  down_read_trylock_rp_handler);
	kptrace_create_tracepoint(set, "down_write", down_write_pre_handler,
				  NULL);
	kptrace_create_tracepoint(set, "down_write_trylock",
				  down_write_trylock_pre_handler,
				  down_write_trylock_rp_handler);
	kptrace_create_tracepoint(set, "up_read", up_read_pre_handler, NULL);
	kptrace_create_tracepoint(set, "up_write", up_write_pre_handler, NULL);
}
#endif

/**
 *      remove_channel_controls - removes produced/consumed control files
 */
static void remove_channel_controls(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		if (produced_control[cpu]) {
			debugfs_remove(produced_control[cpu]);
			produced_control[cpu] = NULL;
			continue;
		}
		break;
	}

	for_each_possible_cpu(cpu) {
		if (consumed_control[cpu]) {
			debugfs_remove(consumed_control[cpu]);
			consumed_control[cpu] = NULL;
			continue;
		}
		break;
	}
}


/**
 *	create_channel_controls - creates produced/consumed control files
 *
 *	Returns 1 on success, 0 otherwise.
 */
static int
create_channel_controls(struct dentry *parent,
			const char *base_filename, struct rchan_buf *buf,
			int cpu)
{
	char *tmpname = kmalloc(NAME_MAX + 1, GFP_KERNEL);
	if (!tmpname)
		return 0;

	kptrace_printk(KERN_DEBUG "create_channel_controls "
			"%s.produced/.consumed\n", base_filename);

	snprintf(tmpname, NAME_MAX, "%s.produced", base_filename);
	produced_control[cpu] =
	    debugfs_create_file(tmpname, S_IRUGO, parent, buf, &produced_fops);
	if (!produced_control[cpu]) {
		printk(KERN_WARNING "Couldn't create relay control file %s\n",
		       tmpname);
		goto cleanup_control_files;
	}

	snprintf(tmpname, NAME_MAX, "%s.consumed", base_filename);
	consumed_control[cpu] = debugfs_create_file(tmpname, S_IWUSR|S_IRUGO,
						    parent, buf,
						    &consumed_fops);
	if (!consumed_control[cpu]) {
		printk(KERN_WARNING "Couldn't create relay control file %s.\n",
		       tmpname);
		goto cleanup_control_files;
	}

	kfree(tmpname);
	return 1;

cleanup_control_files:
	kfree(tmpname);
	remove_channel_controls();
	return 0;
}

/*
 * subbuf_start() relay callback.
 *
 * If all the sub-buffers are full, we don't overwrite them - no
 * more trace is recorded until userspace has consumed some of the
 * existing sub-buffers. The exception is in flight-recorder mode, where
 * the whole point is that nothing is consumed until the end.
 *
 * We printk a warning if the buffer is full, as trace data is being lost
 * (and a larger buffer would prevent it). Those messages can be frequent if
 * the buffer fills, so we limit the number of warnings emitted per trace
 * session.
 */
static int
subbuf_start_handler(struct rchan_buf *buf, void *subbuf,
		     void *prev_subbuf, unsigned int prev_padding)
{
	if (prev_subbuf)
		*((unsigned *)prev_subbuf) = prev_padding;

	if (!overwrite_subbufs && relay_buf_full(buf)) {
		if (buffer_full_warning_ratelimit) {
			printk(KERN_WARNING "kptrace: trace buffer full. "
				"Consider increasing the buffer size.\n");
			buffer_full_warning_ratelimit--;
			if (!buffer_full_warning_ratelimit)
				printk(KERN_WARNING "kptrace: disabling "
						"buffer full warnings.\n");
		}
		return 0;
	}

	subbuf_start_reserve(buf, sizeof(unsigned int));

	return 1;
}

/*
 * file_create() callback.  Creates relay file in debugfs.
 */
static struct dentry *create_buf_file_handler(const char *filename,
					      struct dentry *parent,
					      umode_t mode,
					      struct rchan_buf *buf,
					      int *is_global)
{
	struct dentry *buf_file;
	unsigned long cpu;

	if (kstrtoul(filename + 5, 10, &cpu)) {
		printk(KERN_WARNING
			"kptrace (create_buf_file_handler): "
			"invalid CPU number\n");
		return NULL;
	}

	kptrace_printk(KERN_DEBUG "create_buf_file_handler %s\n", filename);

	buf_file = debugfs_create_file(filename, mode, parent, buf,
				       &relay_file_operations);

	if (!create_channel_controls(parent, filename, buf, cpu)) {
		printk(KERN_WARNING
			"kptrace: unable to create relayfs channel\n");
		return NULL;
	}

	return buf_file;
}

/*
 * file_remove() default callback.  Removes relay file in debugfs.
 */
static int remove_buf_file_handler(struct dentry *dentry)
{
	debugfs_remove(dentry);

	return 0;
}

/*
 * relay callbacks
 */
static struct rchan_callbacks relay_callbacks = {
	.subbuf_start = subbuf_start_handler,
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

/**
 *	create_channel - creates channel /debug/kptrace/trace0
 *
 *	Creates channel along with associated produced/consumed control files
 *
 *	Returns channel on success, NULL otherwise
 */
static struct rchan *create_channel(unsigned subbuf_size, unsigned n_subbufs)
{
	struct rchan *tmpchan;

	tmpchan = relay_open("trace", dir, subbuf_size, n_subbufs,
			     &relay_callbacks, NULL);

	if (!tmpchan) {
		printk(KERN_WARNING "relay app channel creation failed\n");
		return NULL;
	}

	logging = 0;
	mappings = 0;
	suspended = 0;
	dropped = 0;

	return tmpchan;
}

/**
 *	destroy_channel - destroys channel /debug/kptrace/trace0
 *
 *	Destroys channel along with associated produced/consumed control files
 */
static void destroy_channel(void)
{
	if (chan) {
		relay_close(chan);
		chan = NULL;
	}
	remove_channel_controls();
}

/**
 *	remove_controls - removes channel management control files
 */
static void remove_controls(void)
{
	if (enabled_control)
		debugfs_remove(enabled_control);

	if (subbuf_size_control)
		debugfs_remove(subbuf_size_control);

	if (n_subbufs_control)
		debugfs_remove(n_subbufs_control);

	if (create_control)
		debugfs_remove(create_control);

	if (dropped_control)
		debugfs_remove(dropped_control);

	if (overwrite_control)
		debugfs_remove(overwrite_control);

	dropped_control = create_control = n_subbufs_control =
	    subbuf_size_control = enabled_control = NULL;
}

/**
 *	create_controls - creates channel management control files
 *
 *	Returns 1 on success, 0 otherwise.
 */
static int create_controls(void)
{
	enabled_control = debugfs_create_file("enabled", 0, dir,
					      NULL, &enabled_fops);

	if (!enabled_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'enabled'.\n");
		goto fail;
	}

	subbuf_size_control = debugfs_create_file("subbuf_size", 0, dir, NULL,
						  &subbuf_size_fops);
	if (!subbuf_size_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'subbuf_size'.\n");
		goto fail;
	}

	n_subbufs_control = debugfs_create_file("n_subbufs", 0, dir, NULL,
						&n_subbufs_fops);
	if (!n_subbufs_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'n_subbufs'.\n");
		goto fail;
	}

	create_control =
	    debugfs_create_file("create", 0, dir, NULL, &create_fops);
	if (!create_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'create'.\n");
		goto fail;
	}

	dropped_control = debugfs_create_file("dropped", 0, dir, NULL,
					      &dropped_fops);
	if (!dropped_control) {
		printk(KERN_ERR
		       "Couldn't create relay control file 'dropped'.\n");
		goto fail;
	}

	overwrite_control = debugfs_create_file("overwrite", 0, dir,
					      NULL, &overwrite_fops);
	if (!overwrite_control) {
		printk(KERN_WARNING "Couldn't create relay control "
					"file 'overwrite'.\n");
		goto fail;
	}

	return 1;
fail:
	remove_controls();
	return 0;
}

/*
 * control file fileop definitions
 */

/*
 * control files for relay channel management
 */

static ssize_t
enabled_read(struct file *filp, char __user *buffer,
	     size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", logging);
	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
enabled_write(struct file *filp, const char __user *buffer,
	      size_t count, loff_t *ppos)
{
	char buf[16];
	long enabled;

	kptrace_printk(KERN_DEBUG "enabled_write\n");

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (kstrtol(buf, 10, &enabled))
		return -EINVAL;

	if (enabled && chan)
		logging = 1;
	else if (!enabled) {
		logging = 0;
		if (chan)
			relay_flush(chan);
	}

	return count;
}

/*
 * 'enabled' file operations - boolean r/w
 *
 *  toggles logging to the relay channel
 */
static const struct file_operations enabled_fops = {.owner = THIS_MODULE,
	.read = enabled_read, .write = enabled_write,
};

static ssize_t
create_read(struct file *filp, char __user *buffer,
	    size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%d\n", !!chan);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
create_write(struct file *filp, const char __user *buffer,
	     size_t count, loff_t *ppos)
{
	char buf[16];
	long create;

	kptrace_printk(KERN_DEBUG "create_write\n");

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (kstrtol(buf, 10, &create))
		return -EINVAL;

	if (create) {
		destroy_channel();
		chan = create_channel(subbuf_size, n_subbufs);
		if (!chan)
			return -ENOSYS;
	} else
		destroy_channel();

	return count;
}

/*
 * 'create' file operations - boolean r/w
 *
 *  creates/destroys the relay channel
 */
static const struct file_operations create_fops = {.owner = THIS_MODULE,
	.read = create_read, .write = create_write,
};

static ssize_t
subbuf_size_read(struct file *filp, char __user *buffer,
		 size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", subbuf_size);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
subbuf_size_write(struct file *filp, const char __user *buffer,
		  size_t count, loff_t *ppos)
{
	char buf[16];
	long tmp;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if ((kstrtol(buf, 10, &tmp) == -EINVAL)
		|| (tmp < 1)
		|| (tmp > KPTRACE_MAXSUBBUFSIZE))
		return -EINVAL;

	subbuf_size = (size_t)tmp;

	return count;
}

/*
 * 'subbuf_size' file operations - r/w
 *
 *  gets/sets the subbuffer size to use in channel creation
 */
static const struct file_operations subbuf_size_fops = {.owner = THIS_MODULE,
	.read = subbuf_size_read, .write = subbuf_size_write,
};

static ssize_t
n_subbufs_read(struct file *filp, char __user *buffer,
	       size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", n_subbufs);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t
n_subbufs_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	char buf[16];
	long tmp;

	if (count > sizeof(buf))
		return -EINVAL;

	memset(buf, 0, sizeof(buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if ((kstrtol(buf, 10, &tmp) == -EINVAL)
		|| (tmp < 1)
		|| (tmp > KPTRACE_MAXSUBBUFS))
		return -EINVAL;

	n_subbufs = (size_t)tmp;

	return count;
}

/*
 * 'n_subbufs' file operations - r/w
 *
 *  gets/sets the number of subbuffers to use in channel creation
 */
static const struct file_operations n_subbufs_fops = {.owner = THIS_MODULE,
	.read = n_subbufs_read, .write = n_subbufs_write,
};

static ssize_t
dropped_read(struct file *filp, char __user *buffer,
	     size_t count, loff_t *ppos)
{
	char buf[16];

	snprintf(buf, sizeof(buf), "%zu\n", dropped);

	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t overwrite_write(struct file *filp, const char __user *buffer,
			       size_t count, loff_t *ppos)
{
	char buf;

	if (count > 1)
		return -EINVAL;

	if (copy_from_user(&buf, buffer, count))
		return -EFAULT;

	if (buf == '0')
		overwrite_subbufs = 0;
	else if (buf == '1')
		overwrite_subbufs = 1;
	else
		return -EINVAL;

	return count;
}

static const struct file_operations overwrite_fops = {
	.owner = THIS_MODULE,
	.write = overwrite_write,
};

/*
 * 'dropped' file operations - r
 *
 *  gets the number of dropped events seen
 */
static const struct file_operations dropped_fops = {.owner = THIS_MODULE,
	.read = dropped_read,
};

/*
 * control files for relay produced/consumed sub-buffer counts
 */

static int produced_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t
produced_read(struct file *filp, char __user *buffer,
	      size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_produced,
				       sizeof(buf->subbufs_produced));
}

/*
 * 'produced' file operations - r, binary
 *
 *  There is a .produced file associated with each relay file.
 *  Reading a .produced file returns the number of sub-buffers so far
 *  produced for the associated relay buffer.
 */
static const struct file_operations produced_fops = {.owner = THIS_MODULE,
	.open = produced_open, .read = produced_read
};

static int consumed_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;

	return 0;
}

static ssize_t
consumed_read(struct file *filp, char __user *buffer,
	      size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;

	return simple_read_from_buffer(buffer, count, ppos,
				       &buf->subbufs_consumed,
				       sizeof(buf->subbufs_consumed));
}

static ssize_t
consumed_write(struct file *filp, const char __user *buffer,
	       size_t count, loff_t *ppos)
{
	struct rchan_buf *buf = filp->private_data;
	size_t consumed;

	if (copy_from_user(&consumed, buffer, sizeof(consumed)))
		return -EFAULT;

	relay_subbufs_consumed(buf->chan, buf->cpu, consumed);

	return count;
}

/*
 * 'consumed' file operations - r/w, binary
 *
 *  There is a .consumed file associated with each relay file.
 *  Writing to a .consumed file adds the value written to the
 *  subbuffers-consumed count of the associated relay buffer.
 *  Reading a .consumed file returns the number of sub-buffers so far
 *  consumed for the associated relay buffer.
 */
static const struct file_operations consumed_fops = {
	.open = consumed_open,
	.read = consumed_read,
	.write = consumed_write,
};

/*
 * ktprace_init - initialize the relay channel and the sysfs tree
 */
static int __init kptrace_init(void)
{
	/* in /sys/kernel/debug/kptrace/... */

	dir = debugfs_create_dir("kptrace", NULL);

	kptrace_printk(KERN_DEBUG "debugfs_create_dir: dir=%8x\n",
		       (unsigned int)dir);

	if (!dir) {
		printk(KERN_ERR "Couldn't create relay app directory.\n");
		return -ENOMEM;
	}

	if (!create_controls()) {
		printk(KERN_ERR "Couldn't create debugfs files\n");
		debugfs_remove(dir);
		return -ENOMEM;
	}

	/* in /sys/device/system/kptrace/... */

	if (create_sysfs_tree()) {
		debugfs_remove(dir);
		printk(KERN_ERR "Couldn't create sysfs tree\n");
		return -ENOSYS;
	}

	init_core_event_logging();
	init_syscall_logging();
	init_memory_logging();
	init_network_logging();
	init_timer_logging();
#ifdef CONFIG_KPTRACE_SYNC
	init_synchronization_logging();
#endif

	/* Register the relay output, and select it as the default */
	kptrace_register_output_driver(&relay_output_driver);
	current_output_driver = &relay_output_driver;

	printk(KERN_INFO "kptrace: initialized\n");

	return 0;
}

/*
 * kptrace_cleanup - free all the tracepoints and sets, remove the sysdev and
 * destroy the relay channel.
 */
static void kptrace_cleanup(void)
{
	struct list_head *p;
	struct kp_tracepoint_set *set;
	struct kp_tracepoint *tp;

	kptrace_printk(KERN_DEBUG "kptrace_cleanup\n");

	stop_tracing();

	list_for_each(p, &tracepoint_sets) {
		set = list_entry(p, struct kp_tracepoint_set, list);
		if (set != NULL) {
			kobject_put(&set->kobj);
			kfree(set);
		}
	}

	list_for_each(p, &tracepoints) {
		tp = list_entry(p, struct kp_tracepoint, list);
		if (tp != NULL) {
			kobject_put(&tp->kobj);
			kfree(tp);
		}
	}

	destroy_channel();
	remove_controls();
	remove_sysfs_tree();

	kptrace_unregister_output_driver(&relay_output_driver);

	if (dir) {
		kptrace_printk(KERN_DEBUG "debugfs_remove: dir=%8x\n",
			       (unsigned int)dir);
		debugfs_remove(dir);
		dir = NULL;
	}
}

module_init(kptrace_init);
module_exit(kptrace_cleanup);
MODULE_LICENSE("GPL");
