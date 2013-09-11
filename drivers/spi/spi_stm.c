/*
 *  ------------------------------------------------------------------------
 *  spi_stm.c SPI/SSC driver for STMicroelectronics platforms
 *  ------------------------------------------------------------------------
 *
 *  Copyright (c) 2008-2013 STMicroelectronics Limited
 *  Author: Angus Clark <Angus.Clark@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 *  ------------------------------------------------------------------------
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/uaccess.h>
#include <linux/param.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/spi_gpio.h>
#include <linux/stm/platform.h>
#include <linux/stm/ssc.h>

#define NAME "spi-stm"

struct spi_stm {
	/* SSC SPI Controller */
	struct spi_bitbang	bitbang;
	void __iomem		*base;
	struct clk		*clk;
	struct platform_device  *pdev;

	/* Resources */
	struct resource r_mem;
	struct resource r_irq;
	struct stm_pad_state *pad_state;

	/* SSC SPI current transaction */
	const u8		*tx_ptr;
	u8			*rx_ptr;
	u16			bits_per_word;
	u16			bytes_per_word;
	unsigned int		baud;
	unsigned int		words_remaining;
	struct completion	done;
};

#define SPI_STM_REQUESTED_CS_GPIO ((void *) -1l)

static void spi_stm_gpio_chipselect(struct spi_device *spi, int is_active)
{
	unsigned cs = (unsigned)spi->controller_data;
	int out;

	if (cs == SPI_GPIO_NO_CHIPSELECT || cs == STM_GPIO_INVALID)
		return;

	out = (spi->mode & SPI_CS_HIGH) ? is_active : !is_active;
	gpio_set_value(cs, out);

	dev_dbg(&spi->dev, "%s PIO%u[%u] -> %d\n",
		is_active ? "select" : "deselect",
		stm_gpio_port(cs), stm_gpio_pin(cs), out);

	return;
}

static int spi_stm_setup_transfer(struct spi_device *spi,
				     struct spi_transfer *t)
{
	struct spi_stm *spi_stm;
	u32 hz;
	u8 bits_per_word;
	u32 reg;
	u32 sscbrg;

	spi_stm = spi_master_get_devdata(spi->master);
	bits_per_word = (t) ? t->bits_per_word : 0;
	hz = (t) ? t->speed_hz : 0;

	/* If not specified, use defaults */
	if (!bits_per_word)
		bits_per_word = spi->bits_per_word;
	if (!hz)
		hz = spi->max_speed_hz;

	/* Actually, can probably support 2-16 without any other change!!! */
	if (bits_per_word != 8 && bits_per_word != 16) {
		dev_err(&spi->dev, "unsupported bits_per_word=%d\n",
			bits_per_word);
		return -EINVAL;
	}
	spi_stm->bits_per_word = bits_per_word;

	/* Set SSC_BRF */
	sscbrg = clk_get_rate(spi_stm->clk) / (2*hz);
	if (sscbrg < 0x07 || sscbrg > (0x1 << 16)) {
		dev_err(&spi->dev, "baudrate outside valid range"
			" %d (sscbrg = %d)\n", hz, sscbrg);
		return -EINVAL;
	}
	spi_stm->baud = clk_get_rate(spi_stm->clk) / (2 * sscbrg);
	if (sscbrg == (0x1 << 16)) /* 16-bit counter wraps */
		sscbrg = 0x0;
	ssc_store32(spi_stm, SSC_BRG, sscbrg);

	dev_dbg(&spi->dev, "setting baudrate: target = %u hz, "
		"actual = %u hz, sscbrg = %u\n",
		hz, spi_stm->baud, sscbrg);

	 /* Set SSC_CTL and enable SSC */
	 reg = ssc_load32(spi_stm, SSC_CTL);
	 reg |= SSC_CTL_MS;

	 if (spi->mode & SPI_CPOL)
		 reg |= SSC_CTL_PO;
	 else
		 reg &= ~SSC_CTL_PO;

