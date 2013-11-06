/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_IRQ_ILC_H
#define __ASM_SH_IRQ_ILC_H

#include <linux/platform_device.h>
#include <linux/hardirq.h>

#if	defined(CONFIG_CPU_SUBTYPE_STIH415)
/* set this to 65 to allow 64 (INTEVT 0xa00) to demux */
#define ILC_FIRST_IRQ	65
#define ILC_NR_IRQS	224
#define ILC_IRQ(x)	(ILC_FIRST_IRQ + (x))
#elif defined(CONFIG_CPU_SUBTYPE_STXH205)
#define ILC_FIRST_IRQ	176
#define ILC_NR_IRQS	224 /* or is it 300? */
#define ILC_IRQ(x)	(ILC_FIRST_IRQ + (x))
#elif	defined(CONFIG_CPU_SUBTYPE_STX7108)
/* set this to 65 to allow 64 (INTEVT 0xa00) to demux */
#define ILC_FIRST_IRQ	65
#define ILC_NR_IRQS	190
#define ILC_IRQ(x)	(ILC_FIRST_IRQ + (x))
#endif

void __init ilc_early_init(struct platform_device* pdev);
int ilc2irq(unsigned int evtcode);
void ilc_disable_all(void);

#endif
