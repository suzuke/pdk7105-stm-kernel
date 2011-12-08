/*
 * arch/sh/boards/mach-b2000/setup.c
 *
 * Copyright (C) 2010 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *         Srinivas Kandagatla <srinivas.kandagatla@st.com>
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STiH415 processor module board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/leds.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>
#include <asm/irq.h>



static void __init b2000_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics STiH415-MBOARD (b2000) "
			"initialisation\n");

	stih415_early_device_init();

	stih415_configure_asc(2, &(struct stih415_asc_config) {
			.hw_flow_control = 1,
			.is_console = 1, });
}

#define GMII0_PHY_NOT_RESET stm_gpio(25 /*106 -100 + 18 */, 2)
#define GMII1_PHY_NOT_RESET stm_gpio(4, 7)
#define GMII1_PHY_CLKOUT_NOT_TXCLK_SEL stm_gpio(2, 5)
#define HDMI_HOTPLUG	GMII1_PHY_CLKOUT_NOT_TXCLK_SEL
#define GMII0_PHY_CLKOUT_NOT_TXCLK_SEL stm_gpio(13, 4)

#if defined(CONFIG_STM_GMAC1_NONE)
static struct stm_pad_config stih415_hdmi_hp_pad_config = {
        .gpios_num = 1,
        .gpios = (struct stm_pad_gpio []) {
                STM_PAD_PIO_IN(2, 5, 1),      /* HDMI Hotplug */
        },
};
#endif


static int b2000_gmii1_reset(void *bus)
{
	gpio_set_value(GMII1_PHY_NOT_RESET, 1);
	gpio_set_value(GMII1_PHY_NOT_RESET, 0);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(GMII1_PHY_NOT_RESET, 1);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */

	return 1;
}

#ifdef CONFIG_STM_GMAC0_B2032_CARD_GMII_MODE
static void b2000_gmac0_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO13[4]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif

#ifdef CONFIG_STM_GMAC1_B2032_CARD_GMII_MODE
/*
 * On B2000B board PIO 2-5 conflicts with the HDMI hot-plug detection pin.
 * As we have a seperate 125clk pin in to the MAC, we might not need
 * tx clk selection.
 * Removing R59 from B2032A will solve the problem and TXCLK is
 * always connected to PHY TXCLK.
 * NOTE: This will be an issue if the B2032 is used with a 7108 as
 * 125clk comes from phy txclk, and so this signal is needed.
 */
static void b2000_gmac1_txclk_select(int txclk_250_not_25_mhz)
{
	/* When 1000 speed is negotiated we have to set the PIO2[5]. */
	if (txclk_250_not_25_mhz)
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 1);
	else
		gpio_set_value(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
}
#endif

#if defined(CONFIG_STM_GMAC0_B2035_CARD) || defined(CONFIG_STM_GMAC0_B2032_CARD)
static int b2000_gmii0_reset(void *bus)
{
	gpio_set_value(GMII0_PHY_NOT_RESET, 1);
	gpio_set_value(GMII0_PHY_NOT_RESET, 0);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */
	gpio_set_value(GMII0_PHY_NOT_RESET, 1);
	mdelay(10); /* 10 miliseconds is enough for everyone ;-) */

	return 1;
}

static struct stmmac_mdio_bus_data stmmac0_mdio_bus = {
	/* GMII connector CN22 */
	.bus_id = 0,
	.phy_reset = &b2000_gmii0_reset,
	.phy_mask = 0,
};
#endif

static struct stmmac_mdio_bus_data stmmac1_mdio_bus = {
	/* GMII connector CN23 */
	.bus_id = 1,
	.phy_reset = &b2000_gmii1_reset,
	.phy_mask = 0,
};

static struct platform_device b2000_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "FP_LED",
				.default_trigger = "heartbeat",
				.gpio = stm_gpio(STIH415_PIO(105), 7),
			}
		},
	},
};

static struct platform_device *b2000_devices[] __initdata = {
	&b2000_leds,
};

static struct stm_mali_resource b2000_mali_mem[1] = {
	{
		.name   = "OS_MEMORY",
		.start  =  0,
		.end    =  CONFIG_STM_B2000_MALI_OS_MEMORY_SIZE-1,
	},
};

static struct stm_mali_resource b2000_mali_ext_mem[] = {
	{
		.name   = "EXTERNAL_MEMORY_RANGE",
		.start  =  0x40000000,
		.end    =  0xBFFFFFFF,
	}
};

static struct stm_mali_config b2000_mali_config = {
	.num_mem_resources = ARRAY_SIZE(b2000_mali_mem),
	.mem = b2000_mali_mem,
	.num_ext_resources = ARRAY_SIZE(b2000_mali_ext_mem),
	.ext_mem = b2000_mali_ext_mem,
};

