/**************************************************************************

  ST  Fastpath Interface driver
  Copyright(c) 2011 - 2012 ST Microelectronics Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Manish Rathi <manish.rathi@st.com>

TBD
- Multicast Support (All MULTICAST)
- Promiscuous mode
- Watchdog
- VLAN offload
- Scatter Gather
- TSO
- Testing on SMP and optimizations
- Flow control support
- ndo_poll_controller callback
- ioctl for startup queues
- PHY interrupt handling
- Power Management
- Scheduling NAPI based on queue size
- Change MTU for shared channel
**************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/skbuff.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/stmfp.h>
#include <linux/stm/pad.h>
#include <linux/io.h>
#include "stmfp_main.h"

static const u32 default_msg_level = (NETIF_MSG_LINK |
				      NETIF_MSG_IFUP | NETIF_MSG_IFDOWN |
				      NETIF_MSG_TIMER);

static int debug = -1;

enum IFBITMAP {
	DEST_DOCSIS = 1 << 0,
	DEST_GIGE = 1 << 1,
	DEST_ISIS = 1 << 2,
	DEST_APDMA = 1 << 3,
	DEST_NPDMA = 1 << 4,
	DEST_WDMA = 1 << 5,
	DEST_RECIRC = 1 << 6
};

enum IF_SP {
	SP_DOCSIS = 0,
	SP_GIGE = 1,
	SP_ISIS = 2,
	SP_SWDEF = 3
};

static struct fp_qos_queue fp_qos_queue_info[NUM_QOS_QUEUES] = {
	{256, 36, 255, 255, 36},/* DOCSIS QoS Queue 0 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue 1 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue2 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue3 */
	{256, 36, 255, 255, 36},/* GIGE QoS Queue0 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue1 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue2 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue3 */
	{32, 32, 32, 32, 6},	/* ISIS QoS Queue */
	{32, 32, 32, 32, 6},	/* AP QoS Queue */
	{32, 32, 32, 32, 6},	/* NP QoS Queue */
	{32, 32, 32, 32, 6},	/* WIFI QoS Queue */
	{32, 32, 32, 32, 6},	/* RECIRC QoS Queue */
};


static inline void fpif_write_reg(void __iomem *fp_reg, u32 val)
{
	fpdbg2("Writing reg=%p val=%x\n", fp_reg, val);
	writel(val, fp_reg);
}


static inline struct sk_buff *fpif_poll_start_skb(struct fpif_priv *priv,
						  gfp_t mask)
{
	struct sk_buff *skb = NULL;
	struct net_device *netdev = priv->netdev;

	skb = __netdev_alloc_skb(netdev, priv->rx_buffer_size +
			RXBUF_ALIGNMENT, mask);
	if (skb)
		skb_reserve(skb, RXBUF_ALIGNMENT -
			(((unsigned long)skb->data) &
			 (RXBUF_ALIGNMENT - 1)));

	return skb;
}


static void fpif_rxb_release(struct fpif_priv *priv)
{
	unsigned int i;
	struct device *devptr = priv->devptr;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;

	for (i = 0; i < FPIF_RX_RING_SIZE; i++) {
		if (rxdma_ptr->fp_rx_skbuff[i].skb) {
			dma_unmap_single(devptr,
				rxdma_ptr->fp_rx_skbuff[i].dma_ptr,
				priv->rx_buffer_size, DMA_FROM_DEVICE);
			dev_kfree_skb_any(rxdma_ptr->fp_rx_skbuff[i].skb);
			rxdma_ptr->fp_rx_skbuff[i].skb = NULL;
			rxdma_ptr->fp_rx_skbuff[i].dma_ptr = 0;
		}
	}
}


static void fpif_txb_release(struct fpif_priv *priv)
{
	unsigned int i;
	struct device *devptr = priv->devptr;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;

	for (i = 0; i < FPIF_TX_RING_SIZE; i++) {
		if (txdma_ptr->fp_tx_skbuff[i].skb) {
			dma_unmap_single(devptr,
				txdma_ptr->fp_tx_skbuff[i].dma_ptr,
				(txdma_ptr->fp_tx_skbuff[i].skb)->len,
						DMA_TO_DEVICE);
			dev_kfree_skb_any(txdma_ptr->fp_tx_skbuff[i].skb);

			txdma_ptr->fp_tx_skbuff[i].skb = NULL;
			txdma_ptr->fp_tx_skbuff[i].dma_ptr = 0;
			txdma_ptr->fp_tx_skbuff[i].skb_data = NULL;
			txdma_ptr->fp_tx_skbuff[i].len_eop = 0;
			txdma_ptr->fp_tx_skbuff[i].priv = NULL;
		}
	}
}


static inline void fpif_q_rx_buffer(struct fpif_rxdma *rxdma_ptr,
			struct sk_buff *skb, dma_addr_t buf_ptr)
{
	void __iomem *hw_buf_ptr;
	u32 head_rx = rxdma_ptr->head_rx;

	hw_buf_ptr = &rxdma_ptr->bufptr[head_rx];
	fpif_write_reg(hw_buf_ptr, buf_ptr);
	rxdma_ptr->fp_rx_skbuff[head_rx].skb = skb;
	rxdma_ptr->fp_rx_skbuff[head_rx].dma_ptr = buf_ptr;
	rxdma_ptr->head_rx = (head_rx + 1) & RX_RING_MOD_MASK;
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_cpu,
		       rxdma_ptr->head_rx);
}


static int fpif_rxb_setup(struct fpif_priv *priv)
{
	unsigned int i;
	int ret = 0;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct device *devptr = priv->devptr;

	/* Setup the skbuff rings */
	for (i = 0; i < FPIF_RX_BUFS - 1; i++) {
		skb = fpif_poll_start_skb(priv, GFP_KERNEL);
		if (NULL == skb) {
			netdev_err(priv->netdev, "allocating RX buffer\n");
			if (i == 0) {
				fpif_rxb_release(priv);
				return -ENOMEM;
			} else {
				break;
			}
		} else {
			dma_addr = dma_map_single(devptr, skb->data,
						  priv->rx_buffer_size,
						  DMA_FROM_DEVICE);
			fpif_q_rx_buffer(priv->rxdma_ptr, skb, dma_addr);
		}
	}
	return ret;
}


static void fp_txdma_setup(struct fpif_priv *priv)
{
	u32 current_tx;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int tx_ch = priv->tx_dma_ch;

	set_bit(priv->id, &fpgrp->txch_ifmap[tx_ch]);
	atomic_inc(&txdma_ptr->users);
	if (atomic_read(&txdma_ptr->users) > 1)
		return;

	/* TX DMA Init */
	txdma_ptr->tx_ch_reg = &fpgrp->txbase->per_ch[tx_ch];
	txdma_ptr->ch = tx_ch;
	txdma_ptr->bufptr = fpgrp->txbase->buf[tx_ch];
	txdma_ptr->head_tx = 0;
	txdma_ptr->last_tx = 0;
	spin_lock_init(&txdma_ptr->fpif_txlock);
	spin_lock_init(&txdma_ptr->txcollect);
	atomic_set(&txdma_ptr->pending_tx, 0);
	fpif_write_reg(&txdma_ptr->txbase->tx_bpai_clear, 1 << tx_ch);
	current_tx = readl(&txdma_ptr->txbase->tx_irq_enables[0]);
	current_tx |= 1 << tx_ch;
	fpif_write_reg(&txdma_ptr->txbase->tx_irq_enables[0], current_tx);
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTR);
}

static void fp_txdma_release(struct fpif_priv *priv)
{
	u32 current_tx;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int tx_ch = priv->tx_dma_ch;

	clear_bit(priv->id, &fpgrp->txch_ifmap[tx_ch]);
	if (!atomic_dec_and_test(&txdma_ptr->users))
		return;
	fpif_write_reg(&txdma_ptr->txbase->tx_bpai_clear, 1 << tx_ch);
	current_tx = readl(&txdma_ptr->txbase->tx_irq_enables[0]);
	current_tx &= ~(1 << tx_ch);
	fpif_write_reg(&txdma_ptr->txbase->tx_irq_enables[0], current_tx);
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTR);
	txdma_ptr->head_tx = 0;
	txdma_ptr->last_tx = 0;
	fpif_txb_release(priv);
}

