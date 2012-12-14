/*
 *   STMicrolectronics STiH416 SoC audio glue driver
 *
 *   Copyright (c) 2012 STMicroelectronics Limited
 *
 *   Author: Francesco Virlinzi <francesco.virlinzi@st.com>
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
#include <linux/stm/stih416.h>
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
static char *id = "STiH416"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STiH416 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STiH416 audio subsystem card.");


/*
 * Audio initialization
 */

static struct stm_pad_config stm_h416_audio_sysconf_config = {
	.sysconfs_num = 4,
	.sysconfs = (struct stm_pad_sysconf []) {
	/* Select external pcm clock for each channel */
		STM_PAD_SYSCONF(SYSCONF(2519), 8, 11, 0xf),
		/* Set bi-phase idle value */
		STM_PAD_SYSCONF(SYSCONF(2519), 7, 7, 0),
		/* Clear all voip bits for now */
		STM_PAD_SYSCONF(SYSCONF(2519), 2, 5, 0),
		/* Route pcm players */
		STM_PAD_SYSCONF(SYSCONF(2519), 0, 1, 1),
	},
};

#ifdef CONFIG_HIBERNATION
static int snd_stm_stih416_restore(struct device *dev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(dev);

	stm_pad_setup(pad_state);
	return 0;
}

static const struct dev_pm_ops snd_stm_stih416_pm_ops = {
	.thaw		= snd_stm_stih416_restore,
	.restore	= snd_stm_stih416_restore,
};
#else
static const struct dev_pm_ops snd_stm_stih416_pm_ops;
#endif

static int __devinit snd_stm_stih416_probe(struct platform_device *pdev)
{
	int result;
	struct stm_pad_state *pad_state;

	snd_stm_printd(0, "%s()\n", __func__);

	if (!stm_soc_is_stih416()) {
		snd_stm_printe("Unsupported (not STiH416) SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	pad_state = stm_pad_claim(&stm_h416_audio_sysconf_config,
			"Alsa glue logic");
	if (!pad_state) {
		snd_stm_printe("Failed to claim the ALSA glue config PADs\n");
		result = -ENODEV;
		goto error_soc_type;
	}
	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	dev_set_drvdata(&pdev->dev, pad_state);
	return 0;

error_card_register:
	stm_pad_release(pad_state);
error_soc_type:
	return result;
}

static int snd_stm_stih416_remove(struct platform_device *pdev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(&pdev->dev);

	stm_pad_release(pad_state);

	return 0;
}

static struct platform_driver snd_stm_stih416_driver = {
	.driver.name    = "snd_stih416",
	.driver.pm	= &snd_stm_stih416_pm_ops,
	.probe		= snd_stm_stih416_probe,
	.remove		= snd_stm_stih416_remove,
};

static struct platform_device snd_stm_stih416_devices = {
	.name = "snd_stih416",
	.id = -1,
};

static int __init snd_stm_stih416_init(void)
{
	platform_driver_register(&snd_stm_stih416_driver);
	platform_device_register(&snd_stm_stih416_devices);

	return 0;
}

static void __exit snd_stm_stih416_exit(void)
{
	platform_device_unregister(&snd_stm_stih416_devices);
	platform_driver_unregister(&snd_stm_stih416_driver);
}

MODULE_AUTHOR("Francesco Virlinzi <francesco.virlinzi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STiH416 audio glue config driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stih416_init);
module_exit(snd_stm_stih416_exit);
