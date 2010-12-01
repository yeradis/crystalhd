/***************************************************************************
 * Copyright (c) 2005-2009, Broadcom Corporation.
 *
 *  Name: crystalhd_hw . h
 *
 *  Description:
 *		BCM70012 Linux driver hardware layer.
 *
 *  HISTORY:
 *
 **********************************************************************
 * This file is part of the crystalhd device driver.
 *
 * This driver is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this driver.  If not, see <http://www.gnu.org/licenses/>.
 **********************************************************************/

#ifndef _CRYSTALHD_HW_H_
#define _CRYSTALHD_HW_H_
#define DEBUG 1

#include <linux/device.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 24)
#include <linux/semaphore.h>
#else
#include <asm/semaphore.h>
#endif
#include "crystalhd_fw_if.h"
#include "crystalhd_misc.h"
#include "DriverFwShare.h"
#include "FleaDefs.h"

/* HW constants..*/
#define DMA_ENGINE_CNT		2
#define MAX_PIB_Q_DEPTH		64
#define MIN_PIB_Q_DEPTH		2
#define WR_POINTER_OFF		4
#define MAX_VALID_POLL_CNT	1000

#define TX_WRAP_THRESHOLD 128 * 1024

#define	NUMBER_OF_TRANSFERS_TX_SIDE				1
#define NUMBER_OF_TRANSFERS_RX_SIDE				2

struct BC_DRV_PIC_INFO {
	struct C011_PIB			DecoPIB;
	struct BC_DRV_PIC_INFO		*Flink;
};

union desc_low_addr_reg {
	struct {
#ifdef	__LITTLE_ENDIAN_BITFIELD
		uint32_t	list_valid:1;
		uint32_t	reserved:4;
		uint32_t	low_addr:27;
#else
		uint32_t	low_addr:27;
		uint32_t	reserved:4;
		uint32_t	list_valid:1;
#endif
	};

	uint32_t	whole_reg;

};

struct dma_descriptor {	/* 8 32-bit values */
#ifdef	__LITTLE_ENDIAN_BITFIELD
	/* 0th u32 */
	uint32_t sdram_buff_addr:28;	/* bits 0-27:  SDRAM Address */
	uint32_t res0:4;		/* bits 28-31: Reserved */

	/* 1st u32 */
	uint32_t buff_addr_low;		/* 1 buffer address low */
	uint32_t buff_addr_high;	/* 2 buffer address high */

	/* 3rd u32 */
	uint32_t res2:2;		/* 0-1 - Reserved */
	uint32_t xfer_size:23;		/* 2-24 = Xfer size in words */
	uint32_t res3:6;		/* 25-30 reserved */
	uint32_t intr_enable:1;		/* 31 - Interrupt After this desc */

	/* 4th u32 */
	uint32_t endian_xlat_align:2;	/* 0-1 Endian Translation */
	uint32_t next_desc_cont:1;	/* 2 - Next desc is in contig memory */
	uint32_t res4:25;		/* 3 - 27 Reserved bits */
	uint32_t fill_bytes:2;		/* 28-29 Bits Fill Bytes */
	uint32_t dma_dir:1;		/* 30 bit DMA Direction */
	uint32_t last_rec_indicator:1;	/* 31 bit Last Record Indicator */

	/* 5th u32 */
	uint32_t next_desc_addr_low;	/* 32-bits Next Desc Addr lower */

	/* 6th u32 */
	uint32_t next_desc_addr_high;	/* 32-bits Next Desc Addr Higher */

	/* 7th u32 */
	uint32_t res8;			/* Last 32bits reserved */
#else
	/* 0th u32 */
	uint32_t res0:4;		/* bits 28-31: Reserved */
	uint32_t sdram_buff_addr:28;	/* bits 0-27:  SDRAM Address */

	/* 1st u32 */
	uint32_t buff_addr_low;		/* 1 buffer address low */
	uint32_t buff_addr_high;	/* 2 buffer address high */

