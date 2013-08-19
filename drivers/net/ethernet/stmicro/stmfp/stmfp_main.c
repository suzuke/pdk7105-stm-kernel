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
- Watchdog
- VLAN offload
- TSO
- Flow control support
- ndo_poll_controller callback
- ioctl for startup queues
- Power Management
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
#include <linux/of.h>
#include <linux/of_net.h>
#include <net/netevent.h>
#include <linux/inetdevice.h>
#include <../net/bridge/br_private.h>

static const u32 default_msg_level = (NETIF_MSG_LINK |
				      NETIF_MSG_IFUP | NETIF_MSG_IFDOWN |
				      NETIF_MSG_TIMER);

static int debug = -1;

static int fpgige_phyaddr = -1;
module_param(fpgige_phyaddr, int, S_IRUGO);
MODULE_PARM_DESC(fpgige_phyaddr, "fpgige phy address");

static int fplan_phyaddr = -1;
module_param(fplan_phyaddr, int, S_IRUGO);
MODULE_PARM_DESC(fplan_phyaddr, "fplan phy address");

static int fpdocsis_phyaddr = -1;
module_param(fpdocsis_phyaddr, int, S_IRUGO);
MODULE_PARM_DESC(fpdocsis_phyaddr, "fpdocsis phy address");


enum IFBITMAP {
	DEST_DOCSIS = 1 << DEVID_DOCSIS,
	DEST_GIGE = 1 << DEVID_GIGE0,
	DEST_ISIS = 1 << DEVID_GIGE1,
	DEST_APDMA = 1 << 3,
	DEST_NPDMA = 1 << 4,
	DEST_WDMA = 1 << 5,
	DEST_RECIRC = 1 << 6
};

enum IF_SP {
	SP_DOCSIS = DEVID_DOCSIS,
	SP_GIGE = DEVID_GIGE0,
	SP_ISIS = DEVID_GIGE1,
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

static struct fp_qos_queue fpl_qos_queue_info[NUM_QOS_QUEUES] = {
	 {256, 255, 255, 255, 16},	/* DOCSIS QoS Queue 0 */
	 {256, 255, 255, 255, 16},		/* DOCSIS QoS Queue 1 */
	 {256, 255, 255, 255, 16},		/* DOCSIS QoS Queue2 */
	 {256, 255, 255, 255, 40},	/* DOCSIS QoS Queue3 */
	 {256, 255, 255, 255, 16},	/* GIGE0 QoS Queue0 */
	 {256, 255, 255, 255, 16},		/* GIGE0 QoS Queue1 */
	 {256, 255, 255, 255, 16},		/* GIGE0 QoS Queue2 */
	 {256, 255, 255, 255, 40},	/* GIGE0 QoS Queue3 */
	 {256, 255, 255, 255, 16},	/* GIGE1 QoS Queue0 */
	 {256, 255, 255, 255, 16},		/* GIGE1 QoS Queue1 */
	 {256, 255, 255, 255, 16},		/* GIGE1 QoS Queue2 */
	 {256, 255, 255, 255, 40},	/* GIGE1 QoS Queue3 */
	 {32, 31, 31, 31, 16},	/* DMA0 QoS Queue */
	 {32, 31, 31, 31, 16},	/* DMA1 QoS Queue */
	 {32, 31, 31, 31, 16},	/* RECIRC QoS Queue */
};

static int is_fpport(struct net_device *netdev)
{
	if ((!strcmp(netdev->name, "fpdocsis")) ||
	    (!strcmp(netdev->name, "fpgige0")) ||
	     (!strcmp(netdev->name, "fpgige1")))
		return 1;
	return 0;
}

#ifdef CONFIG_OF
static void stmfp_if_config_dt(struct platform_device *pdev,
				struct plat_fpif_data *plat,
				  struct device_node *node, int version)
{
	const char **phy_bus_name = (const char **)&plat->phy_bus_name;
	of_property_read_string(node, "st,phy-bus-name", phy_bus_name);
	of_property_read_u32(node, "st,phy-addr", &plat->phy_addr);
	of_property_read_u32(node, "st,phy-bus-id", &plat->bus_id);
	plat->interface = of_get_phy_mode(node);
	if (*phy_bus_name && strcmp(*phy_bus_name, "stmfp"))
		plat->mdio_bus_data = NULL;
	else
		plat->mdio_bus_data = devm_kzalloc(&pdev->dev,
					sizeof(struct stmfp_mdio_bus_data),
					GFP_KERNEL);
	plat->tso_enabled = 0;
	plat->rx_dma_ch = 0;
	plat->tx_dma_ch = 0;
	strcpy(plat->ifname, node->name);
	if (!strcmp(node->name, "fpdocsis")) {
		plat->iftype = DEVID_DOCSIS;
		plat->q_idx = 3;
		plat->buf_thr = 35;
	} else if (!strcmp(node->name, "fpgige0")) {
		plat->iftype = DEVID_GIGE0;
		plat->q_idx = 7;
		plat->buf_thr = 35;
	} else {
		plat->iftype = DEVID_GIGE1;
		if (version == FP)
			plat->q_idx = 8;
		else
			plat->q_idx = 11;
		plat->buf_thr = 35;
	}
}

static u64 stmfp_dma_mask = DMA_BIT_MASK(32);
static int stmfp_probe_config_dt(struct platform_device *pdev,
				  struct plat_stmfp_data *plat)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *node;
	int if_idx;

	if (!np)
		return -ENODEV;

	of_property_read_u32(np, "st,fp_clk_rate", &plat->fp_clk_rate);