	 if (spi->mode & SPI_CPHA)
		 reg |= SSC_CTL_PH;
	 else
		 reg &= ~SSC_CTL_PH;

	 if ((spi->mode & SPI_LSB_FIRST) == 0)
		 reg |= SSC_CTL_HB;
	 else
		 reg &= ~SSC_CTL_HB;

	 if (spi->mode & SPI_LOOP)
		 reg |= SSC_CTL_LPB;
	 else
		 reg &= ~SSC_CTL_LPB;

	 reg &= 0xfffffff0;
	 reg |= (bits_per_word - 1);

	 reg |= SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO;
	 reg |= SSC_CTL_EN;

	 dev_dbg(&spi->dev, "ssc_ctl = 0x%04x\n", reg);
	 ssc_store32(spi_stm, SSC_CTL, reg);

	 /* Clear the status register */
	 ssc_load32(spi_stm, SSC_RBUF);

	 return 0;
}

static void spi_stm_cleanup(struct spi_device *spi)
{
	unsigned cs = (unsigned)spi->controller_data;

	if (spi->controller_state == SPI_STM_REQUESTED_CS_GPIO) {
		gpio_free(cs);
		spi->controller_state = (void *)0;
	}
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS  (SPI_CPOL | SPI_CPHA | SPI_LSB_FIRST | SPI_LOOP | SPI_CS_HIGH)
static int spi_stm_setup(struct spi_device *spi)
{
	struct spi_stm *spi_stm;
	unsigned cs;
	int ret;

	spi_stm = spi_master_get_devdata(spi->master);

	if (spi->mode & ~MODEBITS) {
		dev_err(&spi->dev, "unsupported mode bits %x\n",
			spi->mode & ~MODEBITS);
		return -EINVAL;
	}

	if (!spi->max_speed_hz)  {
		dev_err(&spi->dev, "max_speed_hz unspecified\n");
		return -EINVAL;
	}

	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	/* In DT environments, the CS GPIO number is held in spi->cs_gpio, with
	 * -ENOENT denoting "No CS".
	 */
	if (spi->dev.of_node) {
		if (spi->cs_gpio == -ENOENT)
			spi->controller_data = (void *)SPI_GPIO_NO_CHIPSELECT;
		else
			spi->controller_data = (void *)spi->cs_gpio;
	}

	/* Get CS GPIO, if required */
	cs = (unsigned)spi->controller_data;
	if (cs != SPI_GPIO_NO_CHIPSELECT &&
	    cs != STM_GPIO_INVALID &&
	    spi->controller_state != SPI_STM_REQUESTED_CS_GPIO) {
		ret = gpio_request(cs, dev_name(&spi->dev));
		if (ret)
			return ret;

		ret = gpio_direction_output(cs, spi->mode & SPI_CS_HIGH);
		if (ret) {
			gpio_free(cs);
			return ret;
		}

		spi->controller_state = SPI_STM_REQUESTED_CS_GPIO;
	}

	ret = spi_stm_setup_transfer(spi, NULL);
	if (ret) {
		if (spi->controller_state == SPI_STM_REQUESTED_CS_GPIO)
			gpio_free(cs);
		return ret;
	}

	return 0;
}

/* Load the TX FIFO */
static void ssc_write_tx_fifo(struct spi_stm *spi_stm)
{

	uint32_t count;
	uint32_t word = 0;
	int i;

	if (spi_stm->words_remaining > 8)
		count = 8;
	else
		count = spi_stm->words_remaining;

	for (i = 0; i < count; i++) {
		if (spi_stm->tx_ptr) {
			if (spi_stm->bytes_per_word == 1) {
				word = *spi_stm->tx_ptr++;
			} else {
				word = *spi_stm->tx_ptr++;
				word = *spi_stm->tx_ptr++ | (word << 8);
			}
		}
		ssc_store32(spi_stm, SSC_TBUF, word);
	}
}

/* Read the RX FIFO */
static void ssc_read_rx_fifo(struct spi_stm *spi_stm)
{

	uint32_t count;
	uint32_t word = 0;
	int i;

	if (spi_stm->words_remaining > 8)
		count = 8;
	else
		count = spi_stm->words_remaining;

	for (i = 0; i < count; i++) {
		word = ssc_load32(spi_stm, SSC_RBUF);
		if (spi_stm->rx_ptr) {
			if (spi_stm->bytes_per_word == 1) {
				*spi_stm->rx_ptr++ = (uint8_t)word;
			} else {
				*spi_stm->rx_ptr++ = (word >> 8);
				*spi_stm->rx_ptr++ = word & 0xff;
			}
		}
	}

	spi_stm->words_remaining -= count;
}

/* Interrupt fired when TX shift register becomes empty */
static irqreturn_t spi_stm_irq(int irq, void *dev_id)
{
	struct spi_stm *spi_stm = (struct spi_stm *)dev_id;

	/* Read RX FIFO */
	ssc_read_rx_fifo(spi_stm);

	/* Fill TX FIFO */
	if (spi_stm->words_remaining) {
		ssc_write_tx_fifo(spi_stm);
	} else {
		/* TX/RX complete */
		ssc_store32(spi_stm, SSC_IEN, 0x0);
		complete(&spi_stm->done);
	}

	return IRQ_HANDLED;
}


static int spi_stm_txrx_bufs(struct spi_device *spi, struct spi_transfer *t)
{
	struct spi_stm *spi_stm;
	uint32_t ctl = 0;

	spi_stm = spi_master_get_devdata(spi->master);

	pm_runtime_get_sync(&spi_stm->pdev->dev);

	/* Setup transfer */
	spi_stm->tx_ptr = t->tx_buf;
	spi_stm->rx_ptr = t->rx_buf;

	if (spi_stm->bits_per_word > 8) {
		/* Anything greater than 8 bits-per-word requires 2
		 * bytes-per-word in the RX/TX buffers */
		spi_stm->bytes_per_word = 2;
		spi_stm->words_remaining = t->len/2;
	} else if (spi_stm->bits_per_word == 8 &&
		   ((t->len & 0x1) == 0)) {
		/* If transfer is even-length, and 8 bits-per-word, then
		 * implement as half-length 16 bits-per-word transfer */
		spi_stm->bytes_per_word = 2;
		spi_stm->words_remaining = t->len/2;

		/* Set SSC_CTL to 16 bits-per-word */
		ctl = ssc_load32(spi_stm, SSC_CTL);
		ssc_store32(spi_stm, SSC_CTL, (ctl | 0xf));

		ssc_load32(spi_stm, SSC_RBUF);

	} else {
		spi_stm->bytes_per_word = 1;
		spi_stm->words_remaining = t->len;
	}

	INIT_COMPLETION(spi_stm->done);

	/* Start transfer by writing to the TX FIFO */
	ssc_write_tx_fifo(spi_stm);
	ssc_store32(spi_stm, SSC_IEN, SSC_IEN_TEEN);

	/* Wait for transfer to complete */
	wait_for_completion(&spi_stm->done);

	/* Restore SSC_CTL if necessary */
	if (ctl)
		ssc_store32(spi_stm, SSC_CTL, ctl);

	pm_runtime_put(&spi_stm->pdev->dev);

	return t->len;

}
#ifdef CONFIG_OF
static void *stm_spi_dt_get_pdata(struct platform_device *pdev)
{

	struct device_node *np = pdev->dev.of_node;
	struct stm_plat_ssc_data *data;
	const char *clk_name;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);

	if (!of_property_read_string(np, "st,dev-clk", &clk_name))
		clk_add_alias(NULL, pdev->name, (char *)clk_name, NULL);

	data->pad_config = stm_of_get_pad_config(&pdev->dev);
	return data;
}
#else
static void *stm_spi_dt_get_pdata(struct platform_device *pdev)
{
	return NULL;
}
#endif

