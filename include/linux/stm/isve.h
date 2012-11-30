/*
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __STM_ISVE_CONFIG_H
#define __STM_ISVE_CONFIG_H

#include <linux/platform_device.h>

/* Get the base address for a device queue */
#define DSFWD_RPT_OFF	32
#define UPIIM_RPT_OFF	40
/* Below the offset of the Queue 0 */
#define QUEUE_DFWS_OFF	0x2E400
#define QUEUE_UPIIM_OFF	0x1A400
#define DSFWD_QUEUE_ADD(base, queue)	(base + QUEUE_DFWS_OFF + \
					 (queue * DSFWD_RPT_OFF))
#define UPIIM_QUEUE_ADD(base, queue)	(base + QUEUE_UPIIM_OFF + \
					 (queue * UPIIM_RPT_OFF))
struct plat_isve_data {
	unsigned int downstream_queue_size;
	unsigned int upstream_queue_size;
	unsigned int queue_number;
	unsigned int header_size;
	char *ifname;
};

#endif /*__STM_ISVE_CONFIG_H*/