	if (of_device_is_compatible(np, "st,fplite")) {
		plat->version = FPLITE;
		plat->available_l2cam = 128;
		plat->l2cam_size = 128;
		plat->common_cnt = 56;
		plat->empty_cnt = 16;
	} else {
		plat->version = FP;
		plat->available_l2cam = 256;
		plat->l2cam_size = 256;
		plat->common_cnt = 60;
		plat->empty_cnt = 6;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &stmfp_dma_mask;

	if_idx = 0;
	for_each_child_of_node(np, node) {
		plat->if_data[if_idx] = devm_kzalloc(&pdev->dev,
				sizeof(struct plat_fpif_data), GFP_KERNEL);
		if (plat->if_data[if_idx] == NULL)
			return -ENOMEM;
		stmfp_if_config_dt(pdev, plat->if_data[if_idx], node,
				   plat->version);
		if_idx++;
	}
	return 0;
}
#else
static int stmfp_probe_config_dt(struct platform_device *pdev,
				 struct plat_stmfp_data *plat)
{
	return -ENOSYS;
}
#endif

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
	fpif_write_reg(&txdma_ptr->txbase->tx_bpai_clear, 1 << tx_ch);
	current_tx = readl(&txdma_ptr->txbase->tx_irq_enables[0]);
	current_tx |= 1 << tx_ch;
	fpif_write_reg(&txdma_ptr->txbase->tx_irq_enables[0], current_tx);
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRH);
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
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRH);
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


static void init_tcam(struct fpif_grp *fpgrp)
{
	int idx, valid;

	for (idx = 0; idx < NUM_TCAM_ENTRIES; idx++) {
		fpif_write_reg(fpgrp->base + FP_FC_RST, 0);
		fpif_write_reg(fpgrp->base + FP_FC_CMD, TCAM_RD | idx);
		valid = readl(fpgrp->base + FP_FC_CTRL);
		valid = valid & (1 << FC_CTRL_VALID_SHIFT);
		if (valid)
			pr_err("fp:ERROR:TCAM entries already used\n");
		fpif_write_reg(fpgrp->base + FP_FC_CMD, TCAM_WR | idx);
	}
}


static void fp_hwinit(struct fpif_grp *fpgrp)
{
	int idx;
	struct fp_qos_queue *fp_qos;
	int start_q, size_q, num_q;

	if (fpgrp->plat->platinit)
		fpgrp->plat->platinit(fpgrp);

	/* FP Hardware Init */
	fpif_write_reg(fpgrp->base + FILT_BADF, PKTLEN_ERR | MALFORM_PKT |
		       EARLY_EOF | L4_CSUM_ERR | IPV4_L3_CSUM_ERR |
		       SAMEIP_SRC_DEST | IPSRC_LOOP | TTL0_ERR | IPV4_BAD_HLEN);
	fpif_write_reg(fpgrp->base + FILT_BADF_DROP, PKTLEN_ERR |
		       MALFORM_PKT | EARLY_EOF | L4_CSUM_ERR |
		       IPV4_L3_CSUM_ERR | SAMEIP_SRC_DEST |
		       IPSRC_LOOP | IPV4_BAD_HLEN);

	fpif_write_reg(fpgrp->base + FP_MISC, MISC_DEFRAG_EN | MISC_PASS_BAD);
	fpif_write_reg(fpgrp->base + FP_DEFRAG_CNTRL, DEFRAG_REPLACE |
		       DEFRAG_PAD_REMOVAL);

	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_MACINFO0,
		       MACINFO_FULL_DUPLEX | MACINFO_SPEED_1000 |
		       MACINFO_RGMII_MODE | MACINFO_DONTDECAPIQ |
		       MACINFO_MTU1 | MACINFO_FLOWCTRL_REACTION_EN);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_RX_STAT_RESET, 0);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_TX_STAT_RESET, 0);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_GLOBAL_MACINFO3,
		       ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);

	if (fpgrp->plat->version != FP) {
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_MACINFO0, MACINFO_FULL_DUPLEX
			       | MACINFO_SPEED_1000 | MACINFO_RGMII_MODE
			       | MACINFO_DONTDECAPIQ | MACINFO_MTU1
			       | MACINFO_FLOWCTRL_REACTION_EN);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_RX_STAT_RESET, 0);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_TX_STAT_RESET, 0);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_GLOBAL_MACINFO3, ETH_DATA_LEN +
			       ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);
	}

	if (fpgrp->plat->version != FPLITE)
		fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_RATE_CONTROL,
			       IMUX_TXDMA_RATE);
	fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_TOE_RATE_CONTROL,
		       IMUX_TXDMA_RATE);

	/* QManager Qos Queue Setup */
	start_q = 0;
	if (fpgrp->plat->version == FPLITE) {
		fp_qos = fpl_qos_queue_info;
		num_q = 15;
	} else {
		fp_qos = fp_qos_queue_info;
		num_q = 13;
	}
	for (idx = 0; idx < num_q; idx++) {
		size_q = fp_qos[idx].q_size;
		fpif_write_reg(fpgrp->base + QOS_Q_START_PTR +
			       idx * QOS_Q_RPT_OFFSET, start_q);
		fpif_write_reg(fpgrp->base + QOS_Q_END_PTR +
			idx * QOS_Q_RPT_OFFSET, start_q + size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_CONTROL +
			       idx * QOS_Q_RPT_OFFSET, 0x00000003);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_0 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_0);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_1 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_1);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_2 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_2);
		fpif_write_reg(fpgrp->base + QOS_Q_DROP_ENTRY_LIMIT +
			       idx * QOS_Q_RPT_OFFSET, size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_BUFF_RSRV +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].buf_rsvd);

		fpif_write_reg(fpgrp->base + QOS_Q_CLEAR_STATS +
			       idx * QOS_Q_RPT_OFFSET, 0);
		start_q = start_q + size_q;
	}

	/* Queue Manager common count setup */
	fpif_write_reg(fpgrp->base +
		       QOS_Q_COMMON_CNT_THRESH, fpgrp->plat->common_cnt);
	fpif_write_reg(fpgrp->base +
		       QOS_Q_COMMON_CNT_EMPTY_COUNT, fpgrp->plat->empty_cnt);

	/* Session Startup Queues */
	for (idx = 0; idx < NUM_STARTUP_QUEUES; idx++) {
		fpif_write_reg(fpgrp->base + SU_Q_BUSY +
			       idx * STARTUP_Q_RPT_OFF, 0);
	}

	/* Session Startup Queue Control */
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_PACKET_RESERVE,
		       SU_Q_MAX_PKT_G);
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_BUFFER_RESERVE,
		       SU_Q_MAX_BUF_G);
	fpif_write_reg(fpgrp->base + SU_Q_PACKET_RESERVE, SU_Q_MAX_PKT);
	fpif_write_reg(fpgrp->base + SU_Q_BUFFER_RESERVE, SU_Q_MAX_BUF);

	/* Interface Settings */
	for (idx = 0; idx < NUM_PORTS; idx++) {
		fpif_write_reg(fpgrp->base + FP_PORTSETTINGS_LO + idx *
			       PORT_SETTINGS_RPT_OFF, DEF_QOSNONIP |
			       DEF_QOSIP | DEF_QOS_LBL | DEF_VLAN_ID |
			       NOVLAN_HW);
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
		       DEVID_DOCSIS * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* GIGE0 SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       DEVID_GIGE0 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* GIGE1 SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       DEVID_GIGE1 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* EMUX thresholds */
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_DOCSIS * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_GIGE0 * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_GIGE1 * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	if (fpgrp->plat->version != FP) {
		fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
			       DEVID_RXDMA * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
		fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
			       DEVID_RECIRC * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	}
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, L2CAM_CLEAR);

	if (fpgrp->plat->version != FPLITE) {
		fpif_write_reg(fpgrp->base + FPTXDMA_ENDIANNESS,
			       DMA_REV_ENDIAN);
		fpif_write_reg(fpgrp->base + FPTXDMA_T3W_CONFIG,
			       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
		fpif_write_reg(fpgrp->base + FPTXDMA_BPAI_PRIORITY, BPAI_PRIO);
	} else {
		fpif_write_reg(fpgrp->base + FPTOE_ENDIANNESS, DMA_REV_ENDIAN);
		fpif_write_reg(fpgrp->base + FPTOE_T3W_CONFIG,
			       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
		fpif_write_reg(fpgrp->base + FPTOE_MAX_NONSEG,
			       DMA_MAX_NONSEG_SIZE);
	}

	fpif_write_reg(fpgrp->base + FPRXDMA_T3R_CONFIG,
		       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
	fpif_write_reg(fpgrp->base + FPRXDMA_ENDIANNESS, DMA_REV_ENDIAN);
	init_tcam(fpgrp);
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
			if (priv->plat->mdio_bus_data)
				fpif_mdio_unregister(priv->netdev);
			if (priv->plat->exit)
				priv->plat->exit(priv->plat);
		}
		if (priv->netdev->reg_state == NETREG_REGISTERED)
			unregister_netdev(priv->netdev);
		free_netdev(priv->netdev);
	}

	for (j = 0; j < fpgrp->l2cam_size; j++)
		fpgrp->l2_idx[j] = NUM_INTFS;

	return 0;
}


