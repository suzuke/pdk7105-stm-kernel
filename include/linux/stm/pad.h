/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PAD_H
#define __LINUX_STM_PAD_H

#include <linux/stm/sysconf.h>

/* The stm_pad_gpio_value structure describes PIOs that are to be claimed in
 * order to achieve I/O configuration required by a driver.
 *
 * "function" means the so-called "alternative PIO function",
 * usually described in SOCs datasheets. It just describes
 * which one of possible signals is to be multiplexed to
 * the actual pin. It is then used by SOC-specific "ops"
 * callbacks provided when stm_pad_init() is called (see below).
 *
 * Function number meaning is absolutely up to the BSP author.
 * There is just a polite suggestion that 0 could mean "normal"
 * PIO functionality (as in: input/output, set high/low level).
 * Other numbers may be related to datasheet definitions (usually
 * 1 and more).
 *
 * "ignored" direction means that the PIO will not be claimed at,
 * so setting it can be used to "remove" a PIO from configuration
 * in runtime. */

enum stm_pad_gpio_direction {
	stm_pad_gpio_direction_unknown,
	stm_pad_gpio_direction_in,
	stm_pad_gpio_direction_in_pull_up,
	stm_pad_gpio_direction_out,
	stm_pad_gpio_direction_bidir,
	stm_pad_gpio_direction_bidir_pull_up,
	stm_pad_gpio_direction_ignored
};

struct stm_pad_gpio {
	unsigned gpio;
	enum stm_pad_gpio_direction direction;
	int out_value;
	int function;
	const char *name;
	void *priv;
};

#define STM_PAD_PIO_IN(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _function, \
	}

#define STM_PAD_PIO_IN_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_OUT(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.out_value = -1, \
		.function = _function, \
	}

#define STM_PAD_PIO_OUT_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.out_value = -1, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_BIDIR(_port, _pin, _function) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir, \
		.out_value = -1, \
		.function = _function, \
	}

#define STM_PAD_PIO_BIDIR_NAMED(_port, _pin, _function, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir, \
		.out_value = -1, \
		.function = _function, \
		.name = _name, \
	}

#define STM_PAD_PIO_STUB_NAMED(_port, _pin, _name) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_unknown, \
		.name = _name, \
	}



/* The bits that give us the most grief are "sysconf" values, and
 * they are the most likely the SOC-specific settings that must
 * be set while configuring the chip to some function, as required
 * by a driver.
 *
 * Notice that you are not supposed to define GPIO muxing (the
 * "alternative functions" mentioned above) related bits here.
 * They should be configured automagically via SOC-specific
 * muxing funtions (see stm_pad_init() below) */

struct stm_pad_sysconf {
	int regtype;
	int regnum;
	int lsb;
	int msb;
	int value;
	const char *name;
};

#define STM_PAD_SYS_CFG(_regnum, _lsb, _msb, _value) \
	{ \
		.regtype = SYS_CFG, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.value = _value, \
	}

#define STM_PAD_SYS_CFG_BANK(_bank, _regnum, _lsb, _msb, _value) \
	{ \
		.regtype = SYS_CFG_BANK##_bank, \
		.regnum = _regnum, \
		.lsb = _lsb, \
		.msb = _msb, \
		.value = _value, \
	}

/* We have to do this indirection to allow the first argument to
 * STM_PAD_SYSCONF to be a macro, as used by 5197 for example. */
#define ___STM_PAD_SYSCONF(_regtype, _regnum, _lsb, _msb, _value, _name) \
        { \
                .regtype = _regtype, \
                .regnum = _regnum, \
                .lsb = _lsb, \
                .msb = _msb, \
                .value = _value, \
		.name = _name, \
        }
#define STM_PAD_SYSCONF(_reg, _lsb, _msb, _value) \
	___STM_PAD_SYSCONF(_reg, _lsb, _msb, _value, NULL)

#define STM_PAD_SYSCONF_NAMED(_reg, _lsb, _msb, _value, _name)		\
	___STM_PAD_SYSCONF(_reg, _lsb, _msb, _value, _name)