	/* 3rd u32 */
	uint32_t intr_enable:1;		/* 31 - Interrupt After this desc */
	uint32_t res3:6;		/* 25-30 reserved */
	uint32_t xfer_size:23;		/* 2-24 = Xfer size in words */
	uint32_t res2:2;		/* 0-1 - Reserved */

	/* 4th u32 */
	uint32_t last_rec_indicator:1;	/* 31 bit Last Record Indicator */
	uint32_t dma_dir:1;		/* 30 bit DMA Direction */
	uint32_t fill_bytes:2;		/* 28-29 Bits Fill Bytes */
	uint32_t res4:25;		/* 3 - 27 Reserved bits */
	uint32_t next_desc_cont:1;	/* 2 - Next desc is in contig memory */
	uint32_t endian_xlat_align:2;	/* 0-1 Endian Translation */

	/* 5th u32 */
	uint32_t next_desc_addr_low;	/* 32-bits Next Desc Addr lower */

	/* 6th u32 */
	uint32_t next_desc_addr_high;	/* 32-bits Next Desc Addr Higher */

	/* 7th u32 */
	uint32_t res8;			/* Last 32bits reserved */
#endif
};

/*
 * We will allocate the memory in 4K pages
 * the linked list will be a list of 32 byte descriptors.
 * The  virtual address will determine what should be freed.
 */
struct dma_desc_mem {
	struct dma_descriptor		*pdma_desc_start;	/* 32-bytes for dma descriptor. should be first element */
	dma_addr_t		phy_addr;		/* physical address of each DMA desc */
	uint32_t		sz;
	struct dma_desc_mem	*Next;			/* points to Next Descriptor in chain */

};

enum list_sts {
	sts_free		= 0,

	/* RX-Y Bits 0:7 */
	rx_waiting_y_intr	= 0x00000001,
	rx_y_error		= 0x00000004,

	/* RX-UV Bits 8:16 */
	rx_waiting_uv_intr	= 0x0000100,
	rx_uv_error		= 0x0000400,

	rx_sts_waiting		= (rx_waiting_y_intr|rx_waiting_uv_intr),
	rx_sts_error		= (rx_y_error|rx_uv_error),

	rx_y_mask		= 0x000000FF,
	rx_uv_mask		= 0x0000FF00,

};


enum INTERRUPT_STATUS {
	NO_INTERRUPT					= 0x0000,
	FPGA_RX_L0_DMA_DONE				= 0x0001, /*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	FPGA_RX_L1_DMA_DONE				= 0x0002, /*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	FPGA_TX_L0_DMA_DONE				= 0x0004, /*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	FPGA_TX_L1_DMA_DONE				= 0x0008, /*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	DECO_PIB_INTR					= 0x0010, /*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	DECO_FMT_CHANGE					= 0x0020,
	DECO_MBOX_RESP					= 0x0040,
	DECO_RESUME_FRM_INTER_PAUSE		= 0x0080, /*Not Handled in DPC Need to Fire Rx cmds on resume from Pause*/
};

enum ERROR_STATUS {
	NO_ERROR			=0,
	RX_Y_DMA_ERR_L0		=0x0001,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	RX_UV_DMA_ERR_L0	=0x0002,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	RX_Y_DMA_ERR_L1		=0x0004,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	RX_UV_DMA_ERR_L1	=0x0008,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	TX_DMA_ERR_L0		=0x0010,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	TX_DMA_ERR_L1		=0x0020,/*DONT CHANGE VALUES...SOME BITWIZE OPERATIONS WILL FAIL*/
	FW_CMD_ERROR		=0x0040,
	DROP_REPEATED		=0x0080,
	DROP_FLEA_FMTCH		=0x0100,/*We do not want to deliver the flea dummy frame*/
	DROP_DATA_ERROR		=0x0200,/*We were not able to get the PIB correctly so drop the frame. */
	DROP_SIZE_ERROR		=0x0400,/*We were not able to get the size properly from hardware. */
	FORCE_CANCEL		=0x8000
};