static inline int fpif_q_tx_buffer(struct fpif_txdma *txdma_ptr,
				   struct fp_tx_ring *tx_ring_ptr)
{
	void __iomem *loptr, *hiptr;
	u32 head_tx = txdma_ptr->head_tx;
	struct tx_buf *bufptr = txdma_ptr->bufptr;

	loptr = (&bufptr[head_tx].lo);
	hiptr = (&bufptr[head_tx].hi);
	fpif_write_reg(loptr, tx_ring_ptr->dma_ptr);
	fpif_write_reg(hiptr, tx_ring_ptr->len_eop);
	txdma_ptr->fp_tx_skbuff[head_tx] = *tx_ring_ptr;
	head_tx = (head_tx + 1) & TX_RING_MOD_MASK;
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_cpu, head_tx);
	txdma_ptr->head_tx = head_tx;

	return NETDEV_TX_OK;
}


static inline void fpif_fill_fphdr(struct fp_hdr *fphdr,
			int skblen, struct fpif_priv *priv)
{
	u16 temp;
	u16 len;
	u32 dmap = priv->dmap;
	u32 len_mask = 0;
	u32 ifidx = priv->ifidx;

	fphdr->word0 = ntohl((ifidx << FPHDR_IFIDX_SHIFT) |
			((SP_SWDEF << FPHDR_SP_SHIFT) & FPHDR_SP_MASK) |
			FPHDR_CSUM_MASK | FPHDR_BRIDGE_MASK |
			(dmap & FPHDR_DEST_MASK));
	fphdr->word1 = ntohl(FPHDR_MANGLELIST_MASK | FPHDR_DONE_MASK);
	fphdr->word2 = ntohl(FPHDR_NEXTHOP_IDX_MASK | FPHDR_SMAC_IDX_MASK);
	len = skblen;
	/**
	 * Here we want to keep 2 bytes after fastpath header intact so
	 * we store this in temp variable and write in word3 with len
	 */
	temp = *(u16 *)((u8 *)fphdr + FP_HDR_SIZE);
	if (priv->plat->tso_enabled)
		len_mask = FPHDR_TSO_LEN_MASK;
	fphdr->word3 = ntohl(len_mask | (len << FPHDR_LEN_SHIFT) |
			htons(temp));
	fpdbg("TX: hdr0=%x hdr1=%x hdr2=%x hdr3=%x\n", fphdr->word0,
	      fphdr->word1, fphdr->word2, fphdr->word3);
}


static int put_l2cam(struct fpif_priv *priv, u8 dev_addr[], int *idx)
{
	u32 val;
	u32 dp = priv->dma_port;
	u32 sp = priv->sp;
	int cam_sts;
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (!fpgrp->available_l2cam) {
		pr_err("ERROR : No available L2CAM entries\n");
		return -EIO;
	}

	fpdbg("put_l2cam(%d) %x-%x-%x-%x-%x-%x\n", priv->id, dev_addr[0],
	      dev_addr[1], dev_addr[2], dev_addr[3], dev_addr[4], dev_addr[5]);

	fpif_write_reg(fpgrp->base + L2_CAM_CFG_MODE, HW_MANAGED);
	val = (dev_addr[2] << 24) | (dev_addr[3] << 16) |
		       (dev_addr[4] << 8) | dev_addr[5];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_LOW, val);
	val = (1 << L2CAM_BRIDGE_SHIFT) | (dp << L2CAM_DP_SHIFT) |
		(sp << L2CAM_SP_SHIFT) | (dev_addr[0] << 8) | dev_addr[1];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_HIGH, val);
	fpif_write_reg(priv->fpgrp->base + L2_CAM_CFG_COMMAND, L2CAM_ADD);

	cam_sts = readl(priv->fpgrp->base + L2_CAM_CFG_STATUS);
	*idx = (cam_sts >> L2CAM_IDX_SHIFT) & L2CAM_IDX_MASK;
	fpdbg("idx=%d cam_sts=%x\n", *idx, cam_sts);
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

	fpgrp->l2_idx[*idx] = priv->id;
	fpgrp->available_l2cam--;

	return 0;
}


