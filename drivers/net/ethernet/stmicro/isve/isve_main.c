/*******************************************************************************
  This is the driver for the SoC Virtual Ethernet

	Copyright(C) 2012 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>

*******************************************************************************/

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include "isve.h"

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

/* Module parameters */
#define TX_TIMEO 10000		/* default 10 */
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit timeout in milliseconds");

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

#define ISVE_XMIT_THRESH	8
static inline int isve_tx_avail(unsigned int dirty_tx, unsigned int cur_tx,
				unsigned int tx_size)
{
	return dirty_tx + tx_size - cur_tx - 1;
}

#if defined(CONFIG_ISVE_DEBUG)
static void print_pkt(unsigned char *buf, int len)
{
	int j;
	pr_info("len = %d byte, buf addr: 0x%p", len, buf);
	for (j = 0; j < len; j++) {
		if ((j % 16) == 0)
			pr_info(" %03x:", j);
		pr_info(" %02x", buf[j]);
	}
	pr_info("\n");
}
#endif /* CONFIG_ISVE_DEBUG */

/**
 * isve_release_resources: release the rx/tx buffers
 * @priv: private structure
 * Description: it de-allocates the TX / RX buffers and un-maps the
 * DMA addresses.
 */
static void isve_release_resources(struct isve_priv *priv)
{
	int i;
	struct device *dev = priv->device;

	DBG("isve: free transmit resources\n");
	for (i = 0; i < priv->upstream_queue_size; i++) {
		dma_addr_t d;

		d = priv->tx_desc[i].buf_addr;
		if (d)
			dma_unmap_single(dev, d, ISVE_BUF_LEN, DMA_TO_DEVICE);

		if (priv->tx_desc[i].buffer != NULL)
			kfree(priv->tx_desc[i].buffer);
	}
	DBG("isve: free receive resources\n");
	for (i = 0; i < priv->downstream_queue_size; i++) {
		dma_addr_t d;

		d = priv->rx_desc[i].buf_addr;
		if (d)
			dma_unmap_single(dev, d, ISVE_BUF_LEN, DMA_TO_DEVICE);

		if (priv->rx_desc[i].buffer != NULL)
			kfree(priv->rx_desc[i].buffer);
	}
}

/**
 * isve_alloc_resources: allocate the rx/tx buffers
 * @priv: private structure
 * Description: it allocates the TX / RX resources (buffers etc).
 * Driver maintains two lists of buffers for managing the trasmission
 * and the reception processes.
 * Streaming DMA mappings are used to DMA transfer,
 */