/* Pad state structure pointer is returned by the claim functions */
struct stm_pad_state;



/* All above bits and pieces are gathered together in the below
 * structure, known as "pad configuration".
 *
 * It contains lists of GPIOs used and sysconfig bits to be
 * configured on demand of a driver.
 *
 * In special cases one may wish to use custom claim function,
 * which is executed as the _last_ in order when claiming
 * and must return 0 or other value in case of error. */

struct stm_pad_config {
	int gpios_num;
	struct stm_pad_gpio *gpios;
	int sysconfs_num;
	struct stm_pad_sysconf *sysconfs;
	int (*custom_claim)(struct stm_pad_state *state, void *priv);
	void (*custom_release)(struct stm_pad_state *state, void *priv);
	void *custom_priv;
};


struct stm_pad_ops {
	int (*gpio_config)(unsigned gpio,
		enum stm_pad_gpio_direction direction,
		int function, void *priv);
	void (*gpio_report)(unsigned int gpio, char *buf, int len);
};

/* Pad manager initialization
 *
 * The ops structure should be provided by the SOC BSP, and contains the
 * gpio_config member which is a pointer to a SoC specific function to
 * configure given GPIO to requested direction & alternative function,
 * and gpio_report which returns the current configuration as a string
 * for use in debugfs.
 *
 * gpios_num is a overall number of PIO lines provided by the SOC,
 * gpio_function_in and gpio_function_out are the numbers that should be passed
 * to the gpio_config function in order to select generic PIO functionality, for
 * input and output directions respectively.
 *
 * See also above (stm_pad_gpio_value definition). */

int stm_pad_init(int gpios_num, int gpio_function_in, int gpio_function_out,
			const struct stm_pad_ops *gpio_ops);



/* Driver interface */

struct stm_pad_state *stm_pad_claim(struct stm_pad_config *config,
		const char *owner);
void stm_pad_setup(struct stm_pad_state *state);
void stm_pad_release(struct stm_pad_state *state);

struct stm_pad_state *devm_stm_pad_claim(struct device *dev,
		struct stm_pad_config *config, const char *owner);
void devm_stm_pad_release(struct device *dev, struct stm_pad_state *state);

int stm_pad_update_gpio(struct stm_pad_state *state, const char* name,
		enum stm_pad_gpio_direction direction,
		int out_value, int function, void *priv);

/* Functions below are private methods, for the GPIO driver use only! */
int stm_pad_claim_gpio(unsigned gpio);
void stm_pad_configure_gpio(unsigned gpio, unsigned direction);
void stm_pad_release_gpio(unsigned gpio);
const char *stm_pad_get_gpio_owner(unsigned gpio);



/* GPIO interface */

/* "name" is the GPIO name as defined in "struct stm_pad_gpio".
 * Returns gpio number or STM_GPIO_INVALID in case of error */
unsigned stm_pad_gpio_request_input(struct stm_pad_state *state,
		const char *name);
unsigned stm_pad_gpio_request_output(struct stm_pad_state *state,
		const char *name, int value);
void stm_pad_gpio_free(struct stm_pad_state *state, unsigned gpio);



/* GPIO definition helpers
 *
 * If a GPIO on the list in pad configuration is defined with a name,
 * it is possible to perform some operations on it in easy way... */

int stm_pad_set_gpio(struct stm_pad_config *config, const char *name,
		unsigned gpio);

#define stm_pad_set_pio(config, name, port, pin) \
	stm_pad_set_gpio(config, name, stm_gpio(port, pin))

int stm_pad_set_direction_function(struct stm_pad_config *config,
		const char *name, enum stm_pad_gpio_direction direction,
		int out_value, int function);

#define stm_pad_set_pio_in(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_in, -1, function)

#define stm_pad_set_pio_out(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_out, -1, function)

#define stm_pad_set_pio_bidir(config, name, function) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_bidir, -1, function)