enum LIST_STATUS {
	ListStsFree=0,				/* Initial state and state the buffer is moved to Ready Buffer list. */
	RxListWaitingForYIntr=1,	/* When the Y Descriptor is posted. */
	RxListWaitingForUVIntr=2,	/* When the UV descriptor is posted. */
	TxListWaitingForIntr =4,
};

struct RX_DMA_LIST {
	enum LIST_STATUS		ListSts;
	/*LIST_ENTRY		ActiveList; */
	uint32_t			ActiveListLen;
	uint32_t			ListLockInd;					/* To Be Filled up During Init */
	uint32_t			ulDiscCount;					/* Discontinuity On this list */
	uint32_t			RxYFirstDescLADDRReg;			/* First Desc Low Addr Y	*/
	uint32_t			RxYFirstDescUADDRReg;			/* First Desc UPPER Addr Y	*/
	uint32_t			RxYCurDescLADDRReg;				/* Current Desc Low Addr Y	*/
	uint32_t			RxYCurDescUADDRReg;				/* First Desc Low Addr Y	*/
	uint32_t			RxYCurByteCntRemReg;			/* Cur Byte Cnt Rem Y		*/

	uint32_t			RxUVFirstDescLADDRReg;			/* First Desc Low Addr UV		*/
	uint32_t			RxUVFirstDescUADDRReg;			/* First Desc UPPER Addr UV		*/
	uint32_t			RxUVCurDescLADDRReg;			/* Current Desc Low Addr UV		*/
	uint32_t			RxUVCurDescUADDRReg;			/* Current Desc UPPER Addr UV	*/
	uint32_t			RxUVCurByteCntRemReg;			/* Cur Byte Cnt Rem UV			*/
};

struct tx_dma_pkt {
	struct dma_desc_mem		desc_mem;
	hw_comp_callback	call_back;
	struct crystalhd_dio_req	*dio_req;
	wait_queue_head_t	*cb_event;
	uint32_t		list_tag;

};

struct crystalhd_rx_dma_pkt {
	struct dma_desc_mem			desc_mem;
	struct crystalhd_dio_req		*dio_req;
	uint32_t			pkt_tag;
	uint32_t			flags;
	BC_PIC_INFO_BLOCK		pib;
	dma_addr_t			uv_phy_addr;
	struct  crystalhd_rx_dma_pkt	*next;
};

struct crystalhd_hw_stats{
	uint32_t	rx_errors;
	uint32_t	tx_errors;
	uint32_t	freeq_count;
	uint32_t	rdyq_count;
	uint32_t	num_interrupts;
	uint32_t	dev_interrupts;
	uint32_t	cin_busy;
	uint32_t	pause_cnt;
	uint32_t	rx_success;
};

enum DECO_STATE {
	DECO_OPERATIONAL				= 0,			/* We start with this state.ST_FW_DWNLD,ST_CAPTURE,STOP_CAPTURE */
	DECO_INTER_PAUSED				= 1,			/* Driver Issued Pause To Decoder */
	DECO_INTER_PAUSE_IN_PROGRESS	= 2,			/* Pause CMD is pending with F/W  */
	DECO_INTER_RESUME_IN_PROGRESS	= 3,			/* Resume CMD is pending with F/W */
	DECO_STOPPED_BY_APP				= 4				/* After STOP Video I do not want to Throttle Decoder.So Special State */
};

/* */
/* These events can be used to notify the hardware layer */
/* to set up it adapter in proper state...or for anyother */
/* purpose for that matter. */
/* We will use this for intermediae events as defined below */

