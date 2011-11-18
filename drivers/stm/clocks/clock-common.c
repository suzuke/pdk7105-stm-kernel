/*****************************************************************************
 *
 * File name   : clock-common.c
 * Description : Low Level API - Common LLA functions (SOC independant)
 *
 * COPYRIGHT (C) 2009 STMicroelectronics - All Rights Reserved
 * May be copied or modified under the terms of the GNU General Public
 * License v2.  See linux/COPYING for more information.
 *
 *****************************************************************************/

/* ----- Modification history (most recent first)----
27/jul/11 fabrice.charpentier@st.com
	  FS660 algo enhancement.
14/mar/11 fabrice.charpentier@st.com
	  Added PLL1200 functions.
07/mar/11 fabrice.charpentier@st.com
	  clk_pll3200_get_params() revisited.
11/mar/10 fabrice.charpentier@st.com
	  clk_pll800_get_params() fully revisited.
10/dec/09 francesco.virlinzi@st.com
	  clk_pll1600_get_params() now same code for OS21 & Linux.
13/oct/09 fabrice.charpentier@st.com
	  clk_fsyn_get_rate() API changed. Now returns error code.
30/sep/09 fabrice.charpentier@st.com
	  Introducing clk_pll800_get_rate() & clk_pll1600_get_rate() to
	  replace clk_pll800_freq() & clk_pll1600_freq().
*/


#include <linux/stm/clk.h>
#include <asm-generic/div64.h>

/*
 * Linux specific function
 */

/* Return the number of set bits in x. */
static unsigned int population(unsigned int x)
{
	/* This is the traditional branch-less algorithm for population count */
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = x + (x << 8);
	x = x + (x << 16);

	return x >> 24;
}

/* Return the index of the most significant set in x.
 * The results are 'undefined' is x is 0 (0xffffffff as it happens
 * but this is a mere side effect of the algorithm. */
static unsigned int most_significant_set_bit(unsigned int x)
{
	/* propagate the MSSB right until all bits smaller than MSSB are set */
	x = x | (x >> 1);
	x = x | (x >> 2);
	x = x | (x >> 4);
	x = x | (x >> 8);
	x = x | (x >> 16);

	/* now count the number of set bits [clz is population(~x)] */
	return population(x) - 1;
}

#include "clock-oslayer.h"
#include "clock-common.h"


/*
 * PLL800
 */

/* ========================================================================
   Name:	clk_pll800_get_params()
   Description: Freq to parameters computation for PLL800
   Input:       input & output freqs (Hz)
   Output:      updated *mdiv, *ndiv & *pdiv (register values)
   Return:      'clk_err_t' error code
   ======================================================================== */

/*
 * PLL800 in FS mode computation algo
 *
 *	     2 * N * Fin Mhz
 * Fout Mhz = -----------------		[1]
 *		M * (2 ^ P)
 *
 * Rules:
 *   6.25Mhz <= output <= 800Mhz
 *   FS mode means 3 <= N <= 255
 *   1 <= M <= 255
 *   1Mhz <= PFDIN (input/M) <= 50Mhz
 *   200Mhz <= FVCO (input*2*N/M) <= 800Mhz
 *   For better long term jitter select M minimum && P maximum
 */