static int isve_alloc_resources(struct isve_priv *priv)
{
	unsigned int txlen = priv->upstream_queue_size;
	unsigned int rxlen = priv->downstream_queue_size;
	int i, ret = 0;

	DBG(">>> %s: txlen %d, rxlen %d\n", __func__, txlen, rxlen);

	priv->rx_desc = kzalloc(sizeof(struct isve_desc *) * rxlen, GFP_KERNEL);
	if (priv->rx_desc == NULL) {
		pr_err("%s: ERROR allocating the Rx buffers\n", __func__);
		return -ENOMEM;
	}
	DBG("Rx resources: rx_desc 0x%p\n", priv->rx_desc);
	for (i = 0; i < rxlen; i++) {
		void *buf;
		dma_addr_t d;

		buf = kzalloc(ISVE_BUF_LEN, GFP_KERNEL);
		if (buf == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		d = dma_map_single(priv->device, buf, ISVE_BUF_LEN,
				   DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, d)) {
			kfree(buf);
			ret = -ENOMEM;
			goto err;
		}

		priv->rx_desc[i].buffer = buf;
		priv->rx_desc[i].buf_addr = d;

		DBG("\t%d: buffer 0x%p, buf_addr 0x%x\n",
		    i, priv->rx_desc[i].buffer, priv->rx_desc[i].buf_addr);
	}

	priv->cur_rx = 0;
	priv->dfwd->init_rx_fifo(priv->ioaddr_dfwd, priv->rx_desc[0].buf_addr);

	/* Allocate the transmit resources */
	priv->tx_desc = kzalloc(sizeof(struct isve_desc *) * txlen, GFP_KERNEL);
	if (priv->tx_desc == NULL) {
		pr_err("%s: ERROR allocating the Tx buffers\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	DBG("Tx resources: tx_desc 0x%p\n", priv->tx_desc);
	for (i = 0; i < txlen; i++) {
		void *buf;
		dma_addr_t d;

		buf = kzalloc(ISVE_BUF_LEN, GFP_KERNEL);
		if (buf == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		d = dma_map_single(priv->device, buf, ISVE_BUF_LEN,
				   DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, d)) {
			kfree(buf);
			ret = -ENOMEM;
			goto err;
		}

		priv->tx_desc[i].buffer = buf;
		priv->tx_desc[i].buf_addr = d;

		DBG("\t%d: buffer 0x%p, buf_addr 0x%x\n",
		    i, priv->tx_desc[i].buffer, priv->tx_desc[i].buf_addr);
	}
	priv->cur_tx = 0;
	priv->dirty_tx = 0;

	DBG("<<< %s\n", __func__);

	return ret;
err:
	isve_release_resources(priv);
	return ret;
}

static inline void isve_dump_tx_status(unsigned int addr, unsigned int entry)
{
	DBG("%s: entry %d: freed addr 0x%x\n", __func__, entry, addr);
}

/**
 * isve_tx: claim and freee the tx resources
 * @priv: private driver structure
 * Description: this actually is the UPIIM interrupt handler to read the free
 * queue address and move the dirty SW entry used for managing the process.
 * It also restarts the tx queue in case of it was stopped and according
 * to the threshold value.
 */
static void isve_tx(struct isve_priv *priv)
{
	spin_lock(&priv->tx_lock);

	while (priv->dirty_tx != priv->cur_tx) {
		unsigned int freed_addr;
		unsigned int entry = priv->dirty_tx % priv->upstream_queue_size;

		freed_addr = priv->upiim->freed_tx_add(priv->ioaddr_upiim);
		isve_dump_tx_status(freed_addr, entry);
		priv->dirty_tx++;
	}

	if (netif_queue_stopped(priv->dev) &&
	    (isve_tx_avail(priv->dirty_tx, priv->cur_tx,
			   priv->upstream_queue_size) > ISVE_XMIT_THRESH)) {
		DBG("%s: restart tx\n", __func__);
		netif_wake_queue(priv->dev);
	}

	spin_unlock(&priv->tx_lock);
}

/**
 * isve_dwfd_isr: isve interrupt handler for the DWFD queue
 * @irq: interrupt number
 * @dev_id: device id
 * Description: this is the interrupt handler. It schedules NAPI poll method
 * to manage the reception (downstream) of the incoming frames.
 */
static irqreturn_t isve_dwfd_isr(int irq, void *dev_id)
{
	int ret;
	struct net_device *dev = (struct net_device *)dev_id;
	struct isve_priv *priv = netdev_priv(dev);

	if (unlikely(!dev)) {
		pr_err("%s: invalid dev pointer\n", __func__);
		return IRQ_NONE;
	}

	ret = priv->dfwd->isr(priv->ioaddr_dfwd, &priv->xstats);
	if (likely(ret == out_packet)) {
		priv->dfwd->enable_irq(priv->ioaddr_dfwd, 0);
		napi_schedule(&priv->napi);
	} else if (unlikely(ret == out_packet_dropped)) {
		dev->stats.rx_errors++;
		priv->dev->stats.rx_dropped++;
	}

	return IRQ_HANDLED;
}

/**
 * isve_upiim_isr: isve interrupt handler for the upstream queue
 * @irq: interrupt number
 * @dev_id: device id
 * Description: this is the interrupt handler. It calls the
 * UPIIM specific handler.
 */
static irqreturn_t isve_upiim_isr(int irq, void *dev_id)
{
	int ret;
	struct net_device *dev = (struct net_device *)dev_id;
	struct isve_priv *priv = netdev_priv(dev);

	if (unlikely(!dev)) {
		pr_err("%s: invalid dev pointer\n", __func__);
		return IRQ_NONE;
	}

	/* Clean tx resources */
	ret = priv->upiim->isr(priv->ioaddr_upiim, &priv->xstats);
	if (likely(ret == in_packet))
		isve_tx(priv);
	else if (ret < 0)	/* never happen */
		priv->dev->stats.tx_errors++;

	return IRQ_HANDLED;
}

/**
 * isve_provide_mac_addr: to provide the MAC address to the virtual iface
 * @priv: private structure
 * Description: verify if the MAC address is valid (that could be passed
 * by using other supports, e.g. nwhwconfig).
 * In case of no MAC address is passed it generates a random one
 */
static void isve_provide_mac_addr(struct isve_priv *priv)
{
	if (!is_valid_ether_addr(priv->dev->dev_addr))
		random_ether_addr(priv->dev->dev_addr);

	pr_info("\t%s: device MAC address %pM\n", priv->dev->name,
		priv->dev->dev_addr);
}

/**
 *  isve_open: open entry point of the driver
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function is the open entry point of the driver.
 *  It sets the MAC address, inits the Downstream and Upstream Cores,
 *  enables NAPI and starts the processes.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
static int isve_open(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);
	int ret = 0;

	DBG(">>> isve open\n");

	isve_provide_mac_addr(priv);

	ret = request_irq(priv->irq_ds, isve_dwfd_isr, IRQF_SHARED, dev->name,
			  dev);
	if (unlikely(ret < 0)) {
		pr_err("%s: ERROR: allocating the DWFD IRQ %d (error: %d)\n",
		       __func__, priv->irq_ds, ret);
		return ret;
	}
	ret = request_irq(priv->irq_us, isve_upiim_isr, IRQF_SHARED, dev->name,
			  dev);
	if (unlikely(ret < 0)) {
		pr_err("%s: ERROR: allocating the UPIIM IRQ %d (error: %d)\n",
		       __func__, priv->irq_ds, ret);
		free_irq(priv->irq_ds, dev);
		return ret;
	}

	DBG("isve: init downstream and upstream modules...\n");
	priv->dfwd->dfwd_init(priv->ioaddr_dfwd, priv->hw_rem_hdr);
	priv->upiim->upiim_init(priv->ioaddr_upiim);

	/* Extra statistics */
	memset(&priv->xstats, 0, sizeof(struct isve_extra_stats));

#if defined(CONFIG_ISVE_DEBUG)
	if (netif_msg_hw(priv)) {
		priv->dfwd->dump_regs(priv->ioaddr_dfwd);
		priv->upiim->dump_regs(priv->ioaddr_upiim);
	}
#endif /* CONFIG_ISVE_DEBUG */

	napi_enable(&priv->napi);
	netif_start_queue(dev);
	DBG("<<< isve open\n");

	return ret;
}

/**
 *  isve_release - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 */
static int isve_release(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);

	DBG(">>> isve release\n");
	priv->dfwd->enable_irq(priv->ioaddr_dfwd, 0);
	priv->upiim->enable_irq(priv->ioaddr_dfwd, 0);

	netif_stop_queue(dev);
	napi_disable(&priv->napi);

	/* Free the IRQ lines */
	free_irq(priv->irq_us, dev);
	free_irq(priv->irq_ds, dev);

	DBG("<<< isve release\n");

	return 0;
}

