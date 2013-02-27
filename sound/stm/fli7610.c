/*
 *   STMicrolectronics FLi7610 SoC audio glue driver
 *
 *   Copyright (c) 2011-2013 STMicroelectronics Limited
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
#include <linux/stm/soc.h>
#include <sound/core.h>

#include "common.h"


/*
 * Audio pad configuration.
 */

static struct stm_pad_config snd_stm_fli7610_pad_config = {
	.sysconfs_num = 10,
	.sysconfs = (struct stm_pad_sysconf []) {
		/* Set spdif clock to clk_256fs_free_run */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 0, 3, 0),
		/* Set main clock to clk_256fs_free_run */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 4, 7, 0),
		/* Set aux i2s clock to clk_256fs_free_run */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 8, 11, 0),
		/* Set dac clock to clk_256fs_free_run */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 12, 15, 0),
		/* Set adc clock to clk_256fs_free_run */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 16, 19, 0),
		/* Turn off spdif clock division by 2 */
		STM_PAD_SYSCONF(TAE_SYSCONF(160), 30, 30, 0),
		/* Select spdif player output */
		STM_PAD_SYSCONF(TAE_SYSCONF(161), 0, 0, 0),
		/* Select ls for main i2s */
		STM_PAD_SYSCONF(TAE_SYSCONF(161), 1, 3, 4),
		/* Select aux for secondary i2s */
		STM_PAD_SYSCONF(TAE_SYSCONF(161), 4, 6, 0),
		/* Disable headphone amplifier standby */
		STM_PAD_SYSCONF(TAE_SYSCONF(164), 0, 31, 0x00010000),
	},
};


/*
 * Configure AATV.
 */

#ifdef CONFIG_MACH_STM_FLI76XXHDK01
#define AATV_BASE_ADDRESS	0xfef80000	/* 512KB */
#define AATV_CMD_MUX_I2S_OUT	0x00076800
#define AATV_SCART_IN_STDBY	0x00077800
#define AATV_ANA_OUT_STDBY	0x00077804
#define AATV_DAC_STDBY		0x00077808
#define AATV_DAC_CONTROL	0x00077810
#define AATV_DAC_ATTN_CONT	0x00077814
#define AATV_DAC_ATTN_MSB	0x00077818
#define AATV_DAC_ATTN_LSB	0x0007781c
#define AATV_SCART_CONF_LOAD	0x00077820
#define AATV_SCART_0_1_2	0x00077824
#define AATV_SCART_0_3_ADC	0x00077828
#define AATV_ANTI_POP_CONTROL	0x00077834
#define AATV_ANTI_POP_STATUS	0x00077838

static void __init snd_stm_fli7610_configure_aatv(struct device *dev)
{
	void *aatv_base;

	dev_dbg(dev, "%s()", __func__);

	/* Map the AATV registers into memory */
	aatv_base = ioremap(AATV_BASE_ADDRESS, SZ_512K);
	if (!aatv_base) {
		dev_err(dev, "Failed to ioremap aatv base!\n");
		goto error_ioremap;
	}

	/* AATV power down sequence */
	writel(0x00, aatv_base + AATV_SCART_CONF_LOAD);
	writel(0x00, aatv_base + AATV_SCART_0_1_2);
	writel(0x00, aatv_base + AATV_SCART_0_3_ADC);
	writel(0x01, aatv_base + AATV_SCART_CONF_LOAD);
	writel(0x00, aatv_base + AATV_SCART_CONF_LOAD);
	writel(0xff, aatv_base + AATV_SCART_IN_STDBY);
	writel(0x7c, aatv_base + AATV_ANA_OUT_STDBY);
	writel(0x07, aatv_base + AATV_DAC_STDBY);
	writel(0x0e, aatv_base + AATV_DAC_CONTROL);

	writel(0x00, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);
	writel(0x2a, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);
	writel(0x00, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);

	/* Read anti-pop status to ensure everything is ready */
	if ((readl(aatv_base + AATV_ANTI_POP_STATUS) & 0x1e) != 0x1e) {
		dev_err(dev, "Anti-pop status indicates not ready!\n");
		goto error_anti_pop;
	}

	/* Set SCARTs, analog outputs and DACs to active and unmute */
	writel(0x00, aatv_base + AATV_SCART_IN_STDBY);
	writel(0x01, aatv_base + AATV_ANA_OUT_STDBY);
	writel(0x00, aatv_base + AATV_DAC_STDBY);
	writel(0x00, aatv_base + AATV_DAC_CONTROL);

	/* Disable SCART matrix configuration clock */
	writel(0x00, aatv_base + AATV_SCART_CONF_LOAD);

	/* Disable SCART 1/2/3 output */
	writel(0x00, aatv_base + AATV_SCART_0_1_2);
	writel(0x00, aatv_base + AATV_SCART_0_3_ADC);

	/* Toggle SCART matrix configuration clock */
	writel(0x01, aatv_base + AATV_SCART_CONF_LOAD);
	writel(0x00, aatv_base + AATV_SCART_CONF_LOAD);

	/* Enable SCART 1/2/3 output and load configuration */
	writel(0x00, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);
	writel(0x15, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);
	writel(0x3f, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);
	writel(0x15, aatv_base + AATV_ANTI_POP_CONTROL);
	udelay(200);

	/* Update I2S mux (0=PCMP0, 1=HP, 2=AVOUT, 3=PCMP0 4=PCMP0) */
	writel(0x00980, aatv_base + AATV_CMD_MUX_I2S_OUT);

	/* DAC attenuation (coarse volume control) */
	writel(0x50, aatv_base + AATV_DAC_ATTN_MSB);
	writel(0x01, aatv_base + AATV_DAC_ATTN_LSB);
	writel(0x04, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x05, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x06, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x07, aatv_base + AATV_DAC_ATTN_CONT);

	/* DAC attenuation (coarse volume control) */
	writel(0x37, aatv_base + AATV_DAC_ATTN_MSB);
	writel(0x01, aatv_base + AATV_DAC_ATTN_LSB);
	writel(0x08, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x09, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x0a, aatv_base + AATV_DAC_ATTN_CONT);
	writel(0x0b, aatv_base + AATV_DAC_ATTN_CONT);

	/* Enable SCART 1/2 output for DAC */
	writel(0x11, aatv_base + AATV_SCART_0_1_2);

	/* Enable SCART matrix configuration clock */
	writel(0x01, aatv_base + AATV_SCART_CONF_LOAD);

error_anti_pop:
	iounmap(aatv_base);
error_ioremap:
	return;
}
#endif


