/************************************************************************
File  : Low Level clock API
	Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008-12 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

enum stm_pll_type {
	stm_pll800c65,
	stm_pll1200c32,
	stm_pll1600c45,
	stm_pll1600c45phi,
	stm_pll1600c65,
	stm_pll3200c32,
};

struct stm_pll {
	enum stm_pll_type type;

	unsigned long mdiv;
	unsigned long ndiv;
	unsigned long pdiv;

	unsigned long odf;
	unsigned long idf;
	unsigned long ldf;
	unsigned long cp;
};

enum stm_fs_type {
	stm_fs216c65,
	stm_fs432c65,
	stm_fs660c32vco,	/* VCO out */
	stm_fs660c32	/* DIG out */
};

struct stm_fs {
	enum stm_fs_type type;

	unsigned long ndiv;
	unsigned long mdiv;
	unsigned long pe;
	unsigned long sdiv;
	unsigned long nsdiv;
};

/* ========================================================================
   Name:	stm_clk_pll_get_params()
   Description: Freq to parameters computation for PLLs
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated 'struct stm_pll *pll'
   Return:      'clk_err_t' error code
   ======================================================================== */

int stm_clk_pll_get_params(unsigned long input, unsigned long output,
		struct stm_pll *pll);

int stm_clk_pll_get_rate(unsigned long input, struct stm_pll *pll,
		unsigned long *output);

/* ========================================================================
   Name:	stm_clk_fs_get_params()
   Description: Freq to parameters computation for FSs
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated 'struct stm_fs *fs'
   Return:      'clk_err_t' error code
   ======================================================================== */

int stm_clk_fs_get_params(unsigned long input, unsigned long output,
		struct stm_fs *fs);

int stm_clk_fs_get_rate(unsigned long input, struct stm_fs *fs,
		unsigned long *output);

/* ========================================================================
   Name:        clk_register_table
   Description: ?
   Returns:     'clk_err_t' error code
   ======================================================================== */

int clk_register_table(struct clk *clks, int num, int enable);

/* ========================================================================
   Name:        clk_best_div
   Description: Returned closest div factor
   Returns:     Best div factor
   ======================================================================== */

static inline unsigned long
clk_best_div(unsigned long parent_rate, unsigned long rate)
{
	return parent_rate / rate + ((rate > (2*(parent_rate % rate))) ? 0 : 1);
}

#endif /* #ifndef __CLKLLA_COMMON_H */
