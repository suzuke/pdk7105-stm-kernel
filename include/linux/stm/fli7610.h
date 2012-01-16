/*
 * Copyright (c) 2010 STMicroelectronics Limited
 * Author: Srinivas.Kandagatla <srinivas.kandagatla@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_STM_FLI7610_H
#define __LINUX_STM_FLI7610_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>



/*
 * ARM and ST40 interrupts are virtually identical, so we can use the same
 * parameter for both. Only mailbox and some A/V interrupts are connected
 * to the ST200's, however 4 ILC outputs are available, which could be
 * used if required.
 */
#if defined(CONFIG_SUPERH)
#define FLI7610_IRQ(irq) ILC_IRQ(irq)
#elif defined(CONFIG_ARM)
#define FLI7610_IRQ(irq) ((irq)+32)
#endif

#define FLI7610_RESOURCE_IRQ(_irq) \
	{ \
		.start = FLI7610_IRQ(_irq), \
		.end = FLI7610_IRQ(_irq),	\
		.flags = IORESOURCE_IRQ, \
	}

#define FLI7610_RESOURCE_IRQ_NAMED(_name, _irq)	\
	{ \
		.start = FLI7610_IRQ(_irq), \
		.end = FLI7610_IRQ(_irq), \
		.name = (_name), \
		.flags = IORESOURCE_IRQ, \
	}

#ifndef CONFIG_ARM
#define IO_ADDRESS(x) 0
#endif


/* MPE SYSCONF 400 - 686*/
#define MPE_SYSCONFG_GROUP(x) \
	(((x)/100)+1)

#define MPE_SYSCONF_OFFSET(x) \
	((x) % 100)

#define MPE_SYSCONF(x) \
	MPE_SYSCONFG_GROUP(x), MPE_SYSCONF_OFFSET(x)

/* TAE SYSCONF 0 - 473 */
#define TAE_SYSCONFG_GROUP(x) \
	(((x) < 100) ? 0 : ((((x) > 299) && ((x) <= 445)) ? 3 : (((x)/100))))

#define TAE_SYSCONF_OFFSET(x) \
	(((x) >= 450 && (x) < 474) ? ((x) - 450) : ((x) % 100))

#define TAE_SYSCONF(x) \
	TAE_SYSCONFG_GROUP(x), TAE_SYSCONF_OFFSET(x)


#define FLI7610_PIO(x) \
	(((x) < 100) ? (x) : ((x)-100+19))

#define LPM_SYSCONF_BANK	(8)
#define LPM_SYSCONF(x) LPM_SYSCONF_BANK, x


struct fli7610_pio_config {
	struct stm_pio_control_mode_config *mode;
	struct stm_pio_control_retime_config *retime;
};


void fli7610_early_device_init(void);

struct fli7610_asc_config {
	int hw_flow_control;
	int is_console;
	int force_m1;
};
void fli7610_configure_asc(int asc, struct fli7610_asc_config *config);
void fli7610_configure_usb(int port);

#define FLI7610_SBC_SSC(num)		(num + 3)
#define FLI7610_SSC(num)		(num)

struct fli7610_ssc_config {
	void (*spi_chipselect)(struct spi_device *spi, int is_on);
};
/* Use the above macros while passing SSC number. */
int fli7610_configure_ssc_spi(int ssc, struct fli7610_ssc_config *config);
int fli7610_configure_ssc_i2c(int ssc);
void fli7610_configure_lirc(void);
struct fli7610_pwm_config {
	enum {
		fli7610_tae_pwm = 0,
		fli7610_sbc_pwm
	} pwm;
	int enabled[STM_PLAT_PWM_NUM_CHANNELS];
};
void fli7610_configure_pwm(struct fli7610_pwm_config *config);

void fli7610_reset(char mode);

#endif