/*
 * Power management.
 */

#ifdef CONFIG_HIBERNATION
static int snd_stm_fli7610_restore(struct device *dev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(dev);

	dev_dbg(dev, "%s()", __func__);

	stm_pad_setup(pad_state);

#ifdef CONFIG_MACH_STM_FLI76XXHDK01
	snd_stm_fli7610_configure_aatv(dev);
#endif

	return 0;
}

static const struct dev_pm_ops snd_stm_fli7610_pm_ops = {
	.thaw		= snd_stm_fli7610_restore,
	.restore	= snd_stm_fli7610_restore,
};
#else
static const struct dev_pm_ops snd_stm_fli7610_pm_ops;
#endif


/*
 * Driver functions.
 */

static int __devinit snd_stm_fli7610_probe(struct platform_device *pdev)
{
	int result;
	struct stm_pad_state *pad_state;

	dev_dbg(&pdev->dev, "%s()", __func__);

	if (!stm_soc_is_fli7610()) {
		dev_err(&pdev->dev, "Unsupported (not Fli7610) SOC detected!");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Claim all of the sysconf fields and initialise */
	pad_state = stm_pad_claim(&snd_stm_fli7610_pad_config,
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

#ifdef CONFIG_MACH_STM_FLI76XXHDK01
	snd_stm_fli7610_configure_aatv(&pdev->dev);
#endif

	/* Save the pad state as driver data */
	dev_set_drvdata(&pdev->dev, pad_state);

	return 0;

error_card_register:
	stm_pad_release(pad_state);
error_pad_claim:
error_soc_type:
	return result;
}

static int __devexit snd_stm_fli7610_remove(struct platform_device *pdev)
{
	struct stm_pad_state *pad_state = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s()", __func__);

	stm_pad_release(pad_state);

	return 0;
}

static struct platform_driver snd_stm_fli7610_driver = {
	.driver.name	= "snd_fli7610",
	.driver.pm	= &snd_stm_fli7610_pm_ops,
	.probe		= snd_stm_fli7610_probe,
	.remove		= snd_stm_fli7610_remove,
};


/*
 * Module initialistaion.
 */

static struct platform_device snd_stm_fli7610_devices = {
	.name = "snd_fli7610",
	.id = -1,
};

static int __init snd_stm_fli7610_init(void)
{
	int result;

	/* Add the Fli7610 audio glue platform device */
	result = platform_device_register(&snd_stm_fli7610_devices);
	BUG_ON(result);

	/* Register the platform driver */
	result = platform_driver_register(&snd_stm_fli7610_driver);
	BUG_ON(result);

	return 0;
}

static void __exit snd_stm_fli7610_exit(void)
{
	platform_device_unregister(&snd_stm_fli7610_devices);
	platform_driver_unregister(&snd_stm_fli7610_driver);
}


MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics FLi7610 audio glue driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_fli7610_init);
module_exit(snd_stm_fli7610_exit);
