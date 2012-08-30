/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/export.h>
#include <linux/init.h>
#include <linux/stm/soc.h>
#include <asm/processor.h>

unsigned long stm_soc_devid;
EXPORT_SYMBOL(stm_soc_devid);

long stm_soc_version_major_id = -1;
EXPORT_SYMBOL(stm_soc_version_major_id);

long stm_soc_version_minor_id = -1;
EXPORT_SYMBOL(stm_soc_version_minor_id);

void __init stm_soc_set(unsigned long devid, long major, long minor)
{
	stm_soc_devid = devid;
	if (major != -1)
		stm_soc_version_major_id = major;
	else
		stm_soc_version_major_id = (devid >> 28) & 0xF;
	stm_soc_version_minor_id = minor;

#ifdef CONFIG_SUPERH
	cpu_data->cut_major = stm_soc_version_major_id;
	cpu_data->cut_minor = stm_soc_version_minor_id;
#endif
}

const char *stm_soc(void)
{
	if (stm_soc_is_fli7610())
		return "FLI7610";
	if (stm_soc_is_stig125())
		return "STiG125";
	if (stm_soc_is_stih415())
		return "STiH415";
	return NULL;
}
EXPORT_SYMBOL(stm_soc);
