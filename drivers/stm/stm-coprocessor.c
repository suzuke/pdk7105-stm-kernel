/*
 * Copyright (C) 2003-2004 Giuseppe Cavallaro (peppe.cavallaro@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interfaces (where required) the co-processors on ST platforms based
 * on multiprocessor architecture, for embedded products like Set-top-Box
 * DVD, etc...
 *
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/bootmem.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_DEVFS_FS
#include <linux/devfs_fs_kernel.h>
#endif

#include <linux/delay.h>
#include <linux/mm.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#include <linux/stm/coprocessor.h>
#include <asm/types.h>
#include <asm/uaccess.h>
#include <asm/sections.h>
#include <asm/io.h>
#include <asm/irq.h>

/* ---------------------------------------------------------------------------
 *     Local (declared out of order) functions
 * ------------------------------------------------------------------------ */

static int __init parse_coproc_mem(char *from);
static int __init proc_st_coproc_init(void);

/* ---------------------------------------------------------------------------
 *    Exported and Imported
 * ------------------------------------------------------------------------ */
extern unsigned long memory_start, memory_end;

/* ---------------------------------------------------------------------------
 * 		Co-processor: Hardware dependent support
 * This includes:
 *    - per platform device and memory addresses
 *    - platform dependent macros
 *    - HW dependent actions required by the generic APIs: Init,
 *      Open, Release, Ioctl (to reset, trigger the start (grant),
 *      peek and poke, etc...) functions
 * ------------------------------------------------------------------------ */

extern struct coproc_board_info coproc_info;

/* ---------------------------------------------------------------------------
 *    Local data structure
 * ------------------------------------------------------------------------ */

extern coproc_t coproc[];	/* The maximum number of copro-  */
					   /* cessors depends on platform   */
					   /* type                          */

#ifdef CONFIG_DEVFS_FS
static devfs_handle_t devfs_reset_hdl;
#endif

#if defined(CONFIG_COPROCESSOR_DEBUG)
/* -------------------------------------------------------------------------
 * 			Co-processor DEBUG Suppor26t
 *
 *  Currently we do not expect to receive asynchronous events from the
 *  slave processor. This routine as well as the IRQ definition has been
 *  foreseen and used for debug purposes.
 *  Once a slave is started the standard and supported way of communicating
 *  with the host processor is to rely on the RPC service.
 */

static int irq_count = 0;
static void mbx_irq_handle(int irq, void *cop, struct pt_regs *regs)
{
	/* avoid a noisy loop if possible! */
	irq_count++;
	if ((irq_count % 100) == 0)
		printk("st-coprocessor: unexpected interrupt %d from %s\n",
		       irq, ((coproc_t *) cop)->dev.bus_id);

	(void)&mbx_irq_handle;	/* warning suppression */
}

#endif				/* CONFIG_COPROCESSOR_DEBUG */

static void __debug(coproc_t * cop, const char *fnc)
{

	/* Print the coprocessor control structure */
	printk("%s Coprocessor -------------------------------------------\n",
	       cop->dev.bus_id);
	if (cop->control == 0 || cop->ram_size == 0) {
		printk("    not configured!\n");
		goto skip_debug;
	} else {
		printk
		    ("    flags %04x RAM start at 0x%08lx  size      0x%08x\n",
		     cop->control, HOST_ADDR(cop, 0), cop->ram_size);
		printk("                  cop. addr 0x%08lx\n",
		       COPR_ADDR(cop, 0));
	}

#ifdef CONFIG_COPROCESSOR_DEBUG
	if (cop->h2c_port)
		printk
		    ("    Channels : h->c 0x%08x (%08lx)    c->h 0x%08x (%08lx)\n",
		     cop->h2c_port, readl(cop->h2c_port), cop->c2h_port,
		     readl(cop->c2h_port));
	else
#endif
		printk("    Channels : Not defined\n");

	if (cop->irq)
		printk("    IRQ      : %d\n", cop->irq);
	else
		printk("    IRQ      : not used\n");

      skip_debug:
	printk
	    ("---------------------------------------------------------------\n");

}

/* ---------------------------------------------------------------------------
 * 			Co-processor driver APIs
 * ------------------------------------------------------------------------ */

