/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PLATFORM_H
#define __LINUX_STM_PLATFORM_H

#include <linux/gpio.h>
#include <media/lirc.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/clkdev.h>
#include <linux/spi/spi.h>
#include <linux/clkdev.h>
#include <linux/pci.h>
#include <linux/stm/pad.h>
#include <linux/stm/nand.h>
#include <linux/stmmac.h>
#include <linux/stm/miphy.h>
#include <linux/stm/amba_bridge.h>
#include <linux/stm/mmc.h>
#include <linux/stm/device.h>

/*** Platform definition helpers ***/

#define STM_PLAT_RESOURCE_MEM(_start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}

#define STM_PLAT_RESOURCE_MEM_NAMED(_name, _start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.name = (_name), \
			.flags = IORESOURCE_MEM, \
		}

#if defined(CONFIG_CPU_SUBTYPE_ST40)

#define STM_PLAT_RESOURCE_IRQ(_st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.flags = IORESOURCE_IRQ, \
		}

#define STM_PLAT_RESOURCE_IRQ_NAMED(_name, _st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.name = (_name), \
			.flags = IORESOURCE_IRQ, \
		}

#elif defined(CONFIG_ARCH_ST200)

#define STM_PLAT_RESOURCE_IRQ(_st40, _st200) \
		{ \
			.start = (_st200), \
			.end = (_st200), \
			.flags = IORESOURCE_IRQ, \
		}

#define STM_PLAT_RESOURCE_IRQ_NAMED(_name, _st40, _st200) \
		{ \
			.start = (_st200), \
			.end = (_st200), \
			.name = (_name), \
			.flags = IORESOURCE_IRQ, \
		}

#endif

#define STM_PLAT_RESOURCE_DMA(_req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.flags = IORESOURCE_DMA, \
		}

#define STM_PLAT_RESOURCE_DMA_NAMED(_name, _req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.name = (_name), \
			.flags = IORESOURCE_DMA, \
		}



/*** ASC platform data ***/

struct stm_plat_asc_data {
	int hw_flow_control:1;
	int txfifo_bug:1;
	int force_m1:1;
	struct stm_pad_config *pad_config;
	void __iomem *regs;
	char *clk_id;
};

extern struct platform_device *stm_asc_console_device;

/*** LPC platform data ***/
struct stm_plat_rtc_lpc {
	unsigned int no_hw_req:1;	/* iomem in sys/serv 5197 */
	unsigned int need_wdt_reset:1;	/* W/A on 7141 */
	unsigned char irq_edge_level;
	char *clk_id;
	unsigned long force_clk_rate;
};

/*** SSC platform data ***/

struct stm_plat_ssc_data {
	struct stm_pad_config *pad_config;
	void (*spi_chipselect)(struct spi_device *, int);
	unsigned int i2c_fastmode:1;
};



/*** LiRC platform data ***/

struct stm_plat_lirc_data {
	unsigned int irbclock;		/* IRB block clock
					 * (set to 0 for auto) */
	unsigned int irbclkdiv;		/* IRB block clock divison
					 * (set to 0 for auto) */
	unsigned int irbperiodmult;	/* manual setting period multiplier */
	unsigned int irbperioddiv;	/* manual setting period divisor */
	unsigned int irbontimemult;	/* manual setting pulse period
					 * multiplier */
	unsigned int irbontimediv;	/* manual setting pulse period
					 * divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus
					   clock by */
	unsigned int rxpolarity;	/* flag to set gpio rx polarity
					 * (usually set to 1) */
	unsigned int subcarrwidth;	/* Subcarrier width in percent - this
					 * is used to make the subcarrier
					 * waveform square after passing
					 * through the 555-based threshold
					 * detector on ST boards */
	struct stm_pad_config *pads;	/* pads to be claimed */
	unsigned int rxuhfmode:1;	/* RX UHF mode enabled */
	unsigned int txenabled:1;	/* TX operation is possible */
};



/*** PWM platform data ***/