static int fp_rxdma_setup(struct fpif_priv *priv)
{
	u32 current_rx;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int err, rx_ch = priv->rx_dma_ch;

	set_bit(priv->id, &fpgrp->rxch_ifmap[rx_ch]);
	atomic_inc(&rxdma_ptr->users);
	if (atomic_read(&rxdma_ptr->users) > 1)
		return 0;
	/* RX DMA Init */
	rxdma_ptr->rx_ch_reg = &fpgrp->rxbase->per_ch[rx_ch];
	rxdma_ptr->ch = rx_ch;
	rxdma_ptr->bufptr = fpgrp->rxbase->buf[rx_ch];
	rxdma_ptr->head_rx = 0;
	rxdma_ptr->last_rx = 0;
	fpif_write_reg(&rxdma_ptr->rxbase->rx_bpai_clear, 1 << rx_ch);
	current_rx = readl(&rxdma_ptr->rxbase->rx_irq_enables[0]);
	current_rx |= 1 << rx_ch;
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_enables[0], current_rx);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_delay, DELAY_RX_INTR);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_thresh, RXDMA_THRESH);
	set_bit(rx_ch, &fpgrp->set_intr);
	set_bit(rx_ch, &fpgrp->active_if);
	err = fpif_rxb_setup(priv);
	return err;
}

static void fp_rxdma_release(struct fpif_priv *priv)
{
	u32 current_rx;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int rx_ch = priv->rx_dma_ch;

	clear_bit(priv->id, &fpgrp->rxch_ifmap[rx_ch]);
	if (!atomic_dec_and_test(&rxdma_ptr->users))
		return;
	fpif_write_reg(&rxdma_ptr->rxbase->rx_bpai_clear, 1 << rx_ch);
	current_rx = readl(&rxdma_ptr->rxbase->rx_irq_enables[0]);
		current_rx &= ~(1 << rx_ch);
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_enables[0], current_rx);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_delay, DELAY_RX_INTR);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_thresh, RXDMA_THRESH);
	rxdma_ptr->head_rx = 0;
	rxdma_ptr->last_rx = 0;
	clear_bit(rx_ch, &fpgrp->set_intr);
	clear_bit(rx_ch, &fpgrp->active_if);
	fpif_rxb_release(priv);
}

static void fp_hwinit(struct fpif_grp *fpgrp)
{
	int start_q, size_q, idx;

	if (fpgrp->plat->platinit)
		fpgrp->plat->platinit(fpgrp);

	/* FP Hardware Init */
	fpif_write_reg(fpgrp->base + FILT_BADF, 0x1f1e);
	fpif_write_reg(fpgrp->base + FILT_BADF_DROP, 0x1f1e);
	fpif_write_reg(fpgrp->base + FP_MISC, 0x20);

	fpif_write_reg(fpgrp->base + RGMII_MACINFO0, 0x06c46800);
	fpif_write_reg(fpgrp->base + RGMII_RX_STAT_RESET, 0);
	fpif_write_reg(fpgrp->base + RGMII_TX_STAT_RESET, 0);
	/*
	   fpif_write_reg(fpgrp->base + RGMII_GLOBAL_MACINFO3, 0x5f6);
	 */
	fpif_write_reg(fpgrp->base + RGMII_GLOBAL_MACINFO3,
		       ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN * 2
		       + ETH_FCS_LEN);

	fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_RATE_CONTROL,
		       0x00010008);
	fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_TOE_RATE_CONTROL,
		       0x00010008);

	/* QManager Qos Queue Setup */
	start_q = 0;
	for (idx = 0; idx < NUM_QOS_QUEUES; idx++) {
		size_q = fp_qos_queue_info[idx].q_size;
		fpif_write_reg(fpgrp->base + QOS_Q_START_PTR +
			       idx * QOS_Q_RPT_OFFSET, start_q);
		fpif_write_reg(fpgrp->base + QOS_Q_END_PTR +
			idx * QOS_Q_RPT_OFFSET, start_q + size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_CONTROL +
			       idx * QOS_Q_RPT_OFFSET, 0x00000003);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_0 +
			       idx * QOS_Q_RPT_OFFSET,
			       fp_qos_queue_info[idx].threshold_0);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_1 +
			       idx * QOS_Q_RPT_OFFSET,
			       fp_qos_queue_info[idx].threshold_1);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_2 +
			       idx * QOS_Q_RPT_OFFSET,
			       fp_qos_queue_info[idx].threshold_2);
		fpif_write_reg(fpgrp->base + QOS_Q_DROP_ENTRY_LIMIT +
			       idx * QOS_Q_RPT_OFFSET, size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_BUFF_RSRV +
			       idx * QOS_Q_RPT_OFFSET,
			       fp_qos_queue_info[idx].buf_rsvd);

		fpif_write_reg(fpgrp->base + QOS_Q_CLEAR_STATS +
			       idx * QOS_Q_RPT_OFFSET, 0);
		start_q = start_q + size_q;
	}

	/* Queue Manager common count setup */
	fpif_write_reg(fpgrp->base + QOS_Q_COMMON_CNT_THRESH, 60);
	fpif_write_reg(fpgrp->base + QOS_Q_COMMON_CNT_EMPTY_COUNT, 6);

	/* Session Startup Queues */
	for (idx = 0; idx < NUM_STARTUP_QUEUES; idx++) {
		fpif_write_reg(fpgrp->base + SU_Q_BUSY +
			       idx * STARTUP_Q_RPT_OFF, 0x00000000);
	}

	/* Session Startup Queue Control */
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_PACKET_RESERVE,
		       0x00000040);
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_BUFFER_RESERVE,
		       0x00000080);
	fpif_write_reg(fpgrp->base + SU_Q_PACKET_RESERVE, 0x00000010);
	fpif_write_reg(fpgrp->base + SU_Q_BUFFER_RESERVE, 0x00000020);

	/* Interface Settings */
	for (idx = 0; idx < NUM_PORTS; idx++) {
		fpif_write_reg(fpgrp->base + FP_PORTSETTINGS_LO +
			       idx * PORT_SETTINGS_RPT_OFF, 0xF0070011);
		fpif_write_reg(fpgrp->base + FP_PORTSETTINGS_HI +
			       idx * PORT_SETTINGS_RPT_OFF,
			       ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN * 2);
	}

	/* QoS label level settings */
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		0 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 3 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		1 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 3 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		2 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 2 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		3 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 2 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		4 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 1 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		5 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 1 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		6 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 0 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		7 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 0 << 1 | 0 << 0);

	/* DOCSIS SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       0 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       1 << 28 | 1000 << 16 | FP_CLK_RATE);

	/* GIGE SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       1 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       1 << 28 | 1000 << 16 | FP_CLK_RATE);

	/* EMUX thresholds */
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       0 * EMUX_THRESHOLD_RPT_OFF, 0x00000180);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       1 * EMUX_THRESHOLD_RPT_OFF, 0x00000180);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       2 * EMUX_THRESHOLD_RPT_OFF, 0x00000180);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       3 * EMUX_THRESHOLD_RPT_OFF, 0x00000180);

	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, 0x00000002);

	fpif_write_reg(fpgrp->base + FPTXDMA_ENDIANNESS, 1);
	fpif_write_reg(fpgrp->base + FPTXDMA_T3W_CONFIG, 0xc);
	fpif_write_reg(fpgrp->base + FPTXDMA_BPAI_PRIORITY, 0x07000000);
	fpif_write_reg(fpgrp->base + FPRXDMA_T3R_CONFIG, 0x7);
	fpif_write_reg(fpgrp->base + FPRXDMA_ENDIANNESS, 1);
}