static int st_coproc_open(struct inode *inode, struct file *file)
{
	/*
	 ** use minor number (ID) to access the current coproc. descriptor
	 */
	coproc_t *cop = FILE_2_COP(coproc, file);

	DPRINTK(">>> %s: %s, cop->control = %d, cop->ram_size = %d\n",
		__FUNCTION__, cop->dev.bus_id, cop->control, cop->ram_size);

	if (((cop - coproc) / sizeof(coproc_t)) >= coproc_info.max_coprs)
		return (-ENODEV);
	if (cop->ram_size == 0)
		return (-ENOSPC);
	if (cop->control & COPROC_IN_USE)
		return (-EBUSY);
	cop->control |= COPROC_IN_USE;

	/* Now call the platform dependent open stage */
	coproc_cpu_open(cop);
#ifdef CONFIG_COPROCESSOR_DEBUG
	__debug(cop, __FUNCTION__);
#endif
	return 0;
}

static int st_coproc_release(struct inode *inode, struct file *file)
{
	coproc_t *cop = FILE_2_COP(coproc, file);

	coproc_cpu_release(cop);
	cop->control &= ~COPROC_IN_USE;

	return 0;
}

static int st_coproc_ioctl(struct inode *inode, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	coproc_t *cop = FILE_2_COP(coproc, file);
	int res = 0;

	switch (cmd) {
	case STCOP_RESET:
		res = coproc_cpu_reset(cop);
		break;
	case STCOP_GRANT:
		/* Release the Slave CPU from reset (do not wait) */
		res = coproc_cpu_grant(cop, arg);
		break;

/* Peek and poke 32 bit cell:  for debug only
 * ------------------------------------------
 * Not generally available, not documented and not supported.
 * Simple and perhaps not reliable way for writing and reading a 32 bit
 * value using the <stslave> application.
 * PAY ATTENTION: the code doesn't make any validity check hence the use
 *                of a wrong address may have undesired effects!
 */
#define PAIRS(p) (p)[0]		/* Number of couple:  addr./value */
#define PORT(p)  (p)[1]		/* the address (port)             */
#define VALUE(p) (p)[2]		/* the 32 bit value               */

	case STCOP_PEEK:
		{
			u_long peek[3];
			res = -EINVAL;

			/* so far we need to peek only a single 32 bit cell */
			if ((res = copy_from_user(peek, arg, sizeof(peek))) < 0)
				break;
			if (PAIRS(peek) != 1)
				break;

			/* make the addr 32 bit aligned  and peek the value */
			PORT(peek) &= ~0x3;
			VALUE(peek) = peek_l(PORT(peek));

			DPRINTK(">>> %s: %s;  peek[%ld] @0x%08lx = 0x%08lx\n",
				__FUNCTION__, cop->dev.bus_id,
				PAIRS(peek), PORT(peek), VALUE(peek));

			/* don't mind wich data, make it availbale to the user */
			res = copy_to_user((void *)arg, peek, sizeof(peek));
			break;
		}

	case STCOP_POKE:
		{
			u_long poke[3];
			res = -EINVAL;

			/* so far we need to peek only a single 32 bit cell */
			if ((res = copy_from_user(poke, arg, sizeof(poke))) < 0)
				break;
			if (PAIRS(poke) != 1)
				break;

			/* make the addr 32 bit aligned  and poke the value */
			PORT(poke) &= ~0x3;
			poke_l(VALUE(poke), PORT(poke));

			DPRINTK(">>> %s: %s;  poke[%ld] @0x%08lx = 0x%08lx\n",
				__FUNCTION__, cop->dev.bus_id,
				PAIRS(poke), PORT(poke), VALUE(poke));
			res = 0;
			break;
		}

	case STCOP_GET_PROPERTIES:
		{
			cop_properties_t clayout;

			strncpy(clayout.name, cop->dev.bus_id,
				sizeof(clayout.name));
			clayout.flags = cop->control;

			clayout.ram_start = HOST_ADDR(cop, 0);
			clayout.ram_size = cop->ram_size;
			clayout.cp_ram_start = COPR_ADDR(cop, 0);

			res =
			    copy_to_user((void *)arg, &clayout,
					 sizeof(cop_properties_t));
			break;
		}

	case STCOP_SET_PROPERTIES:
		{
			/* Not yet supported! */
			printk(KERN_INFO
			       "%s: setting properties not yet available\n",
			       cop->dev.bus_id);
			res = -ENOSYS;
			break;
		}

	default:
		res = -EINVAL;
	}
	return (res);
}

