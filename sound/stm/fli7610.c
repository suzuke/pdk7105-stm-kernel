/*
 *   STMicrolectronics FLi7610 SoC audio glue driver
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
#include <linux/delay.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/fli7610.h>
#include <linux/stm/platform.h>
#include <sound/core.h>

#include "common.h"


static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);


/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "FLi7610"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for FLi7610 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for FLi7610 audio subsystem card.");


/*
 * TAE_SYSCONF fields
 */

static struct sysconf_field *snd_stm_fli7610_spdif_clk;
static struct sysconf_field *snd_stm_fli7610_main_clk;
static struct sysconf_field *snd_stm_fli7610_aux_clk;
static struct sysconf_field *snd_stm_fli7610_dac_clk;
static struct sysconf_field *snd_stm_fli7610_adc_clk;
static struct sysconf_field *snd_stm_fli7610_spdif_div2;

static struct sysconf_field *snd_stm_fli7610_spdif_out_sel;
static struct sysconf_field *snd_stm_fli7610_main_i2s_sel;
static struct sysconf_field *snd_stm_fli7610_sec_i2s_sel;

static struct sysconf_field *snd_stm_fli7610_aud_reg_5;


#ifdef CONFIG_MACH_STM_FLI76XXHDK01
#define AATV_BASE_ADDRESS 0xfef80000 /* 512KB */

static void __init snd_stm_fli7610_configure_aatv(void)
{
	void *aatv_base;

	snd_stm_printd(0, "%s()\n", __func__);

	/* Map the AATV registers into memory */
	aatv_base = ioremap(AATV_BASE_ADDRESS, SZ_512K);
	if (!aatv_base) {
		snd_stm_printe("Failed to ioremap aatv base!\n");
		goto error_ioremap;
	}

	/* Read anti-pop status to ensure everything is ready */
	if ((readl(aatv_base + 0x77838) & 0x1e) == 0x1e) {
		/* Set SCARTs, analog outputs and DACs to active and unmute */
		writel(0x00, aatv_base + 0x77800);
		writel(0x01, aatv_base + 0x77804);
		writel(0x00, aatv_base + 0x77808);
		writel(0x00, aatv_base + 0x77810);

		/* Disable SCART matrix configuration clock */
		writel(0x00, aatv_base + 0x77820);

		/* Disable SCART 1/2/3 output */
		writel(0x00, aatv_base + 0x77824);
		writel(0x00, aatv_base + 0x77828);

		/* Toggle SCART matrix configuration clock */
		writel(0x01, aatv_base + 0x77820);
		writel(0x00, aatv_base + 0x77820);

		/* Enable SCART 1/2/3 output and load configuration */
		writel(0x00, aatv_base + 0x77834);
		udelay(200);
		writel(0x15, aatv_base + 0x77834);
		udelay(200);
		writel(0x3f, aatv_base + 0x77834);
		udelay(200);
		writel(0x15, aatv_base + 0x77834);
		udelay(200);
	} else {
		snd_stm_printe("Anti-pop status indicates not ready!\n");
		goto error_anti_pop;
	}

	/* Update I2S mux (0=PCMP0, 1=HP, 2=AVOUT, 3=PCMP0 4=PCMP0) */
	writel(0x00980, aatv_base + 0x76800);

	/* DAC attenuation (coarse volume control) */
	writel(0x50, aatv_base + 0x77818);
	writel(0x01, aatv_base + 0x7781c);
	writel(0x04, aatv_base + 0x77814);
	writel(0x05, aatv_base + 0x77814);
	writel(0x06, aatv_base + 0x77814);
	writel(0x07, aatv_base + 0x77814);

	/* DAC attenuation (coarse volume control) */
	writel(0x37, aatv_base + 0x77818);
	writel(0x01, aatv_base + 0x7781c);
	writel(0x08, aatv_base + 0x77814);
	writel(0x09, aatv_base + 0x77814);
	writel(0x0a, aatv_base + 0x77814);
	writel(0x0b, aatv_base + 0x77814);

	/* Enable SCART 1/2 output for DAC */
	writel(0x11, aatv_base + 0x77824);

	/* Enable SCART matrix configuration clock */
	writel(0x01, aatv_base + 0x77820);

error_anti_pop:
	iounmap(aatv_base);
error_ioremap:
	return;
}
#endif


