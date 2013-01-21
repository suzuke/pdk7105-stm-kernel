/*
 * (c) 2012 STMicroelectronics Limited
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/stm/device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stig125.h>
#include <linux/stm/amba_bridge.h>
#include <linux/stm/miphy.h>
#include <linux/stm/stig125-periphs.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/ahci_platform.h>
#include <linux/stm/isve.h>
#include <linux/stmfp.h>
#include <linux/phy.h>

#ifdef CONFIG_ARM
#include <asm/mach/map.h>
#include <mach/soc-stig125.h>
#include <mach/hardware.h>
#endif

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#include "../pio-control.h"

static u64 stig125_usb_dma_mask = DMA_BIT_MASK(32);

#define USB_HOST_PWR			"USB_HOST_PWR"
#define USB_PWR_ACK			"USB_PWR_ACK"
#define USB_PHY_SLEEPM			"USB_PHY_SLEEPM"
#define USB_PHY_2_PLL			"USB_PHY-2_PLL"

struct usb_clk_rate {
	char *clk_n;
	unsigned long rate;
};

static int stig125_usb_init(struct stm_device_state *device_state)
{
	static int usb_clk_rate_init_done;
	int ret, i;

	static struct usb_clk_rate usb_clks[] = {
		{
			.clk_n = "CLK_S_D_VCO",
			.rate = 600000000,
		}, {
			.clk_n = "CLK_S_D_USB_REF",
			.rate = 12000000,
		}
	};

	if (usb_clk_rate_init_done)
		return 0;

	for (i = 0; i < ARRAY_SIZE(usb_clks); ++i) {
		struct clk *clk;
		clk = clk_get(NULL, usb_clks[i].clk_n);
		if (IS_ERR(clk)) {
			ret = -EINVAL;
			goto on_error;
		}
		ret = clk_set_rate(clk, usb_clks[i].rate);
		if (ret)
			goto on_error;
		clk_put(clk);
	}

	usb_clk_rate_init_done = 1;

	return ret;

on_error:
	pr_err("Error: on %s\n", __func__);
	return ret;
}
/*
 * USB-0 and USB-1 share the same Phy
 */
static struct sysconf_field *usb_phy_0_1_pll_power;

static void stig125_usb_power_0_1(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	static int phy_pll_0_1_usage_counter;
	int value = (power == stm_device_power_on) ? 0 : 1;
	int sleepm_value = (power == stm_device_power_on) ? 1 : 0;

	phy_pll_0_1_usage_counter += (power == stm_device_power_on ? 1 : -1);

	switch (phy_pll_0_1_usage_counter) {
	case 0:
		sysconf_write(usb_phy_0_1_pll_power, 1);
		break;
	case 1:
		sysconf_write(usb_phy_0_1_pll_power, 0);
		break;
	default:
		break;
	}

	stm_device_sysconf_write(device_state, USB_PHY_SLEEPM, sleepm_value);
	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_PWR_ACK)
			== value)
			break;
		mdelay(10);
	}
}

/*
 * USB-2 has its-own Phy
 */
static void stig125_usb_power_2(struct stm_device_state *device_state,
		enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;
	int sleepm_value = (power == stm_device_power_on) ? 1 : 0;

	stm_device_sysconf_write(device_state, USB_HOST_PWR, value);
	stm_device_sysconf_write(device_state, USB_PHY_2_PLL, value);
	stm_device_sysconf_write(device_state, USB_PHY_SLEEPM, sleepm_value);

	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, USB_PWR_ACK)
			== value)
			break;
		mdelay(10);
	}
}

static struct stm_amba_bridge_config stig125_amba_usb_config = {
	STM_DEFAULT_USB_AMBA_PLUG_CONFIG(256),
	.type2.sd_config_missing = 1,
};

/*
 * Sysconf registers: 923 and 924 should related only
 * on Buil-in Self Test,,,
 */