static ssize_t st_coproc_read(struct file *file, char *buf,
			      size_t count, loff_t * ppos)
{
	coproc_t *cop;
	u_long from;
	ssize_t bytes;
	u_int offset = *ppos;

	cop = FILE_2_COP(coproc, file);

	/*
	 * File position assumes the Coprocessor RAM base addr.
	 * normalaized to 0
	 */
	if (offset >= cop->ram_size)
		return (0);

	from = (u_long) HOST_ADDR(cop, offset);
	bytes = min(count, (cop->ram_size - offset));

	DPRINTK(">>> %s: from 0x%08lx to 0x%08x len 0x%x(%d)\n",
		__FUNCTION__, from, (u_int) buf, bytes, bytes);

	if (copy_to_user(buf, from, bytes))
		return (-EFAULT);

	*ppos += bytes;

	return (bytes);
}

static ssize_t st_coproc_write(struct file *file, const char *buf,
			       size_t count, loff_t * ppos)
{
	coproc_t *cop;
	u_long to;
	ssize_t bytes;
	u_int offset = (u_int) * ppos;

	cop = FILE_2_COP(coproc, file);

	/* File position assumes the RAM base addr. normalaized to 0 */
	if (offset >= (u_long) cop->ram_size)
		return (-EFBIG);

	to = (u_long) HOST_ADDR(cop, offset);
	bytes = min(count, (cop->ram_size - offset));

	DPRINTK(">>> %s: from 0x%08x to 0x%08lx len 0x%x(%d)\n",
		__FUNCTION__, (u_int) buf, to, bytes, bytes);

	if (copy_from_user(to, buf, bytes))
		return (-EFAULT);

	*ppos += bytes;
	return (bytes);
}

static loff_t st_coproc_llseek(struct file *file, loff_t fpos, int orig)
{
	coproc_t *cop;
	u_long offset = fpos;
	u_long base;

	cop = FILE_2_COP(coproc, file);

	/*
	 * fpos  can be a real offset within the coprocessor region or a
	 * direct host address in the region (coming from application in
	 * case of mmap).  This code assumes to be an address if it abs.
	 * value > COPR_ADDR, a normal offset otherwyse.
	 */
	base = HOST_ADDR(cop, 0);

	DPRINTK
	    (">>> %s: seek to: 0x%08lx ->  fpos = offset 0x%lx + base 0x%lx\n",
	     __FUNCTION__, (u_long) fpos, offset, HOST_ADDR(cop, 0));

	switch (orig) {
	case 0:
		file->f_pos = offset;
		break;		/* SEEK_SET */
	case 1:
		file->f_pos += offset;
		break;		/* SEEK_CUR */
	case 2:
		file->f_pos = cop->ram_size + offset;
		break;		/* SEEK_END */
	default:
		return -EINVAL;
	}
/*
 * A little problem with the current (0.6) stslave
 * The current stslave assumes a zero offset
 */
	file->f_pos -= cop->ram_offset;
/*
 * End stslave problem
 */
	if (file->f_pos >= cop->ram_size)
		file->f_pos = cop->ram_size - 1;

	return (file->f_pos);

}

static int st_coproc_mmap(struct file *file, struct vm_area_struct *vma)
{
	coproc_t *cop = FILE_2_COP(coproc, file);

	unsigned int offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned int vsize = vma->vm_end - vma->vm_start;
	unsigned int psize = cop->ram_size - offset;

	DPRINTK(">>> %s: vm_start=0x%lx, vm_end=0x%lx, vm_pgoff=0x%lx\n",
		__FUNCTION__, vma->vm_start, vma->vm_end, vma->vm_pgoff);

	if (vsize > psize)
		return (-ENOSPC);

	/*
	 * Call the remap_pfn_range(...) function to map in user space the
	 * coprocessor memory region. Uses ST40 no cache region
	 */
	vma->vm_flags |= VM_IO;
	/* TODO: double check this. was a call
	 *       to remap_page_range but without the >> PAGE_SHIFT
	 */
	if (remap_pfn_range(vma, vma->vm_start, cop->ram_offset
			    >> PAGE_SHIFT, vsize, vma->vm_page_prot)) {
		DPRINTK(">>> %s: remap_page_range(...) failed\n", __FUNCTION__);
		return (-EAGAIN);
	}

	DPRINTK(">>> %s: ... done: 0x%08lx len 0x%x --> 0x%08lx\n",
		__FUNCTION__, (unsigned long)HOST_ADDR(cop, 0), vsize,
		vma->vm_start);
	return (0);
}

