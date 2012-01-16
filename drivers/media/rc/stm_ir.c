/*
* LIRC driver for the STMicroelectronics IRDA devices
*
* Copyright (C) 2004-2012 STMicroelectronics
*
* June 2004:  first implementation for a 2.4 Linux kernel
*             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
* Marc 2005:  review to support pure raw mode and to adapt to Linux 2.6
*             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
* June 2005:  Change to a MODE2 receive driver and made into a generic
*             ST driver.
*             Carl Shaw <carl.shaw@st.com>
* July 2005:  fix STB7100 MODE2 implementation and improve performance
*             of STm8000 version. <carl.shaw@st.com>
* Aug  2005:  Added clock autoconfiguration support.  Fixed module exit code.
* 	       Added UHF support (kernel build only).
* 	       Carl Shaw <carl.shaw@st.com>
* Sep  2005:  Added first transmit support
*             Added ability to set rxpolarity register
* 	       Angelo Castello <angelo.castello@st.com>
* 	       and Carl Shaw <carl.shaw@st.com>
* Oct  2005:  Added 7100 transmit
*             Added carrier width configuration
* 	       Carl Shaw <carl.shaw@st.com>
* Sept 2006:  Update:
* 		fix timing issues (bugzilla 764)
* 		Thomas Betker <thomas.betker@siemens.com>
* 		allocate PIO pins in driver
* 		update transmit
* 		improve fault handling on init
* 		Carl Shaw <carl.shaw@st.com>
* Oct  2007:  Added both lirc-0.8.2 common interface and integrated out IRB driver
*             to be working for linux-2.6.23-rc7. Removed old platform support...
*             Sti5528 STb8000. Added new IR rx intq mechanism to reduce the amount
*             intq needed to identify one button. Fix TX transmission loop setting up
*             correctly the irq clean register.
* 	       Angelo Castello <angelo.castello@st.com>
* Nov  2007:  Moved here all the platform
*             dependences leaving clear of this task the common interface. (lirc_dev.c)
*	       Code cleaning and optimization.
* 	       Angelo Castello <angelo.castello@st.com>
* Dec  2007:  Added device resource management support.
* 	       Angelo Castello <angelo.castello@st.com>
* Mar  2008:  Fix UHF support and general tidy up
*             Carl Shaw <carl.shaw@st.com>
* Mar  2008:  Fix insmod/rmmod actions. Added new PIO allocate mechanism
*	       based on platform PIOs dependencies values (LIRC_PIO_ON,
*	       LIRC_IR_RX, LIRC_UHF_RX so on )
* 	       Angelo Castello <angelo.castello@st.com>
* Apr  2008:  Added SCD support
* 	       Angelo Castello <angelo.castello@st.com>
*	       Carl Shaw <carl.shaw@st.com>
* Feb  2009:  Added PM capability
* 	       Angelo Castello <angelo.castello@st.com>
*	       Francesco Virlinzi <francesco.virlinzi@st.com>
* Jul  2009:  ported for kernel 2.6.30 from lirc-0.8.5 project.
*	       Updated code to manage SCD values incoming from kconfig.
*	       Default SCD code is for the Futarque RC.
* 	       Angelo Castello <angelo.castello@st.com>
* Apr  2010:  Fixed the following SCD issues:
*             Q1: to manage rightly border-line symbols
*             Q2: to add others constrains for safe initialization.
*             Q3: to manage potential contiguous Pulse/Space when the SCD
*                 programming is not aligned to leading/trailing edge
*             Q4: to rebuild rightly the incoming signals train filtered by SCD
*                 when detected from SCD_CODE or from SCD_ALT_CODE.
*             Angelo Castello <angelo.castello@st.com>
* Apr  2010:  Code review and optimizations to reduce the driver initialization
*             time. All IRB capability are selectable from menuconfig.
*             Angelo Castello <angelo.castello@st.com>
* Feb	2011:  Fixed IR-RX handler routine to manage the input data overrun.
*             Angelo Castello <angelo.castello@st.com>
* Jan 	2012:  Forwared ported stm-lirc driver to linux 3.0 interface.
* 	     - Now driver has capability to generate linux input events.
* 	     - And the driver also capable of providing old LIRC interface too.
* 	     - Cleanup the driver and remove most of the global variables.*
* 	      Srinivas Kandagatla<srinivas.kandagatla@st.com>
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <media/lirc.h>
#include <linux/err.h>
#include <linux/stm/platform.h>
#include <linux/stm/clk.h>
#include <media/rc-core.h>
#include "stm_ir.h"

#define IR_STM_NAME "lirc-stm"


/* General debugging */
#ifdef CONFIG_LIRC_STM_DEBUG
#define DPRINTK(fmt, args...) \
	pr_debug(IR_STM_NAME ": %s: " fmt, __func__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 * LiRC data
 */

#define RX_CLEAR_IRQ(dev, x)	writel((x), dev->rx_base + IRB_RX_INT_CLEAR)

#define RX_WORDS_IN_FIFO_OR_OVERRUN(dev)	\
			(readl(dev->rx_base + IRB_RX_STATUS) & 0xff04)

/* RX graphical example to better understand the difference between ST IR block
 * output and standard definition used by LIRC (and most of the world!)
 *
 *           mark                                     mark
 *      |-IRB_RX_ON-|                            |-IRB_RX_ON-|
 *      ___  ___  ___                            ___  ___  ___             _
 *      | |  | |  | |                            | |  | |  | |             |
 *      | |  | |  | |         space 0            | |  | |  | |   space 1   |
 * _____| |__| |__| |____________________________| |__| |__| |_____________|
 *
 *      |--------------- IRB_RX_SYS -------------|------ IRB_RX_SYS -------|
 *
 *      |------------- encoding bit 0 -----------|---- encoding bit 1 -----|
 *
 * ST hardware returns mark (IRB_RX_ON) and total symbol time (IRB_RX_SYS), so
 * convert to standard mark/space we have to calculate space=(IRB_RX_SYS - mark)
 * The mark time represents the amount of time the carrier (usually 36-40kHz)
 * is detected.
 *
 * TX is the same but with opposite calculation.
 *
 * The above examples shows Pulse Width Modulation encoding where bit 0 is
 * represented by space>mark.
 */

/*
 * IRB UHF-SCD filter configuration
 */
#ifdef CONFIG_LIRC_STM_UHF_SCD

/* Start Code Detect (SCD) graphical example to understand how to configure
 * propely the code, code length and nominal time values based on Remote Control
 * signals train example.
 *
 *      __________________        ________      _______  ___  ___  ___
 *      |                |        |      |      |     |  | |  | |  | |
 *      |                |        |      |      |     |  | |  | |  | |
 * _____|                |________|      |______|     |__| |__| |__| |___......
 *
 *      |---- 1000us ----|- 429 -|- 521 -|- 500-|....
 *      |-- 500 -|- 500 -|- 500 -|- 500 -|  units in us for SCD code.
 *      |--  1  -|-  1  -|-  0  -|-  1  -|  SCD code Ob1101.
 *
 * The nominal symbol duration is 500us, code length 4 and code Ob1101.
 */

#define LIRC_STM_SCD_BUFSIZE            ((LIRC_STM_SCD_MAX_SYMBOLS) * \
						sizeof(struct ir_raw_event))