static int remove_l2cam(struct fpif_priv *priv, int idx)
{
	u32 val;
	u32 status;
	struct fpif_grp *fpgrp = priv->fpgrp;

	if ((idx < 0) || (idx >= fpgrp->l2cam_size)) {
		pr_err("fp:ERR:Invalid idx %d passed in remove_l2cam\n", idx);
		return IDX_INV;
	}

	fpif_write_reg(fpgrp->base + L2_CAM_CFG_MODE, SW_MANAGED);
	val = (idx << 8) | L2CAM_READ;
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, val);
	val = (idx << 8) | L2CAM_DEL;
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, val);
	status = readl(fpgrp->base + L2_CAM_CFG_STATUS);
	fpgrp->available_l2cam++;
	fpdbg("remove_l2cam(%d) %d sts=%x\n", priv->id, idx, status);

	return 0;
}

static int remove_l2cam_if(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;
	int i, cnt = fpgrp->l2cam_size;

	for (i = 0; i < cnt; i++) {
		if (fpgrp->l2_idx[i] == priv->id) {
			remove_l2cam(priv, i);
			fpgrp->l2_idx[i] = NUM_INTFS;
		}
	}
	priv->ifaddr_idx = IDX_INV;
	priv->br_l2cam_idx = IDX_INV;

	return 0;
}


static int fpif_change_mtu(struct net_device *dev, int new_mtu)
{
	struct fpif_priv *priv = netdev_priv(dev);

	if (!priv->rgmii_base)
		return -EPERM;

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

	fpif_write_reg(priv->rgmii_base + RGMII_GLOBAL_MACINFO3, new_mtu +
			ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);

	fpif_write_reg(priv->fpgrp->base + FP_PORTSETTINGS_HI +
		       priv->sp * PORT_SETTINGS_RPT_OFF,
		       new_mtu + ETH_HLEN + VLAN_HLEN * 2);

	return 0;
}


static int is_valid_tcam(struct fpif_grp *fpgrp, int idx)
{
	int valid;

	fpif_write_reg(fpgrp->base + FP_FC_RST, 0);
	fpif_write_reg(fpgrp->base + FP_FC_CMD, TCAM_RD | idx);
	valid = readl(fpgrp->base + FP_FC_CTRL) & (1 << FC_CTRL_VALID_SHIFT);

	return valid;
}


static void remove_tcam(struct fpif_grp *fpgrp, int idx)
{
	int valid;

	valid = is_valid_tcam(fpgrp, idx);
	if (!valid) {
		pr_err("fp:ERROR:TCAM idx %d already removed\n", idx);
		return;
	}
	fpdbg("Removing TCAM idx=%d\n", idx);
	fpif_write_reg(fpgrp->base + FP_FC_RST, 0);
	fpif_write_reg(fpgrp->base + FP_FC_CMD, TCAM_WR | idx);

	return;
}

static void mod_tcam(struct fpif_grp *fpgrp, struct fp_tcam_info *tcam_info,
		int idx)
{
	unsigned char *dev_addr = tcam_info->dev_addr_d;
	u32 fc_ctrl, val, sp = tcam_info->sp;

	fpdbg("mod_tcam: idx=%d\n", idx);
	if (dev_addr)
		fpdbg("mod_tcam: MAC %x %x %x %x %x %x\n", dev_addr[0],
		      dev_addr[1], dev_addr[2], dev_addr[3], dev_addr[4],
		      dev_addr[5]);

	fpif_write_reg(fpgrp->base + FP_FC_RST, 0);
	if (sp) {
		val = FC_SOURCE_SRCP_MASK | sp << FC_SOURCE_SRCP_SHIFT;
		fpif_write_reg(fpgrp->base + FP_FC_SOURCE, val);
	}
	fc_ctrl = readl(fpgrp->base + FP_FC_CTRL) | 1 << FC_CTRL_VALID_SHIFT;
	fc_ctrl = fc_ctrl | (tcam_info->dest << FC_CTRL_DST_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->bridge << FC_CTRL_BRIDGE_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->redir << FC_CTRL_REDIR_SHIFT);
	fc_ctrl = fc_ctrl | (tcam_info->cont << FC_CTRL_CONTINUE_SHIFT);
	fpif_write_reg(fpgrp->base + FP_FC_CTRL, fc_ctrl);

	if (dev_addr) {
		val = dev_addr[0] << 8 | dev_addr[1];
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_H, val);
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_MASK_H, 0xffff);
		val = (dev_addr[2] << 24) | (dev_addr[3] << 16) |
		       (dev_addr[4] << 8) | dev_addr[5];
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_L, val);
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_MASK_L, 0xffffffff);
	}

	if (tcam_info->all_multi) {
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_H, 0x100);
		fpif_write_reg(fpgrp->base + FP_FC_MAC_D_MASK_H, 0x100);
	}

	fpif_write_reg(fpgrp->base + FP_FC_CMD, TCAM_WR | idx);
}


static void add_tcam(struct fpif_grp *fpgrp, struct fp_tcam_info *tcam_info,
		     int idx)
{
	int valid;

	if (idx >= NUM_TCAM_ENTRIES)
		pr_err("fp:ERROR:Invalid TCAM idx passed\n");
	valid = is_valid_tcam(fpgrp, idx);
	if (!valid)
		mod_tcam(fpgrp, tcam_info, idx);
	else
		pr_err("fp:ERROR:idx %d is already used\n", idx);

	return;
}