enum BRCM_EVENT {
	BC_EVENT_ADAPTER_INIT_FAILED	=0,
	BC_EVENT_ADAPTER_INIT_SUCCESS	=1,
	BC_EVENT_FW_DNLD_STARTED		=2,
	BC_EVENT_FW_DNLD_ERR			=3,
	BC_EVENT_FW_DNLD_DONE			=4,
	BC_EVENT_SYS_SHUT_DOWN			=5,
	BC_EVENT_START_CAPTURE			=6,
	BC_EVENT_START_CAPTURE_IMMI		=7,
	BC_EVENT_STOP_CAPTURE			=8,		/* Stop Capturing the Rx buffers Stop the DMA engines UnMapBuffers Discard Free and Ready list */
	BC_EVENT_DO_CLEANUP				=9,		/* Total Cleanup Rx And Tx side */
	BC_DISCARD_RX_BUFFERS			=10		/* Move all the Ready buffers to free list. Stop RX DMA. Post Rx Side buffers. */
};

struct crystalhd_hw; /* forward declaration for the types */

/*typedef void*		(*HW_VERIFY_DEVICE)(struct crystalhd_adp*); */
/*typedef bool		(*HW_INIT_DEVICE_RESOURCES)(struct crystalhd_adp*); */
/*typedef bool		(*HW_CLEAN_DEVICE_RESOURCES)(struct crystalhd_adp*); */
typedef bool		(*HW_START_DEVICE)(struct crystalhd_hw*);
typedef bool		(*HW_STOP_DEVICE)(struct crystalhd_hw*);
/* typedef bool	(*HW_XLAT_AND_FIRE_SGL)(struct crystalhd_adp*,PVOID,PSCATTER_GATHER_LIST,uint32_t); */
/* typedef bool	(*HW_RX_XLAT_SGL)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
typedef bool		(*HW_FIND_AND_CLEAR_INTR)(struct crystalhd_adp*,struct crystalhd_hw*);
typedef uint32_t	(*HW_READ_DEVICE_REG)(struct crystalhd_adp*,uint32_t);
typedef void		(*HW_WRITE_DEVICE_REG)(struct crystalhd_adp*,uint32_t,uint32_t);
typedef uint32_t	(*HW_READ_FPGA_REG)(struct crystalhd_adp*,uint32_t);
typedef void		(*HW_WRITE_FPGA_REG)(struct crystalhd_adp*,uint32_t,uint32_t);
typedef BC_STATUS	(*HW_READ_DEV_MEM)(struct crystalhd_hw*,uint32_t,uint32_t,uint32_t*);
typedef BC_STATUS	(*HW_WRITE_DEV_MEM)(struct crystalhd_hw*,uint32_t,uint32_t,uint32_t*);
/* typedef bool		(*HW_INIT_DRAM)(struct crystalhd_adp*); */
/* typedef bool		(*HW_DISABLE_INTR)(struct crystalhd_adp*); */
/* typedef bool		(*HW_ENABLE_INTR)(struct crystalhd_adp*); */
typedef BC_STATUS	(*HW_POST_RX_SIDE_BUFF)(struct crystalhd_hw*,struct crystalhd_rx_dma_pkt*);
typedef bool		(*HW_CHECK_INPUT_FIFO)(struct crystalhd_hw*, uint32_t, uint32_t*,bool,uint8_t*);
typedef void		(*HW_START_TX_DMA)(struct crystalhd_hw*, uint8_t, addr_64);
typedef BC_STATUS	(*HW_STOP_TX_DMA)(struct crystalhd_hw*);
/* typedef bool		(*HW_EVENT_NOTIFICATION)(struct crystalhd_adp*,BRCM_EVENT); */
/* typedef bool		(*HW_RX_POST_INTR_PROCESSING)(struct crystalhd_adp*,uint32_t,uint32_t); */
typedef void		(*HW_GET_DONE_SIZE)(struct crystalhd_hw *hw, uint32_t, uint32_t*, uint32_t*);
/* typedef bool		(*HW_ADD_DRP_TO_FREE_LIST)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
typedef struct crystalhd_dio_req*	(*HW_FETCH_DONE_BUFFERS)(struct crystalhd_adp*,bool);
/* typedef bool		(*HW_ADD_ROLLBACK_RXBUF)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
typedef bool		(*HW_PEEK_NEXT_DECODED_RXBUF)(struct crystalhd_hw*,uint64_t*,uint32_t*,uint32_t);
typedef BC_STATUS	(*HW_FW_PASSTHRU_CMD)(struct crystalhd_hw*,PBC_FW_CMD);
/* typedef bool		(*HW_CANCEL_FW_CMDS)(struct crystalhd_adp*,OS_CANCEL_CALLBACK); */
/* typedef void*	(*HW_GET_FW_DONE_OS_CMD)(struct crystalhd_adp*); */
/* typedef PBC_DRV_PIC_INFO	(*SEARCH_FOR_PIB)(struct crystalhd_adp*,bool,uint32_t); */
/* typedef bool		(*HW_DO_DRAM_PWR_MGMT)(struct crystalhd_adp*); */
typedef BC_STATUS	(*HW_FW_DOWNLOAD)(struct crystalhd_hw*,uint8_t*,uint32_t);
typedef BC_STATUS	(*HW_ISSUE_DECO_PAUSE)(struct crystalhd_hw*, bool);
typedef void		(*HW_STOP_DMA_ENGINES)(struct crystalhd_hw*);
/*
typedef BOOLEAN		(*FIRE_RX_REQ_TO_HW)	(PHW_EXTENSION,PRX_DMA_LIST);
typedef BOOLEAN		(*PIC_POST_PROC)	(PHW_EXTENSION,PRX_DMA_LIST,PULONG);
typedef BOOLEAN		(*HW_ISSUE_DECO_PAUSE)	(PHW_EXTENSION,BOOLEAN,BOOLEAN);
typedef BOOLEAN		(*FIRE_TX_CMD_TO_HW)	(PCONTEXT_FOR_POST_TX);
*/
typedef void		(*NOTIFY_FLL_CHANGE)(struct crystalhd_hw*,bool);
typedef bool		(*HW_EVENT_NOTIFICATION)(struct crystalhd_hw*, enum BRCM_EVENT);

