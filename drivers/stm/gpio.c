/*
 * drivers/stm/gpio.c
 *
 * (c) 2010 STMicroelectronics Limited
 *
 * Authors: Pawel Moll <pawel.moll@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/stm/platform.h>
#include <linux/stm/pad.h>
#include <linux/stm/pio.h>
#include "reg_pio.h"

#ifndef CONFIG_ARM
#define chained_irq_enter(chip, desc)	do { } while (0)
#define chained_irq_exit(chip, desc)	do { } while (0)
#else
#include <asm/mach/irq.h>
#endif



struct stpio_pin {
#ifdef CONFIG_STPIO
	void (*func)(struct stpio_pin *pin, void *dev);
	void* dev;
	unsigned short port_no, pin_no;
#endif
};

struct stm_gpio_pin {
	unsigned char flags;
#define PIN_FAKE_EDGE		4
#define PIN_IGNORE_EDGE_FLAG	2
#define PIN_IGNORE_EDGE_VAL	1
#define PIN_IGNORE_RISING_EDGE	(PIN_IGNORE_EDGE_FLAG | 0)
#define PIN_IGNORE_FALLING_EDGE	(PIN_IGNORE_EDGE_FLAG | 1)
#define PIN_IGNORE_EDGE_MASK	(PIN_IGNORE_EDGE_FLAG | PIN_IGNORE_EDGE_VAL)

	struct stpio_pin stpio;
};

#define to_stm_gpio_port(chip) \
		container_of(chip, struct stm_gpio_port, gpio_chip)

#define dev_to_stm_gpio(dev) \
		container_of((dev), struct stm_gpio_port, dev)

struct stm_gpio_port {
	struct gpio_chip gpio_chip;
	void *base;
	unsigned long irq_level_mask;
	struct stm_gpio_pin pins[STM_GPIO_PINS_PER_PORT];
	struct platform_device *pdev;
	struct device_node *of_node;
	const char *bank_name;
};

struct stm_gpio_irqmux {
	void *base;
	int port_first;
};

int stm_gpio_num; /* Number of available internal PIOs (pins) */
EXPORT_SYMBOL(stm_gpio_num);

static unsigned int stm_gpio_irq_base; /* First irq number used by PIO "chip" */
static struct stm_gpio_port *stm_gpio_ports; /* PIO port descriptions */

/* PIO port base addresses copy, used by optimized gpio_get_value()
 * and gpio_set_value() in include/linux/stm/gpio.h */
void __iomem **stm_gpio_bases;
EXPORT_SYMBOL(stm_gpio_bases);



/*** PIO interrupt chained-handler implementation ***/

