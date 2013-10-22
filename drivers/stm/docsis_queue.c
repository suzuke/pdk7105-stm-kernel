/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This is a driver used for configuring on some ST platforms the Upstream
 * and Downstream queues before opening the Virtual Interface with ISVE
 * driver.
 * Currently it only enables the Downstream queue but it can be extended
 * to support further settings.
 * It is a platform driver and needs to have some platform fields: it also
 * supports DT model.
 * This driver can be used to expose the queue registers via debugFS support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <linux/stm/device.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/bootmem.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

#define DOCSIS_QUEUE_NUMBER	4

struct stm_docsis_priv {
	void __iomem *ioaddr_dfwd;
	void __iomem *ioaddr_upiim;
	int *enabled_queue;
	unsigned int queue_n;
};

struct stm_docsis_priv *priv;

#define DOWNSTREAM_REG_N	10
#define UPSTREAM_REG_N		12

#define DSFWD_ROUTE_MASK		0x18
#define DSFWD_ROUTE_MASK_ROUTE_MASK	0x0000FFFF
#define USIIM_ARBITRATE_OFFSET		0x2c

#define DSFWD_ENABLE_QUEUE(q)	((1 << q) & DSFWD_ROUTE_MASK_ROUTE_MASK)

#ifdef CONFIG_DEBUG_FS
static struct dentry *docsis_queue_status;

static int docsis_queue_sysfs_read(struct seq_file *seq, void *v)
{
	int i;

	seq_printf(seq, "Forwarding block registers:\n");
	for (i = 0; i < DOWNSTREAM_REG_N; i++)
		seq_printf(seq, "0x%x = 0x%08X\n", i * 0x4,
			   readl(priv->ioaddr_dfwd + i * 0x4));

	seq_printf(seq, "\nUpstream block registers:\n");
	for (i = 0; i < UPSTREAM_REG_N; i++)
		seq_printf(seq, "0x%x = 0x%08X\n", i * 0x4,
			   readl(priv->ioaddr_upiim + i * 0x4));

	return 0;
}

static int docsis_queue_sysfs_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, docsis_queue_sysfs_read, inode->i_private);
}

static const struct file_operations docsis_queue_status_fops = {
	.owner = THIS_MODULE,
	.open = docsis_queue_sysfs_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void docsis_queue_init_debug_fs(void)
{
	docsis_queue_status =
	    debugfs_create_file("docsis_queue", S_IRUGO, NULL, NULL,
				&docsis_queue_status_fops);
	if (!docsis_queue_status || IS_ERR(docsis_queue_status))
		pr_err("docsis core: cannot create debugFS entry\n");
}

static void docsis_queue_exit_debug_fs(void)
{
	debugfs_remove(docsis_queue_status);
}
#else
#define docsis_queue_init_debug_fs()	do {} while (0)
#define docsis_queue_exit_debug_fs()	do {} while (0)
#endif

static void docsis_queue_enable_downstream_queue(void)
{
	int i;
	u32 value = 0;

	value = readl(priv->ioaddr_dfwd + DSFWD_ROUTE_MASK);

	for (i = 0; i < priv->queue_n; i++) {
		int q = priv->enabled_queue[i];
		if (q >= 0)
			value |= DSFWD_ENABLE_QUEUE(q);
	}
	writel(value, priv->ioaddr_dfwd + DSFWD_ROUTE_MASK);
}

static void docsis_queue_enable_upstream_queue(void)
{
	int i;
	u32 value, newvalue;

	/* This address can also be written by another CPU. Since both CPUs
	 * only enable bits we can loop until our changes are accepted.
	 */
	do {
		value = readl(priv->ioaddr_upiim + USIIM_ARBITRATE_OFFSET);

		for (i = 0; i < priv->queue_n; i++) {
			int q = priv->enabled_queue[i];
			if (q >= 0)
				value |= (1 << q);
		}
		writel(value, priv->ioaddr_upiim + USIIM_ARBITRATE_OFFSET);
		newvalue = readl(priv->ioaddr_upiim + USIIM_ARBITRATE_OFFSET);
	} while (newvalue != value);
}

static void docsis_get_queue_dt(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int count = 0;
	const __be32 *ip;

	ip = of_get_property(np, "#docsis-queue-number", NULL);
	if (ip)
		count = be32_to_cpup(ip);

	/* Get the queues that have to be enabled */
	priv->queue_n = count;

	priv->enabled_queue = kmalloc_array(count, sizeof(unsigned int *),
					    GFP_KERNEL);

	of_property_read_u32_array(np, "docsis,enabled_queue",
				   (u32 *) priv->enabled_queue, count);
}

static int __devinit docsis_queue_driver_probe(struct platform_device *pdev)
{
	struct resource *res, *res1;
	struct device *dev = &pdev->dev;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Unable to allocate platform data\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	dev_info(dev, "get dfwd resource: res->start 0x%x\n", res->start);
	priv->ioaddr_dfwd = devm_request_and_ioremap(dev, res);
	if (!priv->ioaddr_dfwd) {
		dev_err(dev, ": ERROR: dfwd memory mapping failed");
		release_mem_region(res->start, resource_size(res));
		return -ENOMEM;
	}

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1)
		return -ENODEV;

	dev_info(dev, "get upiim resource: res->start 0x%x\n", res1->start);
	priv->ioaddr_upiim = devm_request_and_ioremap(dev, res1);
	if (!priv->ioaddr_upiim) {
		dev_err(dev, "ERROR: upiim mem mapping failed\n");
		release_mem_region(res->start, resource_size(res));
		release_mem_region(res1->start, resource_size(res1));
		return -ENOMEM;
	}

	priv->enabled_queue = NULL;

	if (dev->of_node)
		docsis_get_queue_dt(pdev);

	/* Enable the Downstream queue passed from the platform */
	if (priv->enabled_queue) {
		docsis_queue_enable_downstream_queue();
		docsis_queue_enable_upstream_queue();
	} else
		dev_err(dev, "failed to enable queue\n");

	docsis_queue_init_debug_fs();

	return 0;
}

static int docsis_queue_driver_remove(struct platform_device *pdev)
{
	docsis_queue_exit_debug_fs();
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_docsis_queue_match[] = {
	{
	 .compatible = "st,docsis",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, stm_docsis_queue_match);
#endif

static struct platform_driver docsis_queue_driver = {
	.driver = {
		   .name = "docsis_queue",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stm_docsis_queue_match),
		   },
	.probe = docsis_queue_driver_probe,
	.remove = docsis_queue_driver_remove,
};

module_platform_driver(docsis_queue_driver);
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("STM Docsis Queue driver");
MODULE_LICENSE("GPL");
