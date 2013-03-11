/* System Trace Module Platform driver.
 *
 * (c) 2012 STMicroelectronics
 *
 * This driver implements the initialization of the Systrace
 * peripheral for B20xx boards. The functionnal driver is
 * part of the MTT infrastructure
 *
 * see Documentation/mtt.txt and systrace.txt for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/mm.h>
#include <mach/hardware.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mtt/mtt.h>
#include <linux/stm/pad.h>
#include <linux/stm/systrace.h>
#include <linux/mtt/mtt.h>
#include <linux/debugfs.h>

MODULE_AUTHOR("Marc Titinger <marc.titinger@st.com>");
MODULE_DESCRIPTION("System Trace Modules driver.");
MODULE_LICENSE("GPL");

#ifndef __raw_writeq
#define __raw_writeq(v, a) \
	(__chk_io_ptr(a), *(volatile uint64_t __force *)(a) = (v))
#endif

static inline void stm_write_buf(void *chan_addr, char *str, unsigned int len)
{
	register char *ptr = str;
	unsigned long long tmp = 0;

	void __iomem *dst = chan_addr;

	while (len > 8) {
		__raw_writeq(*(uint64_t *) ptr, dst);
		ptr += 8;
		len -= 8;
	}

	if (len == 8) {
		__raw_writeq(*(uint64_t *) ptr, (dst + 0x8));
		return;
	};
	memcpy(&tmp, ptr, len);
	__raw_writeq(tmp, (dst + 0x8));
}

/* Called on mtt_open
 * fixup the co->private field to speedup STM access.
 **/
static void *mtt_drv_stm_comp_alloc(const uint32_t comp_id, void *data)
{
	struct stm_plat_systrace_data *drv_data =
	    (struct stm_plat_systrace_data *)data;

	unsigned int ret = (unsigned int)drv_data->base;

	if (comp_id & MTT_COMPID_ST) {
		/* If it is a predefined component,
		 * the private value is a ready to use addres comp_id */
		ret += ((comp_id & MTT_COMPID_CHMSK) << STM_IPv1_CH_SHIFT);
		ret += (comp_id & MTT_COMPID_MAMSK);	/* core offset */
		return (void *)ret;
	}

	/* If we allocated everything, mux-down */
	if (drv_data->last_ch_ker == MTT_CH_LIN_KER_INVALID)
		return (void *)(ret + (MTT_CH_LIN_KMUX << STM_IPv1_CH_SHIFT));

	drv_data->last_ch_ker++;

	/* co->private will be the pre-computed channel offset;
	 * the core ID will be known only at runtime.
	 * */
	return (void *)(ret + (drv_data->last_ch_ker << STM_IPv1_CH_SHIFT));
};

static int mtt_drv_stm_mmap(struct file *filp, struct vm_area_struct *vma,
			    void *data)
{
	struct stm_plat_systrace_data *drv_data =
	    (struct stm_plat_systrace_data *)data;

	u64 off = (u64) (vma->vm_pgoff) << PAGE_SHIFT;
	u64 physical = drv_data->mem_base->start + off;
	u64 vsize = (vma->vm_end - vma->vm_start);
	u64 psize = (drv_data->mem_base->end - drv_data->mem_base->start) - off;

	/* Only map up to the aperture we have. */
	if (vsize > psize)
		vsize = psize;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_ops = NULL;

	mtt_printk(KERN_DEBUG "mtt_drv_stm_mmap: " \
		   "vm_start=%lx, vm_end=%lx, vm_pgoff=%lx, "
		   "physical=%llx (size=%llx)\n",
		   vma->vm_start, vma->vm_end, vma->vm_pgoff, physical, vsize);

	if (remap_pfn_range(vma, vma->vm_start, physical >> PAGE_SHIFT,
			    vsize, vma->vm_page_prot)) {
		printk(KERN_ERR " Failure returning from stm_mmap\n");
		return -EAGAIN;
	}
	return 0;
}

/* Fast write routine, offsets are prefetched in the component's
 * private field.
 */