static void __stm_gpio_irq_handler(const struct stm_gpio_port *port)
{
	int port_no = port - stm_gpio_ports;
	int pin_no;
	unsigned long port_in, port_mask, port_comp, port_active;
	unsigned long port_level_mask = port->irq_level_mask;

	/* We don't want to mask the INTC2/ILC first level interrupt here,
	 * and as these are both level based, there is no need to ack. */

	port_in = get__PIO_PIN(port->base);
	port_comp = get__PIO_PCOMP(port->base);
	port_mask = get__PIO_PMASK(port->base);

	port_active = (port_in ^ port_comp) & port_mask;

	pr_debug("level_mask = 0x%08lx\n", port_level_mask);

	/* Level sensitive interrupts we can mask for the duration */
	set__PIO_CLR_PMASK(port->base, port_level_mask);

	/* Edge sensitive we want to know about if they change */
	set__PIO_CLR_PCOMP(port->base,
			~port_level_mask & port_active & port_comp);
	set__PIO_SET_PCOMP(port->base,
			~port_level_mask & port_active & ~port_comp);

	while ((pin_no = ffs(port_active)) != 0) {
		unsigned gpio;
		struct stm_gpio_pin *pin;
		unsigned int pin_irq;
		struct irq_data *irq_data;
		struct irq_desc *pin_irq_desc;
		unsigned long pin_mask;

		pin_no--;

		pr_debug("active = %ld  pinno = %d\n", port_active, pin_no);

		gpio = stm_gpio(port_no, pin_no);

		pin_irq = gpio_to_irq(gpio);
		irq_data = irq_get_irq_data(pin_irq);
		pin_irq_desc = irq_to_desc(pin_irq);
		pin = irq_get_chip_data(pin_irq);
		pin_mask = 1 << pin_no;

		port_active &= ~pin_mask;

		if (pin->flags & PIN_FAKE_EDGE) {
			int value = gpio_get_value(gpio);

			pr_debug("pinno %d PIN_FAKE_EDGE val %d\n",
					pin_no, value);
			if (value)
				set__PIO_SET_PCOMP(port->base, pin_mask);
			else
				set__PIO_CLR_PCOMP(port->base, pin_mask);

			if ((pin->flags & PIN_IGNORE_EDGE_MASK) ==
					(PIN_IGNORE_EDGE_FLAG | (value ^ 1)))
				continue;
		}

		if (unlikely(irqd_irq_disabled(irq_data) ||
			     irqd_irq_inprogress(irq_data))) {
			set__PIO_CLR_PMASK(port->base, pin_mask);
			/* The unmasking will be done by enable_irq in
			 * case it is disabled or after returning from
			 * the handler if it's already running.
			 */
			if (irqd_irq_inprogress(irq_data)) {
				/* Level triggered interrupts won't
				 * ever be reentered
				 */
				BUG_ON(port_level_mask & pin_mask);

				/* We used to set the IRQ_PENDING flag here,
				 * but there is now now sensible way to do this.
				 * So in effect we lose the one level of
				 * buffering
				 */
			}
			continue;
		} else {
			pin_irq_desc->handle_irq(pin_irq, pin_irq_desc);

			/* If our handler has disabled interrupts,
			 * then don't re-enable them */
			if (irqd_irq_disabled(irq_data)) {
				pr_debug("handler has disabled interrupts!\n");
				port_mask &= ~pin_mask;
			}
		}
	}

	/* Re-enable level */
	set__PIO_SET_PMASK(port->base, port_level_mask & port_mask);

	/* Do we need a software level as well, to cope with interrupts
	 * which get disabled during the handler execution? */

	pr_debug("exiting\n");
}

static void stm_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct stm_gpio_port *port = irq_get_handler_data(irq);
	struct irq_chip *chip = irq_get_chip(irq);

	chained_irq_enter(chip, desc);
	__stm_gpio_irq_handler(port);
	chained_irq_exit(chip, desc);
}

static void stm_gpio_irqmux_handler(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_get_chip(irq);
	struct stm_gpio_irqmux *irqmux = irq_get_handler_data(irq);
	unsigned long status;
	int bit;

	chained_irq_enter(chip, desc);
	status = readl(irqmux->base);
	while ((bit = ffs(status)) != 0) {
		struct stm_gpio_port *port;

		bit--;
		port = &stm_gpio_ports[irqmux->port_first + bit];
		__stm_gpio_irq_handler(port);
		status &= ~(1 << bit);
	}
	chained_irq_exit(chip, desc);
}

