/*
 *   STMicroelectronics System-on-Chips' SPDIF RX driver
 *
 *   Copyright (c) 2012 STMicroelectronics Limited
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
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <sound/core.h>
#include <sound/info.h>
#include <sound/stm.h>

#include <linux/clk.h>

#include "common.h"
#include "reg_aud_spdif_rx.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * Hardware related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)



/*
 * Internal instance structure
 */

struct snd_stm_conv_spdif_rx {
	/* System information */
	struct snd_stm_conv_converter *converter;
	struct device *device;
	const char *bus_id;

	int index; /* ALSA controls index */

	/* Resources */
	struct resource *mem_region;
	void *base;
	unsigned int irq;
	struct stm_pad_state *pads;
	struct clk *clock;
	unsigned long clock_rate;

	/* Information */
	unsigned int sample_frequency;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};



/*
 * Internal routines
 */

#define SPDIF_RX_32_KHZ_LOWER_THRES 0x17D1
#define SPDIF_RX_32_KHZ_UPPER_THRES 0x182F
#define SPDIF_RX_44_1_KHZ_LOWER_THRES 0x1148
#define SPDIF_RX_44_1_KHZ_UPPER_THRES 0x118C
#define SPDIF_RX_48_KHZ_LOWER_THRES 0xFDF
#define SPDIF_RX_48_KHZ_UPPER_THRES 0x101F
#define SPDIF_RX_88_2_KHZ_LOWER_THRES 0x8A2
#define SPDIF_RX_88_2_KHZ_UPPER_THRES 0x8C6
#define SPDIF_RX_96_KHZ_LOWER_THRES 0x7EC
#define SPDIF_RX_96_KHZ_UPPER_THRES 0x810
#define SPDIF_RX_176_4_KHZ_LOWER_THRES 0x450
#define SPDIF_RX_176_4_KHZ_UPPER_THRES 0x463
#define SPDIF_RX_192_KHZ_LOWER_THRES 0x3F6
#define SPDIF_RX_192_KHZ_UPPER_THRES 0x407

static void snd_stm_conv_spdif_rx_get_sample_frequency(
		struct snd_stm_conv_spdif_rx *conv)
{
	unsigned int freq = get__AUD_SPDIF_RX_SAMP_FREQ(conv);

	snd_stm_printd(2, "%s(conv=0x%p)\n", __func__, conv);

	/*
	 * The NCO increment value is calculated as: (Fout/Fin) * 2^32 * 2
	 *   Fout: Fr * 256
	 *   Fin:  196.608MHz
	 */

	if ((freq > SPDIF_RX_32_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_32_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x5555);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x1555);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 32000;
	} else if ((freq > SPDIF_RX_44_1_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_44_1_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x6666);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x01d6);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 44100;
	} else if ((freq > SPDIF_RX_48_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_48_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x0000);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x2000);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 48000;
	} else if ((freq > SPDIF_RX_88_2_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_88_2_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0xcccc);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x03ac);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 88200;
	} else if ((freq > SPDIF_RX_96_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_96_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x0000);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x4000);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 96000;
	} else if ((freq > SPDIF_RX_176_4_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_176_4_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x9999);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x0759);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 176400;
	} else if ((freq > SPDIF_RX_192_KHZ_LOWER_THRES) &&
		(freq < SPDIF_RX_192_KHZ_UPPER_THRES)) {
		set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x0000);
		set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x8000);
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(conv);
		conv->sample_frequency = 192000;
	} else {
	  snd_stm_printe("Frequency not identifed (freq %x)\n", freq);
	}

	snd_stm_printd(2, "Frequency identified as %dHz\n",
			conv->sample_frequency);
}