static int spi_stm_probe(struct platform_device *pdev)
{
	struct stm_plat_ssc_data *plat_data;
	struct spi_master *master;
	struct resource *res;
	struct spi_stm *spi_stm;
	u32 reg;
	int status = 0;

	if (pdev->dev.of_node)
		pdev->dev.platform_data = stm_spi_dt_get_pdata(pdev);

	plat_data = pdev->dev.platform_data;

	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_stm));
	if (!master) {
		dev_err(&pdev->dev, "failed to allocate spi master\n");
		status = -ENOMEM;
		goto err0;
	}

	platform_set_drvdata(pdev, master);

	spi_stm = spi_master_get_devdata(master);
	master->dev.of_node = pdev->dev.of_node;
	spi_stm->bitbang.master = spi_master_get(master);
	spi_stm->bitbang.setup_transfer = spi_stm_setup_transfer;
	spi_stm->bitbang.txrx_bufs = spi_stm_txrx_bufs;
	spi_stm->bitbang.master->setup = spi_stm_setup;
	spi_stm->bitbang.master->cleanup = spi_stm_cleanup;
	spi_stm->bitbang.chipselect = spi_stm_gpio_chipselect;
	spi_stm->pdev = pdev;

	/* the spi->mode bits understood by this driver: */
	master->mode_bits = MODEBITS;

	/* In non-DT environments, the number of potential chip selects is
	 * limited only by the size of 'master->controller_data', which is used
	 * to hold the GPIO number.  However, to conform with various
	 * assumptions made by the SPI framework we set num_chipselect to the
	 * maximum value supported by the field.
	 *
	 * In DT environments, 'bus_num' and 'num_chipselect' are derived later
	 * from the node data (see
	 * spi.c:spi_register_master->of_spi_register_master()).
	 */
	if (!pdev->dev.of_node) {
		master->num_chipselect = (u16)-1;
		master->bus_num = pdev->id;
	}

	init_completion(&spi_stm->done);

	/* Get resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to find IOMEM resource\n");
		status = -ENOENT;
		goto err1;
	}
	spi_stm->r_mem = *res;

	if (!request_mem_region(res->start,
				res->end - res->start + 1, NAME)) {
		dev_err(&pdev->dev, "request memory region failed [0x%x]\n",
			res->start);
		status = -EBUSY;
		goto err1;
	}

	spi_stm->base = ioremap_nocache(res->start, res->end - res->start + 1);
	if (!spi_stm->base) {
		dev_err(&pdev->dev, "ioremap memory failed [0x%x]\n",
			res->start);
		status = -ENXIO;
		goto err2;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(&pdev->dev, "failed to find IRQ resource\n");
		status = -ENOENT;
		goto err3;
	}
	spi_stm->r_irq = *res;

	if (request_irq(res->start, spi_stm_irq,
			IRQF_DISABLED, dev_name(&pdev->dev), spi_stm)) {
		dev_err(&pdev->dev, "irq request failed\n");
		status = -EBUSY;
		goto err3;
	}

	spi_stm->pad_state = stm_pad_claim(plat_data->pad_config,
					   dev_name(&pdev->dev));
	if (!spi_stm->pad_state) {
		dev_err(&pdev->dev, "pads request failed\n");
		status = -EBUSY;
		goto err4;
	}

	/* Disable I2C and Reset SSC */
	ssc_store32(spi_stm, SSC_I2C, 0x0);
	reg = ssc_load16(spi_stm, SSC_CTL);
	reg |= SSC_CTL_SR;
	ssc_store32(spi_stm, SSC_CTL, reg);

	udelay(1);
	reg = ssc_load32(spi_stm, SSC_CTL);
	reg &= ~SSC_CTL_SR;
	ssc_store32(spi_stm, SSC_CTL, reg);

	/* Set SSC into slave mode before reconfiguring PIO pins */
	reg = ssc_load32(spi_stm, SSC_CTL);
	reg &= ~SSC_CTL_MS;
	ssc_store32(spi_stm, SSC_CTL, reg);

	spi_stm->clk = clk_get(&pdev->dev, "comms_clk");
	if (IS_ERR(spi_stm->clk)) {
		dev_err(&pdev->dev, "Comms clock not found!\n");
		status = PTR_ERR(spi_stm->clk);
		goto err5;
	}

	clk_prepare_enable(spi_stm->clk);
	/* Start "bitbang" worker */
	status = spi_bitbang_start(&spi_stm->bitbang);
	if (status) {
		dev_err(&pdev->dev, "bitbang start failed [%d]\n", status);
		goto err5;
	}

	dev_info(&pdev->dev, "registered SPI Bus %d\n", master->bus_num);

	/* by default the device is on */
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	return status;

 err5:
	stm_pad_release(spi_stm->pad_state);
 err4:
	free_irq(spi_stm->r_irq.start, spi_stm);
 err3:
	iounmap(spi_stm->base);
 err2:
	release_mem_region(spi_stm->r_mem.start,
			   resource_size(&spi_stm->r_mem));
 err1:
	spi_master_put(spi_stm->bitbang.master);
	platform_set_drvdata(pdev, NULL);
 err0:
	return status;
}