static void remove_tcam_br(struct fpif_priv *priv)
{
	int idx;
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->br_tcam_idx == IDX_INV)
		return;

	idx = TCAM_PROMS_FPBR_IDX + priv->id;
	remove_tcam(fpgrp, idx);
	priv->br_tcam_idx = IDX_INV;
}


static void add_tcam_br(struct fpif_priv *priv, struct net_device *netdev)
{
	struct fp_tcam_info tcam_info;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int idx;

	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.dev_addr_d = netdev->dev_addr;
	tcam_info.sp = priv->sp;
	idx = TCAM_PROMS_FPBR_IDX + priv->id;
	add_tcam(fpgrp, &tcam_info, idx);
	priv->br_tcam_idx = idx;
	return;
}


static void remove_tcam_promisc(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->promisc_idx != TCAM_IDX_INV) {
		remove_tcam(fpgrp, TCAM_PROMS_SP_IDX + priv->id);
		remove_tcam(fpgrp, TCAM_PROMS_FP_IDX + priv->id);
		priv->promisc_idx = TCAM_IDX_INV;
	}
}


static void remove_tcam_allmulti(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->allmulti_idx != TCAM_IDX_INV) {
		remove_tcam(fpgrp, TCAM_ALLMULTI_IDX + priv->id);
		priv->allmulti_idx = TCAM_IDX_INV;
	}
}


static void add_tcam_allmulti(struct fpif_priv *priv)
{
	struct fp_tcam_info tcam_info;
	int idx;

	if (priv->allmulti_idx == TCAM_IDX_INV) {
		memset(&tcam_info, 0, sizeof(tcam_info));
		tcam_info.sp = priv->sp;
		tcam_info.bridge = 1;
		tcam_info.cont = 1;
		tcam_info.redir = 1;
		tcam_info.dest = priv->dma_port;
		tcam_info.all_multi = 1;
		idx = TCAM_ALLMULTI_IDX + priv->id;
		add_tcam(priv->fpgrp, &tcam_info, idx);
		priv->allmulti_idx = idx;
	}
}


static void add_tcam_promisc(struct fpif_priv *priv)
{
	struct net_device *netdev = priv->netdev;
	int idx;
	struct fp_tcam_info tcam_info;

	if (priv->promisc_idx == TCAM_IDX_INV) {
		memset(&tcam_info, 0, sizeof(tcam_info));
		tcam_info.sp = priv->sp;
		tcam_info.dev_addr_d = netdev->dev_addr;
		idx = TCAM_PROMS_FP_IDX + priv->id;
		add_tcam(priv->fpgrp, &tcam_info, idx);

		memset(&tcam_info, 0, sizeof(tcam_info));
		tcam_info.sp = priv->sp;
		tcam_info.bridge = 1;
		tcam_info.cont = 1;
		tcam_info.redir = 1;
		tcam_info.dest = priv->dma_port;
		idx = TCAM_PROMS_SP_IDX + priv->id;
		add_tcam(priv->fpgrp, &tcam_info, idx);
		priv->promisc_idx = idx;
	}
}


static int dp_device_event(struct notifier_block *unused, unsigned long event,
			   void *ptr)
{
	struct net_device *netdev = ptr;
	struct net_bridge *br = netdev_priv(netdev);
	int reconfig_hw, fp_bridge, bridge_up;
	struct net_bridge_port *p;
	struct fpif_priv *priv = NULL;
	int idx, err;

	fpdbg("netdev=%ld ptr=%p ifindex=%d\n", event, ptr, netdev->ifindex);
	reconfig_hw = 0;
	bridge_up = 0;
	if ((netdev->priv_flags & IFF_EBRIDGE) && (event == NETDEV_UP)) {
		reconfig_hw = 1;
		bridge_up = 1;
	}

	if ((netdev->priv_flags & IFF_EBRIDGE) && (event == NETDEV_DOWN))
		reconfig_hw = 1;

	if ((netdev->priv_flags & IFF_EBRIDGE) && (netdev->flags & IFF_UP) &&
	    (event == NETDEV_CHANGEADDR)) {
		bridge_up = 1;
		reconfig_hw = 1;
	}

	fp_bridge = 0;
	if (!reconfig_hw)
		return NOTIFY_DONE;

	list_for_each_entry(p, &br->port_list, list) {
		if (!is_fpport(p->dev))
			continue;

		fp_bridge = 1;
		priv = netdev_priv(p->dev);
		if ((priv->br_l2cam_idx != priv->ifaddr_idx) &&
		    (priv->br_l2cam_idx != IDX_INV) &&
		    (p->dev->flags & IFF_UP))
			remove_l2cam(priv, priv->br_l2cam_idx);

		priv->br_l2cam_idx = IDX_INV;
		remove_tcam_br(priv);

		if ((bridge_up) && (p->dev->flags & IFF_UP)) {
			err = put_l2cam(priv, netdev->dev_addr, &idx);
			if (err < 0) {
				pr_err("fp:ERROR in putting br mac in l2cam\n");
				priv->br_l2cam_idx = IDX_INV;
			} else {
				priv->br_l2cam_idx = idx;
				add_tcam_br(priv, netdev);
			}
		}
	}

	return NOTIFY_DONE;
}

struct notifier_block ovs_dp_device_notifier = {
	.notifier_call = dp_device_event
};