static struct stm_plat_usb_data stig125_usb_platform_data[3] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stig125_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 7,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* USB_PHY_COMPDIS */
					STM_PAD_SYSCONF(SYSCONF(921), 0, 2,
								4),
					/* USB_PHY_SQRX */
					STM_PAD_SYSCONF(SYSCONF(921), 9, 11,
								3),
					/* USB_PHY_TXFSLS */
					STM_PAD_SYSCONF(SYSCONF(921), 18, 21,
								3),
					/* USB_PHY_TXPREEMPHASIS */
					STM_PAD_SYSCONF(SYSCONF(922), 0, 0,
								0),
					/* USB_PHY_TXRISE */
					STM_PAD_SYSCONF(SYSCONF(922), 3, 3,
								0),
					/* USB_PHY_TXVREF */
					STM_PAD_SYSCONF(SYSCONF(922), 6, 9,
						8),
					/* USB_PHY_TXHSXV */
					STM_PAD_SYSCONF(SYSCONF(922), 18, 19,
						3),
				},
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(20, 0, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(20, 1, 1),
				},
			},
			.init = stig125_usb_init,
			.power = stig125_usb_power_0_1,
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []) {
				/* host on 925 - 937 */
				STM_DEVICE_SYSCONF(SYSCONF(925), 0, 0,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(937), 0, 0,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(922), 26, 26,
					USB_PHY_SLEEPM),
			},
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stig125_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 7,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* USB_PHY_COMPDIS */
					STM_PAD_SYSCONF(SYSCONF(921), 3, 5,
								4),
					/* USB_PHY_SQRX */
					STM_PAD_SYSCONF(SYSCONF(921), 12, 14,
								3),
					/* USB_PHY_TXFSLS */
					STM_PAD_SYSCONF(SYSCONF(921), 22, 25,
								3),
					/* USB_PHY_TXPREEMPHASIS */
					STM_PAD_SYSCONF(SYSCONF(922), 1, 1,
								0),
					/* USB_PHY_TXRISE */
					STM_PAD_SYSCONF(SYSCONF(922), 4, 4,
								0),
					/* USB_PHY_TXVREF */
					STM_PAD_SYSCONF(SYSCONF(922), 10, 13,
						8),
					/* USB_PHY_TXHSXV */
					STM_PAD_SYSCONF(SYSCONF(922), 20, 21,
						3),
				},
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(19, 4, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(19, 5, 1),
				},
			},
			.power = stig125_usb_power_0_1,
			.init = stig125_usb_init,
			.sysconfs_num = 3,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(925), 1, 1,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(937), 1, 1,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(922), 27, 27,
					USB_PHY_SLEEPM),
			},
		},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT,
		.amba_config = &stig125_amba_usb_config,
		.device_config = &(struct stm_device_config){
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 11,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* USB_PHY_COMPDIS */
					STM_PAD_SYSCONF(SYSCONF(921), 6, 8,
								4),
					/* USB_PHY_SQRX */
					STM_PAD_SYSCONF(SYSCONF(921), 15, 17,
								3),
					/* USB_PHY_TXFSLS */
					STM_PAD_SYSCONF(SYSCONF(921), 26, 29,
								3),
					/* USB_PHY_TXPREEMPHASIS */
					STM_PAD_SYSCONF(SYSCONF(922), 2, 2,
								0),
					/* USB_PHY_TXRISE */
					STM_PAD_SYSCONF(SYSCONF(922), 5, 5,
								0),
					/* USB_PHY_TXVREF */
					STM_PAD_SYSCONF(SYSCONF(922), 14, 17,
						8),
					/* USB_PHY_TXHSXV */
					STM_PAD_SYSCONF(SYSCONF(922), 22, 23,
						3),
					/* the following sysconf aren't related
					 * to the USB-Host-2.
					 * They are required to turn-off the
					 * un-used port on the Phy-2
					 */
					STM_PAD_SYSCONF(SYSCONF(921), 30, 30,
						0),
					STM_PAD_SYSCONF(SYSCONF(922), 29, 29,
						0),
					STM_PAD_SYSCONF(SYSCONF(922), 30, 30,
						1),
					STM_PAD_SYSCONF(SYSCONF(922), 31, 31,
						1),
				},
				.gpios_num = 2,
				.gpios = (struct stm_pad_gpio []) {
					/* Overcurrent detection */
					STM_PAD_PIO_IN(19, 6, 1),
					/* USB power enable */
					STM_PAD_PIO_OUT(19, 7, 1),
				},
			},
			.power = stig125_usb_power_2,
			.init = stig125_usb_init,
			.sysconfs_num = 4,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(922), 25, 25,
					USB_PHY_2_PLL),
				/* host on 925 - 937 */
				STM_DEVICE_SYSCONF(SYSCONF(925), 2, 2,
					USB_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(937), 2, 2,
					USB_PWR_ACK),
				STM_DEVICE_SYSCONF(SYSCONF(922), 28, 28,
					USB_PHY_SLEEPM),
			},
		},
	},
};