static int spi_stm_remove(struct platform_device *pdev)
{
	struct spi_stm *spi_stm;
	struct spi_master *master;

	master = platform_get_drvdata(pdev);
	spi_stm = spi_master_get_devdata(master);

	spi_bitbang_stop(&spi_stm->bitbang);

	clk_disable_unprepare(spi_stm->clk);

	stm_pad_release(spi_stm->pad_state);
	free_irq(spi_stm->r_irq.start, spi_stm);
	iounmap(spi_stm->base);
	release_mem_region(spi_stm->r_mem.start,
			   resource_size(&spi_stm->r_mem));

	spi_master_put(spi_stm->bitbang.master);
	platform_set_drvdata(pdev, NULL);

	return 0;
}
#ifdef CONFIG_PM
static int spi_stm_suspend(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_stm *spi_stm;

	spi_stm = spi_master_get_devdata(master);

	ssc_store32(spi_stm, SSC_IEN, 0);

	clk_disable_unprepare(spi_stm->clk);
	return 0;
}

static int spi_stm_resume(struct device *dev)
{
	struct spi_master *master = dev_get_drvdata(dev);
	struct spi_stm *spi_stm;

	spi_stm = spi_master_get_devdata(master);

	clk_prepare_enable(spi_stm->clk);
	return 0;
}

