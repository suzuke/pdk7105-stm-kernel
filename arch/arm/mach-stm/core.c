/*
 * arch/arm/mach-stm/core.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "core.h"

#include <linux/elfnote.h>

/* Expose PHYS_OFFSET in an ELF note */
#ifdef CONFIG_ARM_PATCH_PHYS_VIRT
ELFNOTE32("PHYS_OFFSET", 0, 0xffffffff);
#else
ELFNOTE32("PHYS_OFFSET", 0, PHYS_OFFSET);
#endif
