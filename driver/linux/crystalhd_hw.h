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
#include <linux/device.h>
#include "crystalhd_fw_if.h"
#include "crystalhd_misc.h"

/* HW constants..*/
#define DMA_ENGINE_CNT		2
#define MAX_PIB_Q_DEPTH		64
#define MIN_PIB_Q_DEPTH		2
#define WR_POINTER_OFF		4

typedef struct _BC_DRV_PIC_INFO_{
	C011_PIB			DecoPIB;
	struct _BC_DRV_PIC_INFO_	*Flink;
} BC_DRV_PIC_INFO, *PBC_DRV_PIC_INFO;

typedef union _desc_low_addr_reg_ {
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

} desc_low_addr_reg;

typedef struct _dma_descriptor_ {	/* 8 32-bit values */
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
} dma_descriptor, *pdma_descriptor;

/*
 * We will allocate the memory in 4K pages
 * the linked list will be a list of 32 byte descriptors.
 * The  virtual address will determine what should be freed.
 */
typedef struct _dma_desc_mem_ {
	pdma_descriptor		pdma_desc_start; /* 32-bytes for dma descriptor. should be first element */
	dma_addr_t		phy_addr;	/* physical address of each DMA desc */
	uint32_t		sz;
	struct _dma_desc_mem_	*Next;		/* points to Next Descriptor in chain */

} dma_desc_mem, *pdma_desc_mem;



typedef enum _list_sts_ {
	sts_free = 0,

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

} list_sts;

typedef struct _tx_dma_pkt_ {
	dma_desc_mem		desc_mem;
	hw_comp_callback	call_back;
	crystalhd_dio_req		*dio_req;
	wait_queue_head_t	*cb_event;
	uint32_t		list_tag;

} tx_dma_pkt;

typedef struct _crystalhd_rx_dma_pkt {
	dma_desc_mem			desc_mem;
	crystalhd_dio_req			*dio_req;
	uint32_t			pkt_tag;
	uint32_t			flags;
	BC_PIC_INFO_BLOCK		pib;
	dma_addr_t			uv_phy_addr;
	struct  _crystalhd_rx_dma_pkt	*next;

} crystalhd_rx_dma_pkt;

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

struct crystalhd_hw; /* forward declaration for the types */

/* typedef void*	(*HW_VERIFY_DEVICE)(struct crystalhd_adp*); */
/* typedef bool	(*HW_INIT_DEVICE_RESOURCES)(struct crystalhd_adp*); */
/* typedef bool	(*HW_CLEAN_DEVICE_RESOURCES)(struct crystalhd_adp*); */
typedef bool	(*HW_START_DEVICE)(struct crystalhd_hw*);
typedef bool	(*HW_STOP_DEVICE)(struct crystalhd_hw*);
/* typedef bool	(*HW_XLAT_AND_FIRE_SGL)(struct crystalhd_adp*,PVOID,PSCATTER_GATHER_LIST,uint32_t); */
/* typedef bool	(*HW_RX_XLAT_SGL)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
/* typedef bool	(*HW_FIND_AND_CLEAR_INTR)(struct crystalhd_adp*,uint32_t*,uint32_t*); */
typedef uint32_t	(*HW_READ_DEVICE_REG)(struct crystalhd_adp*,uint32_t);
typedef void	(*HW_WRITE_DEVICE_REG)(struct crystalhd_adp*,uint32_t,uint32_t);
typedef uint32_t	(*HW_READ_FPGA_REG)(struct crystalhd_adp*,uint32_t);
typedef void	(*HW_WRITE_FPGA_REG)(struct crystalhd_adp*,uint32_t,uint32_t);
typedef BC_STATUS	(*HW_READ_DEV_MEM)(struct crystalhd_hw*,uint32_t,uint32_t,uint32_t*);
typedef BC_STATUS	(*HW_WRITE_DEV_MEM)(struct crystalhd_hw*,uint32_t,uint32_t,uint32_t*);
/* typedef bool	(*HW_INIT_DRAM)(struct crystalhd_adp*); */
/* typedef bool	(*HW_DISABLE_INTR)(struct crystalhd_adp*); */
/* typedef bool	(*HW_ENABLE_INTR)(struct crystalhd_adp*); */
typedef BC_STATUS	(*HW_POST_RX_SIDE_BUFF)(struct crystalhd_hw*,crystalhd_rx_dma_pkt*);
typedef bool	(*HW_CHECK_INPUT_FIFO)(struct crystalhd_hw*, uint32_t, uint32_t*,bool,uint8_t);
typedef void	(*HW_START_TX_DMA)(struct crystalhd_hw*, uint8_t, addr_64);
typedef BC_STATUS	(*HW_STOP_TX_DMA)(struct crystalhd_hw*);
/* typedef bool	(*HW_EVENT_NOTIFICATION)(struct crystalhd_adp*,BRCM_EVENT); */
/* typedef bool	(*HW_RX_POST_INTR_PROCESSING)(struct crystalhd_adp*,uint32_t,uint32_t); */
typedef void    (*HW_GET_DONE_SIZE)(struct crystalhd_hw *hw, uint32_t, uint32_t*, uint32_t*);
/* typedef bool	(*HW_ADD_DRP_TO_FREE_LIST)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
typedef crystalhd_dio_req*	(*HW_FETCH_DONE_BUFFERS)(struct crystalhd_adp*,bool);
/* typedef bool	(*HW_ADD_ROLLBACK_RXBUF)(struct crystalhd_adp*,crystalhd_dio_req *ioreq); */
typedef bool	(*HW_PEEK_NEXT_DECODED_RXBUF)(struct crystalhd_hw*,uint32_t*,uint32_t);
typedef BC_STATUS	(*HW_FW_PASSTHRU_CMD)(struct crystalhd_hw*,PBC_FW_CMD);
/* typedef bool	(*HW_CANCEL_FW_CMDS)(struct crystalhd_adp*,OS_CANCEL_CALLBACK); */
/* typedef void*	(*HW_GET_FW_DONE_OS_CMD)(struct crystalhd_adp*); */
/* typedef PBC_DRV_PIC_INFO	(*SEARCH_FOR_PIB)(struct crystalhd_adp*,bool,uint32_t); */
/* typedef bool	(*HW_DO_DRAM_PWR_MGMT)(struct crystalhd_adp*); */
typedef BC_STATUS	(*HW_FW_DOWNLOAD)(struct crystalhd_hw*,uint8_t*,uint32_t);
typedef BC_STATUS	(*HW_ISSUE_DECO_PAUSE)(struct crystalhd_hw*, bool);
typedef void	(*HW_STOP_DMA_ENGINES)(struct crystalhd_hw*);
/*
typedef BOOLEAN	(*FIRE_RX_REQ_TO_HW)	(PHW_EXTENSION,PRX_DMA_LIST);
typedef BOOLEAN (*PIC_POST_PROC)	(PHW_EXTENSION,PRX_DMA_LIST,PULONG);
typedef BOOLEAN (*HW_ISSUE_DECO_PAUSE)	(PHW_EXTENSION,BOOLEAN,BOOLEAN);
typedef BOOLEAN (*FIRE_TX_CMD_TO_HW)	(PCONTEXT_FOR_POST_TX);
typedef VOID	(*NOTIFY_FLL_CHANGE)	(PHW_EXTENSION,ULONG,BOOLEAN);
*/

