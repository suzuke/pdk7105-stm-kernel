/*
 *   STMicrolectronics STiH415 SoC audio glue driver
 *
 *   Copyright (c) 2010-2011 STMicroelectronics Limited
 *
 *   Author: Sevanand Singh <sevanand.singh@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stih415.h>
#include <linux/stm/platform.h>
#include <linux/stm/soc.h>
#include <sound/core.h>

#include "common.h"


static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);


/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STiH415"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STiH415 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STiH415 audio subsystem card.");


/*
 * Audio initialization
 */

static struct sysconf_field *snd_stm_stih415_pcm_clk_sel;
static struct sysconf_field *snd_stm_stih415_biphase_idle_value;
static struct sysconf_field *snd_stm_stih415_voip;
static struct sysconf_field *snd_stm_stih415_pcmp_valid_sel;
static struct snd_info_entry *snd_stm_stih415_proc_entry;

static void snd_stm_stih415_procfs(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	snd_iprintf(buffer, "pcm_clk_sel        = %08lx\n",
			sysconf_read(snd_stm_stih415_pcm_clk_sel));
	snd_iprintf(buffer, "biphase_idle_value = %08lx\n",
			sysconf_read(snd_stm_stih415_biphase_idle_value));
	snd_iprintf(buffer, "voip               = %08lx\n",
			sysconf_read(snd_stm_stih415_voip));
	snd_iprintf(buffer, "pcmp_valid_sel     = %08lx\n",
			sysconf_read(snd_stm_stih415_pcmp_valid_sel));
}

static void snd_stm_stih415_setup(void)
{
	/* Select external pcm clock for each channel */
	sysconf_write(snd_stm_stih415_pcm_clk_sel, 0xf);
	/* Set bi-phase idle value */
	sysconf_write(snd_stm_stih415_biphase_idle_value, 0);
	/* Clear all voip bits for now */
	sysconf_write(snd_stm_stih415_voip, 0);
	/* Route pcm players */
	sysconf_write(snd_stm_stih415_pcmp_valid_sel, 1);
}

static int __devinit snd_stm_stih415_probe(struct platform_device *pdev)
{
	int result;

	dev_dbg(&pdev->dev, "%s()", __func__);

	if (!stm_soc_is_stih415()) {
		dev_err(&pdev->dev, "Unsupported (not STiH415) SOC detected!");
		return -EINVAL;
	}

	/* Claim external pcm clock sysconf */
	snd_stm_stih415_pcm_clk_sel = sysconf_claim(SYSCONF(331), 8, 11,
						       "PCM_CLK_SEL");
	if (!snd_stm_stih415_pcm_clk_sel) {
		dev_err(&pdev->dev, "Failed to claim PCM_CLK_SEL");
		return -EBUSY;
	}

	/* Claim bi-phase idle value sysconf */
	snd_stm_stih415_biphase_idle_value = sysconf_claim(SYSCONF(331), 7, 7,
						       "BIPHASE_IDLE_VALUE");
	if (!snd_stm_stih415_biphase_idle_value) {
		dev_err(&pdev->dev, "Failed to claim BIPHASE_IDLE_VALUE");
		result = -EBUSY;
		goto error_sysconf_claim_biphase_idle_value;
	}

	/* Claim voip sysconf */
	snd_stm_stih415_voip = sysconf_claim(SYSCONF(331), 2, 5, "VOIP");
	if (!snd_stm_stih415_voip) {
		dev_err(&pdev->dev, "Failed to claim VOIP");
		result = -EBUSY;
		goto error_sysconf_claim_voip;
	}

	/* Claim pcm player routing sysconf */
	snd_stm_stih415_pcmp_valid_sel = sysconf_claim(SYSCONF(331), 0, 1,
						       "PCMP_VALID_SEL");
	if (!snd_stm_stih415_pcmp_valid_sel) {
		dev_err(&pdev->dev, "Failed to claim PCMP_VALID_SEL");
		result = -EBUSY;
		goto error_sysconf_claim_pcmp_valid_sel;
	}

	/* Set the sysconf values */
	snd_stm_stih415_setup();

	/* Register the sound card */
	result = snd_stm_card_register();
	if (result) {
		dev_err(&pdev->dev, "Failed to register ALSA cards!");
		goto error_card_register;
	}

	/* Register a procfs file */
	result = snd_stm_info_register(&snd_stm_stih415_proc_entry,
			dev_name(&pdev->dev), snd_stm_stih415_procfs, NULL);
	if (result) {
		dev_err(&pdev->dev, "Failed to register with procfs");
		goto error_info_register;
	}

	return 0;

error_info_register:
error_card_register:
	sysconf_release(snd_stm_stih415_pcmp_valid_sel);
error_sysconf_claim_pcmp_valid_sel:
	sysconf_release(snd_stm_stih415_voip);
error_sysconf_claim_voip:
	sysconf_release(snd_stm_stih415_biphase_idle_value);
error_sysconf_claim_biphase_idle_value:
	sysconf_release(snd_stm_stih415_pcm_clk_sel);
	return result;
}

static int __devexit snd_stm_stih415_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s()", __func__);

	sysconf_release(snd_stm_stih415_pcmp_valid_sel);
	sysconf_release(snd_stm_stih415_voip);
	sysconf_release(snd_stm_stih415_biphase_idle_value);
	sysconf_release(snd_stm_stih415_pcm_clk_sel);

	return 0;
}


#ifdef CONFIG_PM
static int snd_stm_stih415_suspend(struct device *dev)
{
	dev_dbg(dev, "%s()", __func__);
	return 0;
}

static int snd_stm_stih415_resume(struct device *dev)
{
	dev_dbg(dev, "%s()", __func__);

	/* Re-write the sysconf registers */
	snd_stm_stih415_setup();

	return 0;
}

static const struct dev_pm_ops snd_stm_stih415_pm_ops = {
	.suspend = snd_stm_stih415_suspend,
	.resume	 = snd_stm_stih415_resume,
	.freeze	 = snd_stm_stih415_suspend,
	.restore = snd_stm_stih415_resume,
};
#endif


static struct platform_driver snd_stm_stih415_driver = {
	.driver.name	= "snd_stih415",
#ifdef CONFIG_PM
	.driver.pm	= &snd_stm_stih415_pm_ops,
#endif
	.probe		= snd_stm_stih415_probe,
	.remove		= snd_stm_stih415_remove,
};


/*
 * Module initialistaion.
 */

static struct platform_device *snd_stm_stih415_devices[] __initdata = {
	&(struct platform_device) {
		.name = "snd_stih415",
		.id = -1,
	},
};

static int __init snd_stm_stih415_init(void)
{
	int result;

	/* Add the STiH415 audio glue platform device */
	result = platform_add_devices(snd_stm_stih415_devices, 1);
	BUG_ON(result);

	/* Register the platform driver */
	return platform_driver_register(&snd_stm_stih415_driver);
}

static void __exit snd_stm_stih415_exit(void)
{
	platform_driver_unregister(&snd_stm_stih415_driver);
}


MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STiH415 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stih415_init);
module_exit(snd_stm_stih415_exit);
