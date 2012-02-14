/*
 *   STMicroelectronics System-on-Chips' Uniperipheral bi-phase driver
 *
 *   Copyright (c) 2011 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
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
#include <linux/platform_device.h>
#include <linux/stm/sysconf.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Hardware related definitions
 */

#define FORMAT (SND_STM_FORMAT__SPDIF | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 128



/*
 * Internal instance structure
 */

struct snd_stm_conv_biphase {
	/* System information */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;

	/* Enable bit */
	struct sysconf_field *enable;

	snd_stm_magic_field;
};



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_biphase_get_format(void *priv)
{
	snd_stm_printd(1, "%s(priv=%p)\n", __func__, priv);

	return FORMAT;
}

static int snd_stm_conv_biphase_get_oversampling(void *priv)
{
	snd_stm_printd(1, "%s(priv=%p)\n", __func__, priv);

	return OVERSAMPLING;
}

static int snd_stm_conv_biphase_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_biphase *conv_biphase = priv;

	snd_stm_printd(1, "%s(enabled=%d, priv=%p)\n", __func__, enabled, priv);

	BUG_ON(!conv_biphase);
	BUG_ON(!snd_stm_magic_valid(conv_biphase));

	snd_stm_printd(1, "%sabling bi-phase formatter for %s\n",
			enabled ? "En" : "Dis", conv_biphase->bus_id);

	if (enabled)
		sysconf_write(conv_biphase->enable, 1);
	else
		sysconf_write(conv_biphase->enable, 0);

	return 0;
}

static int snd_stm_conv_biphase_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_biphase *conv_biphase = priv;

	snd_stm_printd(1, "%s(muted=%d, priv=%p)\n", __func__, muted, priv);

	BUG_ON(!conv_biphase);
	BUG_ON(!snd_stm_magic_valid(conv_biphase));

	/* The bi-phase formatter does not have mute functionality */

	return 0;
}

static struct snd_stm_conv_ops snd_stm_conv_biphase_ops = {
	.get_format = snd_stm_conv_biphase_get_format,
	.get_oversampling = snd_stm_conv_biphase_get_oversampling,
	.set_enabled = snd_stm_conv_biphase_set_enabled,
	.set_muted = snd_stm_conv_biphase_set_muted,
};



/*
 * ALSA lowlevel device implementation
 */

static int snd_stm_conv_biphase_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_biphase *conv_biphase = snd_device->device_data;

	BUG_ON(!conv_biphase);
	BUG_ON(!snd_stm_magic_valid(conv_biphase));

	/* Initialise bi-phase formatter to disabled */
	sysconf_write(conv_biphase->enable, 0);

	return 0;
}

static int snd_stm_conv_biphase_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_biphase *conv_biphase = snd_device->device_data;

	BUG_ON(!conv_biphase);
	BUG_ON(!snd_stm_magic_valid(conv_biphase));

	/* Set bi-phase formatter to disabled */
	sysconf_write(conv_biphase->enable, 0);

	return 0;
}

static struct snd_device_ops snd_stm_conv_biphase_snd_device_ops = {
	.dev_register = snd_stm_conv_biphase_register,
	.dev_disconnect = snd_stm_conv_biphase_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_biphase_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_biphase_info *info = pdev->dev.platform_data;
	struct snd_stm_conv_biphase *conv_biphase;
	struct snd_card *card = snd_stm_card_get();

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);
	BUG_ON(!info);

	conv_biphase = kzalloc(sizeof(*conv_biphase), GFP_KERNEL);
	if (!conv_biphase) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_biphase);
	conv_biphase->bus_id = dev_name(&pdev->dev);

	/* Get resources */

	conv_biphase->enable = sysconf_claim(info->enable.group,
			info->enable.num, info->enable.lsb, 
			info->enable.msb, "BIPHASE_ENABLE");
	BUG_ON(!conv_biphase->enable);

	/* Get connections */

	BUG_ON(!info->source_bus_id);
	snd_stm_printd(0, "The bi-phase formatter is attached to uniperipheral "
		       "player '%s'.\n", info->source_bus_id);

	conv_biphase->converter = snd_stm_conv_register_converter(
			"Bi-phase formatter", &snd_stm_conv_biphase_ops,
			conv_biphase, &platform_bus_type, info->source_bus_id,
			info->channel_from, info->channel_to, NULL);

	if (!conv_biphase->converter) {
		snd_stm_printe("Can't attach to uniperipheral player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device */

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_biphase,
			&snd_stm_conv_biphase_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Finished */

	platform_set_drvdata(pdev, conv_biphase);

	return 0;

error_device:
error_attach:
	snd_stm_magic_clear(conv_biphase);
	kfree(conv_biphase);
error_alloc:
	return result;
}

static int snd_stm_conv_biphase_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_biphase *conv_biphase = platform_get_drvdata(pdev);

	BUG_ON(!conv_biphase);
	BUG_ON(!snd_stm_magic_valid(conv_biphase));

	snd_stm_conv_unregister_converter(conv_biphase->converter);

	sysconf_release(conv_biphase->enable);

	snd_stm_magic_clear(conv_biphase);
	kfree(conv_biphase);

	return 0;
}

static struct platform_driver snd_stm_conv_biphase_driver = {
	.driver.name = "snd_conv_biphase",
	.probe = snd_stm_conv_biphase_probe,
	.remove = snd_stm_conv_biphase_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_biphase_init(void)
{
	return platform_driver_register(&snd_stm_conv_biphase_driver);
}

static void __exit snd_stm_conv_biphase_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_biphase_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uniperipheral bi-phase driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_biphase_init);
module_exit(snd_stm_conv_biphase_exit);
