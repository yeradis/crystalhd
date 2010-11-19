#ifndef _DRIVER_FW_SHARE_
#define _DRIVER_FW_SHARE_

#ifndef USE_MULTI_DECODE_DEFINES
#define HOST_TO_FW_PIC_DEL_INFO_ADDR		0x400	/*Original single Decode Offset*/
#else
#if 0
#define HOST_TO_FW_PIC_DEL_INFO_ADDR		0x200	/*New offset that we plan to use eventually*/
#endif
#define HOST_TO_FW_PIC_DEL_INFO_ADDR		0x400	/*This is just for testing..remove this once tested */
#endif


/*
 * The TX address does not change between the
 * single decode and multiple decode.
 */
#define TX_BUFF_UPDATE_ADDR					0x300	/*This is relative to BORCH */

typedef
struct
_PIC_DELIVERY_HOST_INFO_
{
/*
-- The list ping-pong code is already there in the driver
-- to save from re-inventing the code, the driver will indicate
-- to firmware on which list the command should be posted.
 */
	unsigned int ListIndex;
	unsigned int HostDescMemLowAddr_Y;
	unsigned int HostDescMemHighAddr_Y;
	unsigned int HostDescMemLowAddr_UV;
	unsigned int HostDescMemHighAddr_UV;
	unsigned int RxSeqNumber;
	unsigned int ChannelID;
	unsigned int Reserved[1];
}PIC_DELIVERY_HOST_INFO,
*PPIC_DELIVERY_HOST_INFO;

/*
-- We write the driver's FLL to this memory location.
-- This is the array for FLL of all the channels.
*/
#define HOST_TO_FW_FLL_ADDR			(HOST_TO_FW_PIC_DEL_INFO_ADDR + sizeof(PIC_DELIVERY_HOST_INFO))


typedef enum _DRIVER_FW_FLAGS_{
	DFW_FLAGS_CLEAR			=0,
	DFW_FLAGS_TX_ABORT		=BC_BIT(0),	/*Firmware is stopped and will not give anymore buffers. */
	DFW_FLAGS_WRAP			=BC_BIT(1)	/*Instruct the Firmware to WRAP the input buffer pointer */
}DRIVER_FW_FLAGS;

typedef struct
_TX_INPUT_BUFFER_INFO_
{
	unsigned int DramBuffAdd;			/* Address of the DRAM buffer where the data can be pushed*/
	unsigned int DramBuffSzInBytes;		/* Size of the available DRAM buffer, in bytes*/
	unsigned int HostXferSzInBytes;		/* Actual Transfer Done By Host, In Bytes*/
	unsigned int Flags;					/* DRIVER_FW_FLAGS Written By Firmware to handle Stop of TX*/
	unsigned int SeqNum;				/* Sequence number of the tranfer that is done. Read-Modify-Write*/
	unsigned int ChannelID;				/* To which Channel this buffer belongs to*/
	unsigned int Reserved[2];
}TX_INPUT_BUFFER_INFO,
*PTX_INPUT_BUFFER_INFO;


/*
-- Out of band firmware handshake.
=====================================
-- The driver writes the SCRATCH-8 register with a Error code.
-- The driver then writes a mailbox register with 0x01.
-- The driver then polls for the ACK. This ack is if the value of the SCRATCH-8 becomes zero.
*/

#define OOB_ERR_CODE_BASE		70015
typedef enum _OUT_OF_BAND_ERR_CODE_
{
	OOB_INVALID				= 0,
	OOB_CODE_ACK			= OOB_ERR_CODE_BASE,
	OOB_CODE_STOPRX			= OOB_ERR_CODE_BASE + 1,
}OUT_OF_BAND_ERR_CODE;


#define OOB_CMD_RESPONSE_REGISTER		BCHP_ARMCR4_BRIDGE_REG_SCRATCH_8
#define OOB_PCI_TO_ARM_MBOX				BCHP_ARMCR4_BRIDGE_REG_MBOX_ARM3
#define TX_BUFFER_AVAILABLE_INTR		BCHP_ARMCR4_BRIDGE_REG_MBOX_PCI3
#define HEART_BEAT_REGISTER				BCHP_ARMCR4_BRIDGE_REG_SCRATCH_1
#define HEART_BEAT_POLL_CNT				5


#define FLEA_WORK_AROUND_SIG			0xF1EA
#define RX_PIC_Q_STS_WRKARND			BC_BIT(0)
#define RX_DRAM_WRITE_WRKARND			BC_BIT(1)
#define RX_MBOX_WRITE_WRKARND			BC_BIT(2)
#endif