/*
 * Audio initialization
 */

static int __init snd_stm_fli7610_init(void)
{
	int result = 0;

	snd_stm_printd(0, "%s()\n", __func__);

	if (stm_soc_type() != CPU_FLI7610) {
		snd_stm_printe("Detected unsupported SoC (not FLi7610!)\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Set spdif clock to clk_256fs_free_run */
	snd_stm_fli7610_spdif_clk = sysconf_claim(TAE_SYSCONF(160), 0, 3,
							"SPDIF_CLK");
	if (!snd_stm_fli7610_spdif_clk) {
		snd_stm_printe("Failed to claim sysconf (SPDIF_CLK)\n");
		result = -EBUSY;
		goto error_sysconf_claim_spdif_clk;
	}
	sysconf_write(snd_stm_fli7610_spdif_clk, 0x0);

	/* Set main clock to clk_256fs_free_run */
	snd_stm_fli7610_main_clk = sysconf_claim(TAE_SYSCONF(160), 4, 7,
							"MAIN_CLK");
	if (!snd_stm_fli7610_main_clk) {
		snd_stm_printe("Failed to claim sysconf (MAIN_CLK)\n");
		result = -EBUSY;
		goto error_sysconf_claim_main_clk;
	}
	sysconf_write(snd_stm_fli7610_main_clk, 0x0);

	/* Set aux i2s clock to clk_256fs_free_run */
	snd_stm_fli7610_aux_clk = sysconf_claim(TAE_SYSCONF(160), 8, 11,
							"AUX_CLK");
	if (!snd_stm_fli7610_aux_clk) {
		snd_stm_printe("Failed to claim sysconf (AUX_CLK)\n");
		result = -EBUSY;
		goto error_sysconf_claim_aux_clk;
	}
	sysconf_write(snd_stm_fli7610_aux_clk, 0x0);

	/* Set dac clock to clk_256fs_free_run */
	snd_stm_fli7610_dac_clk = sysconf_claim(TAE_SYSCONF(160), 12, 15,
							"DAC_CLK");
	if (!snd_stm_fli7610_dac_clk) {
		snd_stm_printe("Failed to claim sysconf (DAC_CLK)\n");
		result = -EBUSY;
		goto error_sysconf_claim_dac_clk;
	}
	sysconf_write(snd_stm_fli7610_dac_clk, 0x0);

	/* Set adc clock to clk_256fs_free_run */
	snd_stm_fli7610_adc_clk = sysconf_claim(TAE_SYSCONF(160), 16, 19,
							"ADC_CLK");
	if (!snd_stm_fli7610_adc_clk) {
		snd_stm_printe("Failed to claim sysconf (ADC_CLK)\n");
		result = -EBUSY;
		goto error_sysconf_claim_adc_clk;
	}
	sysconf_write(snd_stm_fli7610_adc_clk, 0x0);

	/* Turn spdif clock division by 2 off */
	snd_stm_fli7610_spdif_div2 = sysconf_claim(TAE_SYSCONF(160), 30, 30,
							"SPDIF_DIV2");
	if (!snd_stm_fli7610_spdif_div2) {
		snd_stm_printe("Failed to claim sysconf (SPDIF_DIV2)\n");
		result = -EBUSY;
		goto error_sysconf_claim_spdif_div2;
	}
	sysconf_write(snd_stm_fli7610_spdif_div2, 0x0);

	/* Select spdif player for output */
	snd_stm_fli7610_spdif_out_sel = sysconf_claim(TAE_SYSCONF(161), 0, 0,
							"SPDIF_OUT_SEL");
	if (!snd_stm_fli7610_spdif_out_sel) {
		snd_stm_printe("Failed to claim sysconf (SPDIF_OUT_SEL)\n");
		result = -EBUSY;
		goto error_sysconf_claim_spdif_out_sel;
	}
	sysconf_write(snd_stm_fli7610_spdif_out_sel, 0x0);

	/* Select ls for main i2s */
	snd_stm_fli7610_main_i2s_sel = sysconf_claim(TAE_SYSCONF(161), 1, 3,
							"MAIN_I2S_SEL");
	if (!snd_stm_fli7610_main_i2s_sel) {
		snd_stm_printe("Failed to claim sysconf (MAIN_I2S_SEL)\n");
		result = -EBUSY;
		goto error_sysconf_claim_main_i2s_sel;
	}
	sysconf_write(snd_stm_fli7610_main_i2s_sel, 0x4);

	/* Select aux for secondary i2s */
	snd_stm_fli7610_sec_i2s_sel = sysconf_claim(TAE_SYSCONF(161), 4, 6,
							"SEC_I2S_SEL");
	if (!snd_stm_fli7610_sec_i2s_sel) {
		snd_stm_printe("Failed to claim sysconf (SEC_I2S_SEL)\n");
		result = -EBUSY;
		goto error_sysconf_claim_sec_i2s_sel;
	}
	sysconf_write(snd_stm_fli7610_sec_i2s_sel, 0x0);

	/* Take headphone amplifier out of standby */
	snd_stm_fli7610_aud_reg_5 = sysconf_claim(TAE_SYSCONF(164), 0, 31,
							"AUD_REG_5");
	if (!snd_stm_fli7610_aud_reg_5) {
		snd_stm_printe("Failed to claim sysconf (AUD_REG_5)\n");
		goto error_sysconf_claim_aud_reg_5;
	}
	sysconf_write(snd_stm_fli7610_aud_reg_5, 0x00010000);

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

#ifdef CONFIG_MACH_STM_FLI76XXHDK01
	snd_stm_fli7610_configure_aatv();
#endif

	return 0;

error_card_register:
	sysconf_release(snd_stm_fli7610_aud_reg_5);
error_sysconf_claim_aud_reg_5:
	sysconf_release(snd_stm_fli7610_sec_i2s_sel);
error_sysconf_claim_sec_i2s_sel:
	sysconf_release(snd_stm_fli7610_main_i2s_sel);
error_sysconf_claim_main_i2s_sel:
	sysconf_release(snd_stm_fli7610_spdif_out_sel);
error_sysconf_claim_spdif_out_sel:
	sysconf_release(snd_stm_fli7610_spdif_div2);
error_sysconf_claim_spdif_div2:
	sysconf_release(snd_stm_fli7610_adc_clk);
error_sysconf_claim_adc_clk:
	sysconf_release(snd_stm_fli7610_dac_clk);
error_sysconf_claim_dac_clk:
	sysconf_release(snd_stm_fli7610_aux_clk);
error_sysconf_claim_aux_clk:
	sysconf_release(snd_stm_fli7610_main_clk);
error_sysconf_claim_main_clk:
	sysconf_release(snd_stm_fli7610_spdif_clk);
error_sysconf_claim_spdif_clk:
error_soc_type:
	return result;
}

static void __exit snd_stm_fli7610_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	sysconf_release(snd_stm_fli7610_aud_reg_5);

	/* Release I2S routing fields in audio configuration registers */

	sysconf_release(snd_stm_fli7610_sec_i2s_sel);
	sysconf_release(snd_stm_fli7610_main_i2s_sel);
	sysconf_release(snd_stm_fli7610_spdif_out_sel);

	/* Release clock routing fields in audio configuration registers */

	sysconf_release(snd_stm_fli7610_spdif_div2);
	sysconf_release(snd_stm_fli7610_adc_clk);
	sysconf_release(snd_stm_fli7610_dac_clk);
	sysconf_release(snd_stm_fli7610_aux_clk);
	sysconf_release(snd_stm_fli7610_main_clk);
	sysconf_release(snd_stm_fli7610_spdif_clk);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics FLi7610 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_fli7610_init);
module_exit(snd_stm_fli7610_exit);
