/*
 *   STMicroelectronics System-on-Chips' internal (sysconf controlled)
 *   audio DAC driver
 *
 *   Copyright (c) 2005-2011 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
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
#include <linux/io.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/stm/sysconf.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Hardware-related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 256



/*
 * Internal DAC instance structure
 */

struct snd_stm_conv_dac_sc {
	/* System informations */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;

	/* Control bits */
	struct sysconf_field *nrst;
	struct sysconf_field *mode;
	struct sysconf_field *nsb;
	struct sysconf_field *sb;
	struct sysconf_field *softmute;
	struct sysconf_field *mute_l;
	struct sysconf_field *mute_r;
	struct sysconf_field *pdana;
	struct sysconf_field *pndbg;

	snd_stm_magic_field;
};

#define FIELD_EMPTY(f) \
	((f.group == 0) && (f.num == 0) && (f.lsb == 0) && (f.msb == 0))



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_dac_sc_get_format(void *priv)
{
	snd_stm_printd(1, "%s(priv=%p)\n", __func__, priv);

	return FORMAT;
}

static int snd_stm_conv_dac_sc_get_oversampling(void *priv)
{
	snd_stm_printd(1, "%s(priv=%p)\n", __func__, priv);

	return OVERSAMPLING;
}

static int snd_stm_conv_dac_sc_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = priv;

	snd_stm_printd(1, "%s(enabled=%d, priv=%p)\n", __func__, enabled, priv);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_printd(1, "%sabling DAC %s's digital part.\n",
			enabled ? "En" : "Dis", conv_dac_sc->bus_id);

	if (enabled) {
		/* Take the DAC out of standby */
		if (conv_dac_sc->nsb)
			sysconf_write(conv_dac_sc->nsb, 1);
		if (conv_dac_sc->sb)
			sysconf_write(conv_dac_sc->sb, 0);

		/* Take the DAC out of reset */
		sysconf_write(conv_dac_sc->nrst, 1);
	} else {
		/* Put the DAC into reset */
		sysconf_write(conv_dac_sc->nrst, 0);

		/* Put the DAC into standby */
		if (conv_dac_sc->nsb)
			sysconf_write(conv_dac_sc->nsb, 0);
		if (conv_dac_sc->sb)
			sysconf_write(conv_dac_sc->sb, 1);
	}

	return 0;
}

static int snd_stm_conv_dac_sc_set_muted(int muted, void *priv)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = priv;

	snd_stm_printd(1, "%s(muted=%d, priv=%p)\n", __func__, muted, priv);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_printd(1, "%suting DAC %s.\n", muted ? "M" : "Unm",
			conv_dac_sc->bus_id);

	if (conv_dac_sc->softmute)
		sysconf_write(conv_dac_sc->softmute, muted ? 1 : 0);
	if (conv_dac_sc->mute_l)
		sysconf_write(conv_dac_sc->mute_l, muted ? 1 : 0);
	if (conv_dac_sc->mute_r)
		sysconf_write(conv_dac_sc->mute_r, muted ? 1 : 0);

	return 0;
}

static struct snd_stm_conv_ops snd_stm_conv_dac_sc_ops = {
	.get_format = snd_stm_conv_dac_sc_get_format,
	.get_oversampling = snd_stm_conv_dac_sc_get_oversampling,
	.set_enabled = snd_stm_conv_dac_sc_set_enabled,
	.set_muted = snd_stm_conv_dac_sc_set_muted,
};



/*
 * ALSA lowlevel device implementation
 */

static int snd_stm_conv_dac_sc_register(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc =
			snd_device->device_data;

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	/* Put the DAC into reset */
	sysconf_write(conv_dac_sc->nrst, 0);

	/* Put the DAC into standby */
	if (conv_dac_sc->nsb)
		sysconf_write(conv_dac_sc->nsb, 0);
	if (conv_dac_sc->sb)
		sysconf_write(conv_dac_sc->sb, 1);

	/* Mute the DAC */
	if (conv_dac_sc->softmute)
		sysconf_write(conv_dac_sc->softmute, 1);
	if (conv_dac_sc->mute_l)
		sysconf_write(conv_dac_sc->mute_l, 1);
	if (conv_dac_sc->mute_r)
		sysconf_write(conv_dac_sc->mute_r, 1);

	/* Take the DAC analog bits out of standby */
	if (conv_dac_sc->mode)
		sysconf_write(conv_dac_sc->mode, 0);
	if (conv_dac_sc->pdana)
		sysconf_write(conv_dac_sc->pdana, 1);
	if (conv_dac_sc->pndbg)
		sysconf_write(conv_dac_sc->pndbg, 1);

	return 0;
}