static int fpif_deinit(struct fpif_grp *fpgrp)
{
	int j;
	struct fpif_priv *priv;

	for (j = 0; j < NUM_INTFS; j++) {
		priv = netdev_priv(fpgrp->netdev[j]);
		if (NULL == priv)
			continue;
		if (priv->plat) {
			if ((priv->plat->mdio_enabled) && (priv->mii))
				fpif_mdio_unregister(priv->netdev);
			if (priv->plat->exit)
				priv->plat->exit(priv->plat);
		}
		if (priv->netdev->reg_state == NETREG_REGISTERED)
			unregister_netdev(priv->netdev);
		free_netdev(priv->netdev);
	}
	return 0;
}


static inline int fpif_q_tx_buffer(struct fpif_txdma *txdma_ptr,
				   struct fp_tx_ring *tx_ring_ptr)
{
	void __iomem *loptr, *hiptr;
	int remain;
	u32 head_tx = txdma_ptr->head_tx;
	u32 last_tx = txdma_ptr->last_tx;
	struct tx_buf *bufptr = txdma_ptr->bufptr;

	remain = (head_tx + 1 + FPIF_TX_RING_SIZE - last_tx)
					& TX_RING_MOD_MASK;
	if (!remain) {
		pr_warn("TX buffers are FULL for channel %d\n",
			txdma_ptr->ch);
		return NETDEV_TX_BUSY;
	}
	loptr = (&bufptr[head_tx].lo);
	hiptr = (&bufptr[head_tx].hi);
	fpif_write_reg(loptr, tx_ring_ptr->dma_ptr);
	fpif_write_reg(hiptr, tx_ring_ptr->len_eop);
	txdma_ptr->fp_tx_skbuff[head_tx] = *tx_ring_ptr;
	head_tx = (head_tx + 1) & TX_RING_MOD_MASK;
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_cpu, head_tx);
	txdma_ptr->head_tx = head_tx;
	atomic_inc(&txdma_ptr->pending_tx);

	return NETDEV_TX_OK;
}


static inline void fpif_fill_fphdr(struct fp_hdr *fphdr,
			int skblen, struct fpif_priv *priv)
{
	u16 temp;
	u16 len;
	u32 dmap = priv->dmap;

	fphdr->word0 =
	    ntohl(((SP_SWDEF << FPHDR_SP_SHIFT) & FPHDR_SP_MASK) |
			    FPHDR_CSUM_MASK |
		  FPHDR_BRIDGE_MASK | (dmap & FPHDR_DEST_MASK));
	fphdr->word1 = ntohl(FPHDR_MANGLELIST_MASK);
	fphdr->word2 = ntohl(FPHDR_NEXTHOP_IDX_MASK | FPHDR_SMAC_IDX_MASK);
	len = skblen;
	/**
	 * Here we want to keep 2 bytes after fastpath header intact so
	 * we store this in temp variable and write in word3 with len
	 */
	temp = *(u16 *)((u8 *)fphdr + FP_HDR_SIZE);
	fphdr->word3 = ntohl((len << FPHDR_LEN_SHIFT) | htons(temp));
}

static int put_l2cam(struct fpif_priv *priv, u8 dev_addr[])
{
	u32 val;
	u32 dp = priv->dma_port;
	u32 sp = priv->sp;
	int idx, cam_sts;
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (!fpgrp->available_l2cam) {
		pr_err("ERROR : No available L2CAM entries\n");
		return -EIO;
	}

	val = (dev_addr[2] << 24) | (dev_addr[3] << 16) |
		       (dev_addr[4] << 8) | dev_addr[5];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_LOW, val);
	val = (dp << L2CAM_DP_SHIFT) | (sp << L2CAM_SP_SHIFT) |
	    (dev_addr[0] << 8) | dev_addr[1];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_HIGH, val);
	fpif_write_reg(priv->fpgrp->base + L2_CAM_CFG_COMMAND, 0x00000000);

	cam_sts = readl(priv->fpgrp->base + L2_CAM_CFG_STATUS);
	idx = (cam_sts >> L2CAM_IDX_SHIFT) & L2CAM_IDX_MASK;
	cam_sts = (cam_sts >> L2CAM_STS_SHIFT) & L2CAM_STS_MASK;
	if (cam_sts) {
		/* Return in case of Duplicate Entry */
		if (cam_sts & L2CAM_COLL_MASK) {
			return 0;
		} else {
			pr_err("ERR:adding entry to L2CAM 0x%x\n", cam_sts);
			return -EIO;
		}
	}

	priv->l2_idx[priv->l2cam_count] = idx;
	priv->l2cam_count++;
	fpgrp->available_l2cam--;

	return idx;
}


static int remove_l2cam(struct fpif_priv *priv, int idx)
{
	u32 val;
	u32 status;
	struct fpif_grp *fpgrp = priv->fpgrp;

	val = (idx << 8) | 0x01;
	fpif_write_reg(fpgrp->base + L2CAM_BASE + 0x14, val);
	status = readl(fpgrp->base + L2CAM_BASE + 0x18);
	priv->l2cam_count--;
	fpgrp->available_l2cam++;

	return 0;
}


static int remove_l2cam_if(struct fpif_priv *priv)
{
	int i;
	for (i = 0; i < priv->l2cam_count; i++)
		remove_l2cam(priv, priv->l2_idx[i]);
	return 0;
}


static int fpif_change_mtu(struct net_device *dev, int new_mtu)
{
	struct fpif_priv *priv = netdev_priv(dev);

	if (netif_running(dev)) {
		netdev_err(dev, "must be stopped for MTU change\n");
		return -EBUSY;
	}

	if ((new_mtu < MIN_ETH_FRAME_SIZE) || (new_mtu > JUMBO_FRAME_SIZE)) {
		netdev_err(dev, "Invalid MTU setting\n");
		return -EINVAL;
	}

	/**
	 * Only stop and start the controller if it isn't already
	 * stopped, and we changed something
	 */
	priv->rx_buffer_size = new_mtu + dev->hard_header_len +
						VLAN_HLEN * 2;
	dev->mtu = new_mtu;

	if (priv->sp == SP_GIGE)
		fpif_write_reg(priv->fpgrp->base + RGMII_GLOBAL_MACINFO3,
			       new_mtu + ETH_HLEN + VLAN_HLEN * 2 +
			       ETH_FCS_LEN);

	fpif_write_reg(priv->fpgrp->base + FP_PORTSETTINGS_HI +
		       priv->sp * PORT_SETTINGS_RPT_OFF,
		       new_mtu + ETH_HLEN + VLAN_HLEN * 2);

	return 0;
}


static void fpif_set_multi(struct net_device *dev)
{
	struct netdev_hw_addr *ha;
	struct fpif_priv *priv = netdev_priv(dev);
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (dev->flags & IFF_PROMISC) {
		netdev_err(dev, "Promiscuous mode is not supported\n");
		return;
	}

	if (dev->flags & IFF_ALLMULTI) {
		netdev_err(dev, "No support of all multicast mode\n");
		return;
	}

	if (netdev_mc_empty(dev))
		return;

	if (netdev_mc_count(dev) > fpgrp->available_l2cam) {
		netdev_err(dev, "netdev_mc_count (%d) > l2cam size\n",
			   netdev_mc_count(dev));
		return;
	}

	netdev_for_each_mc_addr(ha, dev) {
		put_l2cam(priv, ha->addr);
	}
}

/**
 * fpif_process_frame() -- handle one incoming packet
 */
static inline void fpif_process_frame(struct fpif_priv *priv,
				     struct sk_buff *skb)
{

	unsigned int pkt_len, src_if, dev_id;
	struct net_device *dev;
	u32 badf_flag;
	struct fp_data *buf_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;

	buf_ptr = (void *)skb->data;
	pkt_len = ntohl((buf_ptr->hdr.word3));
	pkt_len = pkt_len >> FPHDR_LEN_SHIFT;
	badf_flag = (buf_ptr->hdr.word2) & 0x4000;