/*
 * Stig125 has an USB_3002_040LP USB-IP revision
 */
#define USB_DEVICE(_id, _base, _ehci_irq, _ohci_irq)			\
	[_id] = {							\
		.name = "stm-usb",					\
		.id = _id,						\
		.dev = {						\
			.dma_mask = &stig125_usb_dma_mask,		\
			.coherent_dma_mask = DMA_BIT_MASK(32),		\
			.platform_data = &stig125_usb_platform_data[_id],\
		},							\
		.num_resources = 6,					\
		.resource = (struct resource[]) {			\
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper",		\
					_base, 0x100),			\
			STM_PLAT_RESOURCE_MEM_NAMED("ohci",		\
					_base + 0x3c00, 0x100),		\
			STM_PLAT_RESOURCE_MEM_NAMED("ehci",		\
					_base + 0x3e00, 0x100),		\
			STM_PLAT_RESOURCE_MEM_NAMED("protocol",		\
					_base + 0x3f00, 0x100),		\
			STIG125_RESOURCE_IRQ_NAMED("ehci", _ehci_irq),  \
			STIG125_RESOURCE_IRQ_NAMED("ohci", _ohci_irq),  \
		},							\
	}

static struct platform_device stig125_usb_devices[] = {
	USB_DEVICE(0, 0xfe800000, 78, 79),
	USB_DEVICE(1, 0xfe804000, 80, 81),
	USB_DEVICE(2, 0xfe808000, 82, 83),
};

void __init stig125_configure_usb(int port)
{
	static int configured[ARRAY_SIZE(stig125_usb_devices)];
	BUG_ON(port < 0 || port >= ARRAY_SIZE(stig125_usb_devices));

	BUG_ON(configured[port]++);
	if (port != 2 && !usb_phy_0_1_pll_power)
		usb_phy_0_1_pll_power =
			sysconf_claim(SYSCONF(922), 24, 24, "USB_PHY-0_PLL");
	platform_device_register(&stig125_usb_devices[port]);
}

/* MMC/SD resources ------------------------------------------------------ */
/* Custom PAD configuration for the MMC Host controller */
#define STIG125_PIO_MMC_CLK_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir_pull_up, \
		.function = 1, \
		.name = "MMCCLK", \
		.priv = &(struct stm_pio_control_pad_config) {	\
			.retime = RET_NICLK(0, 0),	\
		}, \
	}

#define STIG125_PIO_MMC_OUT(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = 1, \
	}
#define STIG125_PIO_MMC_BIDIR(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir_pull_up, \
		.function = 1, \
	}
#define STIG125_PIO_MMC_IN(_port, _pin) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = 1, \
	}