static void stm_gpio_irq_chip_disable(struct irq_data *d)
{
	unsigned gpio = irq_to_gpio(d->irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	pr_debug("disabling pin %d\n", pin_no);

	set__PIO_CLR_PMASK__CLR_PMASK__CLEAR(stm_gpio_bases[port_no], pin_no);
}

static void stm_gpio_irq_chip_enable(struct irq_data *d)
{
	unsigned gpio = irq_to_gpio(d->irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	pr_debug("enabling pin %d\n", pin_no);

	set__PIO_SET_PMASK__SET_PMASK__SET(stm_gpio_bases[port_no], pin_no);
}

static int stm_gpio_irq_chip_type(struct irq_data *d, unsigned type)
{
	unsigned gpio = irq_to_gpio(d->irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	struct stm_gpio_pin *pin = &port->pins[pin_no];
	int comp;

	pr_debug("setting pin %d to type %d\n", pin_no, type);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_FALLING_EDGE;
		comp = 1;
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		pin->flags = 0;
		comp = 0;
		port->irq_level_mask |= (1 << pin_no);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_RISING_EDGE;
		comp = 0;
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		pin->flags = 0;
		comp = 1;
		port->irq_level_mask |= (1 << pin_no);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		pin->flags = PIN_FAKE_EDGE;
		comp = gpio_get_value(gpio);
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	default:
		return -EINVAL;
	}

	set__PIO_PCOMP__PCOMP(port->base, pin_no, comp);

	return 0;
}

static int stm_gpio_irq_chip_wake(struct irq_data *d, unsigned int on)
{
	return 0;
}

static struct irq_chip stm_gpio_irq_chip = {
	.name		= "stm_gpio_irq",
	.irq_disable	= stm_gpio_irq_chip_disable,
	.irq_mask	= stm_gpio_irq_chip_disable,
	.irq_mask_ack	= stm_gpio_irq_chip_disable,
	.irq_unmask		= stm_gpio_irq_chip_enable,
	.irq_set_type	= stm_gpio_irq_chip_type,
	.irq_set_wake	= stm_gpio_irq_chip_wake,
};


static int stm_gpio_irq_init(int port_no)
{
	struct stm_gpio_pin *pin;
	struct irq_data *data;
	unsigned int pin_irq;
	int pin_no;
	int irq;

	pin = stm_gpio_ports[port_no].pins;
	pin_irq = stm_gpio_irq_base + (port_no * STM_GPIO_PINS_PER_PORT);

	/* Pre-allocate the gpios, this is a bit dumb as 99% of them will never
	 * be used. Should really allocate the irq on demand.
	 */
	irq = irq_alloc_descs(pin_irq, pin_irq, STM_GPIO_PINS_PER_PORT, 0);

	BUG_ON(irq != pin_irq);

	data = irq_get_irq_data(pin_irq);

	for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++) {
		irq_set_chip_and_handler_name(pin_irq, &stm_gpio_irq_chip,
				handle_simple_irq, "stm_gpio");
		irq_set_chip_data(pin_irq, pin);
#ifdef CONFIG_ARM
		set_irq_flags(pin_irq, IRQF_VALID);
#endif
		stm_gpio_irq_chip_type(data, IRQ_TYPE_LEVEL_HIGH);
		pin++;
		pin_irq++;
	}

	return 0;
}



/*** Low level hardware manipulation code for gpio/gpiolib and stpio ***/

static inline int __stm_gpio_get(struct stm_gpio_port *port, unsigned offset)
{
	return get__PIO_PIN__PIN(port->base, offset);
}

static inline void __stm_gpio_set(struct stm_gpio_port *port, unsigned offset,
		int value)
{
	if (value)
		set__PIO_SET_POUT__SET_POUT__SET(port->base, offset);
	else
		set__PIO_CLR_POUT__CLR_POUT__CLEAR(port->base, offset);
}

static inline void __stm_gpio_direction(struct stm_gpio_port *port,
		unsigned offset, unsigned int direction)
{
	WARN_ON(direction != STM_GPIO_DIRECTION_BIDIR &&
			direction != STM_GPIO_DIRECTION_OUT &&
			direction != STM_GPIO_DIRECTION_IN &&
			direction != STM_GPIO_DIRECTION_ALT_OUT &&
			direction != STM_GPIO_DIRECTION_ALT_BIDIR);

	set__PIO_PCx(port->base, offset, direction);
}



/*** Generic gpio & gpiolib interface implementation ***/

static int stm_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return stm_pad_claim_gpio(chip->base + offset);
}

static void stm_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	stm_pad_release_gpio(chip->base + offset);
}

static int stm_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	return __stm_gpio_get(port, offset);
}

static void stm_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	__stm_gpio_set(port, offset, value);
}

static int stm_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	stm_pad_configure_gpio(chip->base + offset, STM_GPIO_DIRECTION_IN);

	return 0;
}

static int stm_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	__stm_gpio_set(port, offset, value);

	stm_pad_configure_gpio(chip->base + offset, STM_GPIO_DIRECTION_OUT);

	return 0;
}

static int stm_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return stm_gpio_irq_base + chip->base + offset;
}

/* gpiolib doesn't support irq_to_gpio() call... */
int irq_to_gpio(unsigned int irq)
{
	if (irq < stm_gpio_irq_base || irq >= stm_gpio_irq_base + stm_gpio_num)
		return -EINVAL;

	return irq - stm_gpio_irq_base;
}
EXPORT_SYMBOL(irq_to_gpio);

int stm_gpio_direction(unsigned int gpio, unsigned int direction)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	BUG_ON(gpio >= stm_gpio_num);

	__stm_gpio_direction(&stm_gpio_ports[port_no], pin_no, direction);

	return 0;
}



/*** Deprecated stpio_... interface */

#ifdef CONFIG_STPIO

static inline int stpio_pin_to_irq(struct stpio_pin *pin)
{
	return gpio_to_irq(stm_gpio(pin->port_no, pin->pin_no));
}