	src_if = ntohl((buf_ptr->hdr.word0));
	src_if = (src_if & FPHDR_SP_MASK) >> FPHDR_SP_SHIFT;
	switch (src_if) {
	case SP_GIGE:
		dev_id = DEVID_GIGE;
		break;
	case SP_ISIS:
		dev_id = DEVID_ISIS;
		break;
	case SP_DOCSIS:
		dev_id = DEVID_DOCSIS;
		break;
	default:
		pr_err("Unknown source on dma channel %d\n", priv->rx_dma_ch);
		return;
	}
	dev = fpgrp->netdev[dev_id];
	priv = netdev_priv(dev);

	skb->dev = dev;
	dev->stats.rx_bytes += pkt_len;
	dev->stats.rx_packets++;
	skb_reserve(skb, FP_HDR_SIZE);
	skb_put(skb, pkt_len);
	dev->last_rx = jiffies;
	/* Tell the skb what kind of packet this is */
	skb->protocol = eth_type_trans(skb, dev);

	if (unlikely(badf_flag))
		pr_info("Bad frame received\n");
	else
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	/* Send the packet up the stack */
	fpdbg("To Stack:rx_prot=0x%x len=%d data_len=%d dma_ch=%d\n",
	      htons(skb->protocol), skb->len, skb->data_len,
	      priv->rx_dma_ch);
	netif_receive_skb(skb);
}


/**
 * fpif_clean_rx_ring() -- Processes each frame in the rx ring
 * until the budget/quota has been reached. Returns the number
 * of frames handled
 */
static inline int fpif_clean_rx_ring(struct fpif_priv *priv, int limit)
{
	struct sk_buff *skb;
	u32 tail_ptr, last_rx;
	int cntr = 0;
	dma_addr_t dma_addr;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;

	tail_ptr = readl(&rxdma_ptr->rx_ch_reg->rx_done);
	last_rx = rxdma_ptr->last_rx;
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_flags,
		       1 << priv->rx_dma_ch);
	while (last_rx != tail_ptr && limit--) {
		dma_addr = rxdma_ptr->fp_rx_skbuff[last_rx].dma_ptr;
		skb = rxdma_ptr->fp_rx_skbuff[last_rx].skb;
		rxdma_ptr->fp_rx_skbuff[last_rx].skb = 0;
		rxdma_ptr->fp_rx_skbuff[last_rx].dma_ptr = 0;
		cntr++;
		dma_unmap_single(priv->devptr, (dma_addr_t)dma_addr,
				 priv->rx_buffer_size, DMA_FROM_DEVICE);

		fpif_process_frame(priv, skb);
		skb = fpif_poll_start_skb(priv, GFP_ATOMIC);
		if (NULL == skb) {
			netdev_err(priv->netdev, "skb is not free\n");
			break;
		}
		dma_addr =
		    dma_map_single(priv->devptr, skb->data,
				   priv->rx_buffer_size, DMA_FROM_DEVICE);
		fpif_q_rx_buffer(rxdma_ptr, skb, dma_addr);
		last_rx = (last_rx + 1) & RX_RING_MOD_MASK;
		/* clear the rx interrupt */
		if (last_rx == tail_ptr)
			fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_flags,
				       1 << priv->rx_dma_ch);
		tail_ptr = readl(&rxdma_ptr->rx_ch_reg->rx_done);
	}
	rxdma_ptr->last_rx = last_rx;

	return cntr;
}

/**
 * fpif_clean_tx_ring() -- Processes each frame in the tx ring
 *   until the work limit has been reached. Returns the number
 *   of frames handled
 */
static int fpif_clean_tx_ring(struct fpif_priv *priv, int tx_work_limit)
{
	int howmany = 0;
	u32 tail_ptr, last_tx;
	struct net_device *netdev;
	struct fp_data *buf_ptr;
	int len, eop;
	dma_addr_t dma_addr;
	struct sk_buff *skb;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;

	if (!spin_trylock(&txdma_ptr->txcollect)) {
		pr_err("Tx already locked for cleanup\n");
		return 0;
	}
	tail_ptr = readl(&txdma_ptr->tx_ch_reg->tx_done);
	last_tx = txdma_ptr->last_tx;
	while (last_tx != tail_ptr && tx_work_limit--) {
		skb = txdma_ptr->fp_tx_skbuff[last_tx].skb;
		dma_addr = txdma_ptr->fp_tx_skbuff[last_tx].dma_ptr;
		buf_ptr = txdma_ptr->fp_tx_skbuff[last_tx].skb_data;
		len = txdma_ptr->fp_tx_skbuff[last_tx].len_eop & 0xffff;
		eop = txdma_ptr->fp_tx_skbuff[last_tx].len_eop >> 16;
		priv = txdma_ptr->fp_tx_skbuff[last_tx].priv;
		netdev = priv->netdev;
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += len - FP_HDR_SIZE;
		if (skb == NULL)
			dma_free_coherent(priv->devptr, FP_HDR_SIZE,
					  buf_ptr, dma_addr);
		else
			dma_unmap_single(priv->devptr,
				(dma_addr_t) dma_addr, len, DMA_TO_DEVICE);
		atomic_dec(&txdma_ptr->pending_tx);
		if (eop)
			dev_kfree_skb_any(skb);
		txdma_ptr->fp_tx_skbuff[last_tx].skb = 0;
		txdma_ptr->fp_tx_skbuff[last_tx].skb_data = 0;
		last_tx = (last_tx + 1) & TX_RING_MOD_MASK;
		howmany++;
		/* clear the tx interrupt */
		if (last_tx == tail_ptr)
			fpif_write_reg(&txdma_ptr->txbase->tx_irq_flags,
				       1 << priv->tx_dma_ch);
		tail_ptr = readl(&txdma_ptr->tx_ch_reg->tx_done);
	}
	txdma_ptr->last_tx = last_tx;
	spin_unlock(&txdma_ptr->txcollect);

	return howmany;
}


static int check_napi_sched(struct fpif_grp *fpgrp)
{
	struct fpif_priv *priv;
	int i, j, ret = 0;
	u32 rx_irq_flags, tx_irq_flags;
	unsigned long irq_flags;

	rx_irq_flags = readl(&fpgrp->rxbase->rx_irq_flags);
	irq_flags = rx_irq_flags & fpgrp->set_intr;

	while (irq_flags) {
		i = find_first_bit(&irq_flags, NUM_INTFS);
		j = find_first_bit(&fpgrp->rxch_ifmap[i], NUM_INTFS);
		priv = netdev_priv(fpgrp->netdev[j]);
		if (napi_schedule_prep(&priv->napi)) {
			__napi_schedule(&priv->napi);
			clear_bit(i, &fpgrp->set_intr);
			ret |= 1 << i;
			irq_flags &= ~(1 << i);
			fpif_write_reg(&fpgrp->rxbase->rx_irq_flags, 1 << i);
		} else {
			pr_err("Could not schedule napi %d\n", j);
			fpif_write_reg(&fpgrp->rxbase->rx_irq_flags, 1 << i);
		}
	}
	if (ret)
		return ret;

	tx_irq_flags = readl(&fpgrp->txbase->tx_irq_flags);
	irq_flags = tx_irq_flags & fpgrp->set_intr;
	while (irq_flags) {
		i = find_first_bit(&irq_flags, NUM_INTFS);
		j = find_first_bit(&fpgrp->txch_ifmap[i], NUM_INTFS);
		priv = netdev_priv(fpgrp->netdev[j]);
		fpdbg2("Checking napi for id=%d txch=%d\n", j, i);
		if (napi_schedule_prep(&priv->napi)) {
			fpdbg2("sched napi for id=%d txch=%d\n", j, i);
			__napi_schedule(&priv->napi);
			clear_bit(i, &fpgrp->set_intr);
			ret |= 1 << i;
			irq_flags &= ~(1 << i);
			fpif_write_reg(&fpgrp->txbase->tx_irq_flags, 1 << i);
		}
	}
	return ret;
}