static struct stm_pad_config stig125_mmc_pad_config = {
	.gpios_num = 15,
	.gpios = (struct stm_pad_gpio []) {
		STIG125_PIO_MMC_BIDIR(21, 0),	/* MMC Data[0]*/
		STIG125_PIO_MMC_BIDIR(21, 1),	/* MMC Data[1]*/
		STIG125_PIO_MMC_BIDIR(21, 2),	/* MMC Data[2]*/
		STIG125_PIO_MMC_BIDIR(21, 3),	/* MMC Data[3]*/
		STIG125_PIO_MMC_BIDIR(21, 4),	/* MMC Data[4]*/
		STIG125_PIO_MMC_BIDIR(21, 5),	/* MMC Data[5]*/
		STIG125_PIO_MMC_BIDIR(21, 6),	/* MMC Data[6]*/
		STIG125_PIO_MMC_BIDIR(21, 7),	/* MMC Data[7]*/

		STIG125_PIO_MMC_CLK_OUT(22, 0),	/* MMC Clk */
		STIG125_PIO_MMC_BIDIR(22, 1),	/* MMC command */
		STIG125_PIO_MMC_IN(22, 2),	/* MMC Write Protection */
		STIG125_PIO_MMC_IN(22, 3),	/* MMC Card Detect */
		STIG125_PIO_MMC_OUT(22, 4),	/* MMC LED on */
		STIG125_PIO_MMC_OUT(22, 5),	/* MMC Card PWR */
		STIG125_PIO_MMC_OUT(22, 6),	/* MMC Boot Data error */
	},
};

static struct stm_mmc_platform_data stig125_mmc_platform_data = {
	.init = mmc_claim_resource,
	.exit = mmc_release_resource,
	.custom_cfg = &stig125_mmc_pad_config,
	.nonremovable = false,
};

static struct platform_device stig125_mmc_device = {
	.name = "sdhci-stm",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM(0xFE96C000, 0x1000),
		STIG125_RESOURCE_IRQ_NAMED("mmcirq", 156),
	},
	.dev = {
		.platform_data = &stig125_mmc_platform_data,
	}
};

void __init stig125_configure_mmc(int emmc)
{
	struct stm_mmc_platform_data *plat_data;

	plat_data = &stig125_mmc_platform_data;

	if (emmc)
		plat_data->nonremovable = true;

	platform_device_register(&stig125_mmc_device);
}


/*
 * AHCI support
 */
static void stig125_sata_mp_select(int port)
{
}

static enum miphy_mode stig125_miphy_modes[2];

struct stm_plat_pcie_mp_data stig125_sata_mp_platform_data[2] = {
	{
		.style_id = ID_MIPHYA40X,
		.miphy_first = 0,
		.miphy_count = 1,
		.miphy_modes = stig125_miphy_modes,
		.mp_select = stig125_sata_mp_select,
	}, {
		.style_id = ID_MIPHYA40X,
		.miphy_first = 1,
		.miphy_count = 1,
		.miphy_modes = stig125_miphy_modes,
		.mp_select = stig125_sata_mp_select,
	},
};

#define SATA_MIPHY(_id, _iomem)						\
	{								\
		.name = "pcie-mp",					\
		.id	= _id,						\
		.num_resources = 1,					\
		.resource = (struct resource[]) {			\
		  STM_PLAT_RESOURCE_MEM_NAMED("pcie-uport", _iomem, 0xff),\
			},						\
		.dev.platform_data =					\
			&stig125_sata_mp_platform_data[_id],		\
	}

static struct platform_device stig125_sata_mp_devices[2] = {
	SATA_MIPHY(0, 0xfefb2000),
	SATA_MIPHY(1, 0xfefb6000),
};

void stig125_configure_miphy(struct stig125_miphy_config *config)
{
	switch (config->id) {
	case 0:
		stig125_miphy_modes[config->id] = SATA_MODE;
		if (config->iface == UPORT_IF)
			platform_device_register(&stig125_sata_mp_devices[0]);
		break;
	case 1:
		stig125_miphy_modes[config->id] = config->mode;
		switch (config->mode) {
		case SATA_MODE:
			if (config->iface == UPORT_IF) {
				stig125_sata_mp_platform_data[1].rx_pol_inv =
						config->rx_pol_inv;
				stig125_sata_mp_platform_data[1].tx_pol_inv =
						config->tx_pol_inv;
			}
			break;
		case PCIE_MODE:
			/* TODO */
			break;
		}
		/* Only tested on UPort I/f */
		if (config->iface == UPORT_IF)
			platform_device_register(&stig125_sata_mp_devices[1]);
		break;
	default:
		BUG_ON(1);
	}
}

#define AHCI_HOST_PWR		"SATA_HOST_PWR"
#define AHCI_HOST_ACK		"SATA_HOST_ACK"
static void stig125_ahci_power(struct stm_device_state *device_state,
	enum stm_device_power_state power)
{
	int i;
	int value = (power == stm_device_power_on) ? 0 : 1;

	stm_device_sysconf_write(device_state, AHCI_HOST_PWR, value);
	for (i = 5; i; --i) {
		if (stm_device_sysconf_read(device_state, AHCI_HOST_ACK)
			== value)
			break;
		mdelay(10);
	}
}

static struct stm_plat_ahci_data stm_ahci_plat_data[2] = {
	{
		.device_config = &(struct stm_device_config) {
			.power = stig125_ahci_power,
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(266), 0, 0,
					AHCI_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(281), 0, 0,
					AHCI_HOST_ACK),
				},
		},
	}, {
		.device_config = &(struct stm_device_config){
			.power = stig125_ahci_power,
			.sysconfs_num = 2,
			.sysconfs = (struct stm_device_sysconf []) {
				STM_DEVICE_SYSCONF(SYSCONF(267), 8, 8,
					AHCI_HOST_PWR),
				STM_DEVICE_SYSCONF(SYSCONF(280), 5, 5,
					AHCI_HOST_ACK),
				},
			.pad_config = &(struct stm_pad_config){
				.sysconfs_num = 2,
				.sysconfs =  (struct stm_pad_sysconf[]){
					/* Select Sata instead of PCIe */
					STM_PAD_SYSCONF(SYSCONF(267), 9, 9, 1),
					/* Sata-phy uses 30MHz for PLL */
					STM_PAD_SYSCONF(SYSCONF(268), 0, 0, 1),
					},
			},
		},
	},
};

static u64 stig125_ahci_dmamask = DMA_BIT_MASK(32);

#define AHCI_STM(_id, _iomem, _irq)				\
{								\
	.name = "ahci_stm",					\
	.id = _id,						\
	.num_resources  = 2,					\
	.resource = (struct resource[]) {			\
		STM_PLAT_RESOURCE_MEM_NAMED("ahci", _iomem,	\
				 0x1000),			\
		STIG125_RESOURCE_IRQ_NAMED("ahci", _irq),	\
	},							\
	.dev = {						\
		.dma_mask = &stig125_ahci_dmamask,		\
		.coherent_dma_mask = DMA_BIT_MASK(32),		\
		.platform_data = &stm_ahci_plat_data[_id],	\
	},							\
}

struct platform_device stig125_ahci_devices[] = {
	AHCI_STM(0, 0xFEFB0000, 84),
	AHCI_STM(1, 0xFEFB4000, 91),
};

void __init sti125_configure_sata(unsigned int sata_port)
{
	platform_device_register(&stig125_ahci_devices[sata_port]);
}

static struct plat_isve_data stig125_isve_platform_data[] = {
	{
		.downstream_queue_size = 32,
		.upstream_queue_size = 32,
		.queue_number = 3,
		.ifname = "if17",
		.skip_hdr = ISVE_ALIGN_HDR,
		.hw_rem_hdr = ISVE_DFWD_REM_HDR_ALL,
	}, {
		.downstream_queue_size = 32,
		.upstream_queue_size = 32,
		.queue_number = 4,
		.ifname = "if18",
		.skip_hdr = ISVE_ALIGN_HDR,
		.hw_rem_hdr = ISVE_DFWD_REM_HDR_ALL,
	}, {
		.downstream_queue_size = 32,
		.upstream_queue_size = 32,
		.queue_number = 5,
		.ifname = "if16",
		.skip_hdr = ISVE_ALIGN_HDR,
		.hw_rem_hdr = ISVE_DFWD_REM_HDR_ALL,
#ifdef CONFIG_STM_ISVE_EROUTER
	}, {
		.downstream_queue_size = 32,
		.upstream_queue_size = 32,
		.queue_number = 7,
		.ifname = "if1",
		.skip_hdr = ISVE_ALIGN_HDR,
		.hw_rem_hdr = ISVE_DFWD_REM_HDR_ALL,
#endif	/* CONFIG_STM_ISVE_EROUTER */
	}
};

/* Integrated SoC Virtual Ethernet ISVE platform data */
static u64 stig125_isve_dma_mask = DMA_BIT_MASK(32);
#define STIG125_DOCSIS_BASE_ADD	0xfee00000

static struct platform_device stig125_isve_devices[] = {
	{
		.name = "isve",
		.id = 0,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				DSFWD_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 3),
						DSFWD_RPT_OFF),
			STM_PLAT_RESOURCE_MEM(
				UPIIM_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 3),
						UPIIM_RPT_OFF),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_ds", 42),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_us", 50),
		},
		.dev = {
			.dma_mask = &stig125_isve_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stig125_isve_platform_data[0],
		},
	}, {
		.name = "isve",
		.id = 1,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				DSFWD_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 4),
						DSFWD_RPT_OFF),
			STM_PLAT_RESOURCE_MEM(
				UPIIM_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 4),
						UPIIM_RPT_OFF),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_ds", 43),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_us", 51),
		},
		.dev = {
			.dma_mask = &stig125_isve_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stig125_isve_platform_data[1],
		},
	}, {
		.name = "isve",
		.id = 2,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				DSFWD_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 5),
						DSFWD_RPT_OFF),
			STM_PLAT_RESOURCE_MEM(
				UPIIM_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 5),
						UPIIM_RPT_OFF),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_ds", 44),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_us", 52),
		},
		.dev = {
			.dma_mask = &stig125_isve_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stig125_isve_platform_data[2],
		},
#ifdef CONFIG_STM_ISVE_EROUTER
	}, {
		.name = "isve",
		.id = 3,
		.num_resources = 4,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(
				DSFWD_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 7),
						DSFWD_RPT_OFF),
			STM_PLAT_RESOURCE_MEM(
				UPIIM_QUEUE_ADD(STIG125_DOCSIS_BASE_ADD, 7),
						UPIIM_RPT_OFF),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_ds", 46),
			STIG125_RESOURCE_IRQ_NAMED("isveirq_us", 54),
		},
		.dev = {
			.dma_mask = &stig125_isve_dma_mask,
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.platform_data = &stig125_isve_platform_data[3],
		},
#endif /* CONFIG_STM_ISVE_EROUTER */
	}
};

static struct platform_device *stig125_isve_configured_device[] __initdata = {
	&stig125_isve_devices[0],
	&stig125_isve_devices[1],
	&stig125_isve_devices[2],
#ifdef CONFIG_STM_ISVE_EROUTER
	&stig125_isve_devices[3],
#endif /* CONFIG_STM_ISVE_EROUTER */
};

static int __init stig125_isve_devices_setup(void)
{
	return platform_add_devices(stig125_isve_configured_device,
				    ARRAY_SIZE(stig125_isve_configured_device));
}
device_initcall(stig125_isve_devices_setup);

/* Fast Path Pad configuration */

#define DATA_IN(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define DATA_OUT(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define CLOCK_IN(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_in, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}

#define CLOCK_OUT(_port, _pin, _func, _retiming) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_out, \
		.function = _func, \
		.priv = &(struct stm_pio_control_pad_config) { \
			.retime = _retiming, \
		}, \
	}
#define MDIO(_port, _pin, _func) \
	{ \
		.gpio = stm_gpio(_port, _pin), \
		.direction = stm_pad_gpio_direction_bidir_pull_up, \
		.function = _func, \
		.name = "MDIO", \
	}

static struct stm_pad_config stig125_fastpath_byoi_pad_config = {
	.gpios_num = 14,
	.gpios = (struct stm_pad_gpio[]){
		 /* BYOI port
		  * TX
		  *
		  * For RET_DE_IO, Parameter 1 is delay, Parameter 2
		  * is clk1notclk0
		  */
		 DATA_OUT(11, 4, 2, RET_DE_IO(0, 1)),/* WAN_TXD0 */
		 DATA_OUT(11, 5, 2, RET_DE_IO(0, 1)),/* WAN_TXD1  */
		 DATA_OUT(11, 6, 2, RET_DE_IO(0, 1)),/* WAN_TXD2  */
		 DATA_OUT(11, 7, 2, RET_DE_IO(0, 1)),/* WAN_TXD3  */
		 DATA_OUT(12, 0, 2, RET_DE_IO(0, 1)),/* WAN_TXCTL */
		 CLOCK_OUT(12, 1, 2, RET_NICLK(3250, 1)),/* WAN_TXCLK */
		 /* RX */
		 DATA_IN(12, 2, 2, RET_DE_IO(0, 0)),/* WAN_RXD0 */
		 DATA_IN(12, 3, 2, RET_DE_IO(0, 0)),/* WAN_RXD1 */
		 DATA_IN(12, 4, 2, RET_DE_IO(0, 0)),/* WAN_RXD2 */
		 DATA_IN(12, 5, 2, RET_DE_IO(0, 0)),/* WAN_RXD3 */
		 DATA_IN(12, 6, 2, RET_DE_IO(0, 0)),/* WAN_RXCTL */

		 CLOCK_IN(12, 7, 2, RET_NICLK(1250, 0)),/* WAN_RXCLK */

		 /* Select PIO Alternate Function for MDIO and MDC Intf */
		 MDIO(14, 4, 1),		/* MDIO */
		 STM_PAD_PIO_OUT(14, 5, 1),	/* MDC */
	},
};

/* ISIS ports 0 and 2 Pad configuration */
static struct stm_pad_config stig125_isis_port_2_pad_config = {

	.gpios_num = 24,
	.gpios = (struct stm_pad_gpio[]){

		 /*
		  * ISIS Port 0
		  * TX
		  * For RET_DE_IO, Parameter 1 is delay,
		  * Parameter 2 is clk1notclk0
		  */
		 DATA_OUT(10, 0, 1, RET_DE_IO(0, 0)),/* LAN2_TXD0  */
		 DATA_OUT(10, 1, 1, RET_DE_IO(0, 0)),/* LAN2_TXD1  */
		 DATA_OUT(10, 2, 1, RET_DE_IO(0, 0)),/* LAN2_TXD2  */
		 DATA_OUT(10, 3, 1, RET_DE_IO(0, 0)),/* LAN2_TXD3  */
		 DATA_OUT(10, 4, 1, RET_DE_IO(0, 0)),/* LAN2_TXCTL */

		 CLOCK_OUT(10, 5, 1, RET_NICLK(3250, 0)),/* LAN2_TXCLK*/
		 /* RX */
		 DATA_IN(10, 6, 1, RET_DE_IO(0, 0)),/* LAN2_RXD0 */
		 DATA_IN(10, 7, 1, RET_DE_IO(0, 0)),/* LAN2_RXD1 */
		 DATA_IN(11, 0, 1, RET_DE_IO(0, 0)),/* LAN2_RXD2 */
		 DATA_IN(11, 1, 1, RET_DE_IO(0, 0)),/* LAN2_RXD3 */
		 DATA_IN(11, 2, 1, RET_DE_IO(0, 0)),/* LAN2_RXCTL */

		 CLOCK_IN(11, 3, 1, RET_NICLK(3000, 0)),/* LAN2_RXCLK */

		 /*  ISIS Port 2 */
		 /*    TX      */
		 DATA_OUT(13, 0, 1, RET_DE_IO(0, 0)),/* LAN2_TXD0  */
		 DATA_OUT(13, 1, 1, RET_DE_IO(0, 0)),/* LAN2_TXD1  */
		 DATA_OUT(13, 2, 1, RET_DE_IO(0, 0)),/* LAN2_TXD2  */
		 DATA_OUT(13, 3, 1, RET_DE_IO(0, 0)),/* LAN2_TXD3  */
		 DATA_OUT(13, 4, 1, RET_DE_IO(0, 0)),/* LAN2_TXCTL */

		 CLOCK_OUT(13, 5, 1, RET_NICLK(1250, 0)),/* LAN2_TXCLK */

		 /* RX */
		 DATA_IN(13, 6, 1, RET_DE_IO(0, 0)),/* LAN2_RXD0 */
		 DATA_IN(13, 7, 1, RET_DE_IO(0, 0)),/* LAN2_RXD1 */
		 DATA_IN(14, 0, 1, RET_DE_IO(0, 0)),/* LAN2_RXD2 */
		 DATA_IN(14, 1, 1, RET_DE_IO(0, 0)),/* LAN2_RXD3 */
		 DATA_IN(14, 2, 1, RET_DE_IO(0, 0)),/* LAN2_RXCTL */

		 CLOCK_IN(14, 3, 1, RET_NICLK(1250, 0)),/* LAN2_RXCLK */
	},
};