struct stpio_pin *__stpio_request_pin(unsigned int port_no,
		unsigned int pin_no, const char *name, int direction,
		int __set_value, unsigned int value)
{
	struct stm_gpio_port *port;
	struct stm_gpio_pin *gpio_pin;
	int num_ports = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	if (port_no >= num_ports || pin_no >= STM_GPIO_PINS_PER_PORT)
		return NULL;

	port = &stm_gpio_ports[port_no];
	gpio_pin = &port->pins[pin_no];

	if (stm_pad_claim_gpio(stm_gpio(port_no, pin_no)) != 0)
		return NULL;

	if (__set_value)
		__stm_gpio_set(port, pin_no, value);

	__stm_gpio_direction(port, pin_no, direction);

	gpio_pin->stpio.port_no = port_no;
	gpio_pin->stpio.pin_no = pin_no;

	return &gpio_pin->stpio;
}
EXPORT_SYMBOL(__stpio_request_pin);

void stpio_free_pin(struct stpio_pin *pin)
{
	stm_pad_release_gpio(stm_gpio(pin->port_no, pin->pin_no));
}
EXPORT_SYMBOL(stpio_free_pin);

void stpio_configure_pin(struct stpio_pin *pin, int direction)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	__stm_gpio_direction(port, pin_no, direction);
}
EXPORT_SYMBOL(stpio_configure_pin);

void stpio_set_pin(struct stpio_pin *pin, unsigned int value)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	__stm_gpio_set(port, pin_no, value);
}
EXPORT_SYMBOL(stpio_set_pin);

unsigned int stpio_get_pin(struct stpio_pin *pin)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	return __stm_gpio_get(port, pin_no);
}
EXPORT_SYMBOL(stpio_get_pin);

static irqreturn_t stpio_irq_wrapper(int irq, void *dev_id)
{
	struct stpio_pin *pin = dev_id;

	pin->func(pin, pin->dev);
	return IRQ_HANDLED;
}

int stpio_flagged_request_irq(struct stpio_pin *pin, int comp,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev, unsigned long flags)
{
	int irq;
	const char *owner;
	int result;

	/* stpio style interrupt handling doesn't allow sharing. */
	BUG_ON(pin->func);

	irq = stpio_pin_to_irq(pin);
	pin->func = handler;
	pin->dev = dev;

	owner = stm_pad_get_gpio_owner(stm_gpio(pin->port_no, pin->pin_no));
	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	result = request_irq(irq, stpio_irq_wrapper, 0, owner, pin);
	BUG_ON(result);

	if (flags & IRQ_DISABLED) {
		/* This is a race condition waiting to happen... */
		disable_irq(irq);
	}

	return 0;
}
EXPORT_SYMBOL(stpio_flagged_request_irq);

void stpio_free_irq(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	free_irq(irq, pin);

	pin->func = 0;
	pin->dev = 0;
}
EXPORT_SYMBOL(stpio_free_irq);

void stpio_enable_irq(struct stpio_pin *pin, int comp)
{
	int irq = stpio_pin_to_irq(pin);

	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
}
EXPORT_SYMBOL(stpio_enable_irq);

/* This function is safe to call in an IRQ UNLESS it is called in */
/* the PIO interrupt callback function                            */
void stpio_disable_irq(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	disable_irq(irq);
}
EXPORT_SYMBOL(stpio_disable_irq);

/* This is safe to call in IRQ context */
void stpio_disable_irq_nosync(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	disable_irq_nosync(irq);
}
EXPORT_SYMBOL(stpio_disable_irq_nosync);

void stpio_set_irq_type(struct stpio_pin* pin, int triggertype)
{
	int irq = stpio_pin_to_irq(pin);

	set_irq_type(irq, triggertype);
}
EXPORT_SYMBOL(stpio_set_irq_type);

#endif /* CONFIG_STPIO */

#ifdef CONFIG_DEBUG_FS