/**
 * isve_xmit: transmit function
 * @skb : the socket buffer
 * @dev : device pointer
 * Description : Tx entry point of the driver. Mainly it gets the skb and
 * fills the buffer to be passed to the DMA queue. Each pointer has to be
 * aligned to a 32-byte boundary so the skb->data is copied and linearized by
 * skb_copy_and_csum_dev callback.
 */
static netdev_tx_t isve_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned int len;
	struct isve_priv *priv = netdev_priv(dev);
	int entry = priv->cur_tx % priv->upstream_queue_size;
	struct isve_desc *p = priv->tx_desc + entry;
	unsigned long flags;
	int pending;

	if (unlikely(skb_padto(skb, ETH_ZLEN)))
		return NETDEV_TX_OK;

	spin_lock_irqsave(&priv->tx_lock, flags);
	len = skb->len;

	DBG("\nisve xmit: skb %p skb->data %p len %d\n", skb, skb->data, len);

	dma_sync_single_for_cpu(priv->device, p->buf_addr, skb->len,
				DMA_TO_DEVICE);
	skb_copy_and_csum_dev(skb, p->buffer);
	dma_sync_single_for_device(priv->device, p->buf_addr, skb->len,
				   DMA_TO_DEVICE);

	priv->cur_tx++;
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += len;

#if defined(CONFIG_ISVE_DEBUG)
	pr_info("entry %d, dirty %d, cur_tx %d, dma mapped add 0x%x "
		"\n dma data buffer %p\n", entry, priv->dirty_tx,
		priv->cur_tx % priv->upstream_queue_size,
		p->buf_addr, p->buffer);

	if (netif_msg_pktdata(priv)) {
		pr_info("Frame to be transmitted (%dbytes)", len);
		print_pkt(p->buffer, len);
	}