/* Private data for the PWM driver */
#define STM_PLAT_PWM_NUM_CHANNELS 4
struct stm_plat_pwm_data {
	int channel_enabled[STM_PLAT_PWM_NUM_CHANNELS];
	struct stm_pad_config *channel_pad_config[STM_PLAT_PWM_NUM_CHANNELS];
};



/* This Allows bypass of the CPU handshake in the chain of the reset generator
 * After boot the modepin value can be bypassed by using the system_config
 * A typical use of this system config bit is to bypass the ST231 resetout,
 * infact the ST40 may change the boot address of the ST231 (by default 0x0).
 * To allow the ST231 to take into account this new boot address it must be
 * reset again via a config register.
 * In this case, the resetout of the ST231 must not be propagates out to
 * other IPs which may have been already configured.
 */
enum stm_cpu_reset_bypass {
	/* No bypass */
	stm_bypass_none,
	/* bypass SH4 + ST231 handshakes */
	stm_bypass_st40_st231_handshake,
	/* bypass ST231 handshakes */
	stm_bypass_st231_handshake,
};

/* st231 platform data
 * at Minimal device_config should contain sysconfig named
 * "BOOT_ADDR" and "RESET"
 */
struct plat_stm_st231_coproc_data {
	const char *name;
	int id;
	struct stm_device_config *device_config;
	int (*reset_bypass)(enum stm_cpu_reset_bypass bypass);
	int (*stbus_req_filter)(struct stm_device_state *state,
				int on);
	int boot_shift;
	int not_reset;
};

/* st40 platform data */
struct plat_stm_st40_coproc_data {
	const char *name;
	int id;
	struct stm_device_config *device_config;
	int (*cpu_boot)(struct stm_device_state *state,
				unsigned long boot_addr,
				unsigned long phys_addr);
	int (*reset_bypass)(enum stm_cpu_reset_bypass bypass);
	int (*stbus_req_filter)(struct stm_device_state *state,
				int on);
};



/*** Temperature sensor data ***/

struct plat_stm_temp_data {
	struct {
		int group, num, lsb, msb;
	} dcorrect, overflow, data;
	struct stm_device_config *device_config;
	int calibrated:1;
	int calibration_value;
	void (*custom_set_dcorrect)(void *priv);
	unsigned long (*custom_get_data)(void *priv);
	void *custom_priv;
};

/*** USB platform data ***/

#define STM_PLAT_USB_FLAGS_STRAP_8BIT			(1<<0)
#define STM_PLAT_USB_FLAGS_STRAP_16BIT			(2<<0)
#define STM_PLAT_USB_FLAGS_STRAP_PLL			(1<<2)

struct stm_plat_usb_data {
	unsigned long flags;
	struct stm_amba_bridge_config *amba_config;
	struct stm_device_config *device_config;
};



/*** TAP platform data ***/

struct tap_sysconf_field {
	u8 group, num;
	u8 lsb, msb;
	enum {POL_NORMAL, POL_INVERTED} pol;
};

struct stm_tap_sysconf {
	struct tap_sysconf_field tms;
	struct tap_sysconf_field tck;
	struct tap_sysconf_field tdi;
	struct tap_sysconf_field tdo;
	struct tap_sysconf_field tap_en;
	struct tap_sysconf_field trstn;
	int tap_en_pol;
	int trstn_pol;
};

struct stm_plat_tap_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
	struct stm_tap_sysconf *tap_sysconf;
	char *style_id;
};


/*** PCIE-MP platform data ***/

struct stm_plat_pcie_mp_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
	void (*mp_select)(int port);
	char *style_id;
};



/*** MiPHY dummy platform data ***/

struct stm_plat_miphy_dummy_data {
	int miphy_first, miphy_count;
	enum miphy_mode *miphy_modes;
};



/*** SATA platform data ***/

struct stm_plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
	unsigned int oob_wa;
	struct stm_amba_bridge_config *amba_config;
	struct stm_device_config *device_config;
	void (*host_restart)(int port);
	int port_num;
	int miphy_num;
};

