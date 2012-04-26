/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2012  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#ifndef __STM_ARM_POKELOOP_h__
#define __STM_ARM_POKELOOP_h__

int stm_pokeloop(const unsigned int *pokeTable);
extern unsigned long stm_pokeloop_sz;

#endif