static int snd_stm_conv_dac_sc_disconnect(struct snd_device *snd_device)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc =
			snd_device->device_data;

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	/* Put the DAC into reset */
	sysconf_write(conv_dac_sc->nrst, 0);

	/* Put the DAC into standby */
	if (conv_dac_sc->nsb)
		sysconf_write(conv_dac_sc->nsb, 0);
	if (conv_dac_sc->sb)
		sysconf_write(conv_dac_sc->sb, 1);

	/* Mute the DAC */
	if (conv_dac_sc->softmute)
		sysconf_write(conv_dac_sc->softmute, 1);
	if (conv_dac_sc->mute_l)
		sysconf_write(conv_dac_sc->mute_l, 1);
	if (conv_dac_sc->mute_r)
		sysconf_write(conv_dac_sc->mute_r, 1);

	/* Put the DAC analog bits into standby */
	if (conv_dac_sc->mode)
		sysconf_write(conv_dac_sc->mode, 0);
	if (conv_dac_sc->pdana)
		sysconf_write(conv_dac_sc->pdana, 0);
	if (conv_dac_sc->pndbg)
		sysconf_write(conv_dac_sc->pndbg, 0);

	return 0;
}

static struct snd_device_ops snd_stm_conv_dac_sc_snd_device_ops = {
	.dev_register = snd_stm_conv_dac_sc_register,
	.dev_disconnect = snd_stm_conv_dac_sc_disconnect,
};



/*
 * Platform driver routines
 */

static int snd_stm_conv_dac_sc_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_dac_sc_info *info =
			pdev->dev.platform_data;
	struct snd_stm_conv_dac_sc *conv_dac_sc;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);
	BUG_ON(!info);

	conv_dac_sc = kzalloc(sizeof(*conv_dac_sc), GFP_KERNEL);
	if (!conv_dac_sc) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv_dac_sc);
	conv_dac_sc->bus_id = dev_name(&pdev->dev);

	/* Get resources */

	conv_dac_sc->nrst = sysconf_claim(info->nrst.group, info->nrst.num,
			info->nrst.lsb, info->nrst.msb, "NRST");
	BUG_ON(!conv_dac_sc->nrst);

	/*
	 * Depending on SoC we will have a 'notstandby' or a 'standby' sysconf
	 * bit. Here we try to claim both, although in reality we will normally
	 * only use one or the other.
	 */

	if (!FIELD_EMPTY(info->nsb))
		conv_dac_sc->nsb = sysconf_claim(info->nsb.group, info->nsb.num,
				info->nsb.lsb, info->nsb.msb, "NSB");
	if (!FIELD_EMPTY(info->sb))
		conv_dac_sc->sb = sysconf_claim(info->sb.group, info->sb.num,
				info->sb.lsb, info->sb.msb, "SB");

	BUG_ON(!conv_dac_sc->nsb && !conv_dac_sc->sb);

	/*
	 * Depending on SoC we will have a 'softmute' or 'mute_l' and 'mute_r'
	 * sysconf bits. Here we try to claim all of bits, although in reality
	 * we will normally only use 'softmute' or 'mute_l' and 'mute_r'.
	 */

	if (!FIELD_EMPTY(info->softmute))
		conv_dac_sc->softmute = sysconf_claim(info->softmute.group,
				info->softmute.num, info->softmute.lsb,
				info->softmute.msb, "SOFTMUTE");
	if (!FIELD_EMPTY(info->mute_l))
		conv_dac_sc->mute_l = sysconf_claim(info->mute_l.group,
				info->mute_l.num, info->mute_l.lsb,
				info->mute_l.msb, "MUTE_L");
	if (!FIELD_EMPTY(info->mute_r))
		conv_dac_sc->mute_r = sysconf_claim(info->mute_r.group,
				info->mute_r.num, info->mute_r.lsb,
				info->mute_r.msb, "MUTE_R");

	BUG_ON(!conv_dac_sc->softmute &&
			!conv_dac_sc->mute_l && !conv_dac_sc->mute_r);

	/*
	 * Depending on SoC, the following 'mode', 'pdana' and 'pndbg' sysconf
	 * bits may or may not be supported.
	 */

	if (!FIELD_EMPTY(info->mode)) {
		conv_dac_sc->mode = sysconf_claim(info->mode.group,
				info->mode.num, info->mode.lsb,
				info->mode.msb, "MODE");
		BUG_ON(!conv_dac_sc->mode);
	}

	if (!FIELD_EMPTY(info->pdana)) {
		conv_dac_sc->pdana = sysconf_claim(info->pdana.group,
				info->pdana.num, info->pdana.lsb,
				info->pdana.msb, "PDANA");
		BUG_ON(!conv_dac_sc->pdana);
	}

	if (!FIELD_EMPTY(info->pndbg)) {
		conv_dac_sc->pndbg = sysconf_claim(info->pndbg.group,
				info->pndbg.num, info->pndbg.lsb,
				info->pndbg.msb, "PNDBG");
		BUG_ON(!conv_dac_sc->pndbg);
	}

	/* Get connections */

	BUG_ON(!info->source_bus_id);
	snd_stm_printd(0, "This DAC is attached to PCM player '%s'.\n",
			info->source_bus_id);
	conv_dac_sc->converter = snd_stm_conv_register_converter(
			"Analog Output", &snd_stm_conv_dac_sc_ops, conv_dac_sc,
			&platform_bus_type, info->source_bus_id,
			info->channel_from, info->channel_to, NULL);
	if (!conv_dac_sc->converter) {
		snd_stm_printe("Can't attach to PCM player!\n");
		goto error_attach;
	}

	/* Create ALSA lowlevel device*/

	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv_dac_sc,
			&snd_stm_conv_dac_sc_snd_device_ops);
	if (result < 0) {
		snd_stm_printe("ALSA low level device creation failed!\n");
		goto error_device;
	}

	/* Done now */

	platform_set_drvdata(pdev, conv_dac_sc);

	return 0;