/*** PIO platform data ***/

struct stm_plat_pio_data {
	void __iomem *regs;
	int (*pin_name)(char *name, int size, int port, int pin);
};

struct stm_plat_pio_irqmux_data {
	int port_first;
	int ports_num;
};



/*** Sysconf block platform data ***/

#define PLAT_SYSCONF_GROUP(_id, _offset) \
	{ \
		.group = _id, \
		.offset = _offset, \
		.name = #_id \
	}

struct stm_plat_sysconf_group {
	int group;
	unsigned long offset;
	const char *name;
	int (*reg_name)(char *name, int size, int group, int num);
};

struct stm_plat_sysconf_data {
	int groups_num;
	struct stm_plat_sysconf_group *groups;
	void __iomem *regs;
};



/*** NAND flash platform data ***/

struct stm_plat_nand_flex_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	unsigned int flex_rbn_connected:1;
};

enum stm_nand_bch_ecc_config {
	BCH_ECC_CFG_AUTO = 0,
	BCH_ECC_CFG_NOECC,
	BCH_ECC_CFG_18BIT,
	BCH_ECC_CFG_30BIT
};

struct stm_plat_nand_bch_data {
	struct stm_nand_bank_data *bank;
	enum stm_nand_bch_ecc_config bch_ecc_cfg;
};

struct stm_plat_nand_emi_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	int emi_rbn_gpio;
};

struct stm_nand_config {
	enum {
		stm_nand_emi,
		stm_nand_flex,
		stm_nand_afm,
		stm_nand_bch,
	} driver;
	int nr_banks;
	struct stm_nand_bank_data *banks;
	union {
		int emi_gpio;
		int flex_connected;
	} rbn;
	enum stm_nand_bch_ecc_config bch_ecc_cfg;
};


/*** STM SPI FSM Serial Flash data ***/

struct stm_spifsm_caps {
	/* Board/SoC/IP capabilities */
	int dual_mode:1;		/* DUAL mode */
	int quad_mode:1;		/* QUAD mode */

	/* IP capabilities */
	int addr_32bit:1;		/* 32bit addressing supported */
	int no_poll_mode_change:1;	/* Polling MODE_CHANGE broken */
	int no_clk_div_4:1;		/* Bug prevents ClK_DIV=4 */
	int no_sw_reset:1;		/* S/W reset not possible */
	int dummy_on_write:1;		/* Bug requires "dummy" sequence on
					 * WRITE */
	int no_read_repeat:1;		/* READ repeat sequence broken */
	int no_write_repeat:1;		/* WRITE repeat sequence broken */
	enum {
		spifsm_no_read_status = 1,	/* READ_STA broken */
		spifsm_read_status_clkdiv4,	/* READ_STA only at CLK_DIV=4 */
	} read_status_bug;
};

struct stm_plat_spifsm_data {
	char			*name;
	struct mtd_partition	*parts;
	unsigned int		nr_parts;
	unsigned int		max_freq;
	struct stm_pad_config	*pads;
	struct stm_spifsm_caps	capabilities;
};


/*** FDMA platform data ***/

struct stm_plat_fdma_slim_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
};

struct stm_plat_fdma_periph_regs {
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};

struct stm_plat_fdma_ram {
	unsigned long offset;
	unsigned long size;
};

struct stm_plat_fdma_hw {
	struct stm_plat_fdma_slim_regs slim_regs;
	struct stm_plat_fdma_ram dmem;
	struct stm_plat_fdma_periph_regs periph_regs;
	struct stm_plat_fdma_ram imem;
};

struct stm_plat_fdma_fw_regs {
	unsigned long rev_id;
	unsigned long cmd_statn;
	unsigned long req_ctln;
	unsigned long ptrn;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
	unsigned long node_size;
};

struct stm_plat_fdma_data {
	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw_regs *fw;
	u8 xbar;
};

struct stm_plat_fdma_xbar_data {
	u8 first_fdma_id;
	u8 last_fdma_id;
};