static void fpif_set_multi(struct net_device *dev)
{
	struct netdev_hw_addr *ha;
	struct fpif_priv *priv = netdev_priv(dev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	int idx;

	/* promisc and allmulti idx share the same location in TCAM */
	if (dev->flags & IFF_PROMISC) {
		add_tcam_promisc(priv);
		priv->allmulti_idx = TCAM_IDX_INV;
		return;
	}
	remove_tcam_promisc(priv);
	if (dev->flags & IFF_ALLMULTI) {
		add_tcam_allmulti(priv);
		return;
	}
	remove_tcam_allmulti(priv);
	if (netdev_mc_empty(dev))
		return;
	if (netdev_mc_count(dev) > fpgrp->available_l2cam) {
		netdev_err(dev, "netdev_mc_count (%d) > l2cam size\n",
			netdev_mc_count(dev));
		add_tcam_allmulti(priv);
		return;
	}
	netdev_for_each_mc_addr(ha, dev)
		put_l2cam(priv, ha->addr, &idx);
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
	dev_id = src_if;
	dev = fpgrp->netdev[dev_id];
	priv = netdev_priv(dev);

	skb->dev = dev;
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
		if (skb == NULL)
			dma_free_coherent(priv->devptr, FP_HDR_SIZE,
					  buf_ptr, dma_addr);
		else
			dma_unmap_single(priv->devptr,
				(dma_addr_t) dma_addr, len, DMA_TO_DEVICE);
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
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRH);
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
	int f, eop;

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
		dma_addr = skb_frag_dma_map(devptr, frag, 0, len,
					    DMA_TO_DEVICE);
		fpdbg2("frag=%d len=%d dmaaddr=%x offset=%p eop=%d\n",
		       f, len, dma_addr, offset, eop);
		tx_ring.skb = skb;
		tx_ring.skb_data = offset;
		tx_ring.dma_ptr = dma_addr;
		tx_ring.len_eop = eop << 16 | len;
		tx_ring.priv = priv;
		fpif_q_tx_buffer(priv->txdma_ptr, &tx_ring);
	}
	return 0;
}

