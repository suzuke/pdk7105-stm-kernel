/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Nunzio Raciti <nunzio.raciti@st.com>
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
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
#include <linux/stm/flashss.h>
#include <linux/clk.h>
#include <linux/of.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif

/* FlashSS TOP registers offset */
#define	TOP_FLASHSS_CONFIG				0x0000
#define	TOP_FLASHSS_CONFIG_HAMMING_NOT_BCH		(0x1 << 0)
#define	TOP_FLASHSS_CONFIG_CFG_EMMC_NOT_EMI		(0x1 << 1)
#define	TOP_FLASHSS_CONFIG_EMMC_BOOT_CLK_DIV_BY_2	(0x1 << 2)

#define	TOP_VSENSE_CONFIG				0x0004
#define	TOP_VSENSE_CONFIG_REG_PSW_EMMC			(0x1 << 0)
#define	TOP_VSENSE_CONFIG_ENB_REG_PSW_EMMC		(0x1 << 1)
#define	TOP_VSENSE_CONFIG_REG_PSW_NAND			(0x1 << 8)
#define	TOP_VSENSE_CONFIG_ENB_REG_PSW_NAND		(0x1 << 9)
#define	TOP_VSENSE_CONFIG_REG_PSW_SPI			(0x1 << 16)
#define	TOP_VSENSE_CONFIG_ENB_REG_PSW_SPI		(0x1 << 17)
#define	TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC		(0x1 << 24)
#define	TOP_VSENSE_CONFIG_LATCHED_PSW_NAND		(0x1 << 25)
#define	TOP_VSENSE_CONFIG_LATCHED_PSW_SPI		(0x1 << 26)

/* eMMC registers */
#define	TOP_EMMC_TX_CLK_DELAY				0x0008
#define	TOP_EMMC_TX_CLK_DELAY_TX_CLK_DELAY		(0xf << 0)

#define	TOP_EMMC_RX_CLK_DELAY				0x000c
#define	TOP_EMMC_RX_CLK_DELAY_TX_CLK_DELAY		(0xf << 0)

#define	TOP_EMMC_RX_DAT_DELAY_A				0x0010
#define	TOP_EMMC_RX_DAT_DELAY_A_RX_DAT0_DELAY		(0xf << 0)
#define	TOP_EMMC_RX_DAT_DELAY_A_RX_DAT1_DELAY		(0xf << 8)
#define	TOP_EMMC_RX_DAT_DELAY_A_RX_DAT2_DELAY		(0xf << 16)
#define	TOP_EMMC_RX_DAT_DELAY_A_RX_DAT3_DELAY		(0xf << 24)

#define	TOP_EMMC_RX_DAT_DELAY_B				0x0014
#define	TOP_EMMC_RX_DAT_DELAY_B_RX_DAT4_DELAY		(0xf << 0)
#define	TOP_EMMC_RX_DAT_DELAY_B_RX_DAT5_DELAY		(0xf << 8)
#define	TOP_EMMC_RX_DAT_DELAY_B_RX_DAT6_DELAY		(0xf << 16)
#define	TOP_EMMC_RX_DAT_DELAY_B_RX_DAT7_DELAY		(0xf << 24)

#define	TOP_EMMC_DELAY_CONTROL				0x0018
#define	TOP_EMMC_DELAY_CONTROL_DLL_BYPASS_CMD		(0x1 << 0)
#define	TOP_EMMC_DELAY_CONTROL_DLL_BYPASS_PH_SEL	(0x1 << 1)
#define	TOP_EMMC_DELAY_CONTROL_TX_DLL_ENABLE		(0x1 << 8)
#define	TOP_EMMC_DELAY_CONTROL_RX_DLL_ENABLE		(0x1 << 9)
#define	TOP_EMMC_DELAY_CONTROL_AUTOTUNE_NOT_CFG_DELAY	(0x1 << 10)
#define	TOP_EMMC_DELAY_CONTROL_UNDOC_BUT_REQUIRED	(0x1 << 11)
#define	TOP_EMMC_DELAY_CONTROL_DLL_BYPASS_CMD_VALUE	(0x3ff << 16)

#define	TOP_EMMC_TX_DLL_STEP_DELAY			0x001c
#define	TOP_EMMC_TX_DLL_STEP_DELAY_TX_DLL_STEP_DELAY	(0xf << 0)
#define	TX_STEP_DEFAULT_DELAY	0x6	/* phase shift delay on the tx clk */

#define	TOP_EMMC_RX_DLL_STEP_DELAY			0x0020
#define	TOP_EMMC_RX_DLL_STEP_DELAY_RX_DLL_STEP_DELAY	(0xf << 0)

#define	TOP_EMMC_RX_CMD_DELAY				0x0024
#define	TOP_EMMC_RX_CMD_DELAY_RX_DLL_STEP_DELAY		(0xf << 0)

#define	FLASHSS_TIMEOUT				(HZ/50)

