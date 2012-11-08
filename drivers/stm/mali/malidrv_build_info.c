/***********************************************************************
 *
 * File: malidrv_build_info.c
 * Copyright (c) 2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
char *__malidrv_build_info(void)
{ return "malidrv:"
	" SOC=" MALIDRV_SOC
	" KERNELDIR=" MALIDRV_KERNELDIR
	" REVISION=" MALIDRV_SVN_REV
	" BUILD_DATE=" MALIDRV_BUILD_DATE
	;
}
