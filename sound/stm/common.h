/*
 * STMicroelectronics' System-on-Chips audio subsystem commons
 */

#ifndef __SOUND_STM_COMMON_H
#define __SOUND_STM_COMMON_H

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/stringify.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/info.h>
#include <sound/control.h>
#include <sound/stm.h>



/*
 * ALSA card management
 */

enum snd_stm_card_type {
	SND_STM_CARD_TYPE_AUDIO,
#if defined(CONFIG_SND_STM_TELSS) || defined(CONFIG_SND_STM_TELSS_MODULE)
	SND_STM_CARD_TYPE_TELSS,
#endif
	SND_STM_CARD_TYPE_COUNT,	/* Number of different card types */
	SND_STM_CARD_TYPE_ALL		/* Used by snd_stm_card_register() */
};

#define SND_STM_CARDS_INIT_AUDIO(cards) \
	cards[SND_STM_CARD_TYPE_AUDIO].name = "AUDIO"; \
	cards[SND_STM_CARD_TYPE_AUDIO].subsystem = "audio";

#ifdef CONFIG_SND_STM_TELSS
#define SND_STM_CARDS_INIT_TELSS(cards) \
	cards[SND_STM_CARD_TYPE_TELSS].name = "TELSS"; \
	cards[SND_STM_CARD_TYPE_TELSS].subsystem = "telss";
#else
#define SND_STM_CARDS_INIT_TELSS(cards)
#endif

int snd_stm_card_register(enum snd_stm_card_type card_type);
int snd_stm_card_is_registered(enum snd_stm_card_type card_type);
struct snd_card *snd_stm_card_get(enum snd_stm_card_type card_type);



/*
 * Converters (DAC, ADC, I2S-SPDIF etc.) control interface
 */

struct snd_stm_conv_source;
struct snd_stm_conv_group;
struct snd_stm_conv_converter;

struct snd_stm_conv_source *snd_stm_conv_register_source(struct bus_type *bus,
		const char *bus_id, int channels_num,
		struct snd_card *card, int card_device);
int snd_stm_conv_unregister_source(struct snd_stm_conv_source *source);

int snd_stm_conv_get_card_device(struct snd_stm_conv_converter *converter);

struct snd_stm_conv_group *snd_stm_conv_request_group(
		struct snd_stm_conv_source *source);
int snd_stm_conv_release_group(struct snd_stm_conv_group *group);

const char *snd_stm_conv_get_name(struct snd_stm_conv_group *group);
unsigned int snd_stm_conv_get_format(struct snd_stm_conv_group *group);
int snd_stm_conv_get_oversampling(struct snd_stm_conv_group *group);

int snd_stm_conv_enable(struct snd_stm_conv_group *group,
		int channel_from, int channel_to);
int snd_stm_conv_disable(struct snd_stm_conv_group *group);
int snd_stm_conv_mute(struct snd_stm_conv_group *group);
int snd_stm_conv_unmute(struct snd_stm_conv_group *group);
int snd_stm_conv_init(void);
void snd_stm_conv_exit(void);


/*
 * Sound Clocks control interface
 * snd_stm_clk is a wrapper around the standard clk
 * this API is required to guarantee balanced
 * clk_enable/clk_disable as the standard clk framework needs
 */
struct snd_stm_clk;
struct snd_stm_clk *snd_stm_clk_get(struct device *dev, const char *id,
		struct snd_card *card, int card_device);
void snd_stm_clk_put(struct snd_stm_clk *clk);
int snd_stm_clk_enable(struct snd_stm_clk *clk);
int snd_stm_clk_disable(struct snd_stm_clk *clk);
int snd_stm_clk_set_rate(struct snd_stm_clk *clk, unsigned long rate);



/*
 * PCM buffer memory management
 */

struct snd_stm_buffer;

struct snd_stm_buffer *snd_stm_buffer_create(struct snd_pcm *pcm,
		struct device *device, int prealloc_size);
#define snd_stm_buffer_dispose(buffer)

int snd_stm_buffer_is_allocated(struct snd_stm_buffer *buffer);

int snd_stm_buffer_alloc(struct snd_stm_buffer *buffer,
		struct snd_pcm_substream *substream, int size);
void snd_stm_buffer_free(struct snd_stm_buffer *buffer);

int snd_stm_buffer_mmap(struct snd_pcm_substream *substream,
		struct vm_area_struct *area);



/*
 * Common ALSA controls routines
 */

int snd_stm_ctl_boolean_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo);

int snd_stm_ctl_iec958_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo);

int snd_stm_ctl_iec958_mask_get_con(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

int snd_stm_ctl_iec958_mask_get_pro(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol);

int snd_stm_iec958_cmp(const struct snd_aes_iec958 *a,
		const struct snd_aes_iec958 *b);



/*
 * Common ALSA parameters constraints
 */

int snd_stm_pcm_transfer_bytes(unsigned int bytes_per_frame,
		unsigned int max_transfer_bytes);
int snd_stm_pcm_hw_constraint_transfer_bytes(struct snd_pcm_runtime *runtime,
		unsigned int max_transfer_bytes);



/*
 * ALSA procfs additional entries
 */

int snd_stm_info_register(struct snd_info_entry **entry,
		const char *name,
		void (read)(struct snd_info_entry *, struct snd_info_buffer *),
		void *private_data);
void snd_stm_info_unregister(struct snd_info_entry *entry);



/*
 * Resources management
 */

int snd_stm_memory_request(struct platform_device *pdev,
		struct resource **mem_region, void **base_address);
#define snd_stm_memory_release(mem_region, base_address)

int snd_stm_irq_request(struct platform_device *pdev,
		unsigned int *irq, irq_handler_t handler, void *dev_id);
#define snd_stm_irq_release(irq, dev_id)



/*
 * Debug features
 */

/* Data dump functions */

void snd_stm_hex_dump(void *data, int size);
void snd_stm_iec958_dump(const struct snd_aes_iec958 *vuc);

/* Debug messages */

#if defined(CONFIG_SND_DEBUG) || defined(DEBUG)

#define snd_stm_printd(level, format, args...) \
		do { \
			if (level <= snd_stm_debug_level) \
				printk(KERN_DEBUG "%s:%d: " format, \
						__FILE__, __LINE__, ##args); \
		} while (0)

#else

#define snd_stm_printd(...) /* nothing */

#endif

/* Error messages */

#define snd_stm_printe(format, args...) \
		printk(KERN_ERR "%s:%d: " format, __FILE__, __LINE__, ##args)

/* Magic value checking in device structures */

#if defined(CONFIG_SND_DEBUG) || defined(DEBUG)

#ifndef snd_stm_magic
#define snd_stm_magic 0xf00d
#endif

enum snd_stm_magic_enum {
	snd_stm_magic_good = 0x600d0000 | snd_stm_magic,
	snd_stm_magic_bad = 0xbaad0000 | snd_stm_magic,
};

#define snd_stm_magic_field \
		enum snd_stm_magic_enum __snd_stm_magic
#define snd_stm_magic_set(object) \
		((object)->__snd_stm_magic) = snd_stm_magic_good
#define snd_stm_magic_clear(object) \
		((object)->__snd_stm_magic) = snd_stm_magic_bad
#define snd_stm_magic_valid(object) \
		((object)->__snd_stm_magic == snd_stm_magic_good)

#else

#define snd_stm_magic_field
#define snd_stm_magic_set(object)
#define snd_stm_magic_clear(object)
#define snd_stm_magic_valid(object) 1

#endif



#endif