static irqreturn_t snd_stm_conv_spdif_rx_irq_handler(int irq, void *dev_id)
{
	struct snd_stm_conv_spdif_rx *conv = dev_id;
	irqreturn_t result = IRQ_NONE;
	unsigned int ctrl0;
	unsigned int status;

	snd_stm_printd(2, "%s(irq=%d, dev_id=0x%p)\n", __func__, irq, dev_id);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Get the interrupt status and immediately clear */
	preempt_disable();
	ctrl0 = get__AUD_SPDIF_RX_CTRL0(conv);
	status = get__AUD_SPDIF_RX_EVENT_STATUS16(conv);
	set__AUD_SPDIF_RX_EVENT_STATUS16(conv, status);
	preempt_enable();

	/* On test interrupt, clear the test interrupt bit */
	if (ctrl0 & mask__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(conv)) {
		set__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(conv);
		result = IRQ_HANDLED;
	}

	/* Only process those asserted interrupts that are enabled */
	if ((ctrl0 & mask__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(conv)) &&
		(status & mask__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(conv))) {
		snd_stm_conv_spdif_rx_get_sample_frequency(conv);

		result = IRQ_HANDLED;

	} else if ((ctrl0 & mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(
								conv)) &&
		(status & mask__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(conv))) {
		set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RESET(conv);

		conv->sample_frequency = 0;

		result = IRQ_HANDLED;

	} else if ((ctrl0 & mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(
								conv)) &&
		(status & mask__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(conv))) {
		conv->sample_frequency = 0;

		result = IRQ_HANDLED;

	} else if (ctrl0 & mask__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(conv)) {
		if (status & mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(
									conv)) {
			snd_stm_printe("FIFO underflow event\n");

			result = IRQ_HANDLED;
		}

		if (status & mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(
									conv)) {
			snd_stm_printe("FIFO overflow event\n");

			result = IRQ_HANDLED;
		}
	}

	BUG_ON(result == IRQ_NONE);

	return result;
}



static int snd_stm_conv_spdif_rx_enable(struct snd_stm_conv_spdif_rx *conv)
{
	snd_stm_printd(1, "%s(conv=%p)\n", __func__, conv);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Enable the clock */
	clk_prepare_enable(conv->clock);
	clk_set_rate(conv->clock, conv->clock_rate);

	/* Set configuration */
	set__AUD_SPDIF_RX_CTRL0__ENABLE(conv);
	set__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(conv);
	set__AUD_SPDIF_RX_CTRL0__RESET_FIFO_NORMAL(conv);
	set__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(conv, 2);
	set__AUD_SPDIF_RX_CTRL0__DATA_MODE_24BIT(conv);
	set__AUD_SPDIF_RX_CTRL0__LSB_ALIGN_MSB(conv);
	set__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL_MSB(conv);
	set__AUD_SPDIF_RX_CTRL0__CH_ST_SEL_LEFT(conv);

	set__AUD_SPDIF_RX_DETECT_MAX__CNT(conv, 0xfff);

	/* Right channel when SWCLK high, data changes on SBCLK falling edge */
	set__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL_RIGHT(conv);
	set__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL_FALLING(conv);

	/* Take receiver logic out of reset */
	set__AUD_SPDIF_RX_RESET__RESET_DISABLE(conv);

	/* Enable selected interrupts */
	enable_irq(conv->irq);

	set__AUD_SPDIF_RX_EVENT_STATUS16__LOCK_EVENT_CLEAR(conv);
	set__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR_EVENT_CLEAR(conv);
	set__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL_EVENT_CLEAR(conv);
	set__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW_EVENT_CLEAR(conv);
	set__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW_EVENT_CLEAR(conv);

	set__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(conv);

	return 0;
}

static int snd_stm_conv_spdif_rx_disable(struct snd_stm_conv_spdif_rx *conv)
{
	snd_stm_printd(1, "%s(conv=%p)\n", __func__, conv);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Disable interrupts */
	set__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(conv);
	set__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(conv);
	disable_irq_nosync(conv->irq);

	/* Put receiver logic into reset */
	set__AUD_SPDIF_RX_RESET__RESET_ENABLE(conv);

	/* Set NCO to reset */
	set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RESET(conv);
	set__AUD_SPDIF_RX_NCO_INCR0(conv, 0x0000);
	set__AUD_SPDIF_RX_NCO_INCR1(conv, 0x0000);

	set__AUD_SPDIF_RX_CTRL0__RESET_FIFO_RESET(conv);
	set__AUD_SPDIF_RX_CTRL0__INSTR_DISABLE(conv);
	set__AUD_SPDIF_RX_CTRL0__DISABLE(conv);

	clk_disable_unprepare(conv->clock);

	return 0;
}



