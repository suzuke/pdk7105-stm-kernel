/*
 *   STMicroelectronics TELSS Top Glue driver
 *
 *   Copyright (c) 2013 STMicroelectronics Limited
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
#include "reg_aud_telss_glue.h"


/*
 * Types.
 */

struct telss_glue {
	struct device *dev;			/* Pointer to device */
	struct snd_stm_telss_glue_info *info;	/* Platform configuration */
	struct resource *mem_region;		/* Registers memory region */
	void __iomem *base;			/* Uncaches base address */
	struct snd_info_entry *proc_entry;	/* Proc entry */
	snd_stm_magic_field;			/* Magic field */
};


/*
 * Register dump routine.
 */

#define TELSS_GLUE_DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_TELSS_%-26s (offset 0x%04x) = " \
				"0x%08x\n", __stringify(r), \
				offset__AUD_TELSS_##r(glue), \
				get__AUD_TELSS_##r(glue))

static void telss_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct telss_glue *glue = entry->private_data;

	BUG_ON(!glue);
	BUG_ON(!snd_stm_magic_valid(glue));

	snd_iprintf(buffer, "--- %s (0x%p) ---\n", dev_name(glue->dev),
			glue->base);

	TELSS_GLUE_DUMP_REGISTER(IT_PRESCALER);
	TELSS_GLUE_DUMP_REGISTER(IT_NOISE_SUPP_WID);
	TELSS_GLUE_DUMP_REGISTER(EXT_RST_N);
	TELSS_GLUE_DUMP_REGISTER(EXT_CS_N);
	TELSS_GLUE_DUMP_REGISTER(TDM_CTRL);
	TELSS_GLUE_DUMP_REGISTER(TDM_DEBUG);

	snd_iprintf(buffer, "\n");
}


/*
 * Platform driver routines.
 */

static int telss_glue_probe(struct platform_device *pdev)
{
	struct telss_glue *glue;
	int result;

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	/* Allocate device structure */
	glue = devm_kzalloc(&pdev->dev, sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "Failed to allocate device structure");
		return -ENOMEM;
	}

	/* Initialise device structure */
	glue->dev = &pdev->dev;
	glue->info = pdev->dev.platform_data;
	snd_stm_magic_set(glue);

	/* Request the telss glue registers memory region */
	result = snd_stm_memory_request(pdev, &glue->mem_region, &glue->base);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed memory request");
		return result;
	}

	/* Set the glue into the correct mode */
	switch (glue->info->mode) {
	case SND_STM_TELSS_GLUE_MODE_DECT:
		dev_err(&pdev->dev, "DECT glue not yet implemented!!!");
		break;

	case SND_STM_TELSS_GLUE_MODE_LANTIQ:
		set__AUD_TELSS_TDM_CTRL__CODEC_SLIC_FS_SEL_FS01(glue);
		set__AUD_TELSS_TDM_CTRL__DECT_FS_SEL_FS02(glue);
		set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_LEVEL_PULL_UP(glue);
		set__AUD_TELSS_TDM_CTRL__EXT_TDM_PATH_ENABLE(glue);
		set__AUD_TELSS_TDM_CTRL__ZSI_TDM_PATH_DISABLE(glue);
		set__AUD_TELSS_TDM_CTRL__EXT_TDM_DATA_IN_DEL_DISABLE(glue);
		break;

	case SND_STM_TELSS_GLUE_MODE_ZSI:
		dev_err(&pdev->dev, "ZSI glue not yet implemented!!!");
		break;

	default:
		dev_err(&pdev->dev, "Unknown TELSS mode");
		return -EINVAL;
	}

	/* Configure loop back mode */
	if (glue->info->loopback)
		set__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK_ENABLE(glue);
	else
		set__AUD_TELSS_TDM_DEBUG__TDM_LOOPBACK_DISABLE(glue);

	/* Add procfs info entry */
	result = snd_stm_info_register(&glue->proc_entry, dev_name(&pdev->dev),
			telss_glue_dump_registers, glue);
	if (result) {
		dev_err(&pdev->dev, "Failed to register with procfs");
		return result;
	}

	/* Save a pointer to the driver state */
	platform_set_drvdata(pdev, glue);

	return 0;
}

static int telss_glue_remove(struct platform_device *pdev)
{
	struct telss_glue *glue = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s(pdev=%p)\n", __func__, pdev);

	BUG_ON(!glue);
	BUG_ON(!snd_stm_magic_valid(glue));

	/* Remove procfs info entry */
	snd_stm_info_unregister(glue->proc_entry);

	/* Everything else should be automatically cleaned up */

	snd_stm_magic_clear(glue);

	return 0;
}


/*
 * Module initialization.
 */

static struct platform_driver telss_glue_driver = {
	.driver.name	= "snd_telss_glue",
	.probe		= telss_glue_probe,
	.remove		= telss_glue_remove,
};

static int __init telss_glue_init(void)
{
	return platform_driver_register(&telss_glue_driver);
}

static void __exit telss_glue_exit(void)
{
	platform_driver_unregister(&telss_glue_driver);
}

module_init(telss_glue_init);
module_exit(telss_glue_exit);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics TELSS Top Glue driver");
MODULE_LICENSE("GPL");