static struct file_operations coproc_fops = {
      llseek:st_coproc_llseek,
      read:st_coproc_read,
      write:st_coproc_write,
      ioctl:st_coproc_ioctl,
      mmap:st_coproc_mmap,
      open:st_coproc_open,
      release:st_coproc_release
};

/* Start: ST-Coprocessor Device Attribute on SysFs*/
static ssize_t st_copro_show_running(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "%d", cop->control & COPROC_IN_USE);
}

static DEVICE_ATTR(running, S_IRUGO, st_copro_show_running, NULL);

static ssize_t st_copro_show_mem_size(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "0x%x", cop->ram_size);
}

static DEVICE_ATTR(mem_size, S_IRUGO, st_copro_show_mem_size, NULL);

static ssize_t st_copro_show_mem_base(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "0x%x", (int)cop->ram_offset);
}

static DEVICE_ATTR(mem_base, S_IRUGO, st_copro_show_mem_base, NULL);
/* End: ST-Coprocessor Device Attribute SysFs*/

static int st_coproc_driver_probe(struct device *dev)
{
	if (!strncmp("st2", dev->bus_id, 3))
		return 1;
	if (!strncmp("lx2", dev->bus_id, 3))
		return 1;
	return 0;
}

static struct device_driver st_coproc_driver = {
	.name = "st-copro",
	.owner = THIS_MODULE,
	.bus = &platform_bus_type,
	.probe = st_coproc_driver_probe,
};

static int __init st_coproc_init(void)
{
	int i;
	coproc_t *cop;
	struct device *dev;

	printk("STMicroelectronics - Coprocessors %s Init\n", coproc_info.name);

	if (driver_register(&st_coproc_driver)) {
		printk(KERN_ERR
		       "Error on ST-Coprocessor device driver registration\n");
		return (-EAGAIN);
	}

	if (register_chrdev(COPROCESSOR_MAJOR, coproc_info.name, &coproc_fops)) {
		printk("Can't allocate major %d for ST Coprocessor Devices\n",
		       COPROCESSOR_MAJOR);
		return (-EAGAIN);
	}

	for (cop = &coproc[0], i = 0; i < coproc_info.max_coprs; i++, cop++) {
		cop->id = i;

		if (!cop->ram_offset) {
			printk("st-coprocessor-%d: No RAM reserved\n", cop->id);
			cop->control &= ~COPROC_SPACE_ALLOCATE;
		} else {
			cop->control |= COPROC_SPACE_ALLOCATE;
			cop->vma_address =
			    ioremap_nocache(cop->ram_offset, cop->ram_size);
		}
		/*
		 ** Nodes:
		 **    STm8000/ST220Eval: /dev/st220-0    c   63   0
		 **                       /dev/st220-1    c   63   1
		 **                       /dev/st220-2    c   63   2
		 **    STb7100          : /dev/st231-0    c   63   0
		 **                    : /dev/st231-1    c   63   1
		 ** if the device file system support is configured the above
		 ** devices are autonatically generated
		 */
		dev = &(cop->dev);
		memset(dev, 0, sizeof(struct device));
		sprintf(cop->dev.bus_id, "%s-%d", coproc_info.name, i);

		dev->driver = &st_coproc_driver;
		dev->parent = &platform_bus;
		dev->bus = &platform_bus_type;

		if (device_register(dev))
			printk(KERN_ERR
			       "Error on ST-Coprocessor device registration\n");
		else {
			/* Add the attributes on the device */
			device_create_file(dev, &dev_attr_mem_base);
			device_create_file(dev, &dev_attr_mem_size);
			device_create_file(dev, &dev_attr_running);
		}

		/* Now complete with the platform dependent init stage */
#if defined(CONFIG_COPROCESSOR_DEBUG) && defined(RPC_DEBUG)
		{
			/* just to debug on STi5528 application using the RPC 2.1,
			 * install a fake interrupt hadler
			 */
			int res;

			cop->irq = EMBX_IRQ + i;
			if ((res = request_irq(cop->irq, &mbx_irq_handle, 0,
					       coproc_info.name, cop)) != 0) {
				printk
				    ("st-coprocessor: Error %d booking IRQ %d for %s\n",
				     res, cop->irq, cop->name);
				cop->irq = 0;
			}
		}
#endif
		__debug(cop, __FUNCTION__);
	}

	proc_st_coproc_init();

	return (0);
}

static void __exit st_coproc_exit(void)
{
	DPRINTK("Release coprocessor module...\n");
}