static struct ir_scd_s scd_s = {
	.code = CONFIG_LIRC_STM_UHF_SCD_CODE,
	.alt_code = CONFIG_LIRC_STM_UHF_SCD_ALTCODE,
	.nomtime = CONFIG_LIRC_STM_UHF_SCD_NTIME,
	.noiserecov = 0,
};

/* SCD support */
struct ir_stm_scd_data_s {
	struct ir_scd_s value;
	int flags;
	struct ir_raw_event *prefix_code;
	struct ir_raw_event *prefix_altcode;
	unsigned int off_prefix_code;
	unsigned int off_prefix_altcode;
};

static struct ir_stm_scd_data_s scd;	/* SCD data config */
#endif /* CONFIG_LIRC_STM_UHF_SCD */

/*
 * IRB TX transmitter configuration
 */
#ifdef CONFIG_LIRC_STM_TX

#define TX_FIFO_USED(dev)	((readl(dev->base + IRB_TX_STATUS) >> 8) & 0x07)

static inline unsigned int ir_stm_time_to_cycles(struct stm_ir_device *dev,
			unsigned int microsecondtime)
{
	/* convert a microsecond time to the nearest number of subcarrier clock
	 * cycles
	 */
	microsecondtime *= dev->tx_mult;
	microsecondtime /= dev->tx_div;
	return microsecondtime * TX_CARRIER_FREQ / 1000000;
}

/* transmit out IR pulses; what you get here is a batch of alternating
 * pulse/space/pulse/space lengths that we should write out completely through
 * the FIFO, blocking on a full FIFO */
static int stm_ir_transmit(struct rc_dev *rdev, unsigned *buf, unsigned n)
{
	struct stm_ir_device *dev = rdev->priv;
	int i;
	unsigned int symbol, mark;
	int fifosyms;

	dev->tx_wbuf = buf;
	dev->tx_size = n;
	dev->tx_done = false;

	if (!dev->pdata->txenabled) {
		pr_err(IR_STM_NAME
		       ": transmit unsupported.\n");
		return -ENOTSUPP;
	}
	/* Wait for transmit to become free... */
	if (wait_event_interruptible(dev->tx_waitq, dev->off_wbuf == 0))
		return -ERESTARTSYS;

	/* load the first words into the FIFO */
	fifosyms = n;
	if (fifosyms > TX_FIFO_DEPTH)
		fifosyms = TX_FIFO_DEPTH;

	for (i = 0; i < fifosyms; i++) {
		mark = dev->tx_wbuf[(i * 2)];
		symbol = mark + dev->tx_wbuf[(i * 2) + 1];
		DPRINTK("TX raw m %d s %d ", mark, symbol);

		mark = ir_stm_time_to_cycles(dev, mark) + 1;
		symbol = ir_stm_time_to_cycles(dev, symbol) + 2;
		DPRINTK("cal m %d s %d\n", mark, symbol);

		dev->off_wbuf++;
		dev->tx_size -= 2;
		writel(mark, dev->base + IRB_TX_ONTIME);
		writel(symbol, dev->base + IRB_TX_SYMPERIOD);
	}

	/* enable the transmit */
	writel(0x07, dev->base + IRB_TX_INT_ENABLE);
	writel(0x01, dev->base + IRB_TX_ENABLE);
	DPRINTK("TX enabled\n");
	wait_event_interruptible(dev->tx_waitq, dev->tx_done == true);
	return n;
}
/* outside interface: set transmitter mask */
static void ir_stm_tx_calc_clocks(struct stm_ir_device *dev,
				unsigned int clockfreq,
				unsigned int subwidthpercent)
{
	/*  We know the system base clock and the required IR carrier frequency
	 *  We now want a divisor of the system base clock that gives the
	 *  nearest integer multiple of the carrier frequency
	 */

	const unsigned int clkratio = clockfreq / TX_CARRIER_FREQ;
	unsigned int scalar, n;
	int delta;
	unsigned int diffbest = clockfreq, nbest = 0, scalarbest = 0;
	unsigned int nmin = clkratio / 255;

	if ((nmin & 0x01) == 1)
		nmin++;

	for (n = nmin; n < clkratio; n += 2) {
		scalar = clkratio / n;
		if ((scalar & 0x01) == 0 && scalar != 0) {
			delta = clockfreq - (scalar * TX_CARRIER_FREQ * n);
			if (delta < 0)
				delta *= -1;

			if (delta < diffbest) {	/* better set of parameters ? */
				diffbest = delta;
				nbest = n;
				scalarbest = scalar;
			}
			if (delta == 0)	/* an exact multiple */
				break;
		}
	}

	scalarbest /= 2;
	nbest *= 2;

	DPRINTK("TX clock scalar = %d\n", scalarbest);
	DPRINTK("TX subcarrier scalar = %d\n", nbest);

	/*  Set the registers now  */
	writel(scalarbest, dev->base + IRB_TX_PRESCALAR);
	writel(nbest, dev->base + IRB_TX_SUBCARRIER);
	writel(nbest * subwidthpercent / 100,
			dev->base + IRB_TX_SUBCARRIER_WIDTH);

	/*  Now calculate timing to subcarrier cycles factors which compensate
	 *  for any remaining difference between our clock ratios and real times
	 *  in microseconds
	 */

	if (diffbest == 0) {
		/* no adjustment required - our clock is running at the required
		 * speed */
		dev->tx_mult = 1;
		dev->tx_div = 1;
	} else {
		/* adjustment is required */
		delta = scalarbest * TX_CARRIER_FREQ * nbest;
		dev->tx_mult = delta / (clockfreq / 10000);

		if (delta < clockfreq) {/* our clock is running too fast */
			DPRINTK("clock running slow at %d\n", delta);
			dev->tx_div = dev->tx_mult;
			dev->tx_mult = 10000;
		} else {	/* our clock is running too slow */

			DPRINTK("clock running fast at %d\n", delta);
			dev->tx_div = 10000;
		}
	}

	DPRINTK("TX fine adjustment mult = %d\n", dev->tx_mult);
	DPRINTK("TX fine adjustment div  = %d\n", dev->tx_div);
	init_waitqueue_head(&dev->tx_waitq);
}