#define stm_pad_set_pio_ignored(config, name) \
	stm_pad_set_direction_function(config, name, \
			stm_pad_gpio_direction_ignored, -1, -1)

int stm_pad_set_priv(struct stm_pad_config *config, const char *name,
		void *priv);

int stm_pad_set_sysconf(struct stm_pad_config *config, const char *name,
		int value);



/* Dynamic pad configuration allocation
 *
 * In some cases it's easier to create a pad configuration in runtime,
 * rather then to prepare 2^16 different static blobs (or to alter
 * 90% of pre-defined one). The API below helps in this... */

struct stm_pad_config *stm_pad_config_alloc(int gpio_values_num,
		int sysconf_values_num);

int stm_pad_config_add_sysconf(struct stm_pad_config *config,
		int regtype, int regnum, int lsb, int msb, int value);

#define stm_pad_config_add_sys_cfg(config, regnum, lsb, msb, value) \
	stm_pad_config_add_sysconf(config, SYS_CFG, regnum, lsb, msb, value)

int stm_pad_config_add_gpio_named(struct stm_pad_config *config,
		unsigned gpio, enum stm_pad_gpio_direction direction,
		int out_value, int function, const char *name);

#define stm_pad_config_add_pio(config, port, pin, \
			direction, out_value, function) \
	stm_pad_config_add_gpio_named(config, stm_gpio(port, pin), \
			direction, out_value, function, NULL)

#define stm_pad_config_add_pio_named(config, port, pin, \
			direction, out_value, function, name) \
	stm_pad_config_add_gpio_named(config, stm_gpio(port, pin), \
			direction, out_value, function, name)

#define stm_pad_config_add_pio_in(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_in, -1, function)

#define stm_pad_config_add_pio_in_named(config, port, pin, function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_in, -1, function, name)

#define stm_pad_config_add_pio_out(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_out, -1, function)

#define stm_pad_config_add_pio_out_named(config, port, pin, \
			function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_out, -1, function, name)

#define stm_pad_config_add_pio_bidir(config, port, pin, function) \
	stm_pad_config_add_pio(config, port, pin, \
			stm_pad_gpio_direction_bidir, -1, function)

#define stm_pad_config_add_pio_bidir_named(config, port, pin, \
			function, name) \
	stm_pad_config_add_pio_named(config, port, pin, \
			stm_pad_gpio_direction_bidir, -1, function, name)

#ifdef CONFIG_OF

/*
 * Pinconf is represented in an opaque unsigned long variable.
 * Below is the bit allocation details for each possible configuration.
 * All the bit fields can be encapsulated into four variables
 * (retime-type, retime-clk, force-delay, retime-delay)
 *
 *        +----------------+
 *[31:24] |   reserved-2   |
 *        +----------------+
 *[25]    |frc-dly-innotout|
 *        +----------------+
 *[24]    |  force-delay   |
 *        +----------------+-------------
 *[23]    |    retime      |		|
 *        +----------------+		|
 *[22]    | retime-invclk  |		|
 *        +----------------+		v
 *[21]    |retime-clknotdat|       [Retime-type	]
 *        +----------------+		^
 *[20]    |   retime-de    |		|
 *        +----------------+-------------
 *[19:18] | retime-clk     |------>[Retime-Clk	]
 *        +----------------+
 *[17:16] |  reserved-1    |
 *        +----------------+
 *[15..0] | retime-delay   |------>[Retime Delay]
 *        +----------------+
 */