struct crystalhd_hw {
	struct tx_dma_pkt		tx_pkt_pool[DMA_ENGINE_CNT];
	spinlock_t		lock;

	uint32_t		tx_ioq_tag_seed;
	uint32_t		tx_list_post_index;

	struct crystalhd_rx_dma_pkt	*rx_pkt_pool_head;
	uint32_t		rx_pkt_tag_seed;

	bool			dev_started;
	struct crystalhd_adp	*adp;

	wait_queue_head_t	*pfw_cmd_event;
	int			fwcmd_evt_sts;

	uint32_t		pib_del_Q_addr;
	uint32_t		pib_rel_Q_addr;
	uint32_t		channelNum;

	struct crystalhd_dioq		*tx_freeq;
	struct crystalhd_dioq		*tx_actq;

	/* Rx DMA Engine Specific Locks */
	spinlock_t		rx_lock;
	uint32_t		rx_list_post_index;
	enum list_sts		rx_list_sts[DMA_ENGINE_CNT];
	struct crystalhd_dioq	*rx_rdyq;
	struct crystalhd_dioq	*rx_freeq;
	struct crystalhd_dioq	*rx_actq;
	uint32_t		stop_pending;

	uint32_t		hw_pause_issued;

	uint32_t		fwcmdPostAddr;
	uint32_t		fwcmdPostMbox;
	uint32_t		fwcmdRespMbox;

	/* HW counters.. */
	struct crystalhd_hw_stats	stats;