static int check_tx_busy(struct fpif_priv *priv, int nr_pkt)
{
	int maxbuf;
	int remain;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	u32 head_tx = txdma_ptr->head_tx;
	u32 last_tx = txdma_ptr->last_tx;

	if (head_tx >= last_tx)
		remain = FPIF_TX_RING_SIZE - (head_tx - last_tx);
	else
		remain = last_tx - head_tx - 1;
	if (nr_pkt > remain) {
		pr_warn("TXQ FULL nr_frags=%d remain=%d\n", nr_pkt, remain);
		return NETDEV_TX_BUSY;
	}

	if (remain < DELAY_TX_THR)
		fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRL);

	maxbuf = readl(priv->fpgrp->base + QOS_Q_MAX_BUFFER_COUNT +
			QOS_Q_RPT_OFFSET * priv->plat->q_idx);
	if (maxbuf >= priv->plat->buf_thr) {
		writel(0, priv->fpgrp->base +
				QOS_Q_MAX_BUFFER_COUNT +
				QOS_Q_RPT_OFFSET * priv->plat->q_idx);
		return NETDEV_TX_BUSY;
	}

	return NETDEV_TX_OK;
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
	int eop, ret = NETDEV_TX_OK;
	dma_addr_t dma_addr, dma_fphdr;
	u32 data_len, nr_frags;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;

	fpdbg("TX:len=%d data_len=%d prot=%x id=%d ch=%d\n",
	      skb->len, skb->data_len, htons(skb->protocol), priv->id,
	      priv->tx_dma_ch);

	spin_lock(&txdma_ptr->fpif_txlock);
	nr_frags = skb_shinfo(skb)->nr_frags;
	fpdbg2("nr_frags=%d\n", nr_frags);

	if (check_tx_busy(priv, nr_frags + 1) == NETDEV_TX_BUSY) {
		spin_unlock(&txdma_ptr->fpif_txlock);
		return NETDEV_TX_BUSY;
	}

	if (unlikely((skb->data - FP_HDR_SIZE) < skb->head)) {
		fphdr = dma_alloc_coherent(devptr, FP_HDR_SIZE,
				&dma_fphdr, GFP_ATOMIC);
		if (fphdr == NULL) {
			netdev_err(netdev, "dma_alloc_coherent failed for fphdr\n");
			spin_unlock(&txdma_ptr->fpif_txlock);
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
	void *base = priv->rgmii_base;

	if (unlikely(phydev == NULL))
		return;

	if (unlikely(base == NULL))
		return;

	if ((phydev->speed != 10) && (phydev->speed != 100) &&
	    (phydev->speed != 1000)) {
		pr_warn("%s:Speed(%d) is not 10/100/1000",
					dev->name, phydev->speed);
		return;
	}

	spin_lock_irqsave(&priv->fpif_lock, flags);
	if (phydev->link) {
		mac_info = readl(base + RGMII_MACINFO0);

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
				break;
			case 100:
				mac_info |= (MACINFO_SPEED_100);
				break;
			case 10:
				mac_info |= (MACINFO_SPEED_10);
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
			fpif_write_reg(base + RGMII_MACINFO0, mac_info);
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

	snprintf(bus_id, MII_BUS_ID_SIZE, "%s-%x", priv->plat->phy_bus_name,
		 priv->plat->bus_id);
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
	pr_info("%s:attached to PHY (UID 0x%x) Link = %d\n", dev->name,
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
	int err, idx;
	u8 bcast_macaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u32 macinfo;

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		random_ether_addr(netdev->dev_addr);
		pr_warning("%s: generated random MAC address %pM\n",
			   netdev->name, netdev->dev_addr);
	}

	err = fpif_init_phy(netdev);
	if (unlikely(err)) {
		netdev_err(netdev, "Can't attach to PHY\n");
		return err;
	}
	if (priv->phydev)
		phy_start(priv->phydev);

	pr_debug("%s:device MAC address :%p\n", priv->netdev->name,
						netdev->dev_addr);

	napi_enable(&priv->napi);
	skb_queue_head_init(&priv->rx_recycle);
	netif_start_queue(netdev);
	mutex_lock(&fpgrp->mutex);
	err = put_l2cam(priv, bcast_macaddr, &idx);
	if (err < 0) {
		netdev_err(netdev, "Unable to put in l2cam for bcast\n");
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	fpdbg2("l2_bcast_idx=%d\n", idx);
	err = put_l2cam(priv, netdev->dev_addr, &idx);
	if (err < 0) {
		netdev_err(netdev, "Unable to put in l2cam\n");
		remove_l2cam_if(priv);
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	priv->ifaddr_idx = idx;
	fpdbg2("l2_idx=%d\n", idx);

	if (netdev->master) {
		err = put_l2cam(priv, netdev->master->dev_addr, &idx);
		if (err < 0) {
			pr_err("fp:ERROR in putting br mac in l2cam\n");
			priv->br_l2cam_idx = IDX_INV;
		} else {
			priv->br_l2cam_idx = idx;
			add_tcam_br(priv, netdev->master);
		}
	}

	err = fp_rxdma_setup(priv);
	if (err) {
		netdev_err(netdev, "Unable to setup buffers\n");
		remove_l2cam_if(priv);
		mutex_unlock(&fpgrp->mutex);
		return err;
	}
	fp_txdma_setup(priv);
	mutex_unlock(&fpgrp->mutex);
	if (priv->rgmii_base) {
		macinfo = readl(priv->rgmii_base + RGMII_MACINFO0);
		macinfo |= MACINFO_TXEN | MACINFO_RXEN;
		fpif_write_reg(priv->rgmii_base + RGMII_MACINFO0, macinfo);
	}
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
	u32 macinfo;

	if (priv->rgmii_base) {
		macinfo = readl(priv->rgmii_base + RGMII_MACINFO0);
		macinfo &= ~(MACINFO_TXEN | MACINFO_RXEN);
		fpif_write_reg(priv->rgmii_base + RGMII_MACINFO0, macinfo);
	}
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
	remove_tcam_promisc(priv);
	remove_tcam_allmulti(priv);
	remove_tcam_br(priv);
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
	int sum = 0, sum_tx_errors = 0;

	dev->stats.rx_dropped = 0;
	dev->stats.tx_dropped = 0;
	if (priv->rgmii_base) {
		dev->stats.tx_packets = readl(priv->rgmii_base +
					RGMII_TX_CMPL_COUNT_LO);
		dev->stats.tx_bytes = readl(priv->rgmii_base +
					RGMII_TX_BYTE_COUNT_LO);

		dev->stats.rx_errors =
		    readl(priv->rgmii_base + RGMII_RX_ERROR_COUNT);
		dev->stats.rx_crc_errors =
		    readl(priv->rgmii_base + RGMII_RX_FCS_ERR_CNT);

		dev->stats.rx_packets =
		    readl(priv->rgmii_base + RGMII_RX_BCAST_COUNT_LO) +
		    readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_LO) +
		    readl(priv->rgmii_base + RGMII_RX_UNICAST_COUNT_LO);

		dev->stats.rx_bytes = readl(priv->rgmii_base +
					RGMII_RX_BYTE_COUNT_LO);
		/* total number of multicast packets received */
		dev->stats.multicast =
		    readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_LO);

		/* Received length is unexpected */
		/* TBC:What if receive less than minimum ethernet frame */
		dev->stats.rx_length_errors =
		    readl(priv->rgmii_base + RGMII_RX_OVERSIZED_ERR_CNT);

		dev->stats.rx_frame_errors =
		    readl(priv->rgmii_base + RGMII_RX_ALIGN_ERR_CNT) +
		    readl(priv->rgmii_base + RGMII_RX_SYMBOL_ERR_CNT);

		dev->stats.rx_missed_errors =
		    dev->stats.rx_errors - (dev->stats.rx_frame_errors +
					    dev->stats.rx_length_errors +
					    dev->stats.rx_over_errors);

		/* TBC : How to get rx collisions */
		dev->stats.collisions =
		    readl(priv->rgmii_base + RGMII_TX_1COLL_COUNT) +
		    readl(priv->rgmii_base + RGMII_TX_MULT_COLL_COUNT) +
		    readl(priv->rgmii_base + RGMII_TX_LATE_COLL) +
		    readl(priv->rgmii_base + RGMII_TX_EXCESS_COLL) +
		    readl(priv->rgmii_base + RGMII_TX_ABORT_INTERR_COLL);
		sum_tx_errors = dev->stats.collisions;
		dev->stats.tx_aborted_errors =
		    readl(priv->rgmii_base + RGMII_TX_ABORT_COUNT);
		sum_tx_errors += dev->stats.tx_aborted_errors;
		sum_tx_errors +=
		    sum + readl(priv->rgmii_base + RGMII_TX_DEFER_COUNT);
	} else {
		dev->stats.rx_packets = readl(priv->fpgrp->base + FP_IMUX_PKTC +
				IMUX_RPT_OFF * priv->sp);
		dev->stats.rx_bytes = readl(priv->fpgrp->base + FP_IMUX_BYTEC +
				IMUX_RPT_OFF * priv->sp);
		dev->stats.tx_packets = readl(priv->fpgrp->base +
					FP_EMUX_PACKET_COUNT +
					EMUX_THRESHOLD_RPT_OFF * priv->sp);
		dev->stats.tx_bytes = readl(priv->fpgrp->base +
					FP_EMUX_BYTE_COUNT +
					EMUX_THRESHOLD_RPT_OFF * priv->sp);
	}

	sum += readl(priv->fpgrp->base + FP_EMUX_DROP_PACKET_COUNT +
		     EMUX_THRESHOLD_RPT_OFF * priv->sp);
	dev->stats.tx_fifo_errors = sum;
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
	fpgrp->l2cam_size = fpgrp->plat->l2cam_size;
	if (fpgrp->plat->version == FP)
		fpgrp->txbase = fpgrp->base + FASTPATH_TXDMA_BASE;
	else
		fpgrp->txbase = fpgrp->base + FASTPATH_TOE_BASE;
	fpgrp->rxbase = fpgrp->base + FASTPATH_RXDMA_BASE;
	for (j = 0; j < NUM_INTFS; j++) {
		if (!fpgrp->plat->if_data[j])
			continue;
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
		fpif_set_ethtool_ops(netdev);
		netdev->hw_features |= NETIF_F_HW_CSUM | NETIF_F_RXCSUM;
		netdev->hw_features |= NETIF_F_SG;
		netdev->features |= netdev->hw_features;
		netdev->hard_header_len += FP_HDR_SIZE;
		strcpy(netdev->name, priv->plat->ifname);

		priv->promisc_idx = TCAM_IDX_INV;
		priv->allmulti_idx = TCAM_IDX_INV;
		priv->br_l2cam_idx = IDX_INV;
		priv->br_tcam_idx = IDX_INV;
		priv->ifaddr_idx = IDX_INV;
		priv->id = priv->plat->iftype;
		switch (priv->id) {
		case DEVID_GIGE0:
			priv->sp = SP_GIGE;
			priv->dmap = DEST_GIGE;
			priv->rgmii_base = fpgrp->base + RGMII0_OFFSET;
			break;
		case DEVID_DOCSIS:
			priv->sp = SP_DOCSIS;
			priv->dmap = DEST_DOCSIS;
			priv->ifidx = 0xf;
			break;
		case DEVID_GIGE1:
			priv->sp = SP_ISIS;
			priv->dmap = DEST_ISIS;
			if (fpgrp->plat->version != FP)
				priv->rgmii_base = fpgrp->base + RGMII1_OFFSET;
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

		if (priv->plat->mdio_bus_data) {
			err = fpif_mdio_register(netdev);
			if (err < 0) {
				netdev_err(netdev, "fpif_mdio_register err=%d\n",
					   err);
				goto err_init;
			}
		}
	}

	for (j = 0; j < fpgrp->l2cam_size; j++)
		fpgrp->l2_idx[j] = NUM_INTFS;

	err = register_netdevice_notifier(&ovs_dp_device_notifier);
	if (err)
		pr_err("fp:ERROR in reg notifier for netdev\n");

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
	.id = 0,
	.iftype = DEVID_GIGE0,
	.ifname = "fpgige",
	.interface = PHY_INTERFACE_MODE_RGMII_ID,
	.phy_addr = 0x1,
	.bus_id = 1,
	.tx_dma_ch = 0,
	.rx_dma_ch = 0,
};

static struct plat_fpif_data fpif_isis_data = {
	.id = 1,
	.iftype = DEVID_GIGE1,
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
	struct plat_stmfp_data *plat_dat;

	pr_debug("%s\n", __func__);

	/* Map FastPath register memory */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("ERROR :%s: platform_get_resource ", __func__);
		return -ENODEV;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		pr_err("ERROR :%s: Couldn't ioremap regs\n", __func__);
		return -ENODEV;
	}
	pr_debug("fastpath base=%p\n", base);

	if (pdev->dev.of_node) {
		plat_dat =
		    devm_kzalloc(&pdev->dev, sizeof(struct plat_stmfp_data),
				 GFP_KERNEL);
		if (!plat_dat)
			return -ENOMEM;

		if (pdev->dev.platform_data)
			memcpy(plat_dat, pdev->dev.platform_data,
			       sizeof(struct plat_stmfp_data));

		err = stmfp_probe_config_dt(pdev, plat_dat);
		if (err) {
			pr_err("%s: FP config of DT failed", __func__);
			return err;
		}
		if (pdev->dev.platform_data)
			pdev->dev.platform_data = plat_dat;
	} else {
		plat_dat = pdev->dev.platform_data;
	}

	if (plat_dat->init) {
		err = plat_dat->init(pdev);
		if (unlikely(err))
			return err;
	}

	/* Get Rx interrupt number */
	rx_irq = platform_get_irq_byname(pdev, "fprxdmairq");
	if (rx_irq == -ENXIO) {
		pr_err("%s: ERROR: Rx IRQ config info not found\n", __func__);
		return -ENXIO;
	}

	/* Get Tx interrupt number */
	tx_irq = platform_get_irq_byname(pdev, "fptxdmairq");
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

	/* Override phy address with kernel parameters if supplied */

	if((fpgige_phyaddr >= 0) && (fpgige_phyaddr <=31)) {
		fpgrp->plat->if_data[DEVID_GIGE0]->phy_addr = fpgige_phyaddr;
		dev_dbg(devptr, "setting %s phy_addr 0x%x\n",
			fpgrp->plat->if_data[DEVID_GIGE0]->ifname,
			fpgige_phyaddr);
	}

	if((fplan_phyaddr >= 0) && (fplan_phyaddr <=31)) {
		fpgrp->plat->if_data[DEVID_GIGE1]->phy_addr = fplan_phyaddr;
		dev_dbg(devptr, "setting %s phy_addr 0x%x\n",
			fpgrp->plat->if_data[DEVID_GIGE1]->ifname,
			fplan_phyaddr);
	}

	if((fpdocsis_phyaddr >= 0) && (fpdocsis_phyaddr <=31)) {
		fpgrp->plat->if_data[DEVID_DOCSIS]->phy_addr = fpdocsis_phyaddr;
		dev_dbg(devptr, "setting %s phy_addr 0x%x\n",
			fpgrp->plat->if_data[DEVID_DOCSIS]->ifname,
			fpdocsis_phyaddr);
	}
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

#ifdef CONFIG_OF
static const struct of_device_id stmfp_dt_ids[] = {
	{.compatible = "st,fp"},
	{.compatible = "st,fplite"},
	{ }
};

MODULE_DEVICE_TABLE(of, stmfp_dt_ids);
#endif

static struct platform_driver fpif_driver = {
	.probe = fpif_probe,
	.remove = fpif_remove,
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(stmfp_dt_ids),
#endif
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

#ifndef MODULE
static int __init fpif_cmdline_opt(char *cmdline)
{
        char *opt;

        if (!cmdline || !*cmdline)
                return -EINVAL;

        while ((opt = strsep(&cmdline, ",")) != NULL) {
                if (!strncmp(opt, "fpdocsis_phyaddr:", 17)) {
                        if (kstrtoint(opt + 17, 0, &fpdocsis_phyaddr))
                                goto err;
                } else if (!strncmp(opt, "fplan_phyaddr:", 14)) {
                        if (kstrtoint(opt + 14, 0, &fplan_phyaddr))
                                goto err;
                } else if (!strncmp(opt, "fpgige_phyaddr:",15)) {
                        if (kstrtoint(opt + 15, 0, &fpgige_phyaddr))
                                goto err;
		}
	}
	return 0;
err:
	pr_err("%s: ERROR in parsing kernel parameters", __func__);
	return -EINVAL;
}

__setup("stmfp=", fpif_cmdline_opt);

#endif

module_init(fpif_init_module);
module_exit(fpif_exit_module);

MODULE_DESCRIPTION("STMFP 10/100/1000 Ethernet driver");
MODULE_AUTHOR("Manish Rathi <manish.rathi@st.com>");
MODULE_LICENSE("GPL");