#endif /* CONFIG_ISVE_DEBUG */

	dev_kfree_skb(skb);

	pending = isve_tx_avail(priv->dirty_tx, priv->cur_tx,
				priv->upstream_queue_size);
	if (pending < ISVE_XMIT_THRESH) {
		DBG("xmit: stop transmitted packets %d\n", pending);
		netif_stop_queue(dev);
	}

	/* Fill HW pointers and len in the right order and trasmit ! */
	priv->upiim->fill_tx_add(priv->ioaddr_upiim, p->buf_addr, len);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return NETDEV_TX_OK;
}

/**
 * isve_rx: receive function.
 * @priv : private structure
 * @limit : budget used for mitigation
 * Description: it handles the receive process and gets the incoming frames
 * from the downstream queue.
 */
static int isve_rx(struct isve_priv *priv, int limit)
{
	unsigned int count = 0;
	unsigned int len;
	void __iomem *ioaddr = priv->ioaddr_dfwd;
	struct sk_buff *skb;
	unsigned int rxlen = priv->downstream_queue_size;
	struct net_device *ndev = priv->dev;

	DBG(">>>> %s", __func__);

	while (count < limit) {
		struct dfwd_fifo fifo;
		unsigned int entry = priv->cur_rx % rxlen;
		struct isve_desc *p = priv->rx_desc + entry;
		u32 addr;

		DBG("Entry %d: curr: %d, buffer 0x%p, buf_addr 0x%x\n", entry,
		    priv->cur_rx, p->buffer, p->buf_addr);

		fifo = priv->dfwd->get_rx_fifo_status(ioaddr);
		DBG("DFWD entries: used %d - free %d\n", fifo.used, fifo.free);
		if (!fifo.used)
			break;

		/* Get the len before */
		len = priv->dfwd->get_rx_len(ioaddr);
		/* Now get the address, the entry is removed from the FIFO */
		addr = priv->dfwd->get_rx_used_add(ioaddr);
		DBG("DFWD Regs: address: 0x%x, len  %d\n", addr, len);

		skb = netdev_alloc_skb_ip_align(priv->dev, len);
		if (unlikely(skb == NULL)) {
			printk(KERN_NOTICE "%s: low memory, packet dropped.\n",
			       ndev->name);
			ndev->stats.rx_dropped++;
			break;
		}

		len -= 2;

		dma_sync_single_for_cpu(priv->device, p->buf_addr, len,
					DMA_FROM_DEVICE);
		/* For some downstream queues it could be needed to skip some
		 * bytes (DOCSIS Header) from the incoming frames.
		 */
		skb_copy_to_linear_data(skb, p->buffer + priv->skip_hdr, len);
		dma_sync_single_for_device(priv->device, p->buf_addr, len,
					   DMA_FROM_DEVICE);
		skb_put(skb, len);

#ifdef CONFIG_ISVE_DEBUG
		if (netif_msg_pktdata(priv)) {
			pr_info("Frame received (%dbytes)", skb->len);
			print_pkt(skb->data, skb->len);
		}
#endif /* CONFIG_ISVE_DEBUG */

		skb->protocol = eth_type_trans(skb, ndev);
		skb_checksum_none_assert(skb);
		netif_receive_skb(skb);

		ndev->stats.rx_packets++;
		ndev->stats.rx_bytes += len;

		count++;
		priv->cur_rx++;
		priv->dfwd->init_rx_fifo(priv->ioaddr_dfwd,
					 priv->rx_desc[priv->cur_rx %
						       rxlen].buf_addr);
	}
	DBG("<<< %s", __func__);
	return count;
}

/**
 *  isve_poll - isve poll method (NAPI)
 *  @napi : pointer to the napi structure.
 *  @budget :napi maximum number of packets that can be received.
 *  Description : this function implements the the reception process.
 */
static int isve_poll(struct napi_struct *napi, int budget)
{
	struct isve_priv *priv = container_of(napi, struct isve_priv, napi);
	int work_done = 0;

	work_done = isve_rx(priv, budget);

	if (work_done < budget) {
		unsigned long flags;
		spin_lock_irqsave(&priv->rx_lock, flags);
		napi_complete(napi);
		priv->dfwd->enable_irq(priv->ioaddr_dfwd, 1);
		spin_unlock_irqrestore(&priv->rx_lock, flags);
	}
	return work_done;
}

