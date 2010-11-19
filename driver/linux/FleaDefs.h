#ifndef _FLEA_DEFS_
#define _FLEA_DEFS_

/*
* Include a whole bunch of RDB files for register definitions
*/
#include "bcm_70015_regs.h"

/* Assume we have 64MB DRam */
#define FLEA_TOTAL_DRAM_SIZE		64*1024*1024
#define FLEA_GISB_DIRECT_BASE		0x50

/*- These definition of the ADDRESS and DATA
  - Registers are not there in RDB.
 */
#define FLEA_GISB_INDIRECT_ADDRESS	0xFFF8
#define FLEA_GISB_INDIRECT_DATA		0xFFFC

/*
 * POLL count for Flea.
 */
#define FLEA_MAX_POLL_CNT		1000

/*
 -- Flea Firmware Signature length (128 bit)
 */
#define FLEA_FW_SIG_LEN_IN_BYTES	16
#define LENGTH_FIELD_SIZE			4
#define FLEA_FW_SIG_LEN_IN_DWORD	(FLEA_FW_SIG_LEN_IN_BYTES/4)
#define FW_DOWNLOAD_START_ADDR		0

/*
 * Some macros to ease the bit specification from RDB
 */
#define SCRAM_KEY_DONE_INT_BIT	BC_BIT(BCHP_WRAP_MISC_INTR2_CPU_STATUS_SCRM_KEY_DONE_INTR_SHIFT)
#define BOOT_VER_DONE_BIT		BC_BIT(BCHP_WRAP_MISC_INTR2_CPU_STATUS_BOOT_VER_DONE_INTR_SHIFT)
#define BOOT_VER_FAIL_BIT		BC_BIT(BCHP_WRAP_MISC_INTR2_CPU_STATUS_BOOT_VER_FAIL_INTR_SHIFT)
#define SHARF_ERR_INTR			BC_BIT(BCHP_WRAP_MISC_INTR2_CPU_STATUS_SHARF_ERR_INTR_SHIFT)
#define SCRUB_ENABLE_BIT		BC_BIT(BCHP_SCRUB_CTRL_SCRUB_ENABLE_SCRUB_EN_SHIFT)
#define DRAM_SCRAM_ENABLE_BIT	BC_BIT(BCHP_SCRUB_CTRL_SCRUB_ENABLE_DSCRAM_EN_SHIFT)
#define ARM_RUN_REQ_BIT			BC_BIT(BCHP_ARMCR4_BRIDGE_REG_BRIDGE_CTL_arm_run_request_SHIFT)
#define GetScrubEndAddr(_Sz)	((FW_DOWNLOAD_START_ADDR + (_Sz - FLEA_FW_SIG_LEN_IN_BYTES -LENGTH_FIELD_SIZE-1))& (BCHP_SCRUB_CTRL_BORCH_END_ADDRESS_BORCH_END_ADDR_MASK))

/*
-- Firmware Command Interface Definitions.
-- We use BCHP_ARMCR4_BRIDGE_REG_MBOX_PCI1 as host to FW mailbox.
-- We use BCHP_ARMCR4_BRIDGE_REG_MBOX_ARM1 as FW to Host mailbox.
*/

/* Address where the command parameters are written. */
#define DDRADDR_4_FWCMDS		0x100

/* */
/* mailbox used for passing the FW Command address (DDR address) to */
/* firmware. */
/* */
#define FW_CMD_POST_MBOX		BCHP_ARMCR4_BRIDGE_REG_MBOX_ARM1

/* Once we get a firmware command done interrupt, */
/* we will need to get the address of the response. */
/* This mailbox is written by FW before asserting the */
/* firmware command done interrupt. */
#define FW_CMD_RES_MBOX			BCHP_ARMCR4_BRIDGE_REG_MBOX_PCI1

/*
-- RxDMA Picture QStatus Mailbox.
-- RxDMA Picture Post Mailbox. < Write DDR address to this mailbox >
 */
#define RX_DMA_PIC_QSTS_MBOX		BCHP_ARMCR4_BRIDGE_REG_MBOX_PCI2
#define RX_POST_MAILBOX				BCHP_ARMCR4_BRIDGE_REG_MBOX_ARM2
#define RX_POST_CONFIRM_SCRATCH		BCHP_ARMCR4_BRIDGE_REG_SCRATCH_5
#define RX_START_SEQ_NUMBER			1
#define INDICATE_TX_DONE_REG		BCHP_ARMCR4_BRIDGE_REG_SCRATCH_9

/*
-- At the end of the picture frame there is the Link's Y0 data
-- and there is Width data. The driver will copy this 32 bit data to Y[0]
-- location. This makes the Flea PIB compatible with Link.
-- Also note that Flea is capable of putting out the odd size picture widths
-- so the PicWidth field is the actual picture width of the picture. In link
-- We were only getting 1920,1280 or 720 as picture widths.
*/
#define PIC_PIB_DATA_OFFSET_FROM_END	4
#define PIC_PIB_DATA_SIZE_IN_BYTES		4	/*The data that use to be in Y[0] component */
#define PIC_WIDTH_OFFSET_FROM_END		8	/*Width information for the driver. */
#define PIC_WIDTH_DATA_SIZE_IN_BYTES	4	/*Width information for the driver. */