const char *stm_gpio_get_direction(int gpio)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	const char *direction;

	switch (get__PIO_PCx(port->base, pin_no)) {
	case value__PIO_PCx__INPUT_WEAK_PULL_UP():
		direction = "input (weak pull up)";
		break;
	case value__PIO_PCx__BIDIR_OPEN_DRAIN():
	case value__PIO_PCx__BIDIR_OPEN_DRAIN__alt():
		direction = "bidirectional (open drain)";
		break;
	case value__PIO_PCx__OUTPUT_PUSH_PULL():
		direction = "output (push-pull)";
		break;
	case value__PIO_PCx__INPUT_HIGH_IMPEDANCE():
	case value__PIO_PCx__INPUT_HIGH_IMPEDANCE__alt():
		direction = "input (high impedance)";
		break;
	case value__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL():
		direction = "alternative function output "
			"(push-pull)";
		break;
	case value__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN():
		direction = "alternative function bidirectional "
			"(open drain)";
		break;
	default:
		/* Should never get here... */
		__WARN();
		direction = "unknown configuration";
		break;
	}

	return direction;
}

int stm_gpio_get_name(int gpio, char *buf, int len)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	int (*pin_name)(char *name, int size, int port, int pin);

	pin_name = NULL;
	if (port->pdev) {
		struct stm_plat_pio_data *plat_data;
		plat_data = dev_get_platdata(&port->pdev->dev);
		pin_name = plat_data->pin_name;
	}

	if (pin_name)
		return pin_name(buf, len, port_no, pin_no);
	else
		return snprintf(buf, len, "PIO%d.%d", port_no, pin_no);
}

#endif /* CONFIG_DEBUG_FS */

#ifdef CONFIG_OF

static int stm_gpio_xlate(struct gpio_chip *gc,
			const struct of_phandle_args *gpiospec, u32 *flags)
{
	if (WARN_ON(gc->of_gpio_n_cells < 1))
		return -EINVAL;

	if (WARN_ON(gpiospec->args_count < gc->of_gpio_n_cells))
		return -EINVAL;

	if (gpiospec->args[0] > gc->ngpio)
		return -EINVAL;
	/* Ignore  alt func and name setup. */
	return gpiospec->args[0];
}

static struct of_device_id stm_gpio_match[] = {
	{
		.compatible = "st,gpio",
	},
	{},
};
MODULE_DEVICE_TABLE(of, stm_gpio_match);

/*** Early initialization ***/

int __init of_stm_gpio_early_init(int irq_base)
{
	int num = 0;

	struct device_node *np, *child = NULL;
	int port_no;

	np = of_find_node_by_path("/gpio-controllers");
	if (!np)
		return 0;

	for_each_child_of_node(np, child) {
		if (of_match_node(stm_gpio_match, child))
			num++;
	}

	stm_gpio_num = num * STM_GPIO_PINS_PER_PORT;
	stm_gpio_irq_base = irq_base;

	stm_gpio_ports = alloc_bootmem(sizeof(*stm_gpio_ports) * num);
	stm_gpio_bases = alloc_bootmem(sizeof(*stm_gpio_bases) * num);
	if (!stm_gpio_ports || !stm_gpio_bases)
		panic("stm_gpio: Can't get bootmem!\n");

	child = NULL;
	port_no = 0;
	for_each_child_of_node(np, child) {
		struct stm_gpio_port *port = &stm_gpio_ports[port_no];
		void __iomem *regs;

		regs = of_iomap(child, 0);
		if (!regs)
			panic("stm_gpio: Can't get IO memory mapping!\n");
		port->base = regs;
		port->gpio_chip.request = stm_gpio_request;
		port->gpio_chip.free = stm_gpio_free;
		port->gpio_chip.get = stm_gpio_get;
		port->gpio_chip.set = stm_gpio_set;
		port->gpio_chip.direction_input = stm_gpio_direction_input;
		port->gpio_chip.direction_output = stm_gpio_direction_output;
		port->gpio_chip.to_irq = stm_gpio_to_irq;
		port->gpio_chip.base = port_no * STM_GPIO_PINS_PER_PORT;
		port->gpio_chip.ngpio = STM_GPIO_PINS_PER_PORT;
		port->of_node = child;
		of_property_read_string(child, "bank-name",
						&port->bank_name);
		port->gpio_chip.of_node = child;
		port->gpio_chip.of_gpio_n_cells = 1;
		port->gpio_chip.of_xlate = stm_gpio_xlate;

		stm_gpio_bases[port_no] = port->base;

		if (gpiochip_add(&port->gpio_chip) != 0)
			panic("stm_gpio: Failed to add gpiolib chip!\n");
		port_no++;
	}
	return num;
}
#else
int __init of_stm_gpio_early_init(int irq_base)
{
	return 0;
}
#endif