error_device:
error_attach:
	snd_stm_magic_clear(conv_dac_sc);
	kfree(conv_dac_sc);
error_alloc:
	return result;
}

static int snd_stm_conv_dac_sc_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_dac_sc *conv_dac_sc = platform_get_drvdata(pdev);

	BUG_ON(!conv_dac_sc);
	BUG_ON(!snd_stm_magic_valid(conv_dac_sc));

	snd_stm_conv_unregister_converter(conv_dac_sc->converter);

	sysconf_release(conv_dac_sc->nrst);

	if (conv_dac_sc->nsb)
		sysconf_release(conv_dac_sc->nsb);
	if (conv_dac_sc->sb)
		sysconf_release(conv_dac_sc->sb);
	if (conv_dac_sc->softmute)
		sysconf_release(conv_dac_sc->softmute);
	if (conv_dac_sc->mute_l)
		sysconf_release(conv_dac_sc->mute_l);
	if (conv_dac_sc->mute_r)
		sysconf_release(conv_dac_sc->mute_r);
	if (conv_dac_sc->mode)
		sysconf_release(conv_dac_sc->mode);
	if (conv_dac_sc->pdana)
		sysconf_release(conv_dac_sc->pdana);
	if (conv_dac_sc->pndbg)
		sysconf_release(conv_dac_sc->pndbg);

	snd_stm_magic_clear(conv_dac_sc);
	kfree(conv_dac_sc);

	return 0;
}

static struct platform_driver snd_stm_conv_dac_sc_driver = {
	.driver.name = "snd_conv_dac_sc",
	.probe = snd_stm_conv_dac_sc_probe,
	.remove = snd_stm_conv_dac_sc_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_dac_sc_init(void)
{
	return platform_driver_register(&snd_stm_conv_dac_sc_driver);
}

static void __exit snd_stm_conv_dac_sc_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_dac_sc_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sysconf-based audio DAC driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_dac_sc_init);
module_exit(snd_stm_conv_dac_sc_exit);
