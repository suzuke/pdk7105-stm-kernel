/*
 * STMicroelectronics  Copyright (C) 2012
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/libelf.h>

#define COPROC_BPA2_NAME "coproc"
#define COPROC_BPA2_NAME_LEN 32		/* eg "coproc_video0" */

#define COPROC_FIRMWARE_NAME "st_firmware"
#define COPROC_FIRMWARE_NAME_LEN 32	/* eg "st_firmware_<SoC>_video0.elf" */

struct coproc_fns;

enum coproc_state {
	coproc_state_idle,
	coproc_state_running,
	coproc_state_paused,
};

struct coproc {
	const char *name;
	int id;
	enum coproc_state state;

	struct bpa2_part *bpa2_partition;
	u_long	    ram_phys;		/* Coprocessor RAM physical address */
	u_long	    ram_size;		/* Coprocessor RAM size (in bytes)  */
	unsigned long bpa2_alloc;	/* Start of allocated BPS2 memory */

	struct device *parent;
	struct device *dev;
	struct coproc_fns *fns;
	dev_t devt;
};

struct coproc_fns {
	Elf32_Half machine;
	int (*check_elf)(struct coproc *cop, struct ELF32_info *elfinfo);
	int (*cpu_grant)(struct coproc *cop, unsigned long boot_addr);
	int (*mode)(struct coproc *cop, int mode);
};

#define coproc_printk(level, cop, format, arg...) \
	dev_printk(level, cop->dev, format, ##arg)
#define coproc_err(cop, format, arg...) \
	coproc_printk(KERN_ERR, cop, format , ## arg)
#define coproc_warn(cop, format, arg...) \
	coproc_printk(KERN_WARNING, cop, format , ## arg)
#define coproc_info(cop, format, arg...) \
	coproc_printk(KERN_INFO, cop, format , ## arg)
#ifdef CONFIG_COPROCESSOR_DEBUG
#define coproc_dbg(cop, format, arg...) \
	coproc_printk(KERN_DEBUG, cop, format , ## arg)
#else
#define coproc_dbg(cop, format, arg...) do {} while (0)
#endif

/* coprocessor driver interface */
int coproc_device_remove(struct coproc *cop);
int coproc_device_add(struct coproc *cop);

/* coprocessor reset bypass controller interface */
void coproc_reset_bypass_pre(void);
void coproc_reset_bypass_post(void);