static void mtt_drv_stm_write(mtt_packet_t *p, int lock)
{
	struct mtt_component_obj *co = (struct mtt_component_obj *)p->comp;

	/* Kptrace and printks have the 'ST' bit set
	 * so, they already did set the core offset in the
	 * component ID.
	 * API calls need to append the core offset.
	 * We store a component ID, that */
	if (unlikely(((unsigned int)(co->private) & MTT_COMPID_ST) == 0))
		co->private = (void *)
		    ((unsigned int)(co->private) +
		     (raw_smp_processor_id() << STM_IPv1_MA_SHIFT));

	/* When lock is DRVLOCK, we already made sure that we have a
	 * dedicated channel and hence we do not need to manage locking here.
	 */
	stm_write_buf(co->private, p->u.buf, p->length);
}

static int systrace_remove(struct platform_device *pdev)
{
	struct stm_plat_systrace_data *drv_data;

	drv_data = (struct stm_plat_systrace_data *)pdev->dev.platform_data;

	if (drv_data->pad_state)
		stm_pad_release(drv_data->pad_state);

	if (drv_data->base)
		iounmap(drv_data->base);

	if (drv_data->regs)
		iounmap(drv_data->regs);

	drv_data->regs = drv_data->base = drv_data->pad_state = 0;

	if (drv_data->private) {
		mtt_unregister_output_driver(drv_data->private);
		kfree(drv_data->private);
	}

	return 0;
}

static int systrace_probe(struct platform_device *pdev)
{
	struct mtt_output_driver *mtt_drv;
	struct stm_plat_systrace_data *drv_data;
	int err = 0;

	drv_data = (struct stm_plat_systrace_data *)pdev->dev.platform_data;

	/* iomap memory ranges
	 */
	drv_data->mem_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!drv_data->mem_base)
		return -EINVAL;

	drv_data->base = ioremap_nocache(drv_data->mem_base->start,
					 drv_data->mem_base->end -
					 drv_data->mem_base->start + 1);
	if (!drv_data->base) {
		err = -ENOMEM;
		goto _failed_exit;
	};

	drv_data->mem_regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!drv_data->mem_regs) {
		err = -EINVAL;
		goto _failed_exit;
	};

	drv_data->regs = ioremap_nocache(drv_data->mem_regs->start,
					 drv_data->mem_regs->end -
					 drv_data->mem_regs->start + 1);
	if (!drv_data->regs) {
		err = -ENOMEM;
		goto _failed_exit;
	};

	/* Claim the Pads */
	drv_data->pad_state = stm_pad_claim(drv_data->pad_config, pdev->name);
	if (!drv_data->pad_state) {
		err = -EIO;
		goto _failed_exit;
	}

	/* 12.5 MHz bitclock */
	__raw_writel(0x00C0, drv_data->regs + STM_IPv1_CR_OFF);

	__raw_writel(0x0000, drv_data->regs + STM_IPv1_MMC_OFF);

	/* Enabling all initiators */
	__raw_writel(0x023d, drv_data->regs + STM_IPv1_TER_OFF);

	printk(KERN_NOTICE "Systrace: driver probed.");

	/* Allocate an MTT output descriptor for this instance */
	mtt_drv = kzalloc(sizeof(struct mtt_output_driver), GFP_KERNEL);
	if (!mtt_drv) {
		err = -EIO;
		goto _failed_exit;
	}

	/* MTT layer output medium declaration structure. */
	mtt_drv->write_func = mtt_drv_stm_write;
	mtt_drv->mmap_func = mtt_drv_stm_mmap;
	mtt_drv->comp_alloc_func = mtt_drv_stm_comp_alloc;
	mtt_drv->guid = MTT_DRV_GUID_STM;
	mtt_drv->private = drv_data;
	mtt_drv->last_error = 0;
	drv_data->last_ch_ker = MTT_CH_LIN_KER_FIRST;
	drv_data->private = mtt_drv;

	/* Register the MTT output device */
	mtt_register_output_driver(mtt_drv);
	return 0;

_failed_exit:
	printk(KERN_NOTICE "Systrace: driver probing failed!");
	systrace_remove(pdev);
	return err;
}

static struct platform_driver systrace_driver = {
	.probe = systrace_probe,
	.remove = systrace_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "stm-systrace"},
};

/* Platform driver init and exit. */
int __init systrace_init(void)
{
	static char banner[] =
	    KERN_INFO "Systrace: platform driver registered.\n";

	printk(banner);

	return platform_driver_register(&systrace_driver);
}

void __exit systrace_exit(void)
{
	platform_driver_unregister(&systrace_driver);
}

module_init(systrace_init);
module_exit(systrace_exit);