static void ir_stm_tx_interrupt(int irq, void *data)
{
	unsigned int symbol, mark, done = 0;
	struct stm_ir_device *dev = data;
	unsigned int tx_irq_status = readl(dev->base + IRB_TX_INT_STATUS);

	if ((tx_irq_status & TX_INT_PENDING) != TX_INT_PENDING)
		return;

	while (done == 0) {
		if (unlikely((readl(dev->base + IRB_TX_INT_STATUS)
				& TX_INT_UNDERRUN) == TX_INT_UNDERRUN)) {
			/* There has been an underrun - clear flag, switch
			 * off transmitter and signal possible exit
			 */
			pr_err(IR_STM_NAME ": transmit underrun!\n");
			writel(0x02, dev->base + IRB_TX_INT_CLEAR);
			writel(0x00, dev->base + IRB_TX_INT_ENABLE);
			writel(0x00, dev->base + IRB_TX_ENABLE);
			done = 1;
			DPRINTK("disabled TX\n");
			wake_up_interruptible(&dev->tx_waitq);
		} else {
			int fifoslots = TX_FIFO_USED(dev);
			while (fifoslots < TX_FIFO_DEPTH) {
				mark = dev->tx_wbuf[(dev->off_wbuf * 2)];
				dev->tx_size--;
				if (dev->tx_size == 0 || dev->tx_size == 1) {
					mark = ir_stm_time_to_cycles(dev, mark)
						+ 1;

					/* Dump out last symbol */
					writel(mark * 2,
						dev->base + IRB_TX_SYMPERIOD);
					writel(mark, dev->base + IRB_TX_ONTIME);
					DPRINTK("TX end m %d s %d\n",
						mark, mark * 2);

					/* flush transmit fifo */
					while (TX_FIFO_USED(dev) != 0)

					;
					writel(0,
						dev->base + IRB_TX_SYMPERIOD);
					writel(0, dev->base + IRB_TX_ONTIME);
					/* spin until TX fifo empty */
					while (TX_FIFO_USED(dev) != 0)

					;
					/* disable tx interrupts and
					 * transmitter */
					writel(0x07,
						dev->base + IRB_TX_INT_CLEAR);
					writel(0x00,
						dev->base + IRB_TX_INT_ENABLE);
					writel(0x00,
						dev->base + IRB_TX_ENABLE);
					DPRINTK("TX disabled\n");
					dev->off_wbuf = 0;
					fifoslots = 999;
					done = 1;
					dev->tx_done = true;
					wake_up_interruptible(&dev->tx_waitq);
				} else {
					symbol = mark + dev->tx_wbuf[
						(dev->off_wbuf * 2) + 1];
					mark = ir_stm_time_to_cycles(dev,
							mark) + 1;
					symbol = ir_stm_time_to_cycles(dev,
							symbol) + 2;
					dev->tx_size--;
					writel(symbol,
						dev->base + IRB_TX_SYMPERIOD);
					writel(mark,
						dev->base + IRB_TX_ONTIME);

					DPRINTK("Nm %d s %d\n", mark, symbol);

					dev->off_wbuf++;
					fifoslots = TX_FIFO_USED(dev);
				}
			}
		}
	}
}
#else
static void ir_stm_tx_interrupt(int irq, void *data)
{
		return; /* NA */
}
static void ir_stm_tx_calc_clocks(struct stm_ir_device *dev,
				unsigned int clockfreq,
				unsigned int subwidthpercent)
{
		return; /* NA */
}
#endif /* CONFIG_LIRC_STM_TX */

#ifdef CONFIG_LIRC_STM_UHF_SCD
static void ir_stm_scd_prefix_symbols(struct stm_ir_device *dev,
					struct ir_raw_event *first_ev)
{
	struct ir_raw_event *lscd;
	unsigned int *offscd;
	int i;

	if (!scd.flags)
		return;

	DPRINTK("SCD_STA(0x%x)\n", scd.flags & (SCD_ALTERNATIVE | SCD_NORMAL));
	/* Here need to take care the following SCD constrain:
	 * If there is only one start code to be detected, the registers for
	 * normal and alternative start codes has been programmed with
	 * identical values. The low significate bits of scd_flags means:
	 *      1    : SCD_ENABLED
	 *       1   : SCD_NOR_EQ_ALT
	 *        1  : SCD_ALTERNATIVE
	 *         1 : SCD_NORMAL
	 *      11xx : normal and alternative are the same due to constrain.
	 *      1001 : normal detected
	 *      1011 : alternative detected
	 */
	if ((scd.flags & SCD_NOR_EQ_ALT) || (scd.flags ^ SCD_ALT_MASK)) {
		/* normal code detected */
		lscd = scd.prefix_code;
		offscd = &scd.off_prefix_code;
	} else {
		/* alternative code detected */
		lscd = scd.prefix_altcode;
		offscd = &scd.off_prefix_altcode;
	}