static int fpif_poll(struct napi_struct *napi, int budget)
{
	struct fpif_priv *priv = container_of(napi,
			struct fpif_priv, napi);
	struct fpif_grp *fpgrp = priv->fpgrp;
	struct fp_rxdma_regs *rxbase = priv->rxdma_ptr->rxbase;
	struct fp_txdma_regs *txbase = priv->txdma_ptr->txbase;
	int howmany_rx;
	unsigned long flags, active_if;

	fpdbg2("%s\n", __func__);
	howmany_rx = fpif_clean_rx_ring(priv, budget);
	fpif_clean_tx_ring(priv, FP_TX_FREE_BUDGET);
	check_napi_sched(fpgrp);

	if (howmany_rx < budget) {
		fpdbg2("napi_complete for id=%d\n", priv->id);
		napi_complete(&priv->napi);
		set_bit(priv->rx_dma_ch, &fpgrp->set_intr);
		if (fpgrp->set_intr == fpgrp->active_if) {
			fpdbg2("Enable interrupts\n");
			active_if = fpgrp->active_if;
			spin_lock_irqsave(&fpgrp->sched_lock, flags);
			fpif_write_reg(&rxbase->rx_irq_enables[0], active_if);
			fpif_write_reg(&txbase->tx_irq_enables[0], active_if);
			spin_unlock_irqrestore(&fpgrp->sched_lock, flags);
		}
	}
	fpdbg2("%s:%d rx pkts processed in this napi cycle\n",
	       priv->netdev->name, howmany_rx);

	return howmany_rx;
}



/**
 * fpif_intr - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network platform interface device structure
 **/
static irqreturn_t fpif_intr(int irq, void *data)
{
	struct fpif_grp *fpgrp = data;

	if (fpgrp->plat->preirq)
		fpgrp->plat->preirq(fpgrp);

	fpdbg2("%s\n", __func__);
	fpif_write_reg(&fpgrp->rxbase->rx_irq_enables[0], 0);
	fpif_write_reg(&fpgrp->txbase->tx_irq_enables[0], 0);
	check_napi_sched(fpgrp);

	if (fpgrp->plat->postirq)
		fpgrp->plat->postirq(fpgrp);
	return IRQ_HANDLED;
}


static int fpif_xmit_frame_sg(struct fpif_priv *priv, struct sk_buff *skb)
{
	struct device *devptr = priv->devptr;
	u32 nr_frags = skb_shinfo(skb)->nr_frags;
	struct skb_frag_struct *frag;
	struct fp_tx_ring tx_ring;
	u32 len;
	void *offset;
	dma_addr_t dma_addr;
	int f, ret = NETDEV_TX_OK, eop;

	fpdbg2("%s:starts\n", __func__);
	for (f = 0; f < nr_frags; f++) {
		if (f == (nr_frags - 1))
			eop = 1;
		else
			eop = 0;

		frag = &skb_shinfo(skb)->frags[f];
		fpdbg2("frag=%p eop=%d\n", frag, eop);
		if (!frag) {
			dev_kfree_skb(skb);
			return 1;
		}
		len = frag->size;
		offset = &frag;
		dma_addr = dma_map_single(devptr, offset, len,
					DMA_TO_DEVICE);
		fpdbg2("frag=%d len=%d dmaaddr=%x offset=%p eop=%d\n",
		       f, len, dma_addr, offset, eop);
		tx_ring.skb = skb;
		tx_ring.skb_data = offset;
		tx_ring.dma_ptr = dma_addr;
		tx_ring.len_eop = eop << 16 | len;
		tx_ring.priv = priv;
		ret = fpif_q_tx_buffer(priv->txdma_ptr, &tx_ring);
		if (ret == NETDEV_TX_BUSY)
			break;
	}
	return ret;
}

/**
 * This is called by the kernel when a frame is ready for transmission.
 * It is pointed to by the dev->hard_start_xmit function pointer
 */
static int fpif_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fp_tx_ring tx_ring;
	struct fp_hdr *fphdr;
	struct device *devptr = priv->devptr;
	int pending, eop, ret = NETDEV_TX_OK;
	dma_addr_t dma_addr, dma_fphdr;
	u32 data_len, nr_frags;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;

	fpdbg("TX:len=%d data_len=%d prot=%x id=%d ch=%d\n",
	      skb->len, skb->data_len, htons(skb->protocol), priv->id,
	      priv->tx_dma_ch);

	spin_lock(&txdma_ptr->fpif_txlock);
	nr_frags = skb_shinfo(skb)->nr_frags;
	fpdbg2("nr_frags=%d\n", nr_frags);
	if (unlikely((skb->data - FP_HDR_SIZE) < skb->head)) {
		fphdr = dma_alloc_coherent(devptr, FP_HDR_SIZE,
				&dma_fphdr, GFP_ATOMIC);
		if (fphdr == NULL) {
			netdev_err(netdev, "dma_alloc_coherent failed for fphdr\n");
			return NETDEV_TX_BUSY;
		}
		fpif_fill_fphdr(fphdr, skb->len, priv);
		tx_ring.skb = NULL;
		tx_ring.skb_data = fphdr;
		tx_ring.dma_ptr = dma_fphdr;
		tx_ring.len_eop = FP_HDR_SIZE;
		tx_ring.priv = priv;
		netdev->trans_start = jiffies;
		fpif_q_tx_buffer(priv->txdma_ptr, &tx_ring);
	} else {
		fphdr = (struct fp_hdr *)skb_push(skb, FP_HDR_SIZE);
		fpif_fill_fphdr(fphdr, skb->len - FP_HDR_SIZE, priv);
	}

	if (nr_frags == 0) {
		data_len = skb->len;
		eop = 1;
	} else {
		data_len = skb->len - skb->data_len;
		eop = 0;
	}

	dma_addr = dma_map_single(devptr, skb->data, data_len,
					DMA_TO_DEVICE);
	tx_ring.skb = skb;
	tx_ring.skb_data = skb->data;
	tx_ring.dma_ptr = dma_addr;
	tx_ring.len_eop = eop << 16 | data_len;
	tx_ring.priv = priv;
	netdev->trans_start = jiffies;
	fpif_q_tx_buffer(priv->txdma_ptr, &tx_ring);
	if (nr_frags)
		ret = fpif_xmit_frame_sg(priv, skb);
	spin_unlock(&txdma_ptr->fpif_txlock);

	pending = atomic_read(&txdma_ptr->pending_tx);
	if (pending > FP_TX_FREE_LIMIT)
		fpif_clean_tx_ring(priv, FP_TX_FREE_BUDGET);
	return ret;
}



/**
 * fpif_adjust_link
 * @dev: net device structure
 * Description: it adjusts the link parameters.
 */