	/* Picture Information Block Management Variables */
	uint32_t	PICWidth;	/* Pic Width Recieved On Format Change for link/With WidthField On Flea*/
	uint32_t	PICHeight;	/* Pic Height Recieved on format change[Link and Flea]/Not Used in Flea*/
	uint32_t	LastPicNo;	/* For Repeated Frame Detection */
	uint32_t	LastTwoPicNo;	/* For Repeated Frame Detection on Interlace clip*/
	uint32_t	LastSessNum;	/* For Session Change Detection */

	struct semaphore fetch_sem; /* semaphore between fetch and probe of the next picture information, since both will be in process context */

	uint32_t	RxCaptureState; /* 0 if capture is not enabled, 1 if capture is enabled, 2 if stop rxdma is pending */

	/* BCM70015 mods */
	uint32_t	PicQSts;		/* This is the bitmap given by PiCQSts Interrupt*/
	uint32_t	TxBuffInfoAddr;		/* Address of the TX Fifo in DRAM*/
	uint32_t	FleaRxPicDelAddr;	/* Memory address where the pictures are fired*/
	uint32_t	FleaFLLUpdateAddr;	/* Memory Address where FLL is updated*/
	uint32_t	FleaBmpIntrCnt;
	uint32_t	RxSeqNum;
	uint32_t	DrvEosDetected;
	uint32_t	DrvCancelEosFlag;

	uint32_t	SkipDropBadFrames;
	uint32_t	TemperatureRegVal;
	TX_INPUT_BUFFER_INFO	TxFwInputBuffInfo;

	enum DECO_STATE			DecoderSt;				/* Weather the decoder is paused or not*/
	uint32_t			PauseThreshold;
	uint32_t			ResumeThreshold;

	uint32_t				RxListPointer;					/* Treat the Rx List As Circular List */
	enum LIST_STATUS				TxList0Sts;
	enum LIST_STATUS				TxList1Sts;

	uint32_t			FleaEnablePWM;
	uint32_t			FleaWaitFirstPlaybackNotify;
	enum FLEA_POWER_STATES	FleaPowerState;
	uint32_t			EmptyCnt;
	bool				SingleThreadAppFIFOEmpty;
	bool				PwrDwnTxIntr; /* Got an TX FIFO status interrupt when in power down state */
	bool				PwrDwnPiQIntr; /* Got a Picture Q interrupt when in power down state */
	uint32_t			OLWatchDogTimer;
	uint32_t			ILWatchDogTimer;
	uint32_t			FwCmdCnt;
	bool				WakeUpDecodeDone; /* Used to indicate that the HW is awake to RX is running so we can actively manage power */

	uint64_t			TickCntDecodePU; /* Time when we first powered up to decode */
	uint64_t			TickSpentInPD; /* Total amount of time spent in PD */
	uint64_t			TickStartInPD; /* Tick count when we start in PD */
	uint32_t			PDRatio; /* % of time spent in power down. Goal is to keep this close to 50 */
	uint32_t			DefaultPauseThreshold; /* default threshold to set when we start power management */

/*	uint32_t			FreeListLen; */
/*	uint32_t			ReadyListLen; */

/* */
/*	Counters needed for monitoring purposes. */
/*	These counters are per session and will be reset to zero in */
/*  start capture. */
/* */
	uint32_t					DrvPauseCnt;					 /* Number of Times the driver has issued pause.*/
#if 0
	uint32_t					DrvServiceIntrCnt;				 /* Number of interrutps the driver serviced. */
	uint32_t					DrvIgnIntrCnt;					 /* Number of Interrupts Driver Ignored.NOT OUR INTR. */
	uint32_t					DrvTotalFrmDropped;				 /* Number of frames dropped by the driver.*/
#endif
	uint32_t					DrvTotalFrmCaptured;			 /* Numner of Good Frames Captured*/
#if 0
	uint32_t					DrvTotalHWErrs;					 /* Total HW Errors.*/
	uint32_t					DrvTotalPIBFlushCnt;			 /* Number of Times the driver flushed PIB Queues.*/
	uint32_t					DrvMissedPIBCnt;				 /* Number of Frames for which the PIB was not found.*/
	uint64_t					TickCntOnPause; */
	uint32_t					TotalTimeInPause;				/* In Milliseconds */
	uint32_t					RepeatedFramesCnt; */
#endif

/*	HW_VERIFY_DEVICE			pfnVerifyDevice; */
/*	HW_INIT_DEVICE_RESOURCES		pfnInitDevResources; */
/*	HW_CLEAN_DEVICE_RESOURCES		pfnCleanDevResources; */
	HW_START_DEVICE				pfnStartDevice;
	HW_STOP_DEVICE				pfnStopDevice;
/*	HW_XLAT_AND_FIRE_SGL			pfnTxXlatAndFireSGL; */
/*	HW_RX_XLAT_SGL				pfnRxXlatSgl; */
	HW_FIND_AND_CLEAR_INTR		pfnFindAndClearIntr;
	HW_READ_DEVICE_REG			pfnReadDevRegister;
	HW_WRITE_DEVICE_REG			pfnWriteDevRegister;
	HW_READ_FPGA_REG			pfnReadFPGARegister;
	HW_WRITE_FPGA_REG			pfnWriteFPGARegister;
	HW_READ_DEV_MEM				pfnDevDRAMRead;
	HW_WRITE_DEV_MEM			pfnDevDRAMWrite;
/*	HW_INIT_DRAM				pfnInitDRAM; */
/*	HW_DISABLE_INTR				pfnDisableIntr; */
/*	HW_ENABLE_INTR				pfnEnableIntr; */
	HW_POST_RX_SIDE_BUFF			pfnPostRxSideBuff;
	HW_CHECK_INPUT_FIFO			pfnCheckInputFIFO;
	HW_START_TX_DMA				pfnStartTxDMA;
	HW_STOP_TX_DMA				pfnStopTxDMA;
	HW_GET_DONE_SIZE			pfnHWGetDoneSize;
/*	HW_EVENT_NOTIFICATION			pfnNotifyHardware; */
/*	HW_ADD_DRP_TO_FREE_LIST			pfnAddRxDRPToFreeList; */
/*	HW_FETCH_DONE_BUFFERS			pfnFetchReadyRxDRP; */
/*	HW_ADD_ROLLBACK_RXBUF			pfnRollBackRxBuf; */
	HW_PEEK_NEXT_DECODED_RXBUF		pfnPeekNextDeodedFr;
	HW_FW_PASSTHRU_CMD			pfnDoFirmwareCmd;
/*	HW_GET_FW_DONE_OS_CMD			pfnGetFWDoneCmdOsCntxt; */
/*	HW_CANCEL_FW_CMDS			pfnCancelFWCmds; */
/*	SEARCH_FOR_PIB				pfnSearchPIB; */
/*	HW_DO_DRAM_PWR_MGMT			pfnDRAMPwrMgmt; */
	HW_FW_DOWNLOAD				pfnFWDwnld;
	HW_ISSUE_DECO_PAUSE			pfnIssuePause;
	HW_STOP_DMA_ENGINES			pfnStopRXDMAEngines;
/*	FIRE_RX_REQ_TO_HW			pfnFireRx; */
/*	PIC_POST_PROC				pfnPostProcessPicture; */
/*	FIRE_TX_CMD_TO_HW			pfnFireTx; */
	NOTIFY_FLL_CHANGE			pfnNotifyFLLChange;
	HW_EVENT_NOTIFICATION		pfnNotifyHardware;
};

struct crystalhd_rx_dma_pkt *crystalhd_hw_alloc_rx_pkt(struct crystalhd_hw *hw);
void crystalhd_hw_free_rx_pkt(struct crystalhd_hw *hw, struct crystalhd_rx_dma_pkt *pkt);
void crystalhd_tx_desc_rel_call_back(void *context, void *data);
void crystalhd_rx_pkt_rel_call_back(void *context, void *data);
void crystalhd_hw_delete_ioqs(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_create_ioqs(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_open(struct crystalhd_hw *hw, struct crystalhd_adp *adp);
BC_STATUS crystalhd_hw_close(struct crystalhd_hw *hw, struct crystalhd_adp *adp);
BC_STATUS crystalhd_hw_setup_dma_rings(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_free_dma_rings(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_tx_req_complete(struct crystalhd_hw *hw, uint32_t list_id, BC_STATUS cs);
BC_STATUS crystalhd_hw_fill_desc(struct crystalhd_dio_req *ioreq,
				struct dma_descriptor *desc,
				dma_addr_t desc_paddr_base,
				uint32_t sg_cnt, uint32_t sg_st_ix,
				uint32_t sg_st_off, uint32_t xfr_sz,
				struct device *dev, uint32_t destDRAMaddr);
BC_STATUS crystalhd_xlat_sgl_to_dma_desc(struct crystalhd_dio_req *ioreq,
					struct dma_desc_mem * pdesc_mem,
					uint32_t *uv_desc_index,
					struct device *dev, uint32_t destDRAMaddr);
BC_STATUS crystalhd_rx_pkt_done(struct crystalhd_hw *hw,
				uint32_t list_index,
				BC_STATUS comp_sts);
BC_STATUS crystalhd_hw_post_tx(struct crystalhd_hw *hw, struct crystalhd_dio_req *ioreq,
				hw_comp_callback call_back,
				wait_queue_head_t *cb_event, uint32_t *list_id,
				uint8_t data_flags);
BC_STATUS crystalhd_hw_cancel_tx(struct crystalhd_hw *hw, uint32_t list_id);
BC_STATUS crystalhd_hw_add_cap_buffer(struct crystalhd_hw *hw,struct crystalhd_dio_req *ioreq, bool en_post);
BC_STATUS crystalhd_hw_get_cap_buffer(struct crystalhd_hw *hw,struct C011_PIB *pib,struct crystalhd_dio_req **ioreq);
BC_STATUS crystalhd_hw_start_capture(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_stop_capture(struct crystalhd_hw *hw, bool unmap);
BC_STATUS crystalhd_hw_suspend(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_resume(struct crystalhd_hw *hw);
void crystalhd_hw_stats(struct crystalhd_hw *hw, struct crystalhd_hw_stats *stats);

#define GET_Y0_ERR_MSK (MISC1_Y_RX_ERROR_STATUS_RX_L0_OVERRUN_ERROR_MASK |		\
						MISC1_Y_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK |		\
						MISC1_Y_RX_ERROR_STATUS_RX_L0_DESC_TX_ABORT_ERRORS_MASK |	\
						MISC1_Y_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK)

#define GET_UV0_ERR_MSK (MISC1_UV_RX_ERROR_STATUS_RX_L0_OVERRUN_ERROR_MASK |		\
						MISC1_UV_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK |		\
						MISC1_UV_RX_ERROR_STATUS_RX_L0_DESC_TX_ABORT_ERRORS_MASK |	\
						MISC1_UV_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK)

#define GET_Y1_ERR_MSK (MISC1_Y_RX_ERROR_STATUS_RX_L1_OVERRUN_ERROR_MASK |		\
						MISC1_Y_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK |		\
						MISC1_Y_RX_ERROR_STATUS_RX_L1_DESC_TX_ABORT_ERRORS_MASK |	\
						MISC1_Y_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK)

#define GET_UV1_ERR_MSK	(MISC1_UV_RX_ERROR_STATUS_RX_L1_OVERRUN_ERROR_MASK |		\
						MISC1_UV_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK |		\
						MISC1_UV_RX_ERROR_STATUS_RX_L1_DESC_TX_ABORT_ERRORS_MASK |	\
						MISC1_UV_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK)

#endif
