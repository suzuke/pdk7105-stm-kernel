/*
 * Copyright (C) 2011 STMicroelectronics Limited
 * Author: David McKay <david.mckay@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */
#ifndef _STM_AMBA_BRIDGE_H
#define _STM_AMBA_BRIDGE_H

/* Non-STBus to STBUS Convertor/Bridge config datastructure */
struct stm_amba_bridge_config {
	 /* Bridge type: */
	enum {
		stm_amba_type1, /* Older version ADCS: 7526996 */
		stm_amba_type2,
	} type;

	/* Type of transaction to generate on stbus side */
	enum {
		stm_amba_opc_LD4_ST4,
		stm_amba_opc_LD8_ST8,
		stm_amba_opc_LD16_ST16,
		stm_amba_opc_LD32_ST32,
		stm_amba_opc_LD64_ST64
	} max_opcode;

	/* These fields must be power of two */
	unsigned int chunks_in_msg;
	unsigned int packets_in_chunk;
	/* Enable STBus posted writes */
	enum {
		stm_amba_write_posting_disabled,
		stm_amba_write_posting_enabled
	} write_posting;

	/* We could use a union here, but it makes
	 * the initialistion code look clunky unfortunately
	 */

	/* Fields unique to v1 */
	struct {
		unsigned int req_timeout;
	} type1;
	/* Fields unique to v2 */
	struct {
		/* Some versions of the type 2 convertor do not
		 * have the sd_config at all, so some of the
		 * fields below cannot be changed or even read!
		 */
		unsigned int sd_config_missing:1;
		unsigned int threshold;/* power of 2 */
		enum {
			stm_amba_ahb_burst_based,
			stm_amba_ahb_cell_based
		} req_notify;
		enum {
			stm_amba_abort_transaction,
			stm_amba_complete_transaction
		} cont_on_error;
		enum {
			stm_amba_msg_merge_disabled,
			stm_amba_msg_merge_enabled
		} msg_merge;
		enum {
			stm_amba_stbus_cell_based,
			stm_amba_stbus_threshold_based
		} trigger_mode;
		enum {
			stm_amba_read_ahead_disabled,
			stm_amba_read_ahead_enabled
		} read_ahead;
	} type2;
};

/* Create an amba plug, the calling code needs to map the registers into base.
 * The device associated with this plug must be passed in. This will not access
 * the registers in any way, so can be called before the device has a clock for
 * example. There is no release method, as all memory allocation is done via
 * the devres functions so will automatically be released when the device goes
 */
struct stm_amba_bridge *stm_amba_bridge_create(struct device *dev,
			void __iomem *base,
			struct stm_amba_bridge_config *bus_config);

/* Set up the amba convertor registers, should be called
 * for first time init and after resume
 */
void stm_amba_bridge_init(struct stm_amba_bridge *plug);

/* Macros to assist in building up the various data structures,
 * there is a lot of commonality between the devices. If there
 * should be as much is an interesting question
 */
#define STM_DEFAULT_TYPE2_AMBA_PLUG_CONFIG				\
	.type			=	stm_amba_type2,			\
	.max_opcode		=	stm_amba_opc_LD32_ST32,		\
	.write_posting		=	stm_amba_write_posting_disabled,\
	.chunks_in_msg		=	0, /* messaging disabled */	\
	.packets_in_chunk	=	0,				\
	.type2.req_notify	=	stm_amba_ahb_burst_based,	\
	.type2.cont_on_error	=	stm_amba_complete_transaction,	\
	.type2.msg_merge	=	stm_amba_msg_merge_disabled,	\
	.type2.trigger_mode	=	stm_amba_stbus_threshold_based,	\
	.type2.read_ahead	=	stm_amba_read_ahead_enabled,	\
	.type2.threshold	=	32


/* Same for USB, though different SOCs seem to use different threshold
 * values. Again they can be overridden.
 */
#define STM_DEFAULT_USB_AMBA_PLUG_CONFIG(thresh)			\
	STM_DEFAULT_TYPE2_AMBA_PLUG_CONFIG,				\
	.packets_in_chunk			= 8,			\
	.type2.threshold			= (thresh)

#endif /* _STM_AMBA_BRIDGE_H */