/*
 * Parse the optional kernel argument:
 *
 * ... coprocessor_mem=size_0@phis_address_0, size_1@phis_address_1
 *
 * It seems to be reasonable to assume that in a "staically partitioned
 * RAM layout", the regions of RAM assigned to each slave processor are
 * not scattered in memory!
 */
static int __init parse_coproc_mem(char *from)
{
	/*
	 * If this proc. is called from point to the next token of
	 * "coprocessor_mem="
	 */
	char *cmdl = (from);	/* start scan from '=' char */
	u_long value = 0;
	int i = 0;
	char *error_msg;
	int ready_for_address = 0;

	DPRINTK(">>> parse_coproc_mem(*from: %11s...)\n", from);

	for (i = 0; *cmdl; cmdl++) {
		value = memparse(cmdl, &cmdl);

		if (ready_for_address) {
			/* the phisical offset must not overlap the code and data kernel */
			/* if so the coprocessor ram isn't (!!!) reserved */
			coproc[i].ram_offset =
			    ((value + PAGE_OFFSET) <= (unsigned long)_end)
			    ? 0 : value;	/* phisical address */
		} else
			coproc[i].ram_size = value;

		switch (*cmdl) {
		case ',':
		case ' ':
		case '\0':
			ready_for_address = 0;
			++i;
			break;
		case '@':
			ready_for_address = 1;
			break;
		default:
			error_msg = "syntax error";
			goto args_error;
		}

	}
	for (i = 0; i < coproc_info.max_coprs; ++i)
		if (coproc[i].ram_offset && coproc[i].ram_size &&
		    ((coproc[i].ram_offset + coproc[i].ram_size) <
		     PHYSADDR(memory_end)))
			reserve_bootmem(coproc[i].ram_offset,
					coproc[i].ram_size);
		else
			coproc[i].ram_offset = coproc[i].ram_size = 0;

	return 0;

      args_error:
	printk(KERN_ERR "Coproc. args: %s. Coproc. ignored!\n", error_msg);
	return -EINVAL;
}

__setup("coprocessor_mem=", parse_coproc_mem);

MODULE_DESCRIPTION("Co-processor manager for multi-core devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.3");
MODULE_LICENSE("GPL");

module_init(st_coproc_init);
module_exit(st_coproc_exit);

#if CONFIG_PROC_FS

static int show_st_coproc(struct seq_file *m, void *v)
{
	int i;
	coproc_t *cop;
	seq_printf(m, "Coprocessors: %d  %s\n",
		   coproc_info.max_coprs, coproc_info.name);
	seq_printf(m,
		   "  CPU (dev)        Host addr.     Copr. addr.     Size\n");
	seq_printf(m,
		   "  -------------------------------------------------------------------\n");
	for (i = 0, cop = &coproc[0]; i < coproc_info.max_coprs; i++, cop++) {
		seq_printf(m, "  /dev/%-8s    ", cop->dev.bus_id);
		if (cop->ram_size == 0)
			seq_printf(m, "not allocated!\n");
		else
			seq_printf(m,
				   "0x%08lx     0x%08lx      0x%08x (%2d Mb)\n",
				   HOST_ADDR(cop, 0), COPR_ADDR(cop, 0),
				   cop->ram_size, (cop->ram_size / MEGA));
	}
	seq_printf(m, "\n");

	coproc_proc_other_info(cop, m);
	return (0);
}

static void *st_coproc_seq_start(struct seq_file *m, loff_t * pos)
{
	return (void *)(*pos == 0);
}

static void *st_coproc_seq_next(struct seq_file *m, void *v, loff_t * pos)
{
	return NULL;
}

static void st_coproc_seq_stop(struct seq_file *m, void *v)
{
}

static struct seq_operations proc_st_coproc_op = {
      start:st_coproc_seq_start,
      next:st_coproc_seq_next,
      stop:st_coproc_seq_stop,
      show:show_st_coproc,
};

static int proc_st_coproc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &proc_st_coproc_op);
}

static struct file_operations proc_st_coproc_operations = {
      open:proc_st_coproc_open,
      read:seq_read,
      llseek:seq_lseek,
      release:seq_release,
};

static int __init proc_st_coproc_init(void)
{
	struct proc_dir_entry *entry;
	entry = create_proc_entry("coprocessor", 0, NULL);
	if (entry != NULL) {
		entry->proc_fops = &proc_st_coproc_operations;
	}

	return 0;
}

#endif				/* CONFIG_PROC_FS */