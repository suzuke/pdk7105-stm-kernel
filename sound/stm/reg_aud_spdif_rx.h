#ifndef __SND_STM_AUD_SPDIF_RX_H
#define __SND_STM_AUD_SPDIF_RX_H


/*
 * Register access macros
 */

#define get__AUD_SPDIF_RX_REG(ip, offset, shift, mask) \
	((readl(ip->base + offset) >> shift) & mask)
#define set__AUD_SPDIF_RX_REG(ip, offset, shift, mask, value) \
	writel(((readl(ip->base + offset) & ~(mask << shift)) | \
		(((value) & mask) << shift)), ip->base + offset)


/*
 * AUD_SPDIF_RX_CTRL0
 */

#define offset__AUD_SPDIF_RX_CTRL0(ip) 0x0000
#define get__AUD_SPDIF_RX_CTRL0(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CTRL0(ip))
#define set__AUD_SPDIF_RX_CTRL0(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CTRL0(ip))

/* ENABLE */
#define shift__AUD_SPDIF_RX_CTRL0__ENABLE(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__ENABLE(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__ENABLE(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__ENABLE(ip))
#define set__AUD_SPDIF_RX_CTRL0__ENABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__ENABLE(ip), 1)
#define set__AUD_SPDIF_RX_CTRL0__DISABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__ENABLE(ip), 0)

/* INSTR_ENABLE */
#define shift__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip))
#define set__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip), 1)
#define set__AUD_SPDIF_RX_CTRL0__INSTR_DISABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__INSTR_ENABLE(ip), 0)

/* RESET_FIFO */
#define shift__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip) 2
#define mask__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip), \
		mask__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip))
#define set__AUD_SPDIF_RX_CTRL0__RESET_FIFO_NORMAL(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip), \
		mask__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip), 0)
#define set__AUD_SPDIF_RX_CTRL0__RESET_FIFO_RESET(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip), \
		mask__AUD_SPDIF_RX_CTRL0__RESET_FIFO(ip), 1)

/* LOCK_INT */
#define shift__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip) 3
#define mask__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip), value)

#define value__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(ip) << \
		shift__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__LOCK_INT_DISABLED(ip))
#define value__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(ip) << \
		 shift__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__LOCK_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__LOCK_INT_ENABLED(ip))


/* DET_ERR_INT */
#define shift__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip) 4
#define mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip), value)

#define value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(ip) << \
		shift__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_DISABLED(ip))
#define value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(ip) << \
		 shift__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__DET_ERR_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__DET_ERR_INT_ENABLED(ip))

/* NO_SIGNAL_INT */
#define shift__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip) 5
#define mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip), value)

#define value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(ip) << \
		shift__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_DISABLED(ip))
#define value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(ip) << \
		 shift__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__NO_SIGNAL_INT_ENABLED(ip))

/* FIFO_INT */
#define shift__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip) 6
#define mask__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip, value)	 \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip), value)

#define value__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(ip) << \
		shift__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__FIFO_INT_DISABLED(ip))
#define value__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(ip) << \
		 shift__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__FIFO_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__FIFO_INT_ENABLED(ip))

/* TEST_INT */
#define shift__AUD_SPDIF_RX_CTRL0__TEST_INT(ip) 7
#define mask__AUD_SPDIF_RX_CTRL0__TEST_INT(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__TEST_INT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__TEST_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__TEST_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__TEST_INT(ip, value)	 \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__TEST_INT(ip), \
		mask__AUD_SPDIF_RX_CTRL0__TEST_INT(ip), value)

#define value__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(ip) 0
#define mask__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(ip) << \
		shift__AUD_SPDIF_RX_CTRL0__TEST_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__TEST_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__TEST_INT_DISABLED(ip))
#define value__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(ip) 1
#define mask__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(ip) \
	(value__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(ip) << \
		 shift__AUD_SPDIF_RX_CTRL0__TEST_INT(ip))
#define set__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(ip) \
	set__AUD_SPDIF_RX_CTRL0__TEST_INT(ip, \
		value__AUD_SPDIF_RX_CTRL0__TEST_INT_ENABLED(ip))

/* SBCLK_DELTA */
#define shift__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip) 8
#define mask__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip) 0xf
#define get__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip), \
		mask__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip))
#define set__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip), \
		mask__AUD_SPDIF_RX_CTRL0__SBCLK_DELTA(ip), value)

/* DATA_MODE */
#define shift__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip) 12
#define mask__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip))
#define set__AUD_SPDIF_RX_CTRL0__DATA_MODE_32BIT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip), 0)
#define set__AUD_SPDIF_RX_CTRL0__DATA_MODE_24BIT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip), \
		mask__AUD_SPDIF_RX_CTRL0__DATA_MODE(ip), 1)

/* LSB_ALIGN */
#define shift__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip) 13
#define mask__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip), \
		mask__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip))
#define set__AUD_SPDIF_RX_CTRL0__LSB_ALIGN_MSB(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip), \
		mask__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip), 0)
#define set__AUD_SPDIF_RX_CTRL0__LSB_ALIGN_LSB(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip), \
		mask__AUD_SPDIF_RX_CTRL0__LSB_ALIGN(ip), 1)

/* I2S_OUT_ENDIAN_SEL */
#define shift__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip) 14
#define mask__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip))
#define set__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL_MSB(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip), 0)
#define set__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL_LSB(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__I2S_OUT_ENDIAN_SEL(ip), 1)

/* CH_ST_SEL */
#define shift__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip) 15
#define mask__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip))
#define set__AUD_SPDIF_RX_CTRL0__CH_ST_SEL_LEFT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip), 0)
#define set__AUD_SPDIF_RX_CTRL0__CH_ST_SEL_RIGHT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL0(ip), \
		shift__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL0__CH_ST_SEL(ip), 1)


/*
 * AUD_SPDIF_RX_CTRL1
 */

#define offset__AUD_SPDIF_RX_CTRL1(ip) 0x0004
#define get__AUD_SPDIF_RX_CTRL1(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CTRL1(ip))
#define set__AUD_SPDIF_RX_CTRL1(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CTRL1(ip))


/*
 * AUD_SPDIF_RX_CTRL2
 */

#define offset__AUD_SPDIF_RX_CTRL2(ip) 0x0008
#define get__AUD_SPDIF_RX_CTRL2(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CTRL2(ip))
#define set__AUD_SPDIF_RX_CTRL2(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CTRL2(ip))

/* SWCLK_POLARITY_SEL */
#define shift__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip) 0
#define mask__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip))
#define set__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL_LEFT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip), 0)
#define set__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL_RIGHT(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SWCLK_POLARITY_SEL(ip), 1)

/* SBCLK_POLARITY_SEL */
#define shift__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip) 1
#define mask__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip) 0x1
#define get__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip))
#define set__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL_FALLING(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip), 0)
#define set__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL_RISING(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_CTRL2(ip), \
		shift__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip), \
		mask__AUD_SPDIF_RX_CTRL2__SBCLK_POLARITY_SEL(ip), 1)

/*
 * AUD_SPDIF_RX_RESET
 */

#define offset__AUD_SPDIF_RX_RESET(ip) 0x000c
#define get__AUD_SPDIF_RX_RESET(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_RESET(ip))
#define set__AUD_SPDIF_RX_RESET(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_RESET(ip))

/* RESET */
#define shift__AUD_SPDIF_RX_RESET__RESET(ip) 0
#define mask__AUD_SPDIF_RX_RESET__RESET(ip) 0x1
#define get__AUD_SPDIF_RX_RESET__RESET(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_RESET(ip), \
		shift__AUD_SPDIF_RX_RESET__RESET(ip), \
		mask__AUD_SPDIF_RX_RESET__RESET(ip))
#define set__AUD_SPDIF_RX_RESET__RESET_DISABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_RESET(ip), \
		shift__AUD_SPDIF_RX_RESET__RESET(ip), \
		mask__AUD_SPDIF_RX_RESET__RESET(ip), 0)
#define set__AUD_SPDIF_RX_RESET__RESET_ENABLE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_RESET(ip), \
		shift__AUD_SPDIF_RX_RESET__RESET(ip), \
		mask__AUD_SPDIF_RX_RESET__RESET(ip), 1)


/*
 * AUD_SPDIF_RX_TEST_CTRL
 */

#define offset__AUD_SPDIF_RX_TEST_CTRL(ip) 0x0010
#define get__AUD_SPDIF_RX_TEST_CTRL(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_TEST_CTRL(ip))
#define set__AUD_SPDIF_RX_TEST_CTRL(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_TEST_CTRL(ip))


/*
 * AUD_SPDIF_RX_SAMP_FREQ
 */

#define offset__AUD_SPDIF_RX_SAMP_FREQ(ip) 0x0014
#define get__AUD_SPDIF_RX_SAMP_FREQ(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_SAMP_FREQ(ip))
#define set__AUD_SPDIF_RX_SAMP_FREQ(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_SAMP_FREQ(ip))


/*
 * AUD_SPDIF_RX_DETECT_MAX
 */

#define offset__AUD_SPDIF_RX_DETECT_MAX(ip) 0x0018
#define get__AUD_SPDIF_RX_DETECT_MAX(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_DETECT_MAX(ip))
#define set__AUD_SPDIF_RX_DETECT_MAX(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_DETECT_MAX(ip))

/* CNT */
#define shift__AUD_SPDIF_RX_DETECT_MAX__CNT(ip) 0
#define mask__AUD_SPDIF_RX_DETECT_MAX__CNT(ip) 0xfff
#define get__AUD_SPDIF_RX_DETECT_MAX__CNT(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_DETECT_MAX(ip), \
		shift__AUD_SPDIF_RX_DETECT_MAX__CNT(ip), \
		mask__AUD_SPDIF_RX_DETECT_MAX__CNT(ip))
#define set__AUD_SPDIF_RX_DETECT_MAX__CNT(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_DETECT_MAX(ip), \
		shift__AUD_SPDIF_RX_DETECT_MAX__CNT(ip), \
		mask__AUD_SPDIF_RX_DETECT_MAX__CNT(ip), value)

/*
 * AUD_SPDIF_RX_CH_STATUS0
 */

#define offset__AUD_SPDIF_RX_CH_STATUS0(ip) 0x001c
#define get__AUD_SPDIF_RX_CH_STATUS0(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS0(ip))
#define set__AUD_SPDIF_RX_CH_STATUS0(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS0(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS1
 */

#define offset__AUD_SPDIF_RX_CH_STATUS1(ip) 0x0020
#define get__AUD_SPDIF_RX_CH_STATUS1(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS1(ip))
#define set__AUD_SPDIF_RX_CH_STATUS1(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS1(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS2
 */

#define offset__AUD_SPDIF_RX_CH_STATUS2(ip) 0x0024
#define get__AUD_SPDIF_RX_CH_STATUS2(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS2(ip))
#define set__AUD_SPDIF_RX_CH_STATUS2(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS2(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS3
 */

#define offset__AUD_SPDIF_RX_CH_STATUS3(ip) 0x0028
#define get__AUD_SPDIF_RX_CH_STATUS3(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS3(ip))
#define set__AUD_SPDIF_RX_CH_STATUS3(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS3(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS4
 */

#define offset__AUD_SPDIF_RX_CH_STATUS4(ip) 0x002c
#define get__AUD_SPDIF_RX_CH_STATUS4(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS4(ip))
#define set__AUD_SPDIF_RX_CH_STATUS4(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS4(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS5
 */

#define offset__AUD_SPDIF_RX_CH_STATUS5(ip) 0x0030
#define get__AUD_SPDIF_RX_CH_STATUS5(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS5(ip))
#define set__AUD_SPDIF_RX_CH_STATUS5(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS5(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS6
 */

#define offset__AUD_SPDIF_RX_CH_STATUS6(ip) 0x0034
#define get__AUD_SPDIF_RX_CH_STATUS6(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS6(ip))
#define set__AUD_SPDIF_RX_CH_STATUS6(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS6(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS7
 */

#define offset__AUD_SPDIF_RX_CH_STATUS7(ip) 0x0038
#define get__AUD_SPDIF_RX_CH_STATUS7(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS7(ip))
#define set__AUD_SPDIF_RX_CH_STATUS7(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS7(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS8
 */

#define offset__AUD_SPDIF_RX_CH_STATUS8(ip) 0x003c
#define get__AUD_SPDIF_RX_CH_STATUS8(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS8(ip))
#define set__AUD_SPDIF_RX_CH_STATUS8(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS8(ip))


/*
 * AUD_SPDIF_RX_CH_STATUS9
 */

#define offset__AUD_SPDIF_RX_CH_STATUS9(ip) 0x0040
#define get__AUD_SPDIF_RX_CH_STATUS9(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUS9(ip))
#define set__AUD_SPDIF_RX_CH_STATUS9(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUS9(ip))


/*
 * AUD_SPDIF_RX_CH_STATUSA
 */

#define offset__AUD_SPDIF_RX_CH_STATUSA(ip) 0x0044
#define get__AUD_SPDIF_RX_CH_STATUSA(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUSA(ip))
#define set__AUD_SPDIF_RX_CH_STATUSA(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUSA(ip))


/*
 * AUD_SPDIF_RX_CH_STATUSB
 */

#define offset__AUD_SPDIF_RX_CH_STATUSB(ip) 0x0048
#define get__AUD_SPDIF_RX_CH_STATUSB(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_CH_STATUSB(ip))
#define set__AUD_SPDIF_RX_CH_STATUSB(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_CH_STATUSB(ip))


/*
 * AUD_SPDIF_RX_EVENT_STATUS16
 */

#define offset__AUD_SPDIF_RX_EVENT_STATUS16(ip) 0x004c
#define get__AUD_SPDIF_RX_EVENT_STATUS16(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_EVENT_STATUS16(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_EVENT_STATUS16(ip))

/* LOCK */
#define shift__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip) 0
#define value__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip) 1
#define mask__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip) \
	(value__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip) << \
		shift__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16__LOCK_EVENT_CLEAR(ip) \
	set__AUD_SPDIF_RX_EVENT_STATUS16(ip, \
		mask__AUD_SPDIF_RX_EVENT_STATUS16__LOCK(ip))

/* DET_ERR */
#define shift__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip) 1
#define value__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip) 1
#define mask__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip) \
	(value__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip) << \
		shift__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR_EVENT_CLEAR(ip) \
	set__AUD_SPDIF_RX_EVENT_STATUS16(ip, \
		mask__AUD_SPDIF_RX_EVENT_STATUS16__DET_ERR(ip))

/* NO_SIGNAL */
#define shift__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip) 2
#define value__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip) 1
#define mask__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip) \
	(value__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip) << \
		shift__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL_EVENT_CLEAR(ip) \
	set__AUD_SPDIF_RX_EVENT_STATUS16(ip, \
		mask__AUD_SPDIF_RX_EVENT_STATUS16__NO_SIGNAL(ip))

/* FIFO_UNDERFLOW */
#define shift__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip) 3
#define value__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip) 1
#define mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip) \
	(value__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip) << \
		shift__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW_EVENT_CLEAR(ip) \
	set__AUD_SPDIF_RX_EVENT_STATUS16(ip, \
		mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_UNDERFLOW(ip))

/* FIFO_OVERFLOW */
#define shift__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip) 4
#define value__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip) 1
#define mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip) \
	(value__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip) << \
		shift__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip))
#define set__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW_EVENT_CLEAR(ip) \
	set__AUD_SPDIF_RX_EVENT_STATUS16(ip, \
		mask__AUD_SPDIF_RX_EVENT_STATUS16__FIFO_OVERFLOW(ip))


/*
 * AUD_SPDIF_RX_NCO_CTRL
 */

#define offset__AUD_SPDIF_RX_NCO_CTRL(ip) 0x0050
#define get__AUD_SPDIF_RX_NCO_CTRL(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_NCO_CTRL(ip))
#define set__AUD_SPDIF_RX_NCO_CTRL(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_NCO_CTRL(ip))

/* NCO_RESET */
#define shift__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip) 0
#define mask__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip) 0x1
#define get__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip))
#define set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RUNNING(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip), 0)
#define set__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET_RESET(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_RESET(ip), 1)

/* NCO_INIT_PHASE_SEL */
#define shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip) 1
#define mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip) 0x1
#define get__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip))
#define set__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL_ZERO_PHASE(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip), 0)
#define set__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL_PROGRAMMED(ip) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE_SEL(ip), 1)

/* NCO_INIT_PHASE */
#define shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip) 2
#define mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip) 0xff
#define get__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip) \
	get__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip))
#define set__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip, value) \
	set__AUD_SPDIF_RX_REG(ip, \
		offset__AUD_SPDIF_RX_NCO_CTRL(ip), \
		shift__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip), \
		mask__AUD_SPDIF_RX_NCO_CTRL__NCO_INIT_PHASE(ip), value)


/*
 * AUD_SPDIF_RX_NCO_INCR0
 */

#define offset__AUD_SPDIF_RX_NCO_INCR0(ip) 0x0054
#define get__AUD_SPDIF_RX_NCO_INCR0(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_NCO_INCR0(ip))
#define set__AUD_SPDIF_RX_NCO_INCR0(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_NCO_INCR0(ip))


/*
 * AUD_SPDIF_RX_NCO_INCR1
 */

#define offset__AUD_SPDIF_RX_NCO_INCR1(ip) 0x0058
#define get__AUD_SPDIF_RX_NCO_INCR1(ip) \
	readl(ip->base + offset__AUD_SPDIF_RX_NCO_INCR1(ip))
#define set__AUD_SPDIF_RX_NCO_INCR1(ip, value) \
	writel(value, ip->base + offset__AUD_SPDIF_RX_NCO_INCR1(ip))


#endif