	/* manage potential contiguous Pulse/Space when the SCD programming
	 * is not aligned to leading/trailing edge
	 */
	if ((lscd[*offscd - 1].pulse) == (first_ev->pulse)) {
		first_ev->duration = first_ev->duration +
					lscd[*offscd - 1].duration;
		(*offscd)--;
	}

	for (i = 0; i < *offscd; i++)
		ir_raw_event_store_with_filter(dev->rdev, &lscd[i]);
	return;
}

static int ir_stm_scd_set(struct stm_ir_device *dev, char enable)
{
	if (!scd.flags)
		return -1;

	if (enable) {
		writel(0x01, dev->base + IRB_SCD_INT_EN);
		writel(0x01, dev->base + IRB_SCD_INT_CLR);
		writel(0x01, dev->base + IRB_SCD_CFG);
	} else {
		writel(0x00, dev->base + IRB_SCD_INT_EN);
		writel(0x00, dev->base + IRB_SCD_CFG);
	}
	DPRINTK("SCD %s\n", (enable ? "enabled" : "disabled"));
	return 0;
}

static int ir_stm_scd_config(struct stm_ir_device *dev, unsigned long clk)
{
	unsigned int nrec, ival, scwidth;
	unsigned int prescalar, tolerance;
	unsigned int space, mark;
	int i, j, k;
	struct ir_raw_event *lscd;
	unsigned int *offscd;
	unsigned int code, codelen, alt_codelen, curr_codelen;
	struct ir_scd_s last_scd = scd.value;

	if (!dev->pdata->rxuhfmode) {
		pr_err(IR_STM_NAME
		       ": SCD not available in IR-RX mode. Not armed\n");
		return -ENOTSUPP;
	}

	scd.value = scd_s;
	scd.flags = 0;
	scd.off_prefix_code = 0;
	scd.off_prefix_altcode = 0;

	codelen = fls(scd.value.code);
	if ((scd.value.code == 0) ||
	    (codelen > LIRC_STM_SCD_MAX_SYMBOLS) || (codelen < 1)) {
		pr_err(IR_STM_NAME ": SCD invalid start code. Not armed\n");
		scd.value = last_scd;
		return -EINVAL;
	}

	alt_codelen = fls(scd.value.alt_code);
	if (alt_codelen > 0) {
		if ((scd.value.alt_code == 0) ||
		    (alt_codelen > LIRC_STM_SCD_MAX_SYMBOLS) ||
		    (alt_codelen < codelen) ||
		    (scd.value.code == (scd.value.alt_code >>
					(alt_codelen - codelen)))) {
			pr_err(IR_STM_NAME
			       ": SCD invalid alternative code. Not armed\n");
			alt_codelen = 0;
		}
	}

	/* SCD disable */
	writel(0x00, dev->base + IRB_SCD_CFG);

	/* Configure pre-scalar clock first to give 1MHz sampling */
	prescalar = clk / 1000000;
	writel(prescalar, dev->base + IRB_SCD_PRESCALAR);

	/* pre-loading of not filtered SCD codes and
	 * preparing data for tolerance calculation.
	 */
	lscd = scd.prefix_code;
	offscd = &scd.off_prefix_code;
	code = scd.value.code;
	curr_codelen = codelen;
	for (k = 0; k < 2; k++) {
		space = 0;
		mark = 0;
		j = 0;
		for (i = (curr_codelen - 1); i >= 0; i--) {
			j = 1 << (i - 1);
			if (code & (1 << i)) {
				mark += scd.value.nomtime;
				if (!(code & j) || (i == 0)) {
					lscd[*offscd].duration = US_TO_NS(mark);
					lscd[*offscd].pulse 	= true;
					DPRINTK("SCD mark rscd[%d](%d)\n",
						*offscd,
						lscd[*offscd].duration);
					(*offscd)++;
					mark = 0;
				}
			} else {
				space += scd.value.nomtime;
				if ((code & j) || (i == 0)) {
					lscd[*offscd].duration =
							US_TO_NS(space);
					lscd[*offscd].pulse 	= false;
					DPRINTK("SCD space rscd[%d](%d)\n",
						*offscd,
						lscd[*offscd].duration);
					(*offscd)++;
					space = 0;
				}
			}
		}
		lscd = scd.prefix_altcode;
		offscd = &scd.off_prefix_altcode;
		code = scd.value.alt_code;
		curr_codelen = alt_codelen;
	}

	/* normaly 20% of nomtine is enough as tolerance */
	tolerance = scd.value.nomtime * LIRC_STM_SCD_TOLERANCE / 100;

	DPRINTK("SCD prescalar %d nominal %d tolerance %d\n",
		prescalar, scd.value.nomtime, tolerance);

	/* Sanity check to garantee all hw constrains must be satisfied */
	if ((tolerance > ((scd.value.nomtime >> 1) - scd.value.nomtime)) ||
	    (scd.value.nomtime < ((scd.value.nomtime >> 1) + tolerance))) {
		tolerance = scd.value.nomtime * LIRC_STM_SCD_TOLERANCE / 100;
		DPRINTK("SCD tolerance out of range. default %d\n", tolerance);
	}
	if (tolerance < 4) {
		tolerance = scd.value.nomtime * LIRC_STM_SCD_TOLERANCE / 100;
		DPRINTK("SCD tolerance too close. default %d\n", tolerance);
	}

	/* Program in scd codes and lengths */
	writel(scd.value.code, dev->base + IRB_SCD_CODE);

	/* Some cuts of chips have broken SCD, so check... */
	i = readl(dev->base + IRB_SCD_CODE);
	if (i != scd.value.code) {
		pr_err(IR_STM_NAME
		       ": SCD hardware fault.  Broken silicon?\n");
		pr_err(IR_STM_NAME
		       ": SCD wrote code 0x%08x read 0x%08x. Not armed\n",
		       scd.value.code, i);
		scd.value = last_scd;
		return -ENODEV;
	}

	/* Program scd min time, max time and nominal time */
	writel(scd.value.nomtime - tolerance,
		dev->base + IRB_SCD_SYMB_MIN_TIME);
	writel(scd.value.nomtime, dev->base + IRB_SCD_SYMB_NOM_TIME);
	writel(scd.value.nomtime + tolerance,
		dev->base + IRB_SCD_SYMB_MAX_TIME);

	/* Program in noise recovery (if required) */
	if (scd.value.noiserecov) {
		nrec = 1 | (1 << 16);	/* primary and alt code enable */

		i = 1 << (codelen - 1);
		ival = scd.value.code & i;
		if (ival)
			nrec |= 2;

		scwidth = 0;
		while (i > 0 && ((scd.value.code & i) == ival)) {
			scwidth++;
			i >>= 1;
		}

		nrec |= (scwidth << 8);

		i = 1 << (alt_codelen - 1);
		ival = scd.value.alt_code & i;
		if (ival)
			nrec |= 1 << 17;

		scwidth = 0;
		while (i > 0 && ((scd.value.alt_code & i) == ival)) {
			scwidth++;
			i >>= 1;
		}

		nrec |= (scwidth << 24);

		DPRINTK("SCD noise recovery 0x%08x\n", nrec);
		writel(nrec, dev->base + IRB_SCD_NOISE_RECOV);
	}

	/* Set supported flag */
	pr_info(IR_STM_NAME
	       ": SCD normal code 0x%x codelen 0x%x nomtime 0x%x armed\n",
	       scd.value.code, codelen, scd.value.nomtime);

	if (alt_codelen > 0) {
		pr_info(IR_STM_NAME
		       ": SCD alternative code 0x%x codelen 0x%x armed\n",
		       scd.value.alt_code, alt_codelen);
		writel(scd.value.alt_code, dev->base + IRB_SCD_ALT_CODE);
	} else {
		writel(scd.value.code, dev->base + IRB_SCD_ALT_CODE);
		alt_codelen = codelen;
		scd.flags |= SCD_NOR_EQ_ALT;
	}

	/* SCD codelen range [00001,...,11111,00000] where 00000 = 32 symbols */
	writel(((alt_codelen & 0x1f) << 8)
	       | (codelen & 0x1f), dev->base + IRB_SCD_CODE_LEN);

	scd.flags |= SCD_ENABLED;
	return 0;
}