/*
-- The format change PIB comes in a dummy frame now.
-- The Width field has the format change flag (bit-31) which
-- the driver uses to detect the format change now.
*/
#define PIB_FORMAT_CHANGE_BIT			BC_BIT(31)
#define PIB_EOS_DETECTED_BIT			BC_BIT(30)

#define FLEA_DECODE_ERROR_FLAG			0x800

/*
-- Interrupt Mask, Set and Clear registers are exactly
-- same as the interrupt status register. We will
-- Use the following union for all the registers.
*/

union FLEA_INTR_BITS_COMMON
{
	struct
	{
		uint32_t	L0TxDMADone:1;		/* Bit-0 */
		uint32_t	L0TxDMAErr:1;		/* Bit-1 */
		uint32_t	L0YRxDMADone:1;		/* Bit-2 */
		uint32_t	L0YRxDMAErr:1;		/* Bit-3 */
		uint32_t	L0UVRxDMADone:1;	/* Bit-4 */
		uint32_t	L0UVRxDMAErr:1;		/* Bit-5 */
		uint32_t	Reserved1:2;		/* Bit-6-7 */
		uint32_t	L1TxDMADone:1;		/* Bit-8 */
		uint32_t	L1TxDMAErr:1;		/* Bit-9 */
		uint32_t	L1YRxDMADone:1;		/* Bit-10 */
		uint32_t	L1YRxDMAErr:1;		/* Bit-11 */
		uint32_t	L1UVRxDMADone:1;	/* Bit-12 */
		uint32_t	L1UVRxDMAErr:1;		/* Bit-13 */
		uint32_t	Reserved2:2;		/* Bit-14-15 */
		uint32_t	ArmMbox0Int:1;		/* Bit-16 */
		uint32_t	ArmMbox1Int:1;		/* Bit-17 */
		uint32_t	ArmMbox2Int:1;		/* Bit-18 */
		uint32_t	ArmMbox3Int:1;		/* Bit-19 */
		uint32_t	Reserved3:4;		/* Bit-20-23 */
		uint32_t	PcieTgtUrAttn:1;	/* Bit-24 */
		uint32_t	PcieTgtCaAttn:1;	/* Bit-25 */
		uint32_t	HaltIntr:1;			/* Bit-26 */
		uint32_t	Reserved4:5;			/* Bit-27-31 */
	};

	 uint32_t	WholeReg;
};

/*
================================================================
-- Flea power state machine
-- FLEA_PS_NONE
--	Enter to this state when system boots up and device is not open.
-- FLEA_PS_ACTIVE:
--	1. Set when the device is started and FW downloaded.
--	2. We come to this state from FLEA_PS_LP_COMPLETE when
--		2.a Free list length becomes greater than X. [Same As Internal Pause Sequence]
--		2.b There is a firmware command issued.
--  3. We come to this state from FLEA_PS_LP_PENDING when
--		3.a Free list length becomes greater than X. [Same As Internal Pause Sequence]
--		3.b There is a firmware command Issued.
-- FLEA_PS_LP_PENDING
--	1. Enter to this state from FLEA_PS_ACTIVE
--		1.a FLL becomes greater less than Y[Same as Internal Resume].
-- FLEA_PS_LP_COMPLETE
--	1. Enter in to this state from FLEA_PS_LP_PENDING
--		1.a There are no Pending TX, RX, and FW Command.
--	2. Enter to This state when the handle is closed.
--  3. Enter to this state From ACTIVE
--		3.a FLL < Y.
--		3.b There is no TX,RX and FW pending.
--  4. Enter this state when RX is not running, either before it is started or after it is stopped.
=================================================================
*/

enum FLEA_POWER_STATES {
	FLEA_PS_NONE=0,
	FLEA_PS_STOPPED,
	FLEA_PS_ACTIVE,
	FLEA_PS_LP_PENDING,
	FLEA_PS_LP_COMPLETE
};

enum FLEA_STATE_CH_EVENT {
	FLEA_EVT_NONE=0,
	FLEA_EVT_START_DEVICE,
	FLEA_EVT_STOP_DEVICE,
	FLEA_EVT_FLL_CHANGE,
	FLEA_EVT_FW_CMD_POST,
	FLEA_EVT_CMD_COMP
};

#define TEST_BIT(_value_,_bit_number_)	(_value_ & (0x00000001 << _bit_number_))

#define CLEAR_BIT(_value_,_bit_number_)\
{_value_ = _value_ & (~(0x00000001 << _bit_number_));}

#define SET_BIT(_value_,_bit_number_)\
{_value_ |=  (0x01 << _bit_number_);}

#endif
