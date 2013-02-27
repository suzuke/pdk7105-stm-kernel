/*
 *   STMicrolectronics STx7108 SoC audio glue driver
 *
 *   Copyright (c) 2010-2013 STMicroelectronics Limited
 *
 *   Authors: John Boddie <john.boddie@st.com>
 *            Pawel Moll <pawel.moll@st.com>
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
#include <linux/stm/stx7108.h>
#include <linux/stm/platform.h>
#include <linux/stm/soc.h>
#include <sound/core.h>

#include "common.h"


/*
 * Audio pad configuration.
 */

static struct stm_pad_config snd_stm_stx7108_pad_config = {
	.sysconfs_num = 1,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* Router player #2 to ext dac (pio) */
		___STM_PAD_SYSCONF(SYS_CFG_BANK4, 43, 0, 1, 2, NULL),
	},
};


/*
 * Power management.
 */

#ifdef CONFIG_HIBERNATION
static int snd_stm_stx7108_restore(struct device *dev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(dev);

	dev_dbg(dev, "%s()", __func__);

	stm_pad_setup(pad_state);

	return 0;
}

static const struct dev_pm_ops snd_stm_stx7108_pm_ops = {
	.thaw		= snd_stm_stx7108_restore,
	.restore	= snd_stm_stx7108_restore,
};
#else
static const struct dev_pm_ops snd_stm_stx7108_pm_ops;
#endif


/*
 * Driver functions.
 */

static int __devinit snd_stm_stx7108_probe(struct platform_device *pdev)
{
	int result;
	struct stm_pad_state *pad_state;

	dev_dbg(&pdev->dev, "%s()", __func__);

	if (!stm_soc_is_stx7108()) {
		dev_err(&pdev->dev, "Unsupported (not STx7108) SOC detected!");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Claim all of the sysconf fields and initialise */
	pad_state = stm_pad_claim(&snd_stm_stx7108_pad_config,
			"ALSA Glue Logic");
	if (!pad_state) {
		dev_err(&pdev->dev, "Failed to claim ALSA sysconf pads");
		return -EBUSY;
		goto error_pad_claim;
	}

	/* Register the sound card */
	result = snd_stm_card_register(SND_STM_CARD_TYPE_AUDIO);
	if (result) {
		dev_err(&pdev->dev, "Failed to register ALSA audio card!");
		goto error_card_register;
	}

	/* Save the pad state as driver data */
	dev_set_drvdata(&pdev->dev, pad_state);

	return 0;

error_card_register:
	stm_pad_release(pad_state);
error_pad_claim:
error_soc_type:
	return result;
}

static int __devexit snd_stm_stx7108_remove(struct platform_device *pdev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s()", __func__);

	stm_pad_release(pad_state);

	return 0;
}

static struct platform_driver snd_stm_stx7108_driver = {
	.driver.name	= "snd_stx7108",
	.driver.pm	= &snd_stm_stx7108_pm_ops,
	.probe		= snd_stm_stx7108_probe,
	.remove		= snd_stm_stx7108_remove,
};


/*
 * Module initialistaion.
 */

static struct platform_device snd_stm_stx7108_devices = {
	.name = "snd_stx7108",
	.id = -1,
};

static int __init snd_stm_stx7108_init(void)
{
	int result;

	/* Add the Stx7108 audio glue platform device */
	result = platform_device_register(&snd_stm_stx7108_devices);
	BUG_ON(result);

	/* Register the platform driver */
	result = platform_driver_register(&snd_stm_stx7108_driver);
	BUG_ON(result);

	return 0;
}

static void __exit snd_stm_stx7108_exit(void)
{
	platform_device_unregister(&snd_stm_stx7108_devices);
	platform_driver_unregister(&snd_stm_stx7108_driver);
}


MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7108 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7108_init);
module_exit(snd_stm_stx7108_exit);