static int ir_stm_scd_kzalloc(struct device *dev)
{
	scd.prefix_code = (struct ir_raw_event *) devm_kzalloc(dev,
						LIRC_STM_SCD_BUFSIZE,
						GFP_KERNEL);
	if (!scd.prefix_code)
		return -ENOMEM;

	scd.prefix_altcode = (struct ir_raw_event *) devm_kzalloc(dev,
					LIRC_STM_SCD_BUFSIZE,
					GFP_KERNEL);
	if (!scd.prefix_altcode)
		return -ENOMEM;

	return 0;
}

static void ir_stm_scd_reactivate(struct stm_ir_device *dev, int lastSymbol)
{
	if (scd.flags && lastSymbol) {
		/* reset the SCD flags */
		scd.flags &= (SCD_ENABLED | SCD_NOR_EQ_ALT);
		writel(0x01, dev->base + IRB_SCD_INT_EN);
		writel(0x01, dev->base + IRB_SCD_CFG);
	}
}

static void ir_stm_scd_set_flags(struct stm_ir_device *dev)
{
	if (scd.flags) {
		/* SCD status catched only the first time */
		if (!(scd.flags & SCD_NORMAL)) {
			scd.flags |= readb(dev->base + IRB_SCD_STA);
			writel(0x01, dev->base + IRB_SCD_INT_CLR);
			writel(0x00, dev->base + IRB_SCD_INT_EN);
		}
	}
}

#ifdef CONFIG_PM
static void ir_stm_scd_restart(struct stm_ir_device *dev)
{
	writel(0x02, dev->base + IRB_SCD_CFG);
	writel(0x01, dev->base + IRB_SCD_CFG);
}
#endif /* CONFIG_PM */

#else /* CONFIG_LIRC_STM_UHF_SCD */
static int ir_stm_scd_set(struct stm_ir_device *dev, char enable)
{
	return 0; /* NA */
}
static void ir_stm_scd_reactivate(struct stm_ir_device *dev, int lastSymbol)
{
	return;	/* NA */
}
static void ir_stm_scd_set_flags(struct stm_ir_device *dev)
{
	return;	/* NA */
}

#ifdef CONFIG_PM
static void ir_stm_scd_restart(struct stm_ir_device *dev)
{
	return;	/* NA */
}
#endif /* CONFIG_PM */

static int ir_stm_scd_kzalloc(struct device *dev)
{
	return 0; /* NA */
}
static void ir_stm_scd_prefix_symbols(struct stm_ir_device *dev,
				struct ir_raw_event *first_ev)
				{
	return;	/* NA */
}
static int ir_stm_scd_config(struct stm_ir_device *dev, unsigned long clk)
{
	return 0; /* NA */
}
#endif /* CONFIG_LIRC_STM_UHF_SCD */