static void fpif_adjust_link(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;
	unsigned long flags;
	int new_state = 0;
	u32 mac_info;

	if (unlikely(phydev == NULL))
		return;

	if ((phydev->speed != 10) && (phydev->speed != 100) &&
	    (phydev->speed != 1000)) {
		pr_warn("%s:Speed(%d) is not 10/100/1000",
					dev->name, phydev->speed);
		return;
	}

	spin_lock_irqsave(&priv->fpif_lock, flags);
	if (phydev->link) {
		mac_info = readl(priv->fpgrp->base + RGMII_MACINFO0);

		if (phydev->duplex != priv->oldduplex) {
			new_state = 1;
			mac_info &= ~(MACINFO_DUPLEX);
			if (!(phydev->duplex))
				mac_info |= (MACINFO_HALF_DUPLEX);
			else
				mac_info |= (MACINFO_FULL_DUPLEX);

			priv->oldduplex = phydev->duplex;
		}

		if (phydev->speed != priv->speed) {
			new_state = 1;
			mac_info &= ~(MACINFO_SPEED);
			switch (phydev->speed) {
			case 1000:
				mac_info |= (MACINFO_SPEED_1000);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_RATE_CONTROL,
					1 << 16 | 8);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_TOE_RATE_CONTROL,
					1 << 16 | 8);
				break;
			case 100:
				mac_info |= (MACINFO_SPEED_100);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_RATE_CONTROL,
					1 << 16 | 80);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_TOE_RATE_CONTROL,
					1 << 16 | 80);
				break;
			case 10:
				mac_info |= (MACINFO_SPEED_10);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_RATE_CONTROL,
					1 << 16 | 800);
				fpif_write_reg(priv->fpgrp->base +
					FP_IMUX_TXDMA_TOE_RATE_CONTROL,
					1 << 16 | 800);
				break;
			default:
				pr_warn("%s:Speed(%d) is not 10/100/1000",
					dev->name, phydev->speed);
				break;
			}

			priv->speed = phydev->speed;
		}

		if (new_state) {
			fpdbg2("Writing %x for speed %d duplex %d\n",
			       mac_info, phydev->speed, phydev->duplex);
			fpif_write_reg(priv->fpgrp->base + RGMII_MACINFO0,
				       mac_info);
		}

		if (!priv->oldlink) {
			new_state = 1;
			priv->oldlink = 1;
		}
	} else if (priv->oldlink) {
		new_state = 1;
		priv->oldlink = 0;
		priv->speed = 0;
		priv->oldduplex = -1;
	}

	if (new_state && netif_msg_link(priv))
		phy_print_status(phydev);

	spin_unlock_irqrestore(&priv->fpif_lock, flags);
}

/**
 * fpif_init_phy - PHY initialization
 * @dev: net device structure
 * Description: it initializes the driver's PHY state, and attaches the PHY
 * to the mac driver.
 *  Return value:
 *  0 on success
 */
static int fpif_init_phy(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phydev;
	char phy_id[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];
	int interface = priv->plat->interface;

	snprintf(bus_id, MII_BUS_ID_SIZE, "%x", priv->plat->bus_id);
	snprintf(phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id,
		 priv->plat->phy_addr);
	priv->oldlink = 0;
	priv->speed = 0;
	priv->oldduplex = -1;
	fpdbg("Trying to attach to %s\n", phy_id);
	phydev = phy_connect(dev, phy_id, &fpif_adjust_link, 0, interface);
	if (IS_ERR(phydev)) {
		netdev_err(dev, "Could not attach to PHY\n");
		return PTR_ERR(phydev);
	}
	fpdbg("%s:attached to PHY (UID 0x%x) Link = %d\n", dev->name,
	      phydev->phy_id, phydev->link);
	priv->phydev = phydev;

	return 0;
}


/**
 * fpif_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 */