/**
 *  isve_tx_timeout: watchdog timer
 *  @dev : Pointer to net device structure
 *  Description: this function is called when a packet transmission fails to
 *  complete within a reasonable tmrate. The driver will mark the error in the
 *  netdev structure and arrange for the device to be reset to a sane state
 *  in order to transmit a new packet.
 */
static void isve_tx_timeout(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);

	netif_stop_queue(priv->dev);

	DBG("isve_tx_timeout...\n");

	priv->upiim->upiim_init(priv->ioaddr_upiim);
	priv->dev->stats.tx_errors++;

	netif_wake_queue(priv->dev);
}

/**
 *  isve_change_mtu: to change the MTU
 *  @dev : Pointer to net device structure
 *  @new_mtu: new MTU
 *  Description: this is to change the MTU that cannot be bigger than the
 *  maximum buffer size (2048bytes).
 */
static int isve_change_mtu(struct net_device *dev, int new_mtu)
{

	if (netif_running(dev)) {
		pr_err("%s: must be stopped to change its MTU\n", dev->name);
		return -EBUSY;
	}

	if (new_mtu > ISVE_BUF_LEN)
		return -EINVAL;

	dev->mtu = new_mtu;
	netdev_update_features(dev);

	return 0;
}

static const struct net_device_ops isve_netdev_ops = {
	.ndo_open = isve_open,
	.ndo_start_xmit = isve_xmit,
	.ndo_stop = isve_release,
	.ndo_change_mtu = isve_change_mtu,
	.ndo_tx_timeout = isve_tx_timeout,
	.ndo_set_mac_address = eth_mac_addr,
};

/**
 * isve_dvr_probe: allocate the netdev resources and the private structure.
 * @device: device pointer
 * @plat_dat: isve platform struct pointer
 * Description: this is the probe function used for callng
 * the alloc_etherdev, allocate the priv structure.
 */
struct isve_priv *isve_dvr_probe(struct device *device,
				 struct plat_isve_data *plat_dat)
{
	int ret = 0;
	struct net_device *ndev = NULL;
	struct isve_priv *priv;

	ndev = alloc_etherdev(sizeof(struct isve_priv));
	if (!ndev) {
		pr_err("%s: ERROR: allocating the device\n", __func__);
		return NULL;
	}

	SET_NETDEV_DEV(ndev, device);

	priv = netdev_priv(ndev);
	priv->device = device;
	priv->dev = ndev;

	/* Override the ethX with the name passed from the platform */
	if (plat_dat->ifname)
		strcpy(priv->dev->name, plat_dat->ifname);

	ether_setup(ndev);

	isve_set_ethtool_ops(ndev);

	ndev->netdev_ops = &isve_netdev_ops;

	/* No csum in HW */
	ndev->hw_features = 0;
	ndev->features = ndev->hw_features;

	ndev->features |= ndev->hw_features | NETIF_F_HIGHDMA;
	ndev->watchdog_timeo = msecs_to_jiffies(watchdog);

	priv->msg_enable = netif_msg_init(debug, default_msg_level);

	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->rx_lock);

	netif_napi_add(ndev, &priv->napi, isve_poll, 16);

	ret = register_netdev(ndev);
	if (ret) {
		pr_err("%s: ERROR %i registering the device\n", __func__, ret);
		unregister_netdev(ndev);
		free_netdev(ndev);

		return NULL;
	}

	return priv;
}

/**
 * isve_pltfr_probe: main probe function.
 * @pdev: platform device pointer
 * Description: platform_device probe function. It allocates
 * the necessary resources and invokes the main to init
 * the net device, register the mdio bus etc.
 * It gets the rResources for Downstream and Upstream where:
 * Resource 0 is for dfwd and resource 1 is for upiim.
 * It also registers two differents interrupt sources for managing the
 * reception and the transmission queues.
 */