struct crystalhd_hw {
	tx_dma_pkt		tx_pkt_pool[DMA_ENGINE_CNT];
	spinlock_t		lock;

	uint32_t		tx_ioq_tag_seed;
	uint32_t		tx_list_post_index;

	crystalhd_rx_dma_pkt	*rx_pkt_pool_head;
	uint32_t		rx_pkt_tag_seed;

	bool			dev_started;
	struct crystalhd_adp	*adp;

	wait_queue_head_t	*pfw_cmd_event;
	int			fwcmd_evt_sts;

	uint32_t		pib_del_Q_addr;
	uint32_t		pib_rel_Q_addr;

	crystalhd_dioq_t	*tx_freeq;
	crystalhd_dioq_t	*tx_actq;

	/* Rx DMA Engine Specific Locks */
	spinlock_t		rx_lock;
	uint32_t		rx_list_post_index;
	list_sts		rx_list_sts[DMA_ENGINE_CNT];
	crystalhd_dioq_t	*rx_rdyq;
	crystalhd_dioq_t	*rx_freeq;
	crystalhd_dioq_t	*rx_actq;
	uint32_t		stop_pending;

	uint32_t		hw_pause_issued;

	/* HW counters.. */
	struct crystalhd_hw_stats	stats;

	/* Picture Information Block Management Variables */
	uint32_t	PICWidth;	/* Pic Width Recieved On Format Change for link/With WidthField On Flea*/
	uint32_t	PICHeight;	/* Pic Height Recieved on format change[Link and Flea]/Not Used in Flea*/
	uint32_t	LastPicNo;	/* For Repeated Frame Detection */
	uint32_t	LastTwoPicNo;	/* For Repeated Frame Detection on Interlace clip*/
	uint32_t	LastSessNum;	/* For Session Change Detection */


//	HW_VERIFY_DEVICE				pfnVerifyDevice;
//	HW_INIT_DEVICE_RESOURCES		pfnInitDevResources;
//	HW_CLEAN_DEVICE_RESOURCES		pfnCleanDevResources;
	HW_START_DEVICE					pfnStartDevice;
	HW_STOP_DEVICE					pfnStopDevice;
//	HW_XLAT_AND_FIRE_SGL			pfnTxXlatAndFireSGL;
//	HW_RX_XLAT_SGL					pfnRxXlatSgl;
//	HW_FIND_AND_CLEAR_INTR			pfnFindAndClearIntr;
	HW_READ_DEVICE_REG				pfnReadDevRegister;
	HW_WRITE_DEVICE_REG				pfnWriteDevRegister;
	HW_READ_FPGA_REG				pfnReadFPGARegister;
	HW_WRITE_FPGA_REG				pfnWriteFPGARegister;
	HW_READ_DEV_MEM					pfnDevDRAMRead;
	HW_WRITE_DEV_MEM				pfnDevDRAMWrite;
//	HW_INIT_DRAM					pfnInitDRAM;
//	HW_DISABLE_INTR					pfnDisableIntr;
//	HW_ENABLE_INTR					pfnEnableIntr;
	HW_POST_RX_SIDE_BUFF			pfnPostRxSideBuff;
	HW_CHECK_INPUT_FIFO				pfnCheckInputFIFO;
	HW_START_TX_DMA					pfnStartTxDMA;
	HW_STOP_TX_DMA					pfnStopTxDMA;
	HW_GET_DONE_SIZE				pfnHWGetDoneSize;
//	HW_EVENT_NOTIFICATION			pfnNotifyHardware;
//	HW_ADD_DRP_TO_FREE_LIST			pfnAddRxDRPToFreeList;
//	HW_FETCH_DONE_BUFFERS			pfnFetchReadyRxDRP;
//	HW_ADD_ROLLBACK_RXBUF			pfnRollBackRxBuf;
	HW_PEEK_NEXT_DECODED_RXBUF		pfnPeekNextDeodedFr;
	HW_FW_PASSTHRU_CMD				pfnDoFirmwareCmd;
//	HW_GET_FW_DONE_OS_CMD			pfnGetFWDoneCmdOsCntxt;
//	HW_CANCEL_FW_CMDS				pfnCancelFWCmds;
//	SEARCH_FOR_PIB					pfnSearchPIB;
//	HW_DO_DRAM_PWR_MGMT				pfnDRAMPwrMgmt;
	HW_FW_DOWNLOAD					pfnFWDwnld;
	HW_ISSUE_DECO_PAUSE				pfnIssuePause;
	HW_STOP_DMA_ENGINES				pfnStopRXDMAEngines;
/*
	FIRE_RX_REQ_TO_HW				pfnFireRx;
	PIC_POST_PROC					pfnPostProcessPicture;

	FIRE_TX_CMD_TO_HW				pfnFireTx;
	NOTIFY_FLL_CHANGE				pfnNotifyFLLChange;
*/

};

crystalhd_rx_dma_pkt *crystalhd_hw_alloc_rx_pkt(struct crystalhd_hw *hw);
void crystalhd_hw_free_rx_pkt(struct crystalhd_hw *hw, crystalhd_rx_dma_pkt *pkt);
void crystalhd_tx_desc_rel_call_back(void *context, void *data);
void crystalhd_rx_pkt_rel_call_back(void *context, void *data);
void crystalhd_hw_delete_ioqs(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_create_ioqs(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_open(struct crystalhd_hw *hw, struct crystalhd_adp *adp);
BC_STATUS crystalhd_hw_close(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_setup_dma_rings(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_free_dma_rings(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_tx_req_complete(struct crystalhd_hw *hw, uint32_t list_id, BC_STATUS cs);
BC_STATUS crystalhd_hw_fill_desc(crystalhd_dio_req *ioreq,
									dma_descriptor *desc,
									dma_addr_t desc_paddr_base,
									uint32_t sg_cnt, uint32_t sg_st_ix,
									uint32_t sg_st_off, uint32_t xfr_sz,
									struct device *dev);
BC_STATUS crystalhd_xlat_sgl_to_dma_desc(crystalhd_dio_req *ioreq,
											pdma_desc_mem pdesc_mem,
											uint32_t *uv_desc_index,
											struct device *dev);
BC_STATUS crystalhd_rx_pkt_done(struct crystalhd_hw *hw,
									uint32_t list_index,
									BC_STATUS comp_sts);
BC_STATUS crystalhd_hw_post_tx(struct crystalhd_hw *hw, crystalhd_dio_req *ioreq,
								hw_comp_callback call_back,
								wait_queue_head_t *cb_event, uint32_t *list_id,
								uint8_t data_flags);
BC_STATUS crystalhd_hw_cancel_tx(struct crystalhd_hw *hw, uint32_t list_id);
BC_STATUS crystalhd_hw_add_cap_buffer(struct crystalhd_hw *hw,crystalhd_dio_req *ioreq, bool en_post);
BC_STATUS crystalhd_hw_get_cap_buffer(struct crystalhd_hw *hw,BC_PIC_INFO_BLOCK *pib,crystalhd_dio_req **ioreq);
BC_STATUS crystalhd_hw_start_capture(struct crystalhd_hw *hw);
BC_STATUS crystalhd_hw_stop_capture(struct crystalhd_hw *hw, bool unmap);
BC_STATUS crystalhd_hw_suspend(struct crystalhd_hw *hw);
void crystalhd_hw_stats(struct crystalhd_hw *hw, struct crystalhd_hw_stats *stats);

// Includes for chip specific stuff
#include "crystalhd_linkfuncs.h"

#endif