static void ir_stm_rx_interrupt(int irq, void *data)
{
	unsigned int symbol, mark = 0;
	struct stm_ir_device *dev = data;
	int lastSymbol = 0, clear_irq = 1;
	DEFINE_IR_RAW_EVENT(ev);

	ir_stm_scd_set_flags(dev);

	while (RX_WORDS_IN_FIFO_OR_OVERRUN(dev)) {
		/* discard the entire collection in case of errors!  */
		if (unlikely(readl(dev->rx_base + IRB_RX_INT_STATUS) &
					LIRC_STM_IS_OVERRUN)) {
			pr_info(IR_STM_NAME ": IR RX overrun\n");
			writel(LIRC_STM_CLEAR_OVERRUN,
				dev->rx_base + IRB_RX_INT_CLEAR);
		}

		/* get the symbol times from FIFO */
		symbol = (readl(dev->rx_base + IRB_RX_SYS));
		mark = (readl(dev->rx_base + IRB_RX_ON));

		if (clear_irq) {
			/*  Clear the interrupt
			 * and leave only the overrun irq enabled */
			RX_CLEAR_IRQ(dev, LIRC_STM_CLEAR_IRQ);
			writel(0x07, dev->rx_base + IRB_RX_INT_EN);
			clear_irq = 0;
		}

		if (symbol == 0xFFFF)
			lastSymbol = 1;
		else
			lastSymbol = 0;

		/* A sequence seems to start with a constant time symbol (1us)
		 * pulse and symbol time length, both of 1us. We ignore this.
		 */
		if ((mark > 2) && (symbol > 1)) {
			/* Make fine adjustments to timings */
			symbol -= mark;	/* to get space timing */
			symbol *= dev->rx_symbol_mult;
			symbol /= dev->rx_symbol_div;
			mark *= dev->rx_pulse_mult;
			mark /= dev->rx_pulse_div;


			ev.duration = US_TO_NS(mark);
			ev.pulse = true;
			if (dev->insert_scd_timing) {
				ir_stm_scd_prefix_symbols(dev, &ev);
				dev->insert_scd_timing = false;
			}
			ir_raw_event_store_with_filter(dev->rdev, &ev);

			ev.duration = US_TO_NS(symbol);
			ev.pulse = false;
			ir_raw_event_store_with_filter(dev->rdev, &ev);
			DPRINTK("PULSE : %d SPACE %d \n", mark, symbol);

			if (lastSymbol) {
				ir_raw_event_handle(dev->rdev);
				dev->insert_scd_timing = true;
			}
		}
	}			/* while */

	RX_CLEAR_IRQ(dev, LIRC_STM_CLEAR_IRQ | 0x02);
	writel(LIRC_STM_ENABLE_IRQ, dev->rx_base + IRB_RX_INT_EN);

	ir_stm_scd_reactivate(dev, lastSymbol);
}

static irqreturn_t ir_stm_interrupt(int irq, void *data)
{
	ir_stm_tx_interrupt(irq, data);
	ir_stm_rx_interrupt(irq, data);
	return IRQ_HANDLED;
}

static void ir_stm_rx_flush(struct stm_ir_device *dev)
{
	ir_stm_scd_set(dev, 0);
	/* Disable receiver */
	writel(0x00, dev->rx_base + IRB_RX_EN);
	/* Disable interrupt */
	writel(0x20, dev->rx_base + IRB_RX_INT_EN);
	/* clean the buffer */
}

static void
ir_stm_rx_calc_clocks(struct stm_ir_device *dev, unsigned long baseclock)
{
	struct stm_plat_lirc_data *data = dev->pdata;
	unsigned int rx_max_symbol_per;


	if (data->irbclkdiv == 0) {
		/* Auto-calculate clock divisor */
		int freqdiff;
		dev->rx_sampling_freq_div = baseclock / 10000000;
		/* Work out the timing adjustment factors */
		freqdiff = baseclock - (dev->rx_sampling_freq_div * 10000000);
		/* freqdiff contains the difference between our clock and a
		 * true 10 MHz clock which the IR block wants
		 */
		if (freqdiff == 0) {
			/* no adjustment required - our clock is running at the
			 * required speed
			 */
			dev->rx_symbol_mult = 1;
			dev->rx_pulse_mult = 1;
			dev->rx_symbol_div = 1;
			dev->rx_pulse_div = 1;
		} else {
			/* adjustment is required */
			dev->rx_symbol_mult =
			    baseclock / (10000 * dev->rx_sampling_freq_div);

			if (freqdiff > 0) {
				/* our clock is running too fast */
				dev->rx_pulse_mult = 1000;
				dev->rx_pulse_div = dev->rx_symbol_mult;
				dev->rx_symbol_mult = dev->rx_pulse_mult;
				dev->rx_symbol_div = dev->rx_pulse_div;
			} else {
				/* our clock is running too slow */
				dev->rx_symbol_div = 1000;
				dev->rx_pulse_mult = dev->rx_symbol_mult;
				dev->rx_pulse_div = 1000;
			}

		}

	} else {
		dev->rx_sampling_freq_div = (data->irbclkdiv);
		dev->rx_symbol_mult = (data->irbperiodmult);
		dev->rx_symbol_div = (data->irbperioddiv);
	}

	writel(dev->rx_sampling_freq_div, dev->base + IRB_SAMPLE_RATE_COMM);
	DPRINTK("IR base clock is %lu\n", baseclock);
	DPRINTK("IR clock divisor is %d\n", dev->rx_sampling_freq_div);
	DPRINTK("IR clock divisor readlack is %d\n",
			readl(dev->base + IRB_SAMPLE_RATE_COMM));
	DPRINTK("IR period mult factor is %d\n", dev->rx_symbol_mult);
	DPRINTK("IR period divisor factor is %d\n", dev->rx_symbol_div);
	DPRINTK("IR pulse mult factor is %d\n", dev->rx_pulse_mult);
	DPRINTK("IR pulse divisor factor is %d\n", dev->rx_pulse_div);
	/* maximum symbol period.
	 * Symbol periods longer than this will generate
	 * an interrupt and terminate a command
	 */
	if ((data->irbrxmaxperiod) != 0)
		rx_max_symbol_per =
		    (data->irbrxmaxperiod) *
		    dev->rx_symbol_mult / dev->rx_symbol_div;
	else
		rx_max_symbol_per = 0;

	DPRINTK("RX Maximum symbol period register 0x%x\n", rx_max_symbol_per);
	writel(rx_max_symbol_per, dev->rx_base + IRB_MAX_SYM_PERIOD);
}

