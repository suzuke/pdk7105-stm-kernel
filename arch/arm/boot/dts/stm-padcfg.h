
#ifndef _STM_PADCFG_CONF_H_
#define _STM_PADCFG_CONF_H_

/*
 * Pinconf is represented in an opaque unsigned long variable.
 * Below is the bit allocation details for each possible configuration.
 *
 * All the bit fields can be encapsulated into four user-friendly variables
 * (retime-type, retime-clk, force-delay, retime-delay)
 *
 *        +----------------+
 *[31:26] |   reserved-2   |
 *        +----------------+
 *[25]    |frc-dly-innotout|
 *        +----------------+
 *[24]    |  force_delay   |
 *        +----------------+------------¬
 *[23]    |    retime      |		|
 *        +----------------+		|
 *[22]    | retime-invclk  |		|
 *        +----------------+		v
 *[21]    |retime-clknotdat|       [Retime-type]
 *        +----------------+		^
 *[20]    |   retime-de    |		|
 *        +----------------+-------------
 *[19:18] | retime-clk     |------>[Retime-Clk]
 *        +----------------+
 *[17:16] |  reserved-1    |
 *        +----------------+
 *[15..0] | retime-delay   |------>[Retime Delay]
 *        +----------------+
 */

#define ALT0	0
#define ALT1	1
#define ALT2	2
#define ALT3	3
#define ALT4	4
#define ALT5	5
#define ALT6	6
#define ALT7	7

#define FORCE_DELAY_SHIFT		24
#define FORCE_DELAY			(1 << FORCE_DELAY_SHIFT)

#define FORCE_DELAY_INNOTOUT_SHIFT	25
#define FORCE_DELAY_INNOTOUT		(1 << FORCE_DELAY_INNOTOUT_SHIFT)

#define FORCE_INPUT_DELAY		(FORCE_DELAY | FORCE_DELAY_INNOTOUT)
#define FORCE_OUTPUT_DELAY		(FORCE_DELAY)

#define RT_MASK		0x1
#define RT_SHIFT	23
#define RT		(1 << RT_SHIFT)

#define INVERTCLK_MASK	0x1
#define INVERTCLK_SHIFT	22
#define INVERTCLK	(1 << INVERTCLK_SHIFT)


#define CLKNOTDATA_MASK	0x1
#define CLKNOTDATA_SHIFT	21
#define CLKNOTDATA	(1 << CLKNOTDATA_SHIFT)

#define DOUBLE_EDGE_MASK	 0x1
#define DOUBLE_EDGE_SHIFT 20
#define DOUBLE_EDGE	(1 << DOUBLE_EDGE_SHIFT)

#define CLK_MASK		0x3
#define CLK_SHIFT	18
#define CLK_A		(0 << CLK_SHIFT)
#define CLK_B		(1 << CLK_SHIFT)
#define CLK_C		(2 << CLK_SHIFT)
#define CLK_D		(3 << CLK_SHIFT)

/* RETIME_DELAY in Pico Secs */
#define DELAY_MASK		0xffff
#define DELAY_SHIFT	0
#define	DELAY_0		(0 << DELAY_SHIFT)
#define	DELAY_300	(300 << DELAY_SHIFT)
#define	DELAY_500	(500 << DELAY_SHIFT)
#define	DELAY_750	(750 << DELAY_SHIFT)
#define	DELAY_1000	(1000 << DELAY_SHIFT)
#define	DELAY_1250	(1250 << DELAY_SHIFT)
#define	DELAY_1500	(1500 << DELAY_SHIFT)
#define	DELAY_1750	(1750 << DELAY_SHIFT)
#define	DELAY_2000	(2000 << DELAY_SHIFT)
#define	DELAY_2250	(2250 << DELAY_SHIFT)
#define	DELAY_2500	(2500 << DELAY_SHIFT)
#define	DELAY_2750	(2750 << DELAY_SHIFT)
#define	DELAY_3000	(3000 << DELAY_SHIFT)
#define	DELAY_3250	(3250 << DELAY_SHIFT)

/* User-frendly defines */
/* Pin Direction */
#define UNKWN		0
#define IN		1
#define IN_PU		2
#define OUT		3
#define BIDIR		4
#define BIDIR_PU	5
#define IGNR		6


/* RETIME_TYPE */
#define DEF		(0x0)
/*
 * B Mode
 * Bypass retime with optional delay parameter
 */
#define BYPASS	(0)
/*
 * R0, R1, R0D, R1D modes
 * single-edge data non inverted clock, retime data with clk
 */
#define SE_NICLK_IO	(RT)
/*
 * RIV0, RIV1, RIV0D, RIV1D modes
 * single-edge data inverted clock, retime data with clk
 */
#define SE_ICLK_IO	(RT | INVERTCLK)
/*
 * R0E, R1E, R0ED, R1ED modes
 * double-edge data, retime data with clk
 */
#define DE_IO	(RT | DOUBLE_EDGE)
/*
 * CIV0, CIV1 modes with inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define ICLK	(RT | CLKNOTDATA | INVERTCLK)
/*
 * CLK0, CLK1 modes with non-inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define NICLK	(RT | CLKNOTDATA)

#endif
