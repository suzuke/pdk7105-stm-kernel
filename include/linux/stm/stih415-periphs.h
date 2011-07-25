/*
 * include/linux/stm/stih415-periphs.h
 *
 * Copyright (C) 2010 STMicroelectronics Limited.
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define STIH415_SBC_LPM_BASE	0xFE400000
#define STIH415_SBC_COMMS_BASE	(STIH415_SBC_LPM_BASE+0x100000)
#define STIH415_SBC_ASC0_BASE	(STIH415_SBC_COMMS_BASE+0x30000)
#define STIH415_SBC_ASC1_BASE	(STIH415_SBC_COMMS_BASE+0x31000)

#define STIH415_SBC_SYSCONF_BASE	0xFE600000 /* 0-44 */
#define STIH415_PIO_SAS_SBC_BASE	0xfe610000

#define STIH415_COMMS_BASE		0xFED00000
#define STIH415_ASC0_BASE		(STIH415_COMMS_BASE+0x30000)
#define STIH415_ASC1_BASE		(STIH415_COMMS_BASE+0x31000)
#define STIH415_ASC2_BASE		(STIH415_COMMS_BASE+0x32000)
#define STIH415_ASC3_BASE		(STIH415_COMMS_BASE+0x33000)

#define STIH415_PIO_MPE_RIGHT_BASE	0xfd6b0000
#define STIH415_PIO_MPE_LEFT_BASE	0xfd330000
#define STIH415_PIO_SAS_REAR_BASE	0xfe820000
#define STIH415_PIO_SAS_FRONT_BASE	0xfee00000

#define STIH415_SAS_FRONT_SYSCONF_BASE		0xfee10000 /* 100-200 */
#define STIH415_SAS_REAR_SYSCONF_BASE		0xfe830000 /* 300-399 */
#define STIH415_MPE_LEFT_SYSCONF_BASE		0xfd690000 /* 400-429 */
#define STIH415_MPE_RIGHT_SYSCONF_BASE		0xfd320000 /* 500-595 */
#define STIH415_MPE_SYSTEM_SYSCONF_BASE		0xfdde0000 /* 600-686 */