static int fpif_open(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	int err;
	u8 bcast_macaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		random_ether_addr(netdev->dev_addr);
		pr_warning("%s: generated random MAC address %pM\n",
			   netdev->name, netdev->dev_addr);
	}

	if (priv->plat->mdio_enabled) {
		err = fpif_init_phy(netdev);
		if (unlikely(err)) {
			netdev_err(netdev, "Can't attach to PHY\n");
			return err;
		}
		if (priv->phydev)
			phy_start(priv->phydev);
	}

	pr_debug("%s:device MAC address :%p\n", priv->netdev->name,
						netdev->dev_addr);

	napi_enable(&priv->napi);
	skb_queue_head_init(&priv->rx_recycle);
	netif_start_queue(netdev);
	mutex_lock(&fpgrp->mutex);
	err = put_l2cam(priv, bcast_macaddr);
	if (err < 0) {
		netdev_err(netdev, "Unable to put in l2cam for bcast\n");
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	fpdbg2("l2_bcast_idx=%d\n", err);
	err = put_l2cam(priv, netdev->dev_addr);
	if (err < 0) {
		netdev_err(netdev, "Unable to put in l2cam\n");
		remove_l2cam_if(priv);
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	fpdbg2("l2_idx=%d\n", err);
	err = fp_rxdma_setup(priv);
	if (err) {
		netdev_err(netdev, "Unable to setup buffers\n");
		remove_l2cam_if(priv);
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	fp_txdma_setup(priv);
	mutex_unlock(&fpgrp->mutex);
	return 0;
}

/**
 * fpif_close - Disables a network interface
 * @netdev: network interface device structure
 * Returns 0, this is not allowed to fail
 * The close entry point is called when an interface is de-activated
 * by the OS.
 **/
static int fpif_close(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fpif_grp *fpgrp = priv->fpgrp;

	netif_stop_queue(netdev);
	skb_queue_purge(&priv->rx_recycle);
	napi_disable(&priv->napi);
	if (priv->phydev) {
		phy_stop(priv->phydev);
		phy_disconnect(priv->phydev);
		priv->phydev = NULL;
	}
	mutex_lock(&fpgrp->mutex);
	fp_txdma_release(priv);
	fp_rxdma_release(priv);
	remove_l2cam_if(priv);
	mutex_unlock(&fpgrp->mutex);
	return 0;
}


/**
 * fpif_get_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 **/
static struct net_device_stats *fpif_get_stats(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	int start_q, end_q, i, sum = 0, sum_tx_errors = 0;
	int rx_dma_ch = priv->rx_dma_ch;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;

	/* Ring buffer overrun */
	dev->stats.rx_over_errors =
	    readl(&rxdma_ptr->rxbase->rx_errcntr_no_buff[rx_dma_ch]);

	switch (priv->plat->iftype) {
	case DEVID_GIGE:
		start_q = GIGE_QOS_START;
		end_q = GIGE_QOS_END;
		dev->stats.tx_dropped = dev->stats.tx_packets -
		    readl(priv->fpgrp->base + RGMII_TX_CMPL_COUNT_LO);
		dev->stats.rx_errors =
		    readl(priv->fpgrp->base + RGMII_RX_ERROR_COUNT);
		dev->stats.rx_crc_errors =
		    readl(priv->fpgrp->base + RGMII_RX_FCS_ERR_CNT);
		dev->stats.rx_dropped =
		    readl(priv->fpgrp->base + RGMII_RX_BCAST_COUNT_LO) +
		    readl(priv->fpgrp->base + RGMII_RX_MCAST_COUNT_LO) +
		    readl(priv->fpgrp->base + RGMII_RX_UNICAST_COUNT_LO) -
		    dev->stats.rx_packets;
		/* total number of multicast packets received */
		dev->stats.multicast =
		    readl(priv->fpgrp->base + RGMII_RX_MCAST_COUNT_LO);

		/* Received length is unexpected */
		/* TBC:What if receive less than minimum ethernet frame */
		dev->stats.rx_length_errors =
		    readl(priv->fpgrp->base + RGMII_RX_OVERSIZED_ERR_CNT);

		dev->stats.rx_frame_errors =
		    readl(priv->fpgrp->base + RGMII_RX_ALIGN_ERR_CNT) +
		    readl(priv->fpgrp->base + RGMII_RX_SYMBOL_ERR_CNT);

		dev->stats.rx_missed_errors =
		    dev->stats.rx_errors - (dev->stats.rx_frame_errors +
					    dev->stats.rx_length_errors +
					    dev->stats.rx_over_errors +
					    dev->stats.rx_frame_errors);

		/* TBC : How to get rx collisions */
		dev->stats.collisions =
		    readl(priv->fpgrp->base + RGMII_TX_1COLL_COUNT) +
		    readl(priv->fpgrp->base + RGMII_TX_MULT_COLL_COUNT) +
		    readl(priv->fpgrp->base + RGMII_TX_LATE_COLL) +
		    readl(priv->fpgrp->base + RGMII_TX_EXCESS_COLL) +
		    readl(priv->fpgrp->base + RGMII_TX_ABORT_INTERR_COLL);
		sum_tx_errors = dev->stats.collisions;
		dev->stats.tx_aborted_errors =
		    readl(priv->fpgrp->base + RGMII_TX_ABORT_COUNT);
		sum_tx_errors += dev->stats.tx_aborted_errors;
		break;

	case DEVID_DOCSIS:
		dev->stats.rx_dropped = 0;
		dev->stats.tx_dropped = 0;
		start_q = DOCSIS_QOS_START;
		end_q = DOCSIS_QOS_END;
		break;

	case DEVID_ISIS:
		dev->stats.rx_dropped = 0;
		dev->stats.tx_dropped = 0;
		start_q = ISIS_QOS_START;
		end_q = ISIS_QOS_END;
		break;

	default:
		netdev_err(dev, "Wrong interface passed\n");
		return &dev->stats;
	}

	for (i = start_q; i <= end_q; i++) {
		sum += readl(priv->fpgrp->base + QOS_Q_DROP_COUNT +
			     QOS_Q_RPT_OFFSET * i);
	}

	sum += readl(priv->fpgrp->base + FP_EMUX_DROP_PACKET_COUNT +
		     EMUX_THRESHOLD_RPT_OFF * priv->sp);
	dev->stats.tx_fifo_errors = sum;
	if (priv->plat->iftype == DEVID_GIGE)
		sum_tx_errors +=
		    sum + readl(priv->fpgrp->base + RGMII_TX_DEFER_COUNT);
	else
		sum_tx_errors += sum;

	dev->stats.tx_errors = sum_tx_errors;

	return &dev->stats;
}

static const struct net_device_ops fpif_netdev_ops = {
	.ndo_open = fpif_open,
	.ndo_start_xmit = fpif_xmit_frame,
	.ndo_stop = fpif_close,
	.ndo_change_mtu = fpif_change_mtu,
	.ndo_set_rx_mode = fpif_set_multi,
	.ndo_get_stats = fpif_get_stats,
	.ndo_set_mac_address = eth_mac_addr,
};

static int fpif_init(struct fpif_grp *fpgrp)
{
	int err, j, rx_dma_ch, tx_dma_ch;
	struct fpif_priv *priv;
	struct net_device *netdev;

	spin_lock_init(&fpgrp->sched_lock);
	mutex_init(&fpgrp->mutex);
	fpgrp->available_l2cam = fpgrp->plat->available_l2cam;
	fpgrp->txbase = fpgrp->base + FASTPATH_TXDMA_BASE;
	fpgrp->rxbase = fpgrp->base + FASTPATH_RXDMA_BASE;
	for (j = 0; j < NUM_INTFS; j++) {
		netdev = alloc_etherdev(sizeof(struct fpif_priv));
		if (!netdev) {
			err = -ENOMEM;
			pr_err("Error in alloc_etherdev for j=%d\n", j);
			goto err_init;
		}
		fpgrp->netdev[j] = netdev;
		priv = netdev_priv(netdev);
		priv->plat = fpgrp->plat->if_data[j];
		priv->fpgrp = fpgrp;
		priv->netdev = netdev;
		priv->msg_enable = netif_msg_init(debug,
				default_msg_level);
		SET_NETDEV_DEV(netdev, fpgrp->devptr);
		priv->netdev->base_addr = (u32) fpgrp->base;
		priv->devptr = fpgrp->devptr;
		tx_dma_ch = priv->plat->tx_dma_ch;
		rx_dma_ch = priv->plat->rx_dma_ch;
		priv->tx_dma_ch = tx_dma_ch;
		priv->rx_dma_ch = rx_dma_ch;
		if (rx_dma_ch >= MAX_RXDMA) {
			netdev_err(priv->netdev, "Wrong rxch(%d) passed\n",
							rx_dma_ch);
			err = -EINVAL;
			goto err_init;
		}
		if (tx_dma_ch >= MAX_TXDMA) {
			netdev_err(priv->netdev, "Wrong txch(%d) passed\n",
							tx_dma_ch);
			err = -EINVAL;
			goto err_init;
		}
		priv->txdma_ptr = &(fpgrp->txdma_info[tx_dma_ch]);
		priv->txdma_ptr->txbase = fpgrp->txbase;
		priv->rxdma_ptr = &(fpgrp->rxdma_info[rx_dma_ch]);
		priv->rxdma_ptr->rxbase = fpgrp->rxbase;
		spin_lock_init(&priv->fpif_lock);
		/* initialize a napi context */
		netif_napi_add(netdev, &priv->napi, fpif_poll, FP_NAPI_BUDGET);
		netdev->netdev_ops = &fpif_netdev_ops;
		if (priv->plat->ethtool_enabled)
			fpif_set_ethtool_ops(netdev);
		netdev->hw_features |= NETIF_F_HW_CSUM | NETIF_F_RXCSUM;
		netdev->features |= netdev->hw_features;
		netdev->hard_header_len += FP_HDR_SIZE;
		strcpy(netdev->name, priv->plat->ifname);

		priv->id = priv->plat->iftype;
		switch (priv->id) {
		case DEVID_GIGE:
			priv->sp = SP_GIGE;
			priv->dmap = DEST_GIGE;
			break;
		case DEVID_DOCSIS:
			priv->sp = SP_DOCSIS;
			priv->dmap = DEST_DOCSIS;
			break;
		case DEVID_ISIS:
			priv->sp = SP_ISIS;
			priv->dmap = DEST_ISIS;
			break;
		default:
			netdev_err(netdev, "port type not correct\n");
			err = -EINVAL;
			goto err_init;
		}

		if (rx_dma_ch == 0)
			priv->dma_port = DEST_NPDMA;
		else if (rx_dma_ch == 1)
			priv->dma_port = DEST_APDMA;
		else
			priv->dma_port = DEST_WDMA;

		priv->rx_buffer_size =
		    netdev->mtu + netdev->hard_header_len + VLAN_HLEN * 2;
		atomic_set(&priv->txdma_ptr->users, 0);
		atomic_set(&priv->rxdma_ptr->users, 0);

		err = register_netdev(netdev);
		if (err) {
			netdev_err(netdev, "register_netdev failed %d\n", err);
			goto err_init;
		}
		if (priv->plat->init) {
			if (priv->plat->init(priv->plat) < 0) {
				netdev_err(netdev, "Error in allocation of resources\n");
				goto err_init;
			}
		}

		if (priv->plat->mdio_enabled) {
			err = fpif_mdio_register(netdev);
			if (err < 0) {
				netdev_err(netdev, "fpif_mdio_register err=%d\n",
					   err);
				goto err_init;
			}
		}
	}
	return 0;
 err_init:
	fpif_deinit(fpgrp);
	return err;
}

#ifdef CONFIG_FP_FPGA
#include <linux/pci.h>
static int __devinit fpif_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent);
static void fpif_remove(struct pci_dev *pdev);
static void fpga_platinit(void *p);
static void fpga_preirq(void *p);
static void fpga_postirq(void *p);

static struct plat_fpif_data fpif_gige_data = {
	.mdio_enabled = 1,
	.ethtool_enabled = 1,
	.id = 0,
	.iftype = DEVID_GIGE,
	.ifname = "fpgige",
	.interface = PHY_INTERFACE_MODE_RGMII_ID,
	.phy_addr = 0x1,
	.bus_id = 1,
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
	.phy_addr = 0x12,
	.bus_id = 1,
	.tx_dma_ch = 1,
	.rx_dma_ch = 1,
};

static struct plat_stmfp_data fpga_stmfp_data = {
	.available_l2cam = 128,
	.if_data[0] = &fpif_gige_data,
	.if_data[1] = &fpif_isis_data,
	.preirq = &fpga_preirq,
	.postirq = &fpga_postirq,
	.platinit = &fpga_platinit,
};

static void fpga_preirq(void *p)
{
	struct fpif_grp *fpgrp = p;
	fpdbg2("pci intr status = %x\n",
	       readl(fpgrp->base + FP_BRIDGE_IRQ_STS));
	/* Disable bridge interrupt */
	writel(0, fpgrp->base + FP_BRIDGE_IRQ_ENABLE);
}

static void fpga_postirq(void *p)
{
	struct fpif_grp *fpgrp = p;
	fpdbg2("fpga_postirq\n");
	/* Enable bridge interrupt */
	writel(1, fpgrp->base + FP_BRIDGE_IRQ_ENABLE);
}

static void fpga_platinit(void *p)
{
	struct fpif_priv *priv = p;
	/* Writing in spare reg */
	fpif_write_reg(priv->fpgrp->base + FP_SPARE_REG1, 0x100);
	fpif_write_reg(priv->fpgrp->base + FP_MDIO_CTL1, 0x02540001);
	fpif_wait_till_done(priv);
}

static int __devinit fpif_probe(struct pci_dev *pdev,
				const struct pci_device_id *ent)
{
	int err;
	struct fpif_grp *fpgrp;
	void __iomem *base;

	fpdbg("fpif_probe(fpga)\n");
	err = pci_enable_device(pdev);
	if (err) {
		pr_err("ERROR : Cannot enable PCI device err=%d\n", err);
		return err;
	}

	err = pci_resource_flags(pdev, 0);
	if (!(err & IORESOURCE_MEM)) {
		pr_err("Cannot find proper PCI device base address\n");
		err = -ENODEV;
		goto err_region;
	}

	err = pci_request_regions(pdev, DRV_NAME);
	if (err) {
		pr_err("ERROR : Can't obtain PCI resources err=%d\n", err);
		goto err_region;
	}

	err = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (err) {
		pr_err("ERROR : No usable DMA config err=%d\n", err);
		goto release_reg;
	}

	pci_set_master(pdev);
	base = pci_ioremap_bar(pdev, 0);
	if (!base) {
		pr_err("ERROR : in pci_ioremap_bar\n");
		err = -ENOMEM;
		goto release_reg;
	}
	pr_debug("probe base=%p irq=%d\n", base, pdev->irq);

	fpgrp = kzalloc(sizeof(struct fpif_grp), GFP_KERNEL);
	if (fpgrp == NULL) {
		pr_err("ERROR : Error in allocation of memory\n");
		err = -ENOMEM;
		goto err_alloc;
	}

	err = request_irq(pdev->irq, fpif_intr, 0,
			  "fastpath_tx_rx", fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate interrupt Error: %d\n",
		       err);
		err = -ENXIO;
		goto err_irq;
	}

	fpgrp->base = base;
	fpgrp->tx_irq = pdev->irq;
	fpgrp->rx_irq = pdev->irq;
	fpgrp->plat = &fpga_stmfp_data;
	fpgrp->devptr = &pdev->dev;

	err = fpif_init(fpgrp);
	if (err < 0) {
		pr_err("ERROR : fpif_init err=%d\n", err);
		goto err_fpinit;

	}

	pci_set_drvdata(pdev, fpgrp);
	pr_debug("Reading bridge fpga version registers %x %x\n",
		 readl(fpgrp->base + FPGA_BRIDGE_VER_REG1),
		 readl(fpgrp->base + FPGA_BRIDGE_VER_REG2));

	fp_hwinit(fpgrp);
	fpdbg("%s:ends\n", __func__);
	return 0;

 err_fpinit:
	free_irq(pdev->irq, fpgrp);
 err_irq:
	kfree(fpgrp);
 err_alloc:
	pci_iounmap(pdev, base);
 release_reg:
	pci_release_regions(pdev);
 err_region:
	pci_disable_device(pdev);
	return err;
}