/* This is called early to allow board start up code to use PIO
 * (in particular console devices). */
void __init stm_gpio_early_init(struct platform_device pdevs[], int num,
		int irq_base)
{
	int port_no;

	stm_gpio_num = num * STM_GPIO_PINS_PER_PORT;
	stm_gpio_irq_base = irq_base;

	stm_gpio_ports = alloc_bootmem(sizeof(*stm_gpio_ports) * num);
	stm_gpio_bases = alloc_bootmem(sizeof(*stm_gpio_bases) * num);
	if (!stm_gpio_ports || !stm_gpio_bases)
		panic("stm_gpio: Can't get bootmem!\n");

	for (port_no = 0; port_no < num; port_no++) {
		struct platform_device *pdev = &pdevs[port_no];
		struct resource *memory;
		struct stm_gpio_port *port = &stm_gpio_ports[port_no];
		struct stm_plat_pio_data *data = dev_get_platdata(&pdev->dev);
		void __iomem *regs;

		/* Skip non existing ports */
		if (!pdev->name)
			continue;

		memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!memory)
			panic("stm_gpio: Can't find memory resource!\n");

		regs = NULL;
		if (data)
			regs = data->regs;
		if (!regs)
			regs = ioremap(memory->start,
				memory->end - memory->start + 1);
		if (!regs)
			panic("stm_gpio: Can't get IO memory mapping!\n");
		port->base = regs;
		port->gpio_chip.request = stm_gpio_request;
		port->gpio_chip.free = stm_gpio_free;
		port->gpio_chip.get = stm_gpio_get;
		port->gpio_chip.set = stm_gpio_set;
		port->gpio_chip.direction_input = stm_gpio_direction_input;
		port->gpio_chip.direction_output = stm_gpio_direction_output;
		port->gpio_chip.to_irq = stm_gpio_to_irq;
		port->gpio_chip.base = port_no * STM_GPIO_PINS_PER_PORT;
		port->gpio_chip.ngpio = STM_GPIO_PINS_PER_PORT;

		stm_gpio_bases[port_no] = port->base;

		if (gpiochip_add(&port->gpio_chip) != 0)
			panic("stm_gpio: Failed to add gpiolib chip!\n");
	}
}

static int of_stm_gpio_pin_name(char *name, int size, int port, int pin)
{
	return snprintf(name, size, "%s.%d",
					stm_gpio_ports[port].bank_name, pin);
}

static int stm_gpio_get_port_num(struct platform_device *pdev)
{
	int nr_ports = stm_gpio_num/STM_GPIO_PINS_PER_PORT;
	int i;
	struct stm_plat_pio_data *data;
	if (!pdev->dev.of_node)
		return pdev->id;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	data->pin_name = of_stm_gpio_pin_name;
	pdev->dev.platform_data = data;

	for (i = 0; i < nr_ports; i++)
		if (pdev->dev.of_node == stm_gpio_ports[i].of_node) {
			pdev->id = i;
			break;
	}

	return pdev->id;
}


/*** PIO bank platform device driver ***/

static int __devinit stm_gpio_probe(struct platform_device *pdev)
{
	int port_no;
	struct stm_gpio_port *port;
	struct resource *memory;
	int irq;

	port_no = stm_gpio_get_port_num(pdev);
	port = &stm_gpio_ports[port_no];

	BUG_ON(port_no < 0);
	BUG_ON(port_no >= stm_gpio_num);

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memory)
		return -EINVAL;

	if (!request_mem_region(memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0) {
		irq_set_chained_handler(irq, stm_gpio_irq_handler);
		irq_set_handler_data(irq, &stm_gpio_ports[port_no]);

		if (stm_gpio_irq_init(port_no) != 0) {
			printk(KERN_ERR "stm_gpio: Failed to init gpio "
					"interrupt!\n");
			return -EINVAL;
		}
	}

	port->gpio_chip.label = dev_name(&pdev->dev);
	dev_set_drvdata(&pdev->dev, port);
	port->pdev = pdev;

	/* This is a good time to check consistency of linux/stm/gpio.h
	 * declarations with the proper source... */
	BUG_ON(STM_GPIO_REG_SET_POUT != offset__PIO_SET_POUT());
	BUG_ON(STM_GPIO_REG_CLR_POUT != offset__PIO_CLR_POUT());
	BUG_ON(STM_GPIO_REG_PIN != offset__PIO_PIN());
	BUG_ON(STM_GPIO_DIRECTION_BIDIR != value__PIO_PCx__BIDIR_OPEN_DRAIN());
	BUG_ON(STM_GPIO_DIRECTION_OUT != value__PIO_PCx__OUTPUT_PUSH_PULL());
	BUG_ON(STM_GPIO_DIRECTION_IN != value__PIO_PCx__INPUT_HIGH_IMPEDANCE());
	BUG_ON(STM_GPIO_DIRECTION_ALT_OUT !=
			value__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL());
	BUG_ON(STM_GPIO_DIRECTION_ALT_BIDIR !=
			value__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN());

	return 0;
}

