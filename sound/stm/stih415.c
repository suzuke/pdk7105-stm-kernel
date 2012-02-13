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

static int __init snd_stm_stih415_init(void)
{
	int result;

	snd_stm_printd(0, "%s()\n", __func__);

	if (stm_soc_type() != CPU_STIH415) {
		snd_stm_printe("Unsupported (not STiH415) SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Select external pcm clock for each channel */
	snd_stm_stih415_pcm_clk_sel = sysconf_claim(SYSCONF(331), 8, 11,
						       "PCM_CLK_SEL");
	if (!snd_stm_stih415_pcm_clk_sel) {
		snd_stm_printe("Failed to claim sysconf (PCM_CLK_SEL)\n");
		result = -EBUSY;
		goto error_sysconf_claim_pcm_clk_sel;
	}
	sysconf_write(snd_stm_stih415_pcm_clk_sel, 0xf);

	/* Set bi-phase idle value */
	snd_stm_stih415_biphase_idle_value = sysconf_claim(SYSCONF(331), 7, 7,
						       "BIPHASE_IDLE_VALUE");
	if (!snd_stm_stih415_biphase_idle_value) {
		snd_stm_printe("Failed to claim sysconf (BIPHASE_IDLE_VALUE)\n");
		result = -EBUSY;
		goto error_sysconf_claim_biphase_idle_value;
	}
	sysconf_write(snd_stm_stih415_biphase_idle_value, 0);

	/* Clear all voip bits for now */
	snd_stm_stih415_voip = sysconf_claim(SYSCONF(331), 2, 5, "VOIP");
	if (!snd_stm_stih415_voip) {
		snd_stm_printe("Failed to claim sysconf (VOIP)\n");
		result = -EBUSY;
		goto error_sysconf_claim_voip;
	}
	sysconf_write(snd_stm_stih415_voip, 0);

	/* Route pcm players */
	snd_stm_stih415_pcmp_valid_sel = sysconf_claim(SYSCONF(331), 0, 1,
						       "PCMP_VALID_SEL");
	if (!snd_stm_stih415_pcmp_valid_sel) {
		snd_stm_printe("Failed to claim sysconf (PCMP_VALID_SEL)\n");
		result = -EBUSY;
		goto error_sysconf_claim_pcmp_valid_sel;
	}
	sysconf_write(snd_stm_stih415_pcmp_valid_sel, 1);

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	sysconf_release(snd_stm_stih415_pcmp_valid_sel);
error_sysconf_claim_pcmp_valid_sel:
	sysconf_release(snd_stm_stih415_voip);
error_sysconf_claim_voip:
	sysconf_release(snd_stm_stih415_biphase_idle_value);
error_sysconf_claim_biphase_idle_value:
	sysconf_release(snd_stm_stih415_pcm_clk_sel);
error_sysconf_claim_pcm_clk_sel:
error_soc_type:
	return result;
}

static void __exit snd_stm_stih415_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	sysconf_release(snd_stm_stih415_pcmp_valid_sel);
	sysconf_release(snd_stm_stih415_voip);
	sysconf_release(snd_stm_stih415_biphase_idle_value);
	sysconf_release(snd_stm_stih415_pcm_clk_sel);
}

MODULE_AUTHOR("Sevanand Singh <sevanand.singh@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STiH415 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stih415_init);
module_exit(snd_stm_stih415_exit);
