/*
 * arch/sh/boards/st/mb442/mach.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the STMicroelectronics STb7100 Reference board.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/machvec_init.h>
#include <asm/irq-stb7100.h>

static void __iomem *mb442_ioport_map(unsigned long port, unsigned int size)
{
#ifdef CONFIG_BLK_DEV_ST40IDE
	/*
	 * The IDE driver appears to use memory addresses with IO port
	 * calls. This needs fixing.
	 */
	return (void __iomem *)port;
#endif

	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb442_init_irq(void)
{
	/* enable individual interrupt mode for externals */
	ipr_irq_enable_irlm();

	/* Set the ILC to route external interrupts to the the INTC */
	/* Outputs 0-3 are the interrupt pins, 4-7 are routed to the INTC */
	ilc_route_external(ILC_EXT_IRQ0, 4, 0);
	ilc_route_external(ILC_EXT_IRQ1, 5, 0);
	ilc_route_external(ILC_EXT_IRQ2, 6, 0);
#ifdef CONFIG_STMMAC_ETH
	ilc_route_external(70, 7, 0);
#else
	ilc_route_external(ILC_EXT_IRQ3, 7, 0);
#endif
}

void __init mb442_setup(char**);

struct sh_machine_vector mv_mb442 __initmv = {
	.mv_name		= "STb7100 Reference board",
	.mv_setup		= mb442_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb442_init_irq,
	.mv_ioport_map		= mb442_ioport_map,
};
ALIAS_MV(mb442)