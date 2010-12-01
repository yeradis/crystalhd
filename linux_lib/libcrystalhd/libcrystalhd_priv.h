/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_priv.h
 *
 *  Description: Driver Interface library Interanl.
 *
 *  AU
 *
 *  HISTORY:
 *
 ********************************************************************
 *
 * This file is part of libcrystalhd.
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 *******************************************************************/

#ifndef _BCM_DRV_IF_PRIV_
#define _BCM_DRV_IF_PRIV_

#include <semaphore.h>
#include "bc_dts_glob_lnx.h"
#include "libcrystalhd_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

enum _bc_ldil_log_level{
        LDIL_ERR            = 0x80000000,   /* Don't disable this option */

        /* Following are allowed only in debug mode */
        LDIL_INFO             = 0x00000001,   /* Generic informational */
        LDIL_DBG              = 0x00000002,   /* First level Debug info */
};

#define LDIL_PRINTS_ON 1
#if LDIL_PRINTS_ON
#define DebugLog_Trace(_tl, fmt, args...)	printf(fmt, ##args);
#else
#define DebugLog_Trace(_tl, fmt, args...)
#endif
#define bc_sleep_ms(x)	usleep(1000*x)

/* Some of the globals from bc_dts_glob.h. are defined
 * here that are applicable only to ldil.
 */
enum _crystalhd_ldil_globals {
	BC_EOS_PIC_COUNT	= 16,			/* EOS check counter..*/
	BC_INPUT_MDATA_POOL_SZ  = 1024,			/* Input Meta Data Pool size */
	BC_INPUT_MDATA_POOL_SZ_COLLECT  = 256,		/* Input Meta Data Pool size for collector */
	BC_MAX_SW_VOUT_BUFFS    = BC_RX_LIST_CNT,	/* MAX - pre allocated buffers..*/
	RX_START_DELIVERY_THRESHOLD = 0,
	PAUSE_DECODER_THRESHOLD = 12,
	RESUME_DECODER_THRESHOLD = 5,
	FLEA_RT_PD_THRESHOLD = 14,
	FLEA_RT_PU_THRESHOLD = 3,
	HARDWARE_INIT_RETRY_CNT = 10,
	HARDWARE_INIT_RETRY_LINK_CNT = 1,
};

enum _DtsRunState {
	BC_DEC_STATE_CLOSE		= 0x00,
	BC_DEC_STATE_STOP		= 0x01,
	BC_DEC_STATE_START		= 0x02,
	BC_DEC_STATE_PAUSE		= 0x03,
	BC_DEC_STATE_FLUSH		= 0x04
};

/* Bit fields */
enum _BCMemTypeFlags {
        BC_MEM_DEC_YUVBUFF  = 0x1,
	BC_MEM_USER_MODE_ALLOC	= 0x80000000,
};

enum _STCapParams{
	NO_PARAM				= 0,
	ST_CAP_IMMIDIATE		= 0x01,
};

/* Application specific run-time configurations */
enum _DtsAppSpecificCfgFlags {
	BC_MPOOL_FLAGS_DEF	= 0x000,
	BC_MPOOL_INCL_YUV_BUFFS	= 0x001,	/* Include YUV Buffs allocation */
	BC_DEC_EN_DUART		= 0x002,	/* Enable DUART for FW log */
	BC_DEC_INIT_MEM		= 0x004,	/* Initialize Entire DRAM takes about a min */
	BC_DEC_VCLK_74MHZ	= 0x008,	/* Enable Vidoe clock to 75 MHZ */
	BC_EN_DIAG_MODE		= 0x010,	/* Enable Diag Mode application */
	BC_PIX_WID_1080		= 0x020,	/* FIX_ME: deprecate this after PIB work */
	BC_ADDBUFF_MOVE		= 0x040,	/* FIX_ME: Deleteme after testing.. */
	BC_DEC_VCLK_77MHZ	= 0x080		/* Enable Vidoe clock to 77 MHZ */

};

#define BC_DTS_DEF_CFG		(BC_MPOOL_INCL_YUV_BUFFS | BC_EN_DIAG_MODE | BC_DEC_VCLK_74MHZ | BC_ADDBUFF_MOVE)

/* !!!!Note!!!!
 * Don't forget to change this value
 * while changing the file names
 */
#define BC_MAX_FW_FNAME_SIZE	32
#define MAX_PATH		256

#define FWBINFILE_70012	"bcm70012fw.bin"
#define FWBINFILE_70015	"bcm70015fw.bin"

#define BC_DTS_DEF_OPTIONS	0x0D
#define BC_DTS_DEF_OPTIONS_LINK	0xB0000005

#define BC_FW_CMD_TIMEOUT		2
//Use the Last Un-Fetched One
#define MAX_DISORDER_GAP	5

#define ALIGN_BUF_SIZE	(512*1024)
#define CIRC_TX_BUF_SIZE (1024*1024)

#define	 BC_EOS_DETECTED		0xffffffff

typedef struct _DTS_MPOOL_TYPE {
	uint32_t	type;
	uint32_t	sz;
	uint8_t		*buff;
} DTS_MPOOL_TYPE;

#define LIB_CTX_SIG	0x11223344

typedef struct _DTS_VIDEO_PARAMS {
	uint32_t	VideoAlgo;
	uint32_t	WidthInPixels;
	uint32_t	HeightInPixels;
	BOOL		FGTEnable;
	BOOL		MetaDataEnable;
	BOOL		Progressive;
	uint32_t	FrameRate;
	uint32_t	OptFlags; //currently has the DEc_operation_mode in bits 4 and 5, bits 0:3 have the default framerate, Ignore frame rate is bit 6. Bit 7 is SingleThreadedAppMode
	BC_MEDIA_SUBTYPE MediaSubType;
	uint32_t	StartCodeSz;
	uint8_t		*pMetaData;
	uint32_t	MetaDataSz;
	uint32_t	NumOfRefFrames;
	uint32_t	LevelIDC;
	uint32_t	StreamType;
} DTS_VIDEO_PARAMS;

/* Input MetaData handling.. */
typedef struct _BC_SEQ_HDR_FORMAT{
	uint8_t		StartCode[4];
	uint8_t		PacketLen;
	uint8_t		StartCodeEnd;
	uint8_t		SeqNum[2];
	uint8_t		Command;
	uint8_t		PicLength[2];
	uint8_t		Reserved;
}BC_SEQ_HDR_FORMAT;

typedef struct _BC_PES_HDR_FORMAT{
	uint8_t		StartCode[4];
	uint8_t		PacketLen[2];
	uint8_t		OptPesHdr[2];
	uint8_t		optPesHdrLen;
}BC_PES_HDR_FORMAT;

typedef struct _DTS_INPUT_MDATA{
	struct _DTS_INPUT_MDATA	*flink;
	struct _DTS_INPUT_MDATA	*blink;
	uint32_t			IntTag;
	uint32_t			Reserved;
	uint64_t			appTimeStamp;
	BC_SEQ_HDR_FORMAT	Spes;
}DTS_INPUT_MDATA;


#define DTS_MDATA_PEND_LINK(_c)	( (DTS_INPUT_MDATA *)&_c->MDPendHead )

#define DTS_MDATA_TAG_MASK		(0x00010000)
#define DTS_MDATA_MAX_TAG		(0x0000FFFF)

typedef struct _TXBUFFER{
	uint32_t	readPointer; // Index into the buffer for next read
	uint32_t	writePointer; // Index into the buffer for next write
	uint32_t	freeSize;
	uint32_t	totalSize;
	uint32_t	busySize;
	uint8_t		*basePointer; // First byte that can be written
	uint8_t		*endPointer; // Last byte that can be written
	uint8_t		*buffer;
	pthread_mutex_t flushLock; // LOCK used only for flushing
	pthread_mutex_t pushpopLock; // LOCK for push and pop operations
}TXBUFFER, *pTXBUFFER;

BC_STATUS txBufPush(pTXBUFFER txBuf, uint8_t* bufToPush, uint32_t sizeToPush);
BC_STATUS txBufPop(pTXBUFFER txBuf, uint8_t* bufToPop, uint32_t sizeToPop);
BC_STATUS txBufFlush(pTXBUFFER txBuf);
BC_STATUS txBufInit(pTXBUFFER txBuf, uint32_t sizeInit);
BC_STATUS txBufFree(pTXBUFFER txBuf);

// TX Thread function
void * txThreadProc(void *ctx);

typedef struct _DTS_LIB_CONTEXT{
	uint32_t				Sig;			/* Mazic number */
	uint32_t				State;			/* DIL's Run State */
	int				DevHandle;		/* Driver handle */
	BC_IOCTL_DATA	*pIoDataFreeHd;	/* IOCTL data pool head */
	DTS_MPOOL_TYPE	*Mpools;		/* List of memory pools created */
	uint32_t				MpoolCnt;		/* Number of entries */
	uint32_t				CfgFlags;		/* Application specifi flags */
	uint32_t				OpMode;			/* Mode of operation playback etc..*/
	uint32_t				DevId;			/* HW Device ID */
	uint32_t				hwRevId;		/* HW revision ID */
	uint32_t				VendorId;		/* HW vendor ID - should always be Broadcom 0x14e4 */
	uint32_t				fwcmdseq;		/* FW Cmd Sequence number */
	uint32_t				FixFlags;		/* Flags for conditionally enabling fixes */

	pthread_mutex_t  thLock;

	DTS_VIDEO_PARAMS VidParams;		/* App specific Video Params */

	DecRspChannelStreamOpen OpenRsp;/* Channel Open Response */
	DecRspChannelStartVideo	sVidRsp;/* Start Video Response */

	/* Proc Output Related */
	BOOL			ProcOutPending;	/* To avoid muliple ProcOuts */
	BOOL			CancelWaiting;	/* Notify FetchOut to signal */

	/* pOutData is dedicated for ProcOut() use only. Every other
	 * Interface should use the memory from IocData pool. This
	 * is to provide priority for ProcOut() interface due to its
	 * performance criticality.
	 *
	 * !!!!!NOTE!!!!
	 *
	 * Using this data structures for other interfaces, will cause
	 * thread related race conditions.
	 */
	BC_IOCTL_DATA	*pOutData;		/* Current Active Proc Out Buffer */

	/* Place Holder for FW file paths */
	char			StreamFile[MAX_PATH+1];
	char			VidInner[MAX_PATH+1];
	char			VidOuter[MAX_PATH+1];

	uint32_t		InMdataTag;				/* InMetaData Tag for TimeStamp */
	void			*MdataPoolPtr;			/* allocated memory PoolPointer */

	struct _DTS_INPUT_MDATA	*MDFreeHead;	/* MetaData Free List Head */

	struct _DTS_INPUT_MDATA	*MDPendHead;	/* MetaData Pending List Head */
	struct _DTS_INPUT_MDATA	*MDPendTail;	/* MetaData Pending List Tail */

	//Reserve the Last Fetch Tag
	uint32_t		MDLastFetchTag;

	/* End Of Stream detection */
	BOOL			bEOSCheck;				/* Flag to start EOS detection */
	uint32_t		EOSCnt;					/* Last picture repetition count */
	uint32_t		DrvStatusEOSCnt;
	uint32_t		LastPicNum;				/* Last picture number */
	uint32_t		LastSessNum;			/* Last session number */
	uint8_t			PullDownFlag;
	BOOL			bEOS;

	/* Statistics Related */
	uint32_t		prevPicNum;				/* Previous received frame */
	uint32_t		CapState;				/* 0 = Not started, 1 = Interlaced, 2 = progressive */
	uint32_t		PibIntToggle;			/* Toggle flag to detect PIB miss in Interlaced mode.*/
	uint32_t		prevFrameRate;			/* Previous frame rate */

	BC_REG_CONFIG	RegCfg;					/* Registry Configurable options.*/

	char			FwBinFile[MAX_PATH+1];	/* Firmware Bin file place holder */

	BC_OUTPUT_FORMAT b422Mode;				/* 422 Mode Identifier for Link */
	uint32_t		HWOutPicWidth;
	uint32_t		HWOutPicHeight;

	char			DilPath[MAX_PATH+1];	/* DIL runtime Location.. */

	uint8_t			SingleThreadedAppMode;	/* flag to indicate that we are running in single threaded mode */
	bool			hw_paused;
	bool			fw_cmd_issued;
	PES_CONVERT_PARAMS	PESConvParams;
	BC_HW_CAPS		capInfo;
//	uint16_t		InSampleCount;
	uint8_t			bMapOutBufDone;

	BC_PIC_INFO_BLOCK	FormatInfo;

	TXBUFFER		circBuf;
	bool			txThreadExit; // Handle to event to indicate to the tx thread to exit
	pthread_t		htxThread; // Handle to TX thread
	uint8_t			*alignBuf;

	uint32_t		EnableScaling;
	uint8_t			bEnable720pDropHalf;
	pid_t			ProcessID;
	uint32_t		nRefNum;
	uint32_t		DrvMode;

}DTS_LIB_CONTEXT;

/* Helper macro to get lib context from user handle */
#define DTS_GET_CTX(_uh, _c)											\
{																		\
	if( !(_c = DtsGetContext(_uh)) ){									\
		return BC_STS_INV_ARG;											\
	}																	\
}


extern DTS_LIB_CONTEXT	*	DtsGetContext(HANDLE userHnd);
BOOL DtsIsPend(DTS_LIB_CONTEXT	*Ctx);
BOOL DtsDrvIoctl
(
	  HANDLE	hDevice,
	  DWORD		dwIoControlCode,
	  LPVOID	lpInBuffer,
	  DWORD		nInBufferSize,
	  LPVOID	lpOutBuffer,
	  DWORD		nOutBufferSize,
	  LPDWORD	lpBytesReturned,
	  BOOL		Async
);
BC_STATUS DtsDrvCmd(DTS_LIB_CONTEXT	*Ctx, DWORD Code, BOOL Async, BC_IOCTL_DATA *pIoData, BOOL Rel);
void DtsRelIoctlData(DTS_LIB_CONTEXT *Ctx, BC_IOCTL_DATA *pIoData);
BC_IOCTL_DATA *DtsAllocIoctlData(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsAllocMemPools(DTS_LIB_CONTEXT *Ctx);
void DtsReleaseMemPools(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsAddOutBuff(DTS_LIB_CONTEXT *Ctx, PVOID buff, uint32_t BuffSz, uint32_t flags);
BC_STATUS DtsRelRxBuff(DTS_LIB_CONTEXT *Ctx, BC_DEC_YUV_BUFFS *buff,BOOL SkipAddBuff);
BC_STATUS DtsFetchOutInterruptible(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *DecOut, uint32_t dwTimeout);
BC_STATUS DtsCancelFetchOutInt(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsMapYUVBuffs(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsInitInterface(int hDevice,HANDLE *RetCtx, uint32_t mode);
BC_STATUS DtsSetupConfig(DTS_LIB_CONTEXT *Ctx, uint32_t did, uint32_t rid, uint32_t FixFlags);
BC_STATUS DtsReleaseInterface(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsGetBCRegConfig(DTS_LIB_CONTEXT	*Ctx);
BC_STATUS DtsGetFirmwareFiles(DTS_LIB_CONTEXT	*Ctx);
DTS_INPUT_MDATA	*DtsAllocMdata(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsFreeMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata, BOOL sync);
BC_STATUS DtsClrPendMdataList(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsInsertMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata);
BC_STATUS DtsRemoveMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata, BOOL sync);
BC_STATUS DtsFetchMdata(DTS_LIB_CONTEXT *Ctx, uint16_t snum, BC_DTS_PROC_OUT *pout);
BC_STATUS DtsFetchTimeStampMdata(DTS_LIB_CONTEXT *Ctx, uint16_t snum, uint64_t *TimeStamp);
BC_STATUS DtsPrepareMdataASFHdr(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA *mData, uint8_t* buf);
BC_STATUS DtsPrepareMdata(DTS_LIB_CONTEXT *Ctx, uint64_t timeStamp, DTS_INPUT_MDATA **mData, uint8_t** pDataBuf, uint32_t *pSize);
BC_STATUS DtsNotifyOperatingMode(HANDLE hDevice, uint32_t Mode);
BC_STATUS DtsReleaseUserHandle(DTS_LIB_CONTEXT *Ctx);
BC_STATUS DtsGetHWFeatures(uint32_t *pciids);
BC_STATUS DtsSetupHardware(HANDLE hDevice, BOOL IgnClkChk);

/* Internal helper function */
uint32_t DtsGetWidthfromResolution(DTS_LIB_CONTEXT *Ctx, uint32_t Resolution);

/*====================== Performance Counter Routines ============================*/
void DtsUpdateInStats(DTS_LIB_CONTEXT	*Ctx, uint32_t	size);
void DtsUpdateOutStats(DTS_LIB_CONTEXT	*Ctx, BC_DTS_PROC_OUT *pOut);

/*============== Global shared area usage ======================*/
#define BC_DIL_HWINIT_NOT_YET		0
#define BC_DIL_HWINIT_IN_PROGRESS	1
#define BC_DIL_HWINIT_DONE		2

#define BC_DIL_SHMEM_KEY 0xBABEFACE

typedef struct _bc_dil_glob_s{
	uint32_t 		gDilOpMode;
	uint32_t 		gHwInitSts;
	BC_DTS_STATS 	stats;
	pid_t			g_nProcID;
	bool			g_bDecOpened;
	uint32_t 		DevID;
} bc_dil_glob_s;


BC_STATUS DtsGetDilShMem(uint32_t shmid);
BC_STATUS DtsDelDilShMem(void);
BC_STATUS DtsCreateShMem(int *shmem_id);

/* DTS Global Parameters Utility functions */
uint32_t 		DtsGetOPMode(void);
void 			DtsSetOPMode(uint32_t value);
uint32_t 		DtsGetHwInitSts(void);
void 			DtsSetHwInitSts(uint32_t value);
void 			DtsRstStats(void);
BC_DTS_STATS * 	DtsGetgStats (void);
uint32_t		DtsGetgDevID(void);
void DtsSetgDevID(uint32_t DevID);

BC_STATUS DtsGetDevType(uint32_t *pDevID, uint32_t *pVendID, uint32_t *pRevID);
uint32_t DtsGetDevID();

bool DtsIsDecOpened(pid_t nNewPID);
void DtsSetDecStat(bool bDecOpen, pid_t PID);
bool DtsChkPID(pid_t nCurPID);

void DtsLock(DTS_LIB_CONTEXT	*Ctx);
void DtsUnLock(DTS_LIB_CONTEXT	*Ctx);

/*====================== Debug Routines ========================================*/
void DtsTestMdata(DTS_LIB_CONTEXT	*gCtx);
BOOL DtsDbgCheckPointers(DTS_LIB_CONTEXT *Ctx,BC_IOCTL_DATA *pIo);

BOOL DtsCheckRptPic(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut);
BC_STATUS DtsUpdateVidParams(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut);

#ifdef __cplusplus
}
#endif

#endif