/*** PCI platform data ***/

struct stm_pci_window_info {
	phys_addr_t start;	/* Start of PCI memory window hole */
	unsigned long size;
	phys_addr_t io_start;	/* Where IO addresses are */
	unsigned long io_size;	/* Zero for no IO region */
	phys_addr_t lmi_start;	/* Main memory physical start */
	unsigned long lmi_size;
};


#define PCI_PIN_ALTERNATIVE -3 /* Use alternative PIO rather than default */
#define PCI_PIN_DEFAULT     -2 /* Use whatever the default is for that pin */
#define PCI_PIN_UNUSED	    -1 /* Pin not in use */

/* In the board setup, you can pass in the external interrupt numbers instead
 * if you have wired up your board that way. It has the advantage that the PIO
 * pins freed up can then be used for something else. */
struct stm_plat_pci_config {
	struct stm_pci_window_info pci_window;
	/* PCI_PIN_DEFAULT/PCI_PIN_UNUSED. Other IRQ can be passed in */
	int pci_irq[4];
	/* As above for SERR */
	int serr_irq;
	/* Lowest address line connected to an idsel  - slot 0 */
	char idsel_lo;
	/* Highest address line connected to an idsel - slot n */
	char idsel_hi;
	/* Set to PCI_PIN_DEFAULT if the corresponding req/gnt lines are
	 * in use */
	char req_gnt[4];
	/* PCI clock in Hz. If zero default to 33MHz */
	unsigned long pci_clk;

	/* If you supply a pci_reset() function, that will be used to reset
	 * the PCI bus.  Otherwise it is assumed that the reset is done via
	 * PIO, the number is specified here. Specify -EINVAL if no PIO reset
	 * is required either, for example if the PCI reset is done as part
	 * of power on reset. */
	unsigned pci_reset_gpio;
	void (*pci_reset)(void);

	/* Maps the irqs for this bus */
	int (*pci_map_irq)(const struct pci_dev *dev, u8 slot, u8 pin);

	/* You may define a PCI clock name. If NULL it will fall
	 * back to "pci" */
	const char *clk_name;

	/* Various PCI tuning parameters. Set by SOC layer. You don't have
	 * to specify these as the defaults are usually fine. However, if
	 * you need to change them, you can set ad_override_default and
	 * plug in your own values. */
	unsigned ad_threshold:4;
	unsigned ad_chunks_in_msg:5;
	unsigned ad_pcks_in_chunk:5;
	unsigned ad_trigger_mode:1;
	unsigned ad_posted:1;
	unsigned ad_max_opcode:4;
	unsigned ad_read_ahead:1;
	/* Set to override default values for your board */
	unsigned ad_override_default:1;

	/* Some SOCs have req0 pin connected to req3 signal to work around
	 * some problems with NAND.  These bits will be set by the chip layer,
	 * the board layer should NOT touch this.
	 */
	unsigned req0_to_req3:1;
};



/* How these are done vary considerable from SOC to SOC. Sometimes
 * they are wired up to sysconfig bits, other times they are simply
 * memory mapped registers.
 */

struct stm_plat_pcie_ops {
	void (*init)(void *handle);
	void (*enable_ltssm)(void *handle);
	void (*disable_ltssm)(void *handle);
};

/* PCIe platform data */
struct stm_plat_pcie_config {
	struct stm_pci_window_info pcie_window;
	/* Which PIO the PERST# signal is on.
	 * If it is not connected, and you rely on the autonomous reset,
	 * then specifiy -EINVAL here
	 */
	unsigned reset_gpio;
	/* If you have a really wierd way of wanging PERST# (unlikely),
	 * then do it here. Given PCI express is defined in such a way
	 * that autonomous reset should work it is OK to not connect it at
	 * all.
	 */
	void (*reset)(void);
	/* Magic number to shove into the amba bus bridge. The AHB driver will
	 * be commoned up at some point in the future so this will change
	 */
	unsigned long ahb_val;
	/* Which miphy this pcie is using */
	int miphy_num;
	/* Magic handle to pass through to the ops */
	void *ops_handle;
	struct stm_plat_pcie_ops *ops;
};