static int ir_stm_hardware_init(struct stm_ir_device *dev)
{
	unsigned int scwidth;
	int baseclock;
	struct stm_plat_lirc_data *data = dev->pdata;


	/* Set the polarity inversion bit to the correct state */
	writel(data->rxpolarity, dev->rx_base + IRB_RX_POLARITY_INV);

	/*  Get or calculate the clock and timing adjustment values.
	 *  We can auto-calculate these in some cases
	 */
	baseclock = clk_get_rate(dev->sys_clock) / data->sysclkdiv;

	ir_stm_rx_calc_clocks(dev, baseclock);
	/*  Set up the transmit timings  */
	if (data->subcarrwidth != 0)
		scwidth = data->subcarrwidth;
	else
		scwidth = 50;

	if (scwidth > 100)
		scwidth = 50;

	DPRINTK("subcarrier width set to %d %%\n", scwidth);
	ir_stm_tx_calc_clocks(dev, baseclock, scwidth);

	ir_stm_scd_config(dev, baseclock);

	return 0;
}

static int ir_stm_remove(struct platform_device *pdev)
{
	struct stm_ir_device *ir_dev = platform_get_drvdata(pdev);
	DPRINTK("ir_stm_remove called\n");
	clk_disable(ir_dev->sys_clock);
	rc_unregister_device(ir_dev->rdev);
	return 0;
}

/* outside interface: called on first open*/
static int stm_ir_open(struct rc_dev *rdev)
{
	struct stm_ir_device *dev = rdev->priv;
	DPRINTK("entering open\n");
	if (!dev->enabled) {
		unsigned long flags;
		local_irq_save(flags);
		/* enable interrupts and receiver */
		writel(LIRC_STM_ENABLE_IRQ, dev->rx_base + IRB_RX_INT_EN);
		writel(0x01, dev->rx_base + IRB_RX_EN);
		local_irq_restore(flags);
		dev->enabled = true;
		ir_stm_scd_set(dev, 1);
		dev->insert_scd_timing = true;
	} else
		DPRINTK("Device Already Enabled\n");

	return 0;
}

/* outside interface: called on device close*/
static void stm_ir_close(struct rc_dev *rdev)
{
	struct stm_ir_device *dev = rdev->priv;
	DPRINTK("entering close\n");

	/* The last close disable the receiver */
	if (dev->enabled) {
		ir_stm_rx_flush(dev);
		dev->enabled = false;
	}

}

static int ir_stm_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct rc_dev *rdev;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irb_irq, irb_irq_wup;
	struct stm_ir_device *ir_dev;

	irb_irq = irb_irq_wup = 0;

	if (pdev->name == NULL) {
		pr_err(IR_STM_NAME
		       ": probe failed. Check kernel SoC config.\n");
		return -ENODEV;
	}

	ir_dev = devm_kzalloc(dev, sizeof(struct stm_ir_device), GFP_KERNEL);
	rdev = rc_allocate_device();
	if (!ir_dev || !rdev)
		goto error1;

	ir_dev->sys_clock = clk_get(dev, "comms_clk");
	if (IS_ERR(ir_dev->sys_clock)) {
		pr_err(IR_STM_NAME " system clock not found\n");
		return PTR_ERR(ir_dev->sys_clock);
	}

	clk_enable(ir_dev->sys_clock);
	pr_info(IR_STM_NAME
	       ": probe found data for platform device %s\n", pdev->name);
	ir_dev->pdata = dev->platform_data;

	irb_irq = platform_get_irq(pdev, 0);
	if (irb_irq < 0) {
		pr_err(IR_STM_NAME
		       ": IRQ configuration not found\n");
		return -ENODEV;
	}

	if (devm_request_irq(dev, irb_irq, ir_stm_interrupt, IRQF_DISABLED,
			     IR_STM_NAME, (void *)ir_dev) < 0) {
		pr_err(IR_STM_NAME ": IRQ %d register failed\n", irb_irq);
		return -EIO;
	}

	/* Enable wakeup interrupt if any */
	irb_irq_wup = platform_get_irq(pdev, 1);
	if (irb_irq_wup >= 0) {
		if (devm_request_irq
		    (dev, irb_irq_wup, ir_stm_interrupt, IRQF_DISABLED,
		     IR_STM_NAME, (void *)ir_dev) < 0) {
			pr_err(IR_STM_NAME
			       ": wakeup IRQ %d register failed\n",
			       irb_irq_wup);
			return -EIO;
		}
		disable_irq(irb_irq_wup);
		enable_irq_wake(irb_irq_wup);
		pr_info(IR_STM_NAME
		       ": the driver has wakeup IRQ %d\n", irb_irq_wup);
	}


	/* Hardware IR block setup - the PIO ports should already be set up
	 * in the board-dependent configuration.  We need to remap the
	 * IR registers into kernel space - we do this in one chunk
	 */
	if (ir_stm_scd_kzalloc(dev)) {
		pr_err(IR_STM_NAME
		       ": Unable to allocate Memory\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err(IR_STM_NAME ": IO MEM not found\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start,
				     res->end - res->start, IR_STM_NAME)) {
		pr_err(IR_STM_NAME ": request_mem_region failed\n");
		return -EBUSY;
	}

	ir_dev->base =
	    devm_ioremap_nocache(dev, res->start, res->end - res->start);

	if (ir_dev->base == NULL) {
		pr_err(IR_STM_NAME ": ioremap failed\n");
		ret = -ENOMEM;
	} else {
			/* Configure for ir or uhf. rxuhfmode==1 is UHF */
		if (ir_dev->pdata->rxuhfmode)
			ir_dev->rx_base = ir_dev->base + 0x40;
		else
			ir_dev->rx_base = ir_dev->base;

		DPRINTK("ioremapped register block at 0x%x\n", res->start);
		DPRINTK("ioremapped to 0x%x\n", (unsigned int)ir_dev->base);

		pr_info(IR_STM_NAME ": STM LIRC plugin using IRQ %d"
		       " in %s mode\n", irb_irq,
		       ir_dev->pdata->rxuhfmode ? "UHF" : "IR");

		if (!devm_stm_pad_claim(dev, ir_dev->pdata->pads,
					IR_STM_NAME)) {
			pr_err(IR_STM_NAME ": Failed to claim "
			       "pads!\n");
			return -EIO;
		}

		/* enable signal detection */
		ret = ir_stm_hardware_init(ir_dev);

		rdev->driver_type = RC_DRIVER_IR_RAW;
		rdev->allowed_protos = RC_TYPE_ALL;
		rdev->priv = ir_dev;
		rdev->open = stm_ir_open;
		rdev->close = stm_ir_close;
		rdev->driver_name = IR_STM_NAME;
		rdev->map_name = RC_MAP_STM_PVR_1;
		rdev->input_name = "STM Infrared Remote Receiver";

#ifdef CONFIG_LIRC_STM_TX
		rdev->tx_ir = stm_ir_transmit;
		rdev->input_name = "STM Infrared Remote Transceiver";

#endif
		device_set_wakeup_capable(dev, true);

		ret = rc_register_device(rdev);
		if (ret < 0)
			goto error1;
		ir_dev->rdev = rdev;
		/* by default the device is on */
		pm_runtime_set_active(dev);
		pm_suspend_ignore_children(dev, 1);
		pm_runtime_enable(dev);



	}
	platform_set_drvdata(pdev, ir_dev);

	return ret;