#define STM_PINCONF_UNPACK(conf, param) \
				((conf >> STM_PINCONF_ ##param ##_SHIFT)\
				& STM_PINCONF_ ##param ##_MASK)

#define STM_PINCONF_PACK(conf, val, param)	{ conf |=  \
				((val & STM_PINCONF_ ##param ##_MASK) << \
					STM_PINCONF_ ##param ##_SHIFT) }

#define STM_PINCONF_FORCE_DELAY_MASK		1
#define STM_PINCONF_FORCE_DELAY_SHIFT		24
#define STM_PINCONF_FORCE_DELAY			(1 << FORCE_DELAY_SHIFT)
#define STM_PINCONF_UNPACK_FORCE_DELAY(conf)	\
			STM_PINCONF_UNPACK(conf, FORCE_DELAY)
#define STM_PINCONF_PACK_FORCE_DELAY(conf, val)	\
			STM_PINCONF_PACK(conf, val, FORCE_DELAY)

#define STM_PINCONF_FORCE_DELAY_INNOTOUT_MASK	1
#define STM_PINCONF_FORCE_DELAY_INNOTOUT_SHIFT	25
#define STM_PINCONF_FORCE_DELAY_INNOTOUT	\
			(1 << FORCE_DELAY_INNOTOUT_SHIFT)
#define STM_PINCONF_UNPACK_FORCE_DELAY_INNOTOUT(conf)	\
			STM_PINCONF_UNPACK(conf, FORCE_DELAY_INNOTOUT)
#define STM_PINCONF_PACK_FORCE_DELAY_INNOTOUT(conf, val)	\
			STM_PINCONF_PACK(conf, val, FORCE_DELAY_INNOTOUT)

#define STM_PINCONF_RT_MASK		0x1
#define STM_PINCONF_RT_SHIFT		23
#define STM_PINCONF_RT			(1 << STM_PINCONF_RT_SHIFT)
#define STM_PINCONF_UNPACK_RT(conf)	STM_PINCONF_UNPACK(conf, RT)
#define STM_PINCONF_PACK_RT(conf, val)	STM_PINCONF_PACK(conf, val, RT)

#define STM_PINCONF_RT_INVERTCLK_MASK	0x1
#define STM_PINCONF_RT_INVERTCLK_SHIFT	22
#define STM_PINCONF_RT_INVERTCLK	(1 << STM_PINCONF_RT_INVERTCLK_SHIFT)
#define STM_PINCONF_UNPACK_RT_INVERTCLK(conf) \
				STM_PINCONF_UNPACK(conf, RT_INVERTCLK)
#define STM_PINCONF_PACK_RT_INVERTCLK(conf, val) \
				STM_PINCONF_PACK(conf, val, RT_INVERTCLK)


#define STM_PINCONF_RT_CLKNOTDATA_MASK	0x1
#define STM_PINCONF_RT_CLKNOTDATA_SHIFT	21
#define STM_PINCONF_RT_CLKNOTDATA	(1 << STM_PINCONF_RT_CLKNOTDATA_SHIFT)
#define STM_PINCONF_UNPACK_RT_CLKNOTDATA(conf)	\
					STM_PINCONF_UNPACK(conf, RT_CLKNOTDATA)
#define STM_PINCONF_PACK_RT_CLKNOTDATA(conf, val) \
				STM_PINCONF_PACK(conf, val, RT_CLKNOTDATA)

#define STM_PINCONF_RT_DOUBLE_EDGE_MASK	 0x1
#define STM_PINCONF_RT_DOUBLE_EDGE_SHIFT 20
#define STM_PINCONF_RT_DOUBLE_EDGE	(1 << STM_PINCONF_RT_DOUBLE_EDGE_SHIFT)
#define STM_PINCONF_UNPACK_RT_DOUBLE_EDGE(conf)	\
				STM_PINCONF_UNPACK(conf, RT_DOUBLE_EDGE)
#define STM_PINCONF_PACK_RT_DOUBLE_EDGE(conf, val) \
				STM_PINCONF_PACK(conf, val, RT_DOUBLE_EDGE)

#define STM_PINCONF_RT_CLK_MASK		0x3
#define STM_PINCONF_RT_CLK_SHIFT	18
#define STM_PINCONF_RT_CLK_A		(0 << STM_PINCONF_RT_CLK_SHIFT)
#define STM_PINCONF_RT_CLK_B		(1 << STM_PINCONF_RT_CLK_SHIFT)
#define STM_PINCONF_RT_CLK_C		(2 << STM_PINCONF_RT_CLK_SHIFT)
#define STM_PINCONF_RT_CLK_D		(3 << STM_PINCONF_RT_CLK_SHIFT)

#define STM_PINCONF_UNPACK_RT_CLK(conf)	STM_PINCONF_UNPACK(conf, RT_CLK)
#define STM_PINCONF_PACK_RT_CLK(conf, val) \
					STM_PINCONF_PACK(conf, val, RT_CLK)


/* RETIME_DELAY in Pico Secs */
#define STM_PINCONF_RT_DELAY_MASK		0xffff
#define STM_PINCONF_RT_DELAY_SHIFT	0
#define	STM_PINCONF_RT_DELAY_0		(0 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_300	(300 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_500	(500 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_750	(750 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_1000	(1000 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_1250	(1250 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_1500	(1500 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_1750	(1750 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_2000	(2000 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_2250	(2250 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_2500	(2500 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_2750	(2750 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_3000	(3000 << STM_PINCONF_RT_DELAY_SHIFT)
#define	STM_PINCONF_RT_DELAY_3250	(3250 << STM_PINCONF_RT_DELAY_SHIFT)

#define STM_PINCONF_UNPACK_RT_DELAY(conf) \
				STM_PINCONF_UNPACK(conf, RT_DELAY)
#define STM_PINCONF_PACK_RT_DELAY(conf, val) \
				STM_PINCONF_PACK(conf, val, RT_DELAY)



#define STM_PINCONF_RT_TYPE_DEF		(0x0)
/*
 * B Mode
 * Bypass retime with optional delay parameter
 */
#define STM_PINCONF_RT_TYPE_BYPASS	(0)
/*
 * R0, R1, R0D, R1D modes
 * single-edge data non inverted clock, retime data with clk
 */
#define STM_PINCONF_RT_TYPE_SE_NICLK_IO	(STM_PINCONF_RT)
/*
 * RIV0, RIV1, RIV0D, RIV1D modes
 * single-edge data inverted clock, retime data with clk
 */
#define STM_PINCONF_RT_TYPE_SE_ICLK_IO	(STM_PINCONF_RT | \
					STM_PINCONF_RT_INVERTCLK)
/*
 * R0E, R1E, R0ED, R1ED modes
 * double-edge data, retime data with clk
 */
#define STM_PINCONF_RT_TYPE_DE_IO	(STM_PINCONF_RT | \
					STM_PINCONF_RT_DOUBLE_EDGE)
/*
 * CIV0, CIV1 modes with inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define STM_PINCONF_RT_TYPE_ICLK	(STM_PINCONF_RT | \
					STM_PINCONF_RT_CLKNOTDATA | \
					STM_PINCONF_RT_INVERTCLK)
/*
 * CLK0, CLK1 modes with non-inverted clock
 * Retiming the clk pins will park clock & reduce the noise within the core.
 */
#define STM_PINCONF_RT_TYPE_NICLK \
				(STM_PINCONF_RT | STM_PINCONF_RT_CLKNOTDATA)


struct stm_pad_config *stm_of_get_pad_config(struct device *dev);
struct stm_pad_config *stm_of_get_pad_config_index(struct device *dev,
				int index);
struct stm_pad_config *stm_of_get_pad_config_from_node(struct device *dev,
				struct device_node *np, int index);

void stm_of_pad_config_fixup(struct device *dev,
		struct device_node *fixup_np, struct stm_pad_state *state);

#else /* CONFIG_OF */

static inline struct stm_pad_config *stm_of_get_pad_config(struct device *dev)
{
	return NULL;
}

static inline struct stm_pad_config *stm_of_get_pad_config_index(
		struct device *dev, int index)
{
	return NULL;
}

static inline struct stm_pad_config *stm_of_get_pad_config_from_node(
		struct device *dev, struct device_node *np, int index)
{
	return NULL;
}


static inline void stm_of_pad_config_fixup(struct device *dev,
		struct device_node *fixup_np, struct stm_pad_state *state)
{
	return;
}
#endif /* CONFIG_OF */

#endif