static int __init b2000_devices_init(void)
{
	/* Reset */
	gpio_request(GMII0_PHY_NOT_RESET, "GMII0_PHY_NOT_RESET");
	gpio_direction_output(GMII0_PHY_NOT_RESET, 0);

	gpio_request(GMII1_PHY_NOT_RESET, "GMII1_PHY_NOT_RESET");
	gpio_direction_output(GMII1_PHY_NOT_RESET, 0);

#if !defined(CONFIG_STM_GMAC1_NONE)
	/* Default to 100 Mbps */
	gpio_request(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII1_TXCLK_SEL");
	gpio_direction_output(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
	gpio_free(GMII1_PHY_CLKOUT_NOT_TXCLK_SEL);
#else
	/* Default to HDMI HotPlug */
	if (stm_pad_claim(&stih415_hdmi_hp_pad_config, "HDMI_Hotplug") == NULL)
		printk(KERN_ERR "Failed to claim HDMI-Hotplug pad!\n");

#endif

#if !defined(CONFIG_STM_GMAC0_NONE)
	/* Default to 100 Mbps */
	gpio_request(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, "GMII0_TXCLK_SEL");
	gpio_direction_output(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL, 0);
	gpio_free(GMII0_PHY_CLKOUT_NOT_TXCLK_SEL);
#endif

/*
 * B2032A (MII or GMII) Ethernet card
 * GMII Mode on B2032A needs R26 to be fitted with 51R
 * On B2000B board, to get GMAC0 working make sure that jumper
 * on PIN 9-10 on CN35 and CN36 are removed.
 */

/*
 * B2035A (RMII + MMC(on CN22)) Ethernet + MMC card
 * B2035A board has IP101ALF PHY connected in RMII mode
 * and an MMC card
 * It is designed to be conneted to GMAC0 (CN22) to get MMC working,
 * however we can connect it to GMAC1 for RMII testing.
 */

/* GMAC0 */
#if defined(CONFIG_STM_GMAC0_B2035_CARD) || defined(CONFIG_STM_GMAC0_B2032_CARD)
	stih415_configure_ethernet(0, &(struct stih415_ethernet_config) {
#ifdef CONFIG_STM_GMAC0_B2035_CARD
			.mode = stih415_ethernet_mode_rmii,
			.ext_clk = 0,
#endif /* CONFIG_STM_GMAC0_B2035_CARD */

#ifdef CONFIG_STM_GMAC0_B2032_CARD
/* B2032 modified to support GMII */
#ifdef CONFIG_STM_GMAC0_B2032_CARD_GMII_MODE
			.mode = stih415_ethernet_mode_gmii_gtx,
			.txclk_select = b2000_gmac0_txclk_select,
#else
			.mode = stih415_ethernet_mode_mii,
#endif /* CONFIG_STM_GMAC0_B2032_CARD_GMII_MODE */
			.ext_clk = 1,
#endif /* CONFIG_STM_GMAC0_B2032_CARD */
			.phy_bus = 0,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac0_mdio_bus, });
#endif

/* GMAC1 */
#if !defined(CONFIG_STM_CN23_NONE)
	stih415_configure_ethernet(1, &(struct stih415_ethernet_config) {
#ifdef CONFIG_STM_GMAC1_B2035_CARD
			.mode = stih415_ethernet_mode_rmii,
			.ext_clk = 0,
#endif /* CONFIG_STM_GMAC1_B2035_CARD */

#ifdef CONFIG_STM_GMAC1_B2032_CARD
			.mode = stih415_ethernet_mode_mii,
			.ext_clk = 1,
#endif /* CONFIG_STM_GMAC1_B2032_CARD */

#ifdef CONFIG_STM_GMAC1_B2032_CARD_GMII_MODE
			.mode = stih415_ethernet_mode_gmii_gtx,
			.txclk_select = b2000_gmac1_txclk_select,
			.ext_clk = 1,
#endif /* CONFIG_STM_GMAC1_B2032_CARD_GMII_MODE */
			.phy_bus = 1,
			.phy_addr = -1,
			.mdio_bus_data = &stmmac1_mdio_bus, });
#endif /* CONFIG_STM_CN23_NONE */

	stih415_configure_usb(0);
	stih415_configure_usb(1);
	stih415_configure_usb(2);

	/* HDMI */
	stih415_configure_ssc_i2c(STIH415_SSC(1));

	/* Frontend I2C, make sure J17-J20 are configured accordingly */
	stih415_configure_ssc_i2c(STIH415_SSC(3));

	/* Backend I2C, make sure J50, J51 are configured accordingly */
	stih415_configure_ssc_i2c(STIH415_SBC_SSC(0));

	/* IR_IN */
	stih415_configure_lirc(&(struct stih415_lirc_config) {
			.rx_mode = stih415_lirc_rx_mode_ir, });

	stih415_configure_mali(&b2000_mali_config);

	stih415_configure_pwm(&(struct stih415_pwm_config) {
			.pwm = stih415_sbc_pwm,
			.out0_enabled = 1, });

#if defined(CONFIG_STM_GMAC0_B2035_CARD) || defined(CONFIG_STM_MMC_B2048A_CARD)
#ifdef CONFIG_STM_B2048A_MMC_EMMC
	/* eMMC on board */
	stih415_configure_mmc(1);
#else
	/*
	 * In case of B2035 or B2048A without eMMC, we actually have the
	 * MMC/SD slot on the daughter board.
	 */
	stih415_configure_mmc(0);
#endif
#endif

	return platform_add_devices(b2000_devices,
			ARRAY_SIZE(b2000_devices));
}
arch_initcall(b2000_devices_init);

struct sh_machine_vector mv_b2000 __initmv = {
	.mv_name		= "b2000",
	.mv_setup		= b2000_setup,
	.mv_nr_irqs		= NR_IRQS,
};