error1:
	rc_free_device(rdev);
	return ret;
}


#ifdef CONFIG_PM
static void ir_stm_rx_restore(struct stm_ir_device *dev)
{
	ir_stm_scd_set(dev, 1);
	/* enable interrupts and receiver */
	writel(LIRC_STM_ENABLE_IRQ, dev->rx_base + IRB_RX_INT_EN);
	writel(0x01, dev->rx_base + IRB_RX_EN);
	/* clean the buffer */
}

static int ir_stm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_ir_device *ir_dev = platform_get_drvdata(pdev);

	if (device_may_wakeup(&pdev->dev)) {
		/* need for the resuming phase */
		ir_stm_rx_flush(ir_dev);
		ir_stm_rx_calc_clocks(pdev, clk_get_rate(ir_dev->sys_clock));
		ir_stm_scd_config(ir_dev, clk_get_rate(ir_dev->sys_clock));
		ir_stm_rx_restore(ir_dev);
		ir_stm_scd_restart(ir_dev);
	} else
		clk_disable(ir_dev->sys_clock);

	return 0;
}

static int ir_stm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_ir_device *ir_dev = platform_get_drvdata(pdev);

	if (!device_may_wakeup(&pdev->dev))
		clk_enable(ir_dev->sys_clock);

	ir_stm_hardware_init(pdev);
	ir_stm_rx_restore(ir_dev);
	ir_stm_scd_restart(ir_dev);

	return 0;
}

static int ir_stm_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_ir_device *ir_dev = platform_get_drvdata(pdev);

	/* disable IR RX/TX interrupts plus clear status */
	writel(0x00, ir_dev->rx_base + IRB_RX_EN);
	writel(0xff, ir_dev->rx_base + IRB_RX_INT_CLEAR);
	/* disabling LIRC irq request */
	/* flush LIRC plugin data */

	clk_disable(ir_dev->sys_clock);
	return 0;
}

static int ir_stm_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_ir_device *ir_dev = platform_get_drvdata(pdev);

	clk_enable(ir_dev->sys_clock);
	ir_stm_hardware_init(pdev);
	/* enable interrupts and receiver */
	writel(LIRC_STM_ENABLE_IRQ, IRB_RX_INT_EN);
	writel(0x01, ir_dev->rx_bse + IRB_RX_EN);

	return 0;
}

static struct dev_pm_ops stm_ir_pm_ops = {
	.suspend = ir_stm_suspend,	/* on standby/memstandby */
	.resume = ir_stm_resume,	/* resume from standby/memstandby */
	.freeze = ir_stm_freeze,	/* hibernation */
	.restore = ir_stm_restore,	/* resume from hibernation */
	.runtime_suspend = ir_stm_suspend,
	.runtime_resume = ir_stm_resume,
};
#endif

static struct platform_driver stm_ir_device_driver = {
	.driver = {
		.name = IR_STM_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm     = &stm_ir_pm_ops,
#endif
	},
	.probe = ir_stm_probe,
	.remove = ir_stm_remove,
};

static int __init ir_stm_init(void)
{
	DPRINTK("initializing the IR receiver...\n");

	if (platform_driver_register(&stm_ir_device_driver)) {
		pr_err(IR_STM_NAME
		       ": platform driver register failed\n");
		goto out_err;
	}

	request_module("lirc_dev");
	pr_info("STMicroelectronics LIRC driver initialized.\n");
	return 0;
out_err:
	pr_err("STMicroelectronics LIRC driver not initialized.\n");
	return -EINVAL;
}

void __exit ir_stm_release(void)
{
	DPRINTK("removing STM IR driver\n");

	platform_driver_unregister(&stm_ir_device_driver);

	pr_info("STMicroelectronics LIRC driver removed\n");
}

module_init(ir_stm_init);
module_exit(ir_stm_release);
MODULE_DESCRIPTION
	("Linux IR Transceiver driver for STMicroelectronics platforms");
MODULE_AUTHOR("Carl Shaw <carl.shaw@st.com>");
MODULE_AUTHOR("Angelo Castello <angelo.castello@st.com>");
MODULE_AUTHOR("Srinivas Kandagatla <srinivas.kandagatla@st.com>");
MODULE_LICENSE("GPL");