/* Statistic delay table */
/* 0000: 0.0 ns		*/
/* 0001: 0.3 ns		*/
/* 0010: 0.5 ns		*/
/* 0011: 0.75 ns	*/
/* 0100: 1.0 ns		*/
/* 0101: 1.25 ns	*/
/* 0110: 1.5 ns		*/
/* 0111: 1.75 ns	*/
/* 1000: 2.0 ns		*/
/* 1001: 2.25 ns	*/
/* 1010: 2.5 ns		*/
/* 1011: 2.75 ns	*/
/* 1100: 3.0 ns		*/
/* 1101: 3.25 ns	*/
/* 1110: 3.25 ns	*/
/* 1111: 3.25 ns	*/

static void __iomem *flashss_config;

enum vsense_voltages flashss_get_vsense(enum vsense_devices fdev)
{
	unsigned value = 0, mask = 0;

	BUG_ON(!flashss_config);

	switch (fdev) {
	case VSENSE_DEV_EMMC:
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC;
		break;
	case VSENSE_DEV_NAND:
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_NAND;
		break;
	case VSENSE_DEV_SPI:
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_SPI;
		break;
	default:
		pr_err("flashSS: error: broken device\n");
		return -ENXIO;
	}
	value = readl(flashss_config + TOP_VSENSE_CONFIG);

	if (value & mask)
		return VSENSE_3V3;
	else
		return VSENSE_1V8;

}
EXPORT_SYMBOL_GPL(flashss_get_vsense);

/**
 * flashss_set_vsense: switch to voltage
 * @fdev: device (NAND/SPI/eMMC)
 * @v: voltage
 * Description: this function is to switch among different voltages by using
 * a regulator. For example, when the MMC HC configures the HS200 mode
 * it is mandatory to use 1.8V instead of 3.3V (that can be the default
 * on some SoCs). To do this, the VSENSE register in the FlashSS needs to
 * be programmed. After programming it, we need to verify that the voltage
 * is stable by looking at the latched bits.
 */
int flashss_set_vsense(enum vsense_devices fdev, enum vsense_voltages v)
{
	unsigned value = 0, mask = 0;
	unsigned long curr;
	unsigned long finish = jiffies + FLASHSS_TIMEOUT;

	BUG_ON(!flashss_config);

	if ((v != VSENSE_3V3) || (v != VSENSE_1V8)) {
		pr_err("flashSS: error: unsupported voltage\n");
		return -EINVAL;
	}

	switch (fdev) {
	case VSENSE_DEV_EMMC:
		value = TOP_VSENSE_CONFIG_ENB_REG_PSW_EMMC;
		if (v == VSENSE_3V3)
			value |= TOP_VSENSE_CONFIG_REG_PSW_EMMC;
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_EMMC;
		break;
	case VSENSE_DEV_NAND:
		value = TOP_VSENSE_CONFIG_ENB_REG_PSW_NAND;
		if (v == VSENSE_3V3)
			value |= TOP_VSENSE_CONFIG_REG_PSW_NAND;
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_NAND;
		break;
	case VSENSE_DEV_SPI:
		value = TOP_VSENSE_CONFIG_ENB_REG_PSW_SPI;
		if (v == VSENSE_3V3)
			value |= TOP_VSENSE_CONFIG_REG_PSW_SPI;
		mask = TOP_VSENSE_CONFIG_LATCHED_PSW_SPI;
		break;
	default:
		pr_err("flashSS: error: broken device\n");
		return -ENXIO;
	}

	writel(value, flashss_config + TOP_VSENSE_CONFIG);

	do {
		curr = jiffies;
		value = readl(flashss_config + TOP_VSENSE_CONFIG);
		if (((v == VSENSE_1V8) && !(value & mask)) ||
					 ((v == VSENSE_3V3) && (value & mask)))
			return 0;
		cpu_relax();
	} while (!time_after_eq(curr, finish));

	return -ENXIO;
}
EXPORT_SYMBOL_GPL(flashss_set_vsense);

/**
 * flashss_emmc_set_dll: program DLL
 * Description: this function is to enable the Dynamic Loop used by the eMMC HC
 * to perform the auto-tuning process while switching on super-speed mode.
 */
void flashss_emmc_set_dll(void)
{
	unsigned value;

	BUG_ON(!flashss_config);

	/* Enable DLL, Auto Tuning, Tx DLL */
	value = TOP_EMMC_DELAY_CONTROL_TX_DLL_ENABLE |
		TOP_EMMC_DELAY_CONTROL_AUTOTUNE_NOT_CFG_DELAY |
		TOP_EMMC_DELAY_CONTROL_UNDOC_BUT_REQUIRED;

	writel(value, flashss_config + TOP_EMMC_DELAY_CONTROL);
	writel(TX_STEP_DEFAULT_DELAY, flashss_config +
	       TOP_EMMC_TX_DLL_STEP_DELAY);
}
EXPORT_SYMBOL_GPL(flashss_emmc_set_dll);

/**
 * flashss_nandi_select: program nand controller
 * @controller: nandi_controllers
 * Description: this function, called by the nand driver, is to program
 * HAMMING or BCH NAND controller.
 */