static struct plat_fpif_data fpif_docsis_data = {
	.id = 2,
	.iftype = DEVID_DOCSIS,
	.ifname = "fpdocsis",
	.tx_dma_ch = 2,
	.rx_dma_ch = 2,
};

static struct plat_fpif_data fpif_gige_data = {
	.mdio_enabled = 1,
	.ethtool_enabled = 1,
	.id = 0,
	.iftype = DEVID_GIGE,
	.ifname = "fpgige",
	.interface = PHY_INTERFACE_MODE_RGMII_ID,
	.phy_addr = 0x1,
	.bus_id = 1,
	.pad_config = &stig125_fastpath_byoi_pad_config,
	.init = &stmfp_claim_resources,
	.exit = &stmfp_release_resources,
	.tx_dma_ch = 0,
	.rx_dma_ch = 0,
};
static struct plat_fpif_data fpif_isis_data = {
	.mdio_enabled = 0,
	.ethtool_enabled = 0,
	.id = 1,
	.iftype = DEVID_ISIS,
	.ifname = "fplan",
	.interface = PHY_INTERFACE_MODE_RGMII_ID,
	.phy_addr = 0x1,
	.bus_id = 1,
	.pad_config = &stig125_isis_port_2_pad_config,
	.init = &stmfp_claim_resources,
	.exit = &stmfp_release_resources,
	.tx_dma_ch = 1,
	.rx_dma_ch = 1,
};
static struct plat_stmfp_data fp_data = {
	.available_l2cam = 128,
	.if_data[0] = &fpif_gige_data,
	.if_data[1] = &fpif_isis_data,
	.if_data[2] = &fpif_docsis_data,
};
static struct platform_device stig125_fp_device = {
	.name = "fpif",
	.id = 0,
	.num_resources = 17,
	.dev = {
		.dma_mask = (void *)DMA_BIT_MASK(32),
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &fp_data,
		},
	.resource = (struct resource[]){
		STM_PLAT_RESOURCE_MEM_NAMED("fp_mem",
					    0xfee80000,
					    0x40000),
		/* STIG125_IRQ macro increases these interrupt numbers
		 * by 32.
		 */
		STIG125_RESOURCE_IRQ_NAMED("FastPath_0", 55),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_1", 56),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_2", 57),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_3", 58),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_4", 59),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_5", 60),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_6", 61),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_7", 62),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_8", 63),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_9", 64),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_10", 65),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_11", 66),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_12", 67),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_13", 68),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_14", 69),
		STIG125_RESOURCE_IRQ_NAMED("FastPath_15", 70),
	}
};/* end STIG125_fp_device */

void __init stig125_configure_fp(void)
{
	platform_device_register(&stig125_fp_device);
}