/*** ILC platform data ***/

struct stm_plat_ilc3_data {
	unsigned short inputs_num;
	unsigned short outputs_num;
	unsigned short first_irq;

	/*
	 * The ILC supports the wakeup capability but on some chip when enabled
	 * the system is unstable during the resume from suspend, so disable
	 * it.
	 */
	int disable_wakeup:1;
};

/*** IRQ-MUX data ***/

struct stm_plat_irq_mux_data {
	char *name;
	unsigned short num_input;
	unsigned char num_output;
	int (*custom_mapping)(struct stm_plat_irq_mux_data const *pdata,
			      long input, long *enable,
			      long *output, long *inv);
};

/*
 * build_pad_claim and build_pad_release (generated when compiling for
 * each driver) are two simple macros can be invoked by STM drivers to claim
 * and release the PAD resources from the platform.
 *
 * For example, the stmmac and sdhci use them inside their probe and release
 * functions.
 *
 * To use these simple API the driver needs to have two simple hooks
 * (e.g. init/exit) and custom_data/cfg fields used to manage the PAD setup
 * also when PM stuff is invoked.
 */
#define build_pad_claim(name, driver_plat_data)				\
static inline int name(struct platform_device *pdev)			\
{									\
	int ret = 0;							\
	struct driver_plat_data *plat_dat = pdev->dev.platform_data;	\
									\
	plat_dat->custom_data = devm_stm_pad_claim(&pdev->dev,		\
		(struct stm_pad_config *) plat_dat->custom_cfg,		\
		dev_name(&pdev->dev));					\
	if (!plat_dat->custom_data)					\
		ret = -ENODEV;						\
	return ret; \
}

#define build_pad_release(name, driver_plat_data)			\
static inline void name(struct platform_device *pdev)			\
{									\
	struct driver_plat_data *plat_dat = pdev->dev.platform_data;	\
	if (!plat_dat->custom_data)					\
		return;							\
	devm_stm_pad_release(&pdev->dev, plat_dat->custom_data);	\
	plat_dat->custom_data = NULL; \
}

build_pad_claim(stmmac_claim_resource, plat_stmmacenet_data)
build_pad_release(stmmac_release_resource, plat_stmmacenet_data)
build_pad_claim(mmc_claim_resource, stm_mmc_platform_data)
build_pad_release(mmc_release_resource, stm_mmc_platform_data)

/* Mali specific */
struct stm_mali_resource {
	resource_size_t start;
	resource_size_t end;
	const char *name;
};
struct stm_mali_config {
	/* Memory allocated by Linux kernel and
	 Memory regions managed by mali driver */
	int num_mem_resources;
	struct stm_mali_resource *mem;
	/* Access to other regions of memory to directly render */
	int num_ext_resources;
	struct stm_mali_resource *ext_mem;
};

static inline int clk_add_alias_platform_device(const char *alias,
	struct platform_device *pdev, char *id, struct device *dev)
{
	char dev_name_buf[20];
	const char *dev_name;

	if (pdev->id == -1) {
		dev_name = pdev->name;
	} else {
		snprintf(dev_name_buf, sizeof(dev_name_buf), "%s.%d",
			pdev->name, pdev->id);
		dev_name = dev_name_buf;
	}
	return clk_add_alias(alias, dev_name, id, dev);
}


extern void (*stm_board_reset)(char mode);


/* stm-keyscan platform data */

#define STM_KEYSCAN_MAXKEYS 16
struct stm_keyscan_config {
	unsigned int num_out_pads, num_in_pads;
	unsigned int debounce_us;
	int keycodes[STM_KEYSCAN_MAXKEYS];
};

struct stm_plat_keyscan_data {
	struct stm_pad_config *pad_config;
	struct stm_keyscan_config keyscan_config;
};

#endif /* __LINUX_STM_PLATFORM_H */
