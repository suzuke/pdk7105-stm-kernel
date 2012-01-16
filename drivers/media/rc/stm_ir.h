#ifndef _STM_IR_DEV_H_
#define _STM_IR_DEV_H_

struct stm_ir_device {
	struct stm_plat_lirc_data 	*pdata;
	struct device 			*dev;
	struct clk 			*sys_clock;
	void 				*base;	/* Register base address */
	void 				*rx_base;	/* RX Register base address */

	int 				enabled; /* State of the device */
	int 				insert_scd_timing;

	struct rc_dev 			*rdev;	
	/* RX */
	int 				rx_symbol_mult;
	int 				rx_symbol_div;
	int 				rx_pulse_mult;
	int 				rx_pulse_div;
	/* data configuration */
	unsigned int 			rx_sampling_freq_div;

	/* TX */
	wait_queue_head_t 		tx_waitq;
	/* timing fine control */
	unsigned int 			tx_mult;
	unsigned int 			tx_div;
	/* transmit buffer */
	unsigned int 			*tx_wbuf;
	unsigned int 			off_wbuf;
	unsigned int 			tx_size;
	unsigned int			tx_done;
};

/*
 * start code detect (SCD) support
 */
struct ir_scd_s {
	unsigned int 		code;		/* code symbols to be detect. */
	unsigned int 		alt_code;	/* alternative SCD to be detected */
	unsigned int 		nomtime;	/* nominal symbol time in us */
	unsigned int 		noiserecov;	/* noise recovery configuration */
};

/* Register */

/*
 * IRB IR/UHF common configurations
 */
#define IRB_SAMPLE_RATE_COMM	0x64	/* sample freq divisor*/
#define IRB_CLOCK_SEL		0x70	/* clock select       */
#define IRB_CLOCK_SEL_STATUS	0x74	/* clock status       */

/* IRB IR/UHF receiver registers */
#define IRB_RX_ON               0x40	/* pulse time capture */
#define IRB_RX_SYS              0X44	/* sym period capture */
#define IRB_RX_INT_EN           0x48	/* IRQ enable (R/W)   */
#define IRB_RX_INT_STATUS       0x4C	/* IRQ status (R/W)   */
#define IRB_RX_EN               0x50	/* Receive enablei    */
#define IRB_MAX_SYM_PERIOD      0x54	/* max sym value      */
#define IRB_RX_INT_CLEAR        0x58	/* overrun status     */
#define IRB_RX_STATUS           0x6C	/* receive status     */
#define IRB_RX_NOISE_SUPPR      0x5C	/* noise suppression  */
#define IRB_RX_POLARITY_INV     0x68	/* polarity inverter  */


 /* IRB UHF-SCD filter registers */
#define IRB_SCD_CFG             0x200	/* config           */
#define IRB_SCD_STA             0x204	/* status           */
#define IRB_SCD_CODE            0x208	/* normal code      */
#define IRB_SCD_CODE_LEN        0x20c	/* num code symbols */
#define IRB_SCD_SYMB_MIN_TIME   0x210	/* min symbol time  */
#define IRB_SCD_SYMB_MAX_TIME   0x214	/* max symbol time  */
#define IRB_SCD_SYMB_NOM_TIME   0x218	/* nom symbol time  */
#define IRB_SCD_PRESCALAR       0x21c	/* prescalar        */
#define IRB_SCD_INT_EN          0x220	/* interrupt enable */
#define IRB_SCD_INT_CLR         0x224	/* interrupt clear  */
#define IRB_SCD_INT_STA         0x22c	/* interrupt status */
#define IRB_SCD_NOISE_RECOV     0x228	/* noise recovery   */
#define IRB_SCD_ALT_CODE        0x230	/* alternative code */


 /* IRB TX registers */
#define IRB_TX_PRESCALAR	0x00	/* clock prescalar   */
#define IRB_TX_SUBCARRIER	0x04	/* subcarrier freq   */
#define IRB_TX_SYMPERIOD	0x08	/* symbol period     */
#define IRB_TX_ONTIME		0x0c	/* symbol pulse time */
#define IRB_TX_INT_ENABLE	0x10	/* irq enable        */
#define IRB_TX_INT_STATUS	0x14	/* irq status        */
#define IRB_TX_ENABLE		0x18	/* enable            */
#define IRB_TX_INT_CLEAR	0x1c	/* interrupt clear   */
#define IRB_TX_SUBCARRIER_WIDTH	0x20	/* subcarrier freq   */
#define IRB_TX_STATUS		0x24	/* status            */




/* Bit settings */
#define LIRC_STM_SCD_MAX_SYMBOLS        32
#define LIRC_STM_SCD_TOLERANCE          25
/* rx.scd_flags fields:
 *	scd normal                      1 -> bit 0
 *	scd altenative                  1 -> bit 1
 *	scd normal = alternative        1 -> bit 2
 *	scd enabled                     1 -> bit 3
 */
#define SCD_NORMAL                      0x01
#define SCD_ALTERNATIVE                 0x02
#define SCD_NOR_EQ_ALT                  0x04
#define SCD_ENABLED                     0x08
#define SCD_ALT_MASK                    (SCD_NORMAL|SCD_ALTERNATIVE|SCD_ENABLED)


#define TX_INT_PENDING		0x01
#define TX_INT_UNDERRUN		0x02

#define TX_FIFO_DEPTH		7
#define TX_CARRIER_FREQ		38000	/* 38KHz */

#define LIRC_STM_IS_OVERRUN	0x04
#define LIRC_STM_CLEAR_IRQ	0x38
#define LIRC_STM_CLEAR_OVERRUN	0x04
/* IRQ set: Enable full FIFO                 1  -> bit  3;
 *          Enable overrun IRQ               1  -> bit  2;
 *          Enable last symbol IRQ           1  -> bit  1:
 *          Enable RX interrupt              1  -> bit  0;
 */
#define LIRC_STM_ENABLE_IRQ		0x0f

#endif /* _STM_IR_DEV_H_ */