int clk_pll800_get_params(unsigned long input, unsigned long output,
	unsigned long *mdiv, unsigned long *ndiv, unsigned long *pdiv)
{
	unsigned long m, n, pfdin, fvco;
	unsigned long deviation, new_freq;
	long new_deviation, pi;

	/* Output clock range: 6.25Mhz to 800Mhz */
	if (output < 6250000 || output > 800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (pi = 5; pi >= 0 && deviation; pi--) {
		for (m = 1; (m < 255) && deviation; m++) {
			n = m * (1 << pi) * output / (input * 2);

			/* Checks */
			if (n < 3)
				continue;
			if (n > 255)
				break;
			pfdin = input / m; /* 1Mhz <= PFDIN <= 50Mhz */
			if (pfdin < 1000 || pfdin > 50000)
				continue;
			/* 200Mhz <= FVCO <= 800Mhz */
			fvco = (input * 2 * n) / m;
			if (fvco > 800000)
				continue;
			if (fvco < 200000)
				break;

			new_freq = (input * 2 * n) / (m * (1 << pi));
			new_deviation = new_freq - output;
			if (new_deviation < 0)
				new_deviation = -new_deviation;
			if (!new_deviation || new_deviation < deviation) {
				*mdiv	= m;
				*ndiv	= n;
				*pdiv	= pi;
				deviation = new_deviation;
			}
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll800_get_rate()
   Description: Convert input/mdiv/ndiv/pvid values to frequency for PLL800
   Params:      'input' freq (Hz), mdiv/ndiv/pvid values
   Output:      '*rate' updated
   Return:      Error code.
   ======================================================================== */

int clk_pll800_get_rate(unsigned long input, unsigned long mdiv,
	unsigned long ndiv, unsigned long pdiv, unsigned long *rate)
{
	if (!mdiv)
		mdiv++; /* mdiv=0 or 1 => MDIV=1 */

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((2 * (input/1000) * ndiv) / mdiv) / (1 << pdiv)) * 1000;

	return 0;
}

/*
 * PLL1200
 */

/* ========================================================================
   Name:	clk_pll1200_get_params()
   Description: Freq to parameters computation for PLL1200.
   Input:       input=input freq (Hz),output=output freq (Hz)
		WARNING: Output freq is given for PHI (FVCO/ODF with ODF=1),
			 so BEFORE output dividers.
   Output:      updated *idf & *ndiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 *   FVCO >> Divider (ODF) >> PHI
 * 
 * PHI = (INFF * LDF) / (ODF * IDF) when BYPASS = L
 *
 * Rules:
 *   9.6Mhz <= input (INFF) <= 350Mhz
 *   600Mhz <= FVCO <= 1200Mhz
 *   9.52Mhz <= PHI output <= 1200Mhz
 *   1 <= m (register value for IDF) <= 7
 *   8 <= n (register value for NDIV=LDF) <= 127
 *   1 <= n (register value for ODF) <= 63
 */

int clk_pll1200_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv)
{
	unsigned long m, n;
	unsigned long deviation, new_freq;
	long new_deviation;

	/* Output clock range: 9.52Mhz to 1200Mhz */
	if (output < 9520000 || output > 1200000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (m=1; (m < 7) && deviation; m++) {
		n = m * output / input;

		/* Checks */
		if (n < 8) continue;
		if (n > 127) break;

		new_freq = (input * n) / m;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*idf	= m;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1200_get_rate()
   Description: Convert input/idf/ndiv values to FVCO frequency for PLL1200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCO output (PHI/1).
   Return:      Error code.
   ======================================================================== */

int clk_pll1200_get_rate(unsigned long input, unsigned long idf,
			unsigned long ndiv, unsigned long *rate)
{
	if (!idf)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = (((input / 1000) * ndiv) / idf) * 1000;

	return 0;
}

/*
 * PLL1600
 */

/* ========================================================================
   Name:	clk_pll1600_get_params()
   Description: Freq to parameters computation for PLL1600
   Input:       input,output=input/output freqs (Hz)
   Output:      updated *mdiv (rdiv) & *ndiv (ddiv)
   Return:      'clk_err_t' error code
   ======================================================================== */

/* Revisited algo, adding checks and jitter enhancements */
/*
 * Rules:
 *   600Mhz <= output (FVCO) <= 1800Mhz
 *   1 <= M (also called R) <= 7
 *   4 <= N <= 255
 *   4Mhz <= PFDIN (input/M) <= 75Mhz
 */

int clk_pll1600_get_params(unsigned long input, unsigned long output,
			   unsigned long *mdiv, unsigned long *ndiv)
{
	unsigned long m, n, pfdin;
	unsigned long deviation, new_freq;
	long new_deviation;

	/* Output clock range: 600Mhz to 1800Mhz */
	if (output < 600000000 || output > 1800000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (m = 1; (m < 7) && deviation; m++) {
		n = m * output / (input * 2);

		/* Checks */
		if (n < 4) continue;
		if (n > 255) break;
		pfdin = input / m; /* 4Mhz <= PFDIN <= 75Mhz */
		if (pfdin < 4000 || pfdin > 75000) continue;

		new_freq = (input * 2 * n) / m;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*mdiv	= m;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;
	return 0;
}

/* ========================================================================
   Name:	clk_pll1600_get_rate()
   Description: Convert input/mdiv/ndiv values to frequency for PLL1600
   Params:      'input' freq (Hz), mdiv/ndiv values
		Info: mdiv also called rdiv, ndiv also called ddiv
   Output:      '*rate' updated with value of HS output.
   Return:      Error code.
   ======================================================================== */

int clk_pll1600_get_rate(unsigned long input, unsigned long mdiv,
						 unsigned long ndiv, unsigned long *rate)
{
	if (!mdiv)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input/1000) * ndiv) / mdiv) * 1000;

	return 0;
}

/*
 * PLL3200
 */

/* ========================================================================
   Name:	clk_pll3200_get_rate()
   Description: Convert input/idf/ndiv values to FVCOby2 frequency for PLL3200
   Params:      'input' freq (Hz), idf/ndiv values
   Output:      '*rate' updated with value of FVCOby2 output (PHIx / 1).
   Return:      Error code.
   ======================================================================== */

int clk_pll3200_get_rate(unsigned long input, unsigned long idf,
			unsigned long ndiv, unsigned long *rate)
{
	if (!idf)
		return CLK_ERR_BAD_PARAMETER;

	/* Note: input is divided by 1000 to avoid overflow */
	*rate = ((2 * (input/1000) * ndiv) / idf) * 1000;

	return 0;
}

/* ========================================================================
   Name:	clk_pll3200_get_params()
   Description: Freq to parameters computation for PLL3200.
   Input:       input=input freq (Hz), output=FVCOBY2 freq (Hz)
   Output:      updated *idf & *ndiv, plus *cp value (charge pump)
   Return:      'clk_err_t' error code
   ======================================================================== */

/* PLL output structure
 * VCO >> /2 >> FVCOBY2
 *		 |> Divider (ODF0) >> PHI0
 *		 |> Divider (ODF1) >> PHI1
 *		 |> Divider (ODF2) >> PHI2
 *		 |> Divider (ODF3) >> PHI3
 * 
 * FVCOby2 output = (input*4*NDIV) / (2*IDF) (assuming FRAC_CONTROL==L)
 *
 * Rules:
 *   4Mhz <= input <= 350Mhz
 *   800Mhz <= output (FVCOby2) <= 1600Mhz
 *   1 <= i (register value for IDF) <= 7
 *   8 <= n (register value for NDIV) <= 200
 */

int clk_pll3200_get_params(unsigned long input, unsigned long output,
			   unsigned long *idf, unsigned long *ndiv,
			   unsigned long *cp)
{
	unsigned long i, n = 0;
	unsigned long deviation, new_freq;
	long new_deviation;
	/* Charge pump table: highest ndiv value for cp=6 to 25 */
	static const unsigned char cp_table[] = 
		{ 48, 56, 64, 72, 80, 88, 96, 104, 112, 120,
		  128, 136, 144, 152, 160, 168, 176, 184, 192 };

	/* Output clock range: 800Mhz to 1600Mhz */
	if (output < 800000000 || output > 1600000000)
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	deviation = output;
	for (i=1; (i < 7) && deviation; i++) {
		n = i * output / (2 * input);

		/* Checks */
		if (n < 8) continue;
		if (n > 200) break;

		new_freq = (input * 2 * n) / i;
		new_deviation = new_freq - output;
		if (new_deviation < 0)
			new_deviation = -new_deviation;
		if (!new_deviation || new_deviation < deviation) {
			*idf	= i;
			*ndiv	= n;
			deviation = new_deviation;
		}
	}

	if (deviation == output) /* No solution found */
		return CLK_ERR_BAD_PARAMETER;

	/* Computing recommended charge pump value */
	for( *cp = 6; n > cp_table[*cp-6]; (*cp)++ );

	return 0;
}

/*
 * FS216
 */

/* ========================================================================
   Name:	clk_fsyn_get_params()
   Description: Freq to parameters computation for frequency synthesizers
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *md, *pe & *sdiv
   Return:      'clk_err_t' error code
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types.
   Currently based on C090_4FS216_25. */

int clk_fsyn_get_params(unsigned long input, unsigned long output,
			unsigned long *md, unsigned long *pe, unsigned long *sdiv)
{
	unsigned long long p, q;
	unsigned int predivide;
	int preshift; /* always +ve but used in subtraction */
	unsigned int lsdiv;
	int lmd;
	unsigned int lpe = 1 << 14;

	/* pre-divide the frequencies */
	p = 1048576ull * input * 8;    /* <<20? */
	q = output;

	predivide = (unsigned int)div64_u64(p, q);

	/* determine an appropriate value for the output divider using eqn. #4
	 * with md = -16 and pe = 32768 (and round down) */
	lsdiv = predivide / 524288;
	if (lsdiv > 1) {
		/* sdiv = fls(sdiv) - 1; // this doesn't work
		 * for some unknown reason */
		lsdiv = most_significant_set_bit(lsdiv);
	} else
		lsdiv = 1;

	/* pre-shift a common sub-expression of later calculations */
	preshift = predivide >> lsdiv;

	/* determine an appropriate value for the coarse selection using eqn. #5
	 * with pe = 32768 (and round down which for signed values means away
	 * from zero) */
	lmd = ((preshift - 1048576) / 32768) - 1;	 /* >>15? */

	/* calculate a value for pe that meets the output target */
	lpe = -1 * (preshift - 1081344 - (32768 * lmd));  /* <<15? */

	/* finally give sdiv its true hardware form */
	lsdiv--;
	/* special case for 58593.75Hz and harmonics...
	* can't quite seem to get the rounding right */
	if (lmd == -17 && lpe == 0) {
		lmd = -16;
		lpe = 32767;
	}

	/* update the outgoing arguments */
	*sdiv = lsdiv;
	*md = lmd;
	*pe = lpe;

	/* return 0 if all variables meet their contraints */
	return (lsdiv <= 7 && -16 <= lmd && lmd <= -1 && lpe <= 32767) ? 0 : -1;
}

/* ========================================================================
   Name:	clk_fsyn_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   ======================================================================== */

/* This has to be enhanced to support several Fsyn types */

int clk_fsyn_get_rate(unsigned long input, unsigned long pe,
		unsigned long md, unsigned long sd, unsigned long *rate)
{
	int md2 = md;
	long long p, q, r, s, t;
	if (md & 0x10)
		md2 = md | 0xfffffff0;/* adjust the md sign */

	input *= 8;

	p = 1048576ll * input;
	q = 32768 * md2;
	r = 1081344 - pe;
	s = r + q;
	t = (1 << (sd + 1)) * s;
	*rate = div64_u64(p, t);

	return 0;
}

/*
 * 4FS432 Freq Syn support:
 *
 * 	 	   	 [32x2^15 x ndiv x Fin]
 *  FOut = 	-----------------------------------------
 *  		(sdiv x nsdiv3 x [33x2^15 + mdx2^15 - pe])
 */

int clk_4fs432_get_rate(unsigned long input, unsigned long pe,
				unsigned long md, unsigned long sd,
				unsigned long nsd3, unsigned long *rate)
{
	int md2 = md;
	long long p, q, r, s, t, u;
	if (md & 0x10)
		md2 = md | 0xfffffff0;/* adjust the md sign */

	if (input == 30000000)
		input *= 16;
	else
		input *= 8;

	p = 1048576ll * input;
	q = 32768 * md2;
	r = 1081344 - pe;
	s = r + q;
	u = nsd3 ? 1 : 3;
	t = (1 << (sd + 1)) * s * u;

	*rate =  div64_u64(p, t);
	return 0;
}

int clk_4fs432_get_params(unsigned long input, unsigned long output,
				unsigned long *md, unsigned long *pe,
				unsigned long *sdiv, unsigned long *sdiv3)
{
	int md2;
	unsigned long rate, pe2, sd, nsd3;
	for (nsd3 = 1; nsd3 >= 0 ; nsd3--) {
		for (sd = 0; sd < 8 ; sd++) {
			for (md2 = -16 ; md2 < 0; md2++) {
				for (pe2 = 0 ; pe2 < 32768; pe2++) {
					clk_4fs432_get_rate(input, pe2,
						md2 + 32, sd, nsd3, &rate);
					if (rate == output) {
						/* Match */
						*sdiv = sd;
						*md = md2 + 32;
						*pe = pe2;
						*sdiv3 = nsd3;
						return 0;
					}
				}
			}
		}
	}
	return 0;
}
/*
   FS660
   Based on C32_4FS_660MHZ_LR_EG_5U1X2T8X_um spec.

   This FSYN embed a programmable PLL which then serve the 4 digital blocks
   
   clkin => PLL660 => DIG660_0 => clkout0
		   => DIG660_1 => clkout1
		   => DIG660_2 => clkout2
		   => DIG660_3 => clkout3
   For this reason the PLL660 is programmed separately from digital parts.
*/

/* ========================================================================
   Name:	clk_fs660_vco_get_params()
   Description: Compute params for embeded PLL660
   Input:       input=input freq (Hz), output=output freq (Hz)
   Output:      updated *ndiv (register value). Note that PDIV is frozen to 1.
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660_vco_get_params(unsigned long input, unsigned long output,
			     unsigned long *ndiv)
{
	/* Formula
	 * VCO frequency = (fin x ndiv) / pdiv
	 * ndiv = VCOfreq * pdiv / fin
	 */
	unsigned long pdiv = 1, n;

	/* Output clock range: 384Mhz to 660Mhz */
	if (output < 384000000 || output > 660000000)
		return CLK_ERR_BAD_PARAMETER;

	if (input > 40000000)
		/* This means that PDIV would be 2 instead of 1.
		   Not supported today. */
		return CLK_ERR_BAD_PARAMETER;

	input /= 1000;
	output /= 1000;

	n = output * pdiv / input;
	/* FCh: opened point. Min value is 16. To be clarified */
	if (n < 16)
		n = 16;
	*ndiv = n - 16; /* Converting formula value to reg value */

	return 0;
}

/* ========================================================================
   Name:	clk_fs660_dig_get_params()
   Description: Compute params for digital part of FS660
   Input:       input=VCO freq, output=requested freq (Hz)
   Output:      
   Return:      'clk_err_t' error code
   ======================================================================== */

int clk_fs660_dig_get_params(unsigned long input, unsigned long output,
			     unsigned long *md, unsigned long *pe, unsigned long *sdiv)
{
	int si;
	unsigned long p20 = 1048576; /* 2 power 20 */
	unsigned long s; /* sdiv value = 1 << sdiv_reg_value */
	unsigned long ns = 1; /* nsdiv value. Stuck to 1 on ORLY.
				May require param for futur chips */
	unsigned long p; /* pe value */
	unsigned long m; /* md value */
	unsigned long new_freq, new_deviation, deviation;

	/* Reduce freq to prevent overflows */
	input /= 10000;
	output /= 10000;

	deviation = output;
	for (si = 0; (si < 9) && deviation; si++) {
		s = (1 << si);
		for (m = 0; (m < 32) && deviation; m++) {
			p = (input * 2048) ;
			p = p - 2048 * (s * ns * output) - (s * ns * output) * (m * (2048/ 32));
			p = p * (p20 / 2048);
			p = p / (s * ns * output);
			if (p > 32767) continue;
			new_freq = (input * 2048) / (s * ns * (2048 + (m * (2048 / 32)) + (p * (2048 / p20))));
			if (new_freq < output)
				new_deviation = output - new_freq;
			else
				new_deviation = new_freq - output;
			if (!new_deviation || (new_deviation < deviation)) {
				*pe = p;
				*md = m;
				*sdiv = si;
				deviation = new_deviation;
			}
		}
	}

	return 0;
}

/* ========================================================================
   Name:	clk_fs660_get_rate()
   Description: Parameters to freq computation for frequency synthesizers.
   Inputs:	input=VCO frequency
   Outputs:	*rate updated
   ======================================================================== */

int clk_fs660_get_rate(unsigned long input, unsigned long pe,
			unsigned long md, unsigned long sdiv,
			unsigned long *rate)
{
	unsigned long s = (1 << sdiv); /* sdiv value = 1 << sdiv_reg_value */
	unsigned long ns = 1; /* nsdiv value. Stuck to 1 on ORLY */
	unsigned long p20 = 1048576; /* 2 power 20 */

	/* Reduce freq to prevent overflow */
	input /= 20000;
	*rate = (input * 100000) / (s * ns * (100000 + (md * 100000 / 32) + (pe * 100000 / p20)));
	*rate *= 20000;

	return 0;
}

/* ========================================================================
   Name:	clk_fs660_vco_get_rate()
   Description: Compute VCO frequency of FS660 embeded PLL (PLL660)
   Input: ndiv & pdiv registers values
   Output: updated *rate (Hz)
   ======================================================================== */

int clk_fs660_vco_get_rate(unsigned long input, unsigned long ndiv,
			   unsigned long *rate)
{
	unsigned long nd = ndiv + 16; /* ndiv value */
	unsigned long pdiv = 1; /* Frozen. Not configurable so far */

	*rate = (input * nd) / pdiv;

	return 0;
}


#include <linux/clkdev.h>
int __init clk_register_table(struct clk *clks, int num, int enable)
{
	int i;

	for (i = 0; i < num; i++) {
		struct clk *clk = &clks[i];
		int ret;
		struct clk_lookup *cl;

		/*
		 * Some devices have clockgen outputs which are unused.
		 * In this case the LLA may still have an entry in its
		 * tables for that clock, and try and register that clock,
		 * so we need some way to skip it.
		 */
		if (!clk->name)
			continue;

		ret = clk_register(clk);
		if (ret)
			return ret;

		/*
		 * We must ignore the result of clk_enables as some of
		 * the LLA enables functions claim to support an
		 * enables function, but then fail if you call it!
		 */
		if (enable) {
			ret = clk_enable(clk);
			if (ret)
			        pr_warning("Failed to enable clk %s, "
			                   "ignoring\n", clk->name);
		}

		cl = clkdev_alloc(clk, clk->name, NULL);
		if (!cl)
			return -ENOMEM;
		clkdev_add(cl);
	}

	return 0;
}
