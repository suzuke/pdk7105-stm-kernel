/*
 *   STMicrolectronics STiG125 SoC audio glue driver
 *
 *   Copyright (c) 2012 STMicroelectronics Limited
 *
 *   Authors: John Boddie <john.boddie@st.com>
 *            Japneet Chhatwal <Japneet.chhatwal@st.com>
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
#include <linux/stm/stig125.h>
#include <linux/stm/platform.h>
#include <linux/stm/soc.h>
#include <sound/core.h>

#include "common.h"


static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);


/*
 * Audio initialization
 */

static struct sysconf_field *snd_stm_stig125_biphase_idle_value;
static struct sysconf_field *snd_stm_stig125_pcm_valid_sel;
static struct sysconf_field *snd_stm_stig125_pcm_toextdac_sel;
static struct sysconf_field *snd_stm_stig125_mclk_sel;
static struct sysconf_field *snd_stm_stig125_extdac_mclk_invert;


static int __init snd_stm_stig125_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	if (!stm_soc_is_stig125()) {
		snd_stm_printe("Unsupported (not STiG125) SoC detected!\n");
		return -EINVAL;
	}

	/* Set bi-phase idle value */
	snd_stm_stig125_biphase_idle_value = sysconf_claim(SYSCONF(918), 8, 8,
			"BIPHASE_IDLE_VALUE");
	if (!snd_stm_stig125_biphase_idle_value) {
		snd_stm_printe("Failed to claim BIPHASE_IDLE_VALUE sysconf\n");
		result = -EBUSY;
		goto error_sysconf_claim_biphase_idle_value;
	}
	sysconf_write(snd_stm_stig125_biphase_idle_value, 0);

	/* Set PCM reader input from pads (1 = input from player #1) */
	snd_stm_stig125_pcm_valid_sel = sysconf_claim(SYSCONF(918), 6, 6,
			"PCM_VALID_SEL");
	if (!snd_stm_stig125_pcm_valid_sel) {
		snd_stm_printe("Failed to claim PCM_VALID_SEL sysconf\n");
		result = -EBUSY;
		goto error_sysconf_claim_pcm_valid_sel;
	}
	sysconf_write(snd_stm_stig125_pcm_valid_sel, 0);

	/* Route player #1 to Ext DAC (PIO) */
	snd_stm_stig125_pcm_toextdac_sel = sysconf_claim(SYSCONF(918), 3, 5,
			"PCM_TOEXTDAC_SEL");
	if (!snd_stm_stig125_pcm_toextdac_sel) {
		snd_stm_printe("Failed to claim PCM_TOEXTDAC_SEL sysconf\n");
		result = -EBUSY;
		goto error_sysconf_claim_pcm_toextdac_sel;
	}
	sysconf_write(snd_stm_stig125_pcm_toextdac_sel, 1);

	/* Set Ext DAC master clock to same clock as player #1 */
	snd_stm_stig125_mclk_sel = sysconf_claim(SYSCONF(918), 1, 2,
			"MCLK_SEL");
	if (!snd_stm_stig125_mclk_sel) {
		snd_stm_printe("Failed to claim MCLK_SEL sysconf\n");
		result = -EBUSY;
		goto error_sysconf_claim_mclk_sel;
	}
	sysconf_write(snd_stm_stig125_mclk_sel, 1);

	/* Set Ext DAC master clock to not inverted */
	snd_stm_stig125_extdac_mclk_invert = sysconf_claim(SYSCONF(918), 0, 0,
			"EXTDAC_MCLK_INVERT");
	if (!snd_stm_stig125_extdac_mclk_invert) {
		snd_stm_printe("Failed to claim EXTDAC_MCLK_INVERT sysconf\n");
		result = -EBUSY;
		goto error_sysconf_claim_extdac_mclk_invert;
	}
	sysconf_write(snd_stm_stig125_extdac_mclk_invert, 0);

	result = snd_stm_card_register(SND_STM_CARD_TYPE_AUDIO);
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA audio card!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	sysconf_release(snd_stm_stig125_extdac_mclk_invert);
error_sysconf_claim_extdac_mclk_invert:
	sysconf_release(snd_stm_stig125_mclk_sel);
error_sysconf_claim_mclk_sel:
	sysconf_release(snd_stm_stig125_pcm_toextdac_sel);
error_sysconf_claim_pcm_toextdac_sel:
	sysconf_release(snd_stm_stig125_pcm_valid_sel);
error_sysconf_claim_pcm_valid_sel:
	sysconf_release(snd_stm_stig125_biphase_idle_value);
error_sysconf_claim_biphase_idle_value:
	return result;
}


static void __exit snd_stm_stig125_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	sysconf_release(snd_stm_stig125_extdac_mclk_invert);
	sysconf_release(snd_stm_stig125_mclk_sel);
	sysconf_release(snd_stm_stig125_pcm_toextdac_sel);
	sysconf_release(snd_stm_stig125_pcm_valid_sel);
	sysconf_release(snd_stm_stig125_biphase_idle_value);
}

MODULE_AUTHOR("Japneet Chhatwal <japneet.chhatwal@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STiG125 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stig125_init);
module_exit(snd_stm_stig125_exit);