static struct platform_driver stm_gpio_driver = {
	.driver	= {
		.name = "stm-gpio",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_gpio_match),
	},
	.probe = stm_gpio_probe,
};


static void *stm_gpio_irqmux_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_pio_irqmux_data *data;

	if (!np)
		return pdev->dev.platform_data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	of_property_read_u32(np, "first-port", &data->port_first);
	of_property_read_u32(np, "ports", &data->ports_num);
	pdev->dev.platform_data = data;
	return data;
}

/*** PIO IRQ status register platform device driver ***/

static int __devinit stm_gpio_irqmux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm_plat_pio_irqmux_data *plat_data;
	struct stm_gpio_irqmux *irqmux;
	struct resource *memory;
	int irq;
	int port_no;

	plat_data = stm_gpio_irqmux_get_pdata(pdev);
	BUG_ON(!plat_data);

	irqmux = devm_kzalloc(dev, sizeof(*irqmux), GFP_KERNEL);
	if (!irqmux)
		return -ENOMEM;

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!memory || irq < 0)
		return -EINVAL;

	if (!devm_request_mem_region(dev, memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	irqmux->base = devm_ioremap_nocache(dev, memory->start,
			memory->end - memory->start + 1);
	if (!irqmux->base)
		return -ENOMEM;

	irqmux->port_first = plat_data->port_first;

	irq_set_chained_handler(irq, stm_gpio_irqmux_handler);
	irq_set_handler_data(irq, irqmux);


	for (port_no = irqmux->port_first;
			port_no < irqmux->port_first + plat_data->ports_num;
			port_no++) {
		BUG_ON(port_no >= stm_gpio_num);

		if (stm_gpio_irq_init(port_no) != 0) {
			printk(KERN_ERR "stm_gpio: Failed to init gpio "
					"interrupt for port %d!\n", port_no);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_gpio_irqmux_match[] = {
	{
		.compatible = "st,gpio-irqmux",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_gpio_irqmux_match);
#endif

static struct platform_driver stm_gpio_irqmux_driver = {
	.driver	= {
		.name = "stm-gpio-irqmux",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_gpio_irqmux_match),
	},
	.probe = stm_gpio_irqmux_probe,
};



/*** Drivers initialization ***/

#ifdef CONFIG_PM
static void stm_gpio_port_suspend(struct stm_gpio_port *port)
{
	int port_no = port - stm_gpio_ports;
	int pin_no;

	/* Enable the wakeup pin IRQ if required */
	for (pin_no = 0; pin_no < port->gpio_chip.ngpio; ++pin_no) {
		int irq = gpio_to_irq(stm_gpio(port_no, pin_no));
		struct irq_data *data = irq_get_irq_data(irq);

		if (irqd_is_wakeup_set(data))
			stm_gpio_irq_chip_enable(data);
		else
			stm_gpio_irq_chip_disable(data);
	}
}

static int stm_gpio_suspend(void)
{
	int port_no;
	int port_num = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	for (port_no = 0; port_no < port_num; ++port_no)
		stm_gpio_port_suspend(&stm_gpio_ports[port_no]);

	return 0;
}

static struct syscore_ops stm_gpio_syscore = {
	.suspend = stm_gpio_suspend,
};

static __init int stm_gpio_syscore_init(void)
{
	register_syscore_ops(&stm_gpio_syscore);
	return 0;
}

module_init(stm_gpio_syscore_init);
#endif

static int __init stm_gpio_init(void)
{
	int ret;

	ret = platform_driver_register(&stm_gpio_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&stm_gpio_irqmux_driver);
	if (ret)
		return ret;

	return ret;
}
postcore_initcall(stm_gpio_init);
