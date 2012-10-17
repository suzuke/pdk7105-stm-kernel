/*
 * STMicroelectronics FDMA dmaengine driver debug functions
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>

#include "stm_fdma.h"


/*
 * Data
 */

static struct dentry *stm_fdma_debugfs_root;


/*
 * Debugfs register file functions
 */

static int stm_fdma_debugfs_regs_show(struct seq_file *m, void *v)
{
	struct stm_fdma_device *fdev = m->private;
	int i;

	seq_printf(m, "ID\t\t0x%08x\n", readl(fdev->io_base + fdev->regs.id));
	seq_printf(m, "VER\t\t0x%08x\n", readl(fdev->io_base + fdev->regs.ver));
	seq_printf(m, "EN\t\t0x%08x\n", readl(fdev->io_base + fdev->regs.en));
	seq_printf(m, "CLK_GATE\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.clk_gate));
	seq_printf(m, "STBUS_SYNC\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.sync_reg));
	seq_printf(m, "REV_ID\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.rev_id));

	for (i = 1; i < 31; ++i)
		seq_printf(m, "REQ_CONTROL[%d]\t0x%08x\n", i,
			   readl(REQ_CONTROLn_REG(fdev, i)));

	seq_printf(m, "CMD_STA\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.cmd_sta));
	seq_printf(m, "CMD_SET\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.cmd_set));
	seq_printf(m, "CMD_CLR\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.cmd_clr));
	seq_printf(m, "CMD_MASK\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.cmd_mask));

	seq_printf(m, "INT_STA\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.int_sta));
	seq_printf(m, "INT_SET\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.int_set));
	seq_printf(m, "INT_CLR\t\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.int_clr));
	seq_printf(m, "INT_MASK\t0x%08x\n",
			readl(fdev->io_base + fdev->regs.int_mask));

	return 0;
}

static int stm_fdma_debugfs_regs_open(struct inode *inode, struct file *file)
{
	return single_open(file, stm_fdma_debugfs_regs_show, inode->i_private);
}

static const struct file_operations stm_fdma_debugfs_regs_fops = {
	.open		= stm_fdma_debugfs_regs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 * Debugfs channel file functions
 */

static int stm_fdma_debugfs_chan_show(struct seq_file *m, void *v)
{
	struct stm_fdma_chan *fchan = m->private;

	seq_printf(m, "CMD_STAT\t0x%08x\n", readl(CMD_STAT_REG(fchan)));
	seq_printf(m, "PTR\t\t0x%08x\n", readl(NODE_PTR_REG(fchan)));
	seq_printf(m, "COUNT\t\t0x%08x\n", readl(NODE_COUNT_REG(fchan)));
	seq_printf(m, "SADDR\t\t0x%08x\n", readl(NODE_SADDR_REG(fchan)));
	seq_printf(m, "DADDR\t\t0x%08x\n", readl(NODE_DADDR_REG(fchan)));

	return 0;
}

static int stm_fdma_debugfs_chan_open(struct inode *inode, struct file *file)
{
	return single_open(file, stm_fdma_debugfs_chan_show, inode->i_private);
}

static const struct file_operations stm_fdma_debugfs_chan_fops = {
	.open		= stm_fdma_debugfs_chan_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};


/*
 * Device register/unregister and initialise/shutdown
 */

void stm_fdma_debugfs_register(struct stm_fdma_device *fdev)
{
	int c;
	char name[16];

	/* Create a debugfs directory for this device  */
	fdev->debug_dir = debugfs_create_dir(dev_name(&fdev->pdev->dev),
					     stm_fdma_debugfs_root);
	if (!fdev->debug_dir)
		goto error_create_dir;

	/* Create entry for */
	fdev->debug_regs = debugfs_create_file("registers", S_IRUGO,
					fdev->debug_dir, fdev,
					&stm_fdma_debugfs_regs_fops);
	if (!fdev->debug_regs)
		goto error_regs_file;

	/* Create a debugfs entry for each channel */
	for (c = STM_FDMA_MIN_CHANNEL; c <= STM_FDMA_MAX_CHANNEL; c++) {
		snprintf(name, sizeof(name), "channel%d", c);

		fdev->debug_chans[c] = debugfs_create_file(name, S_IRUGO,
				fdev->debug_dir, &fdev->ch_list[c],
				&stm_fdma_debugfs_chan_fops);
		if (fdev->debug_chans[c] == NULL)
			goto error_chan_file;
	}

	return;

error_chan_file:
	/* Remove the channel file entries */
	for (c = STM_FDMA_MIN_CHANNEL; c <= STM_FDMA_MAX_CHANNEL; c++) {
		if (fdev->debug_chans[c]) {
			debugfs_remove(fdev->debug_chans[c]);
			fdev->debug_chans[c] = NULL;
		}
	}
	/* Remove the registers file entry */
	debugfs_remove(fdev->debug_regs);
	fdev->debug_regs = NULL;
error_regs_file:
	/* Remove the directory entry */
	debugfs_remove(fdev->debug_dir);
	fdev->debug_dir = NULL;
error_create_dir:
	return;
}

void stm_fdma_debugfs_unregister(struct stm_fdma_device *fdev)
{
	int c;

	/* Remove the debugfs entry for each channel */
	for (c = STM_FDMA_MIN_CHANNEL; c <= STM_FDMA_MAX_CHANNEL; c++)
		debugfs_remove(fdev->debug_chans[c]);

	debugfs_remove(fdev->debug_regs);
	debugfs_remove(fdev->debug_dir);
}


void stm_fdma_debugfs_init(void)
{
	/* Create the root directory in debugfs */
	stm_fdma_debugfs_root = debugfs_create_dir("fdma", NULL);
}

void stm_fdma_debugfs_exit(void)
{
	debugfs_remove(stm_fdma_debugfs_root);
}