/*
 * Converter interface implementation
 */

static unsigned int snd_stm_conv_spdif_rx_get_format(void *priv)
{
	snd_stm_printd(1, "%s(priv=%p)\n", __func__, priv);

	return FORMAT;
}

static int snd_stm_conv_spdif_rx_set_enabled(int enabled, void *priv)
{
	struct snd_stm_conv_spdif_rx *conv = priv;

	snd_stm_printd(1, "%s(enabled=%d, priv=%p)\n", __func__, enabled, priv);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	snd_stm_printd(1, "%sabling SPDIF RX converter '%s'.\n",
			enabled ? "En" : "Dis",
			conv->bus_id);

	if (enabled)
		return snd_stm_conv_spdif_rx_enable(conv);
	else
		return snd_stm_conv_spdif_rx_disable(conv);
}

static struct snd_stm_conv_ops snd_stm_conv_spdif_rx_ops = {
	.get_format  = snd_stm_conv_spdif_rx_get_format,
	.set_enabled = snd_stm_conv_spdif_rx_set_enabled,
};


/*
 * ALSA lowlevel device implementation
 */

#define DUMP_REGISTER(r) \
		snd_iprintf(buffer, "AUD_SPDIF_RX_%s (offset 0x%03x) =" \
				" 0x%08x\n", __stringify(r), \
				offset__AUD_SPDIF_RX_##r(conv), \
				get__AUD_SPDIF_RX_##r(conv))

static void snd_stm_conv_spdif_rx_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_conv_spdif_rx *conv = entry->private_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	snd_iprintf(buffer, "--- %s ---\n", conv->bus_id);
	snd_iprintf(buffer, "base = 0x%p\n", conv->base);

	DUMP_REGISTER(CTRL0);
	DUMP_REGISTER(CTRL1);
	DUMP_REGISTER(CTRL2);
	DUMP_REGISTER(RESET);
	DUMP_REGISTER(TEST_CTRL);
	DUMP_REGISTER(SAMP_FREQ);
	DUMP_REGISTER(DETECT_MAX);
	DUMP_REGISTER(CH_STATUS0);
	DUMP_REGISTER(CH_STATUS1);
	DUMP_REGISTER(CH_STATUS2);
	DUMP_REGISTER(CH_STATUS3);
	DUMP_REGISTER(CH_STATUS4);
	DUMP_REGISTER(CH_STATUS5);
	DUMP_REGISTER(CH_STATUS6);
	DUMP_REGISTER(CH_STATUS7);
	DUMP_REGISTER(CH_STATUS8);
	DUMP_REGISTER(CH_STATUS9);
	DUMP_REGISTER(CH_STATUSA);
	DUMP_REGISTER(CH_STATUSB);
	DUMP_REGISTER(EVENT_STATUS16);
	DUMP_REGISTER(NCO_CTRL);
	DUMP_REGISTER(NCO_INCR0);
	DUMP_REGISTER(NCO_INCR1);

	snd_iprintf(buffer, "\n");
}


/*
 * Platform driver routines
 */

