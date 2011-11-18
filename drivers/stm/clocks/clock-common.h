/************************************************************************
File  : Low Level clock API
	Common LLA functions (SOC independant)

Author: F. Charpentier <fabrice.charpentier@st.com>

Copyright (C) 2008 STMicroelectronics
************************************************************************/

#ifndef __CLKLLA_COMMON_H
#define __CLKLLA_COMMON_H

/* ========================================================================
   Name:        clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll800_get_params(unsigned long input, unsigned long output,
			  unsigned long *mdiv, unsigned long *ndiv,
			  unsigned long *pdiv);

/* ========================================================================
   Name:        clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
			unsigned long ndiv, unsigned long pdiv,
			unsigned long *rate);

/* ========================================================================
   Name:        clk_pll1200_get_params()
   Description: Freq to parameters computation for PLL1200.
   Input:       input=input freq (Hz),output=output freq (Hz)
                WARNING: Output freq is given for PHI (FVCO/ODF),
                         so BEFORE output dividers.
   Output:      updated *idf & *ndiv (LDF)
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll1200_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv);

/* ========================================================================
   Name:        clk_pll1200_get_rate()
   Description: Convert input/idf/ndiv values to FVCO frequency for PLL1200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCO output (PHI/1).
   Return:      Error code.
   ======================================================================== */

int clk_pll1200_get_rate(unsigned long input, unsigned long idf,
			 unsigned long ndiv, unsigned long *rate);

/* ========================================================================
   Name:        clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv & *ndiv
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv);

/* ========================================================================
   Name:        clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
                Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output
   Return:      Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
			 unsigned long ndiv, unsigned long *rate);

/* ========================================================================
   Name:        clk_pll3200_get_params()
   Description: Freq to parameters computation for PLL3200.
   Input:       input=input freq (Hz), output=FVCOBY2 freq (Hz)
   Output:      updated *idf & *ndiv, plus *cp value (charge pump)
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_pll3200_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv,
			   unsigned long *cp);

/* ========================================================================
   Name:        clk_pll3200_get_rate()
   Description: Convert input/idf/ndiv values to FVCOby2 frequency for PLL3200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCOby2 output.
   Return:      Error code.
   ======================================================================== */

int clk_pll3200_get_rate(unsigned long input, unsigned long idf,
			 unsigned long ndiv, unsigned long *rate);

/* ========================================================================
   Name:        clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fsyn_get_params(unsigned long input, unsigned long output,
			unsigned long *md, unsigned long *pe,
			unsigned long *sdiv);

/* ========================================================================
   Name:        clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_fsyn_get_rate(unsigned long input, unsigned long pe, unsigned long md,
		      unsigned long sd, unsigned long *rate);

/* ========================================================================
   Name:        clk_4fs432_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe, *sdiv & *sdiv3
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_4fs432_get_params(unsigned long input, unsigned long output,
			unsigned long *md, unsigned long *pe,
			unsigned long *sdiv, unsigned long *sdiv3);

/* ========================================================================
   Name:        clk_4fsf432_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_4fs432_get_rate(unsigned long input, unsigned long pe,
				unsigned long md, unsigned long sd,
				unsigned long nsdv3, unsigned long *rate);

/* ========================================================================
   Name:        clk_fs660_vco_get_params()
   Description: Compute params for embeded PLL660
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *ndiv (register value)
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660_vco_get_params(unsigned long input, unsigned long output,
			     unsigned long *ndiv);

/* ========================================================================
   Name:        clk_fs660_dig_get_params()
   Description: Compute params for digital part of FS660
   Input:       input=VCO freq, output=requested freq (Hz)
   Output:      
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660_dig_get_params(unsigned long input, unsigned long output,
			     unsigned long *md,
			     unsigned long *pe, unsigned long *sdiv);

/* ========================================================================
   Name:        clk_fs660_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

int clk_fs660_get_rate(unsigned long input, unsigned long pe,
		       unsigned long md, unsigned long sd, unsigned long *rate);

/* ========================================================================
   Name:        clk_fs660_vco_get_rate()
   Description: Compute VCO frequency of FS660 embeded PLL (PLL660)
   Input: ndiv & pdiv registers values
   Output: updated *rate (Hz)
   ======================================================================== */

int clk_fs660_vco_get_rate(unsigned long input, unsigned long ndiv,
			   unsigned long *rate);

int clk_register_table(struct clk *clks, int num, int enable);
#endif /* #ifndef __CLKLLA_COMMON_H */