static void fpif_remove(struct pci_dev *pdev)
{
	struct fpif_grp *fpgrp = pci_get_drvdata(pdev);
	void __iomem *base = fpgrp->base;

	fpif_deinit(fpgrp);
	free_irq(pdev->irq, fpgrp);
	kfree(fpgrp);
	pci_iounmap(pdev, base);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
}

static DEFINE_PCI_DEVICE_TABLE(fp_id_table) = {
	/* fastpath fpga device */
	{
		PCI_DEVICE(PCI_FPGA_VEND_ID, PCI_FPGA_DEV_ID)
	},
	{
		0,
	},
};

static struct pci_driver fpif_driver = {
	.name = DRV_NAME,
	.id_table = fp_id_table,
	.probe = fpif_probe,
	.remove = fpif_remove,
};

static int __init fpif_init_module(void)
{

	fpdbg("fpif_init_module\n");
	return pci_register_driver(&fpif_driver);
}

/**
 * fpif_exit_module - Driver exit Routine
 * fpif_exit_module is to alert the driver
 * that it should release a device because
 * the driver is going to be removed from
 * memory.
*/
static void __exit fpif_exit_module(void)
{
	fpdbg("fpif_exit_module\n");
	pci_unregister_driver(&fpif_driver);
}

#else
static int __devinit fpif_probe(struct platform_device *pdev)
{
	int err;
	void __iomem *base;
	struct resource *res = NULL;
	int tx_irq;
	int rx_irq;
	struct device *devptr = &pdev->dev;
	struct fpif_grp *fpgrp;

	pr_debug("%s\n", __func__);

	/* Map FastPath register memory */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "fp_mem");
	if (!res) {
		pr_err("ERROR :%s: platform_get_resource_byname ", __func__);
		return -ENODEV;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		pr_err("ERROR :%s: Couldn't ioremap regs\n", __func__);
		return -ENODEV;
	}
	pr_debug("fastpath base=%p\n", base);

	/* Get Rx interrupt number */
	rx_irq = platform_get_irq_byname(pdev, "FastPath_6");
	if (rx_irq == -ENXIO) {
		pr_err("%s: ERROR: Rx IRQ config info not found\n", __func__);
		return -ENXIO;
	}

	/* Get Tx interrupt number */
	tx_irq = platform_get_irq_byname(pdev, "FastPath_2");
	if (tx_irq == -ENXIO) {
		pr_err("%s: ERROR: Tx IRQ config info not found\n", __func__);
		return -ENXIO;
	}

	fpgrp = devm_kzalloc(&pdev->dev, sizeof(struct fpif_grp),
						GFP_KERNEL);
	if (fpgrp == NULL) {
		pr_err("ERROR : Error in allocation of memory\n");
		return -ENOMEM;
	}

	err = devm_request_irq(&pdev->dev, rx_irq, fpif_intr, 0,
					"fastpath_rx", fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate tx intr err=%d\n", err);
		return -ENXIO;
	}

	err = devm_request_irq(&pdev->dev, tx_irq, fpif_intr, 0,
				"fastpath_tx", (void *)fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate tx intr err=%d\n", err);
		return -ENXIO;
	}

	fpgrp->base = base;
	fpgrp->devptr = devptr;
	fpgrp->tx_irq = tx_irq;
	fpgrp->rx_irq = rx_irq;
	fpgrp->plat = devptr->platform_data;

	err = fpif_init(fpgrp);
	if (err < 0) {
		pr_err("ERROR: %s: err=%d\n", __func__, err);
		return err;
	}
	platform_set_drvdata(pdev, fpgrp);
	fp_hwinit(fpgrp);
	pr_debug("%s ends\n", __func__);

	return 0;
}

static int fpif_remove(struct platform_device *pdev)
{
	struct fpif_grp *fpgrp = platform_get_drvdata(pdev);

	fpif_deinit(fpgrp);
	return 0;
}

static struct platform_driver fpif_driver = {
	.probe = fpif_probe,
	.remove = fpif_remove,
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init fpif_init_module(void)
{
	fpdbg("fpif_init_module\n");
	return platform_driver_register(&fpif_driver);
}

/**
 * fpif_exit_module - Device exit Routine
 * fpif_exit_module is to alert the driver
 * that it should release a device because
 * the driver is going to be removed from
 * memory.
 **/
static void __exit fpif_exit_module(void)
{
	fpdbg("fpif_exit_module\n");
	platform_driver_unregister(&fpif_driver);
}

#endif

module_init(fpif_init_module);
module_exit(fpif_exit_module);

MODULE_DESCRIPTION("STMFP 10/100/1000 Ethernet driver");
MODULE_AUTHOR("Manish Rathi <manish.rathi@st.com>");
MODULE_LICENSE("GPL");