static int snd_stm_conv_spdif_rx_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_conv_spdif_rx_info *info = pdev->dev.platform_data;
	struct snd_stm_conv_spdif_rx *conv;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	BUG_ON(!card);
	BUG_ON(!info);

	/* Allocate internal structure */

	conv = kzalloc(sizeof(*conv), GFP_KERNEL);
	if (!conv) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(conv);
	conv->bus_id = dev_name(&pdev->dev);
	conv->device = &pdev->dev;

	/* Get resources */

	result = snd_stm_memory_request(pdev, &conv->mem_region, &conv->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	result = snd_stm_irq_request(pdev, &conv->irq,
			snd_stm_conv_spdif_rx_irq_handler, conv);
	if (result < 0) {
		snd_stm_printe("IRQ request failed!\n");
		goto error_irq_request;
	}

	if (info->pad_config) {
		conv->pads = stm_pad_claim(info->pad_config, conv->bus_id);
		if (!conv->pads) {
			snd_stm_printe("Failed to claimed pads for '%s'!\n",
					conv->bus_id);
			result = -EBUSY;
			goto error_pad_claim;
		}
	}

	conv->clock = clk_get(conv->device, "spdif_rx_clk");
	if (!conv->clock || IS_ERR(conv->clock)) {
		snd_stm_printe("Failed to get clock\n");
		return -EINVAL;
	}

	BUG_ON(info->clock_rate != 48000);
	conv->clock_rate = info->clock_rate;

	/* Initialise hardware */

	set__AUD_SPDIF_RX_CTRL0__RESET_FIFO_RESET(conv);
	set__AUD_SPDIF_RX_CTRL0__INSTR_DISABLE(conv);
	set__AUD_SPDIF_RX_CTRL0__DISABLE(conv);
	set__AUD_SPDIF_RX_RESET__RESET_ENABLE(conv);
	set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RESET(conv);

	/* Register procfs entry */

	snd_stm_info_register(&conv->proc_entry,
			conv->bus_id,
			snd_stm_conv_spdif_rx_dump_registers,
			conv);

	/* Register converter */

	BUG_ON(!info->source_bus_id);
	snd_stm_printd(0, "The SPDIF RX is attached to uniperipheral reader "
			"'%s'.\n", info->source_bus_id);

	conv->converter = snd_stm_conv_register_converter(
			"SPDIF RX",
			&snd_stm_conv_spdif_rx_ops, conv,
			&platform_bus_type, info->source_bus_id,
			info->channel_from,
			info->channel_to,
			&conv->index);

	if (!conv->converter) {
		snd_stm_printe("Can't attach to uniperipheral reader!\n");
		result = -EINVAL;
		goto error_attach;
	}

	/* Finished */

	platform_set_drvdata(pdev, conv);

	return 0;

error_attach:
	if (conv->pads)
		stm_pad_release(conv->pads);
error_pad_claim:
	snd_stm_irq_release(conv->irq, conv);
error_irq_request:
	snd_stm_memory_release(conv->mem_region, conv->base);
error_memory_request:
	snd_stm_magic_clear(conv);
	kfree(conv);
error_alloc:
	return result;
}

static int snd_stm_conv_spdif_rx_remove(struct platform_device *pdev)
{
	struct snd_stm_conv_spdif_rx *conv = platform_get_drvdata(pdev);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	/* Unregister converter */

	snd_stm_conv_unregister_converter(conv->converter);

	/* Unregister procfs entry */

	snd_stm_info_unregister(conv->proc_entry);

	/* Release resources */

	clk_put(conv->clock);

	if (conv->pads)
		stm_pad_release(conv->pads);

	snd_stm_irq_release(conv->irq, conv);
	snd_stm_memory_release(conv->mem_region, conv->base);

	/* Free internal structure */

	snd_stm_magic_clear(conv);
	kfree(conv);

	return 0;
}

static struct platform_driver snd_stm_conv_spdif_rx_driver = {
	.driver.name = "snd_conv_spdif_rx",
	.probe = snd_stm_conv_spdif_rx_probe,
	.remove = snd_stm_conv_spdif_rx_remove,
};



/*
 * Initialization
 */

static int __init snd_stm_conv_spdif_rx_init(void)
{
	return platform_driver_register(&snd_stm_conv_spdif_rx_driver);
}

static void __exit snd_stm_conv_spdif_rx_exit(void)
{
	platform_driver_unregister(&snd_stm_conv_spdif_rx_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics SPDIF RX driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_conv_spdif_rx_init);
module_exit(snd_stm_conv_spdif_rx_exit);
