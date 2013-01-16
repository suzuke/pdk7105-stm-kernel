/*
 * videobuf2-bpa2-contig.c - bpa2 integration for videobuf2
 *
 * Copyright (C) 2012 STMicroelectronics Ltd.
 * Copyright (C) 2010 Samsung Electronics
 *
 * Based on code by:
 * Author: Pawel Osciak <pawel@osciak.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

#include <linux/bpa2.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

struct vb2_dc_conf {
	struct device *dev;
};

struct vb2_dc_buf {
	struct vb2_dc_conf *conf;
	struct bpa2_part *part;
	void *vaddr;
	unsigned long paddr;
	unsigned long size;
	struct vm_area_struct *vma;
	atomic_t refcount;
};

static void *vb2_bpa2_contig_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->paddr;
}

static void *vb2_bpa2_contig_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	if (!buf)
		return 0;

	return buf->vaddr;
}

static unsigned int vb2_bpa2_contig_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static void *vb2_bpa2_contig_get_userptr(void *alloc_ctx, unsigned long vaddr,
					 unsigned long size, int write)
{
	struct vb2_dc_buf *buf;
	struct vm_area_struct *vma;

	struct bpa2_part *part;
	dma_addr_t base = 0;
	void *addr;

	int ret;

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = vb2_get_contig_userptr(vaddr, size, &vma, &base);
	if (ret) {
		printk(KERN_ERR
		       "bpa2: failed acquiring VMA for vaddr 0x%08lx\n", vaddr);
		kfree(buf);
		return ERR_PTR(ret);
	}

	part = bpa2_find_part_addr((unsigned long)base, size);
	if (!part) {
		printk(KERN_ERR"bpa2: failed acquiring bpa2 partition\n");
		kfree(buf);
		return ERR_PTR(-EINVAL);
	}

	if (bpa2_low_part(part))
		addr = phys_to_virt((unsigned long)base);
	else {
		addr = ioremap_nocache((unsigned long)base, size);
		if (!addr) {
			printk(KERN_ERR
			       "bpa2: couldn't ioremap() region at 0x%08lx\n",
			       (unsigned long)base);
			kfree(buf);
			return ERR_PTR(-ENOMEM);
		}
	}

	buf->size = size;
	buf->part = part;
	buf->paddr = (unsigned long)base;
	buf->vaddr = addr;
	buf->vma = vma;

	return buf;
}

static void vb2_bpa2_contig_put_userptr(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	if (!buf)
		return;

	BUG_ON(!buf->part);

	if (buf->part && !bpa2_low_part(buf->part))
		iounmap(buf->vaddr);

	vb2_put_vma(buf->vma);
	kfree(buf);
}

const struct vb2_mem_ops vb2_bpa2_contig_memops = {
	.alloc		= NULL,
	.put		= NULL,
	.cookie		= vb2_bpa2_contig_cookie,
	.vaddr		= vb2_bpa2_contig_vaddr,
	.mmap		= NULL,
	.get_userptr	= vb2_bpa2_contig_get_userptr,
	.put_userptr	= vb2_bpa2_contig_put_userptr,
	.num_users	= vb2_bpa2_contig_num_users,
};
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_memops);

void *vb2_bpa2_contig_init_ctx(struct device *dev)
{
	struct vb2_dc_conf *conf;

	conf = kzalloc(sizeof *conf, GFP_KERNEL);
	if (!conf)
		return ERR_PTR(-ENOMEM);

	conf->dev = dev;

	return conf;
}
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_init_ctx);

void vb2_bpa2_contig_cleanup_ctx(void *alloc_ctx)
{
	kfree(alloc_ctx);
}
EXPORT_SYMBOL_GPL(vb2_bpa2_contig_cleanup_ctx);

MODULE_DESCRIPTION("bpa2 integration for videobuf2");
MODULE_AUTHOR("Ilyes Gouta <ilyes.gouta@st.com>");
MODULE_LICENSE("GPL");