static struct dev_pm_ops spi_stm_pm = {
	.suspend = spi_stm_suspend,
	.resume = spi_stm_resume,
	.freeze = spi_stm_suspend,
	.restore = spi_stm_resume,
	.runtime_suspend = spi_stm_suspend,
	.runtime_resume = spi_stm_resume,
};
#else
static struct dev_pm_ops spi_stm_pm;
#endif

#ifdef CONFIG_OF
static struct of_device_id stm_spi_match[] = {
	{
		.compatible = "st,spi",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_spi_match);
#endif

static struct platform_driver spi_stm_driver = {
	.driver.name = NAME,
	.driver.owner = THIS_MODULE,
	.driver.pm = &spi_stm_pm,
	.driver.of_match_table = of_match_ptr(stm_spi_match),
	.probe = spi_stm_probe,
	.remove = spi_stm_remove,
};


static int __init spi_stm_init(void)
{
	return platform_driver_register(&spi_stm_driver);
}

static void __exit spi_stm_exit(void)
{
	platform_driver_unregister(&spi_stm_driver);
}

module_init(spi_stm_init);
module_exit(spi_stm_exit);

MODULE_AUTHOR("STMicroelectronics <www.st.com>");
MODULE_DESCRIPTION("STM SSC SPI driver");
MODULE_LICENSE("GPL");