static int isve_pltfr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res, *res1;
	struct isve_priv *priv = NULL;
	void __iomem *addr_dfwd = NULL;
	void __iomem *addr_upiim = NULL;
	struct plat_isve_data *plat_dat = NULL;
	struct device *dev = &pdev->dev;

	/* Get Resources for Downstream and Upstream.
	 * Resource 0 is for dfwd and resource 1 is for  upiim.
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	DBG("%s: get dfwd resource: res->start 0x%x\n", __func__, res->start);
	addr_dfwd = devm_request_and_ioremap(dev, res);
	if (!addr_dfwd) {
		pr_err("%s: ERROR: dfwd memory mapping failed", __func__);
		release_mem_region(res->start, resource_size(res));
		return -ENOMEM;
	}

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1)
		return -ENODEV;

	DBG("%s: get upiim resource: res->start 0x%x\n", __func__, res1->start);
	addr_upiim = devm_request_and_ioremap(dev, res1);
	if (!addr_upiim) {
		pr_err("%s: ERROR: upiim memory mapping failed", __func__);
		release_mem_region(res->start, resource_size(res));
		release_mem_region(res1->start, resource_size(res1));
		return -ENOMEM;
	}

	plat_dat = dev->platform_data;

	priv = isve_dvr_probe(dev, plat_dat);
	if (!priv) {
		pr_err("%s: main driver probe failed", __func__);
		return -ENODEV;
	}

	priv->ioaddr_dfwd = addr_dfwd;
	priv->ioaddr_upiim = addr_upiim;

	priv->irq_ds = platform_get_irq_byname(pdev, "isveirq_ds");
	if (priv->irq_ds == -ENXIO) {
		pr_err("%s: ERROR: DFWD IRQ configuration not found\n",
		       __func__);
		return -ENXIO;
	}
	priv->irq_us = platform_get_irq_byname(pdev, "isveirq_us");
	if (priv->irq_ds == -ENXIO) {
		pr_err("%s: ERROR: UPIIM IRQ configuration not found\n",
		       __func__);
		return -ENXIO;
	}

	platform_set_drvdata(pdev, priv->dev);

	/* Init main private resources: queue numbers and sizes. */
	priv->downstream_queue_size = plat_dat->downstream_queue_size;
	priv->upstream_queue_size = plat_dat->upstream_queue_size;
	priv->skip_hdr = plat_dat->skip_hdr;
	priv->hw_rem_hdr = plat_dat->hw_rem_hdr;

	/* Init Upstream/Downstream modules */
	priv->dfwd = isve_dfwd_core(priv->ioaddr_dfwd);
	priv->upiim = isve_upiim_core(priv->ioaddr_upiim);

	if ((!priv->dfwd) || (!priv->upiim)) {
		pr_err("isve: hard error allocating HW ops\n");
		return -ENOMEM;
	}

	pr_info("ISVE (%s):\n\tioaddr_dfwd: 0x%p, ioaddr_upiim: 0x%p\n"
		"\tIrq_ds %d, Irq_us %d, queue #%d\n"
		"\tOutput queue size: %d, Input queue size: %d\n",
		priv->dev->name, priv->ioaddr_dfwd, priv->ioaddr_upiim,
		priv->irq_ds, priv->irq_us, plat_dat->queue_number,
		priv->downstream_queue_size, priv->upstream_queue_size);

	DBG("isve: init and allocate the resources...\n");
	ret = isve_alloc_resources(priv);

	return ret;
}

/**
 * isve_pltfr_remove: remove driver function
 * @pdev: platform device pointer
 * Description: this function resets the TX/RX processes and release the
 * resources used.
 */
static int isve_pltfr_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct isve_priv *priv = netdev_priv(ndev);

	isve_release_resources(priv);

	unregister_netdev(ndev);
	free_netdev(ndev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver isve_driver = {
	.probe = isve_pltfr_probe,
	.remove = isve_pltfr_remove,
	.driver = {
		   .name = ISVE_RESOURCE_NAME,
		   .owner = THIS_MODULE,
		   },
};

module_platform_driver(isve_driver);

#ifndef MODULE
static int __init isve_cmdline_opt(char *str)
{
	char *opt;

	if (!str || !*str)
		return -EINVAL;

	while ((opt = strsep(&str, ",")) != NULL) {
		if (!strncmp(opt, "debug:", 6)) {
			if (kstrtoul(opt + 6, 0, (unsigned long *)&debug))
				goto err;
		} else if (!strncmp(opt, "watchdog:", 9)) {
			if (kstrtoul(opt + 9, 0, (unsigned long *)&watchdog))
				goto err;

		}
	}
	return 0;

err:
	pr_err("%s: ERROR broken module parameter conversion", __func__);
	return -EINVAL;
}

__setup("isve=", isve_cmdline_opt);
#endif /* MODULE */

MODULE_DESCRIPTION("Integrated SoC Virtual Ethernet driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