void flashss_nandi_select(enum nandi_controllers controller)
{
	unsigned v;

	BUG_ON(!flashss_config);

	v = readl(flashss_config + TOP_FLASHSS_CONFIG);

	if (controller == STM_NANDI_HAMMING) {
		if (v & TOP_FLASHSS_CONFIG_HAMMING_NOT_BCH)
			return;
		v |= TOP_FLASHSS_CONFIG_HAMMING_NOT_BCH;
	} else {
		if (!(v & TOP_FLASHSS_CONFIG_HAMMING_NOT_BCH))
			return;
		v &= ~TOP_FLASHSS_CONFIG_HAMMING_NOT_BCH;
	}

	writel(v, flashss_config + TOP_FLASHSS_CONFIG);
	readl(flashss_config + TOP_FLASHSS_CONFIG);
}
EXPORT_SYMBOL_GPL(flashss_nandi_select);

#ifdef CONFIG_DEBUG_FS
static struct dentry *flashss_status;

static int flashss_sysfs_read(struct seq_file *seq, void *v)
{
	seq_printf(seq, "FlashSS: TOP Config registers dump:\n");

	seq_printf(seq, "TOP_FLASHSS_CONFIG=0x%08X\n",
			readl(flashss_config + TOP_FLASHSS_CONFIG));
	seq_printf(seq, "TOP_VSENSE_CONFIG=0x%08X\n",
			readl(flashss_config + TOP_VSENSE_CONFIG));
	seq_printf(seq, "TOP_EMMC_TX_CLK_DELAY=0x%08X\n",
			readl(flashss_config + TOP_EMMC_TX_CLK_DELAY));
	seq_printf(seq, "TOP_EMMC_RX_CLK_DELAY=0x%08X\n",
			readl(flashss_config + TOP_EMMC_RX_CLK_DELAY));
	seq_printf(seq, "TOP_EMMC_RX_DAT_DELAY_A=0x%08X\n",
			readl(flashss_config + TOP_EMMC_RX_DAT_DELAY_A));
	seq_printf(seq, "TOP_EMMC_RX_DAT_DELAY_B=0x%08X\n",
			readl(flashss_config + TOP_EMMC_RX_DAT_DELAY_B));
	seq_printf(seq, "TOP_EMMC_DELAY_CONTROL=0x%08X\n",
			readl(flashss_config + TOP_EMMC_DELAY_CONTROL));
	seq_printf(seq, "TOP_EMMC_TX_DLL_STEP_DELAY=0x%08X\n",
			readl(flashss_config + TOP_EMMC_TX_DLL_STEP_DELAY));
	seq_printf(seq, "TOP_EMMC_RX_DLL_STEP_DELAY=0x%08X\n",
			readl(flashss_config + TOP_EMMC_RX_DLL_STEP_DELAY));
	seq_printf(seq, "TOP_EMMC_RX_CMD_DELAY=0x%08X\n",
			readl(flashss_config + TOP_EMMC_RX_CMD_DELAY));

	return 0;
}

static int flashss_sysfs_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, flashss_sysfs_read, inode->i_private);
}

static const struct file_operations flashss_status_fops = {
	.owner = THIS_MODULE,
	.open = flashss_sysfs_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void flashss_init_debug_fs(void)
{
	flashss_status = debugfs_create_file("flashss", S_IRUGO, NULL, NULL,
					     &flashss_status_fops);
	if (!flashss_status || IS_ERR(flashss_status))
		pr_err("FlashSS: cannot create debugFS entry\n");
}

static void flashss_exit_debug_fs(void)
{
	debugfs_remove(flashss_status);
}
#else
#define flashss_init_debug_fs()	do {} while (0)
#define flashss_exit_debug_fs()	do {} while (0)
#endif

/**
 * flashss_driver_probe: get memory resources
 * @pdev: platform pointer
 * Description: this function is to map the memory from the platform;
 * At this stage, the flashSS can only manage TOP configuration registers
 * and eMMC core / phy. Also it inits the debugFS if supported.
 */
static int __devinit flashss_driver_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;

	BUG_ON(flashss_config);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "flashss config");
	if (!res) {
		dev_err(dev, "failed to find resource\n");
		return -ENXIO;
	}

	flashss_config = devm_request_and_ioremap(dev, res);
	if (!flashss_config) {
		dev_err(dev, "failed to remap mem [0x%08x-0x%08x]",
			res->start, res->end);
		return -ENOMEM;
	}

	flashss_init_debug_fs();

	return 0;
}

static int flashss_driver_remove(struct platform_device *pdev)
{
	flashss_exit_debug_fs();
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_flashss_match[] = {
	{
		.compatible = "st,flashss",
	},
	{},
};
MODULE_DEVICE_TABLE(of, stm_flashss_match);
#endif

static struct platform_driver flashss_driver = {
	.driver = {
		.name = "flashss",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_flashss_match),
	},
	.probe = flashss_driver_probe,
	.remove = flashss_driver_remove,
};

static int __init stm_flashss_driver_init(void)
{
	return platform_driver_register(&flashss_driver);
}
postcore_initcall(stm_flashss_driver_init);
