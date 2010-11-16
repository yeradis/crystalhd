/***************************************************************************
 * Copyright (c) 2005-2009, Broadcom Corporation.
 *
 *  Name: crystalhd_linkfuncs . h
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

#ifndef _CRYSTALHD_LINKFUNCS_H_
#define _CRYSTALHD_LINKFUNCS_H_

#define ASPM_L1_ENABLE		(BC_BIT(27))

/*************************************************
  7412 Decoder  Registers.
**************************************************/
#define FW_CMD_BUFF_SZ		64
#define TS_Host2CpuSnd		0x00000100
#define HW_PauseMbx		0x00000300
#define Hst2CpuMbx1		0x00100F00
#define Cpu2HstMbx1		0x00100F04
#define MbxStat1		0x00100F08
#define Stream2Host_Intr_Sts	0x00100F24
#define C011_RET_SUCCESS	0x0	/* Reutrn status of firmware command. */

/* TS input status register */
#define TS_StreamAFIFOStatus	0x0010044C
#define TS_StreamBFIFOStatus	0x0010084C

/*UART Selection definitions*/
#define UartSelectA		0x00100300
#define UartSelectB		0x00100304

#define BSVS_UART_DEC_NONE	0x00
#define BSVS_UART_DEC_OUTER	0x01
#define BSVS_UART_DEC_INNER	0x02
#define BSVS_UART_STREAM	0x03

/* Code-In fifo */
#define REG_DecCA_RegCinCTL	0xa00
#define REG_DecCA_RegCinBase	0xa0c
#define REG_DecCA_RegCinEnd	0xa10
#define REG_DecCA_RegCinWrPtr	0xa04
#define REG_DecCA_RegCinRdPtr	0xa08

#define REG_Dec_TsUser0Base	0x100864
#define REG_Dec_TsUser0Rdptr	0x100868
#define REG_Dec_TsUser0Wrptr	0x10086C
#define REG_Dec_TsUser0End	0x100874

/* ASF Case ...*/
#define REG_Dec_TsAudCDB2Base	0x10036c
#define REG_Dec_TsAudCDB2Rdptr  0x100378
#define REG_Dec_TsAudCDB2Wrptr  0x100374
#define REG_Dec_TsAudCDB2End	0x100370

/* DRAM bringup Registers */
#define SDRAM_PARAM		0x00040804
#define SDRAM_PRECHARGE		0x000408B0
#define SDRAM_EXT_MODE		0x000408A4
#define SDRAM_MODE		0x000408A0
#define SDRAM_REFRESH		0x00040890
#define SDRAM_REF_PARAM		0x00040808

#define DecHt_PllACtl		0x34000C
#define DecHt_PllBCtl		0x340010
#define DecHt_PllCCtl		0x340014
#define DecHt_PllDCtl		0x340034
#define DecHt_PllECtl		0x340038
#define AUD_DSP_MISC_SOFT_RESET	0x00240104
#define AIO_MISC_PLL_RESET	0x0026000C
#define PCIE_CLK_REQ_REG	0xDC
#define	PCI_CLK_REQ_ENABLE	(BC_BIT(8))

/*************************************************
  F/W Copy engine definitions..
**************************************************/
#define BC_FWIMG_ST_ADDR	0x00000000

#define DecHt_HostSwReset	0x340000
#define BC_DRAM_FW_CFG_ADDR	0x001c2000

union intr_mask_reg {
	struct {
		uint32_t	mask_tx_done:1;
		uint32_t	mask_tx_err:1;
		uint32_t	mask_rx_done:1;
		uint32_t	mask_rx_err:1;
		uint32_t	mask_pcie_err:1;
		uint32_t	mask_pcie_rbusmast_err:1;
		uint32_t	mask_pcie_rgr_bridge:1;
		uint32_t	reserved:25;
	};

	uint32_t	whole_reg;

};

union link_misc_perst_deco_ctrl {
	struct {
		uint32_t	bcm7412_rst:1;		/* 1 -> BCM7412 is held in reset. Reset value 1.*/
		uint32_t	reserved0:3;		/* Reserved.No Effect*/
		uint32_t	stop_bcm_7412_clk:1;	/* 1 ->Stops branch of 27MHz clk used to clk BCM7412*/
		uint32_t	reserved1:27;		/* Reseved. No Effect*/
	};

	uint32_t	whole_reg;

};

union link_misc_perst_clk_ctrl {
	struct {
		uint32_t	sel_alt_clk:1;	  /* When set, selects a 6.75MHz clock as the source of core_clk */
		uint32_t	stop_core_clk:1;  /* When set, stops the branch of core_clk that is not needed for low power operation */
		uint32_t	pll_pwr_dn:1;	  /* When set, powers down the main PLL. The alternate clock bit should be set
						     to select an alternate clock before setting this bit.*/
		uint32_t	reserved0:5;	  /* Reserved */
		uint32_t	pll_mult:8;	  /* This setting controls the multiplier for the PLL. */
		uint32_t	pll_div:4;	  /* This setting controls the divider for the PLL. */
		uint32_t	reserved1:12;	  /* Reserved */
	};

	uint32_t	whole_reg;

};


union link_misc_perst_decoder_ctrl {
	struct {
		uint32_t	bcm_7412_rst:1; /* 1 -> BCM7412 is held in reset. Reset value 1.*/
		uint32_t	res0:3; /* Reserved.No Effect*/
		uint32_t	stop_7412_clk:1; /* 1 ->Stops branch of 27MHz clk used to clk BCM7412*/
		uint32_t	res1:27; /* Reseved. No Effect */
	};

	uint32_t	whole_reg;

};

/* DMA engine register BIT mask wrappers.. */
#define DMA_START_BIT		MISC1_TX_SW_DESC_LIST_CTRL_STS_TX_DMA_RUN_STOP_MASK

#define GET_RX_INTR_MASK (INTR_INTR_STATUS_L1_UV_RX_DMA_ERR_INTR_MASK |		\
			  INTR_INTR_STATUS_L1_UV_RX_DMA_DONE_INTR_MASK |	\
			  INTR_INTR_STATUS_L1_Y_RX_DMA_ERR_INTR_MASK |		\
			  INTR_INTR_STATUS_L1_Y_RX_DMA_DONE_INTR_MASK |		\
			  INTR_INTR_STATUS_L0_UV_RX_DMA_ERR_INTR_MASK |		\
			  INTR_INTR_STATUS_L0_UV_RX_DMA_DONE_INTR_MASK |	\
			  INTR_INTR_STATUS_L0_Y_RX_DMA_ERR_INTR_MASK |		\
			  INTR_INTR_STATUS_L0_Y_RX_DMA_DONE_INTR_MASK)

uint32_t link_dec_reg_rd(struct crystalhd_adp *adp, uint32_t reg_off);
void link_dec_reg_wr(struct crystalhd_adp *adp, uint32_t reg_off, uint32_t val);
uint32_t crystalhd_link_reg_rd(struct crystalhd_adp *adp, uint32_t reg_off);
void crystalhd_link_reg_wr(struct crystalhd_adp *adp, uint32_t reg_off, uint32_t val);
uint32_t crystalhd_link_dram_rd(struct crystalhd_hw *hw, uint32_t mem_off);
void crystalhd_link_dram_wr(struct crystalhd_hw *hw, uint32_t mem_off, uint32_t val);
BC_STATUS crystalhd_link_mem_rd(struct crystalhd_hw *hw, uint32_t start_off, uint32_t dw_cnt, uint32_t *rd_buff);
BC_STATUS crystalhd_link_mem_wr(struct crystalhd_hw *hw, uint32_t start_off, uint32_t dw_cnt, uint32_t *wr_buff);
void crystalhd_link_enable_uarts(struct crystalhd_hw *hw);
void crystalhd_link_start_dram(struct crystalhd_hw *hw);
bool crystalhd_link_bring_out_of_rst(struct crystalhd_hw *hw);
bool crystalhd_link_put_in_reset(struct crystalhd_hw *hw);
void crystalhd_link_disable_interrupts(struct crystalhd_hw *hw);
void crystalhd_link_enable_interrupts(struct crystalhd_hw *hw);
void crystalhd_link_clear_errors(struct crystalhd_hw *hw);
void crystalhd_link_clear_interrupts(struct crystalhd_hw *hw);
void crystalhd_link_soft_rst(struct crystalhd_hw *hw);
bool crystalhd_link_load_firmware_config(struct crystalhd_hw *hw);
bool crystalhd_link_start_device(struct crystalhd_hw *hw);
bool crystalhd_link_stop_device(struct crystalhd_hw *hw);
uint32_t link_GetPicInfoLineNum(struct crystalhd_dio_req *dio, uint8_t *base);
uint32_t link_GetMode422Data(struct crystalhd_dio_req *dio, PBC_PIC_INFO_BLOCK pPicInfoLine, int type);
uint32_t link_GetMetaDataFromPib(struct crystalhd_dio_req *dio,	PBC_PIC_INFO_BLOCK pPicInfoLine);
uint32_t link_GetHeightFromPib(struct crystalhd_dio_req *dio, PBC_PIC_INFO_BLOCK pPicInfoLine);
bool link_GetPictureInfo(struct crystalhd_hw *hw, uint32_t picHeight, uint32_t picWidth, struct crystalhd_dio_req *dio,
								uint32_t *PicNumber, uint64_t *PicMetaData);
uint32_t link_GetRptDropParam(struct crystalhd_hw *hw, uint32_t picHeight, uint32_t picWidth, void *pRxDMAReq);
bool crystalhd_link_peek_next_decoded_frame(struct crystalhd_hw *hw, uint64_t *meta_payload, uint32_t *picNumFlags, uint32_t PicWidth);
bool crystalhd_link_check_input_full(struct crystalhd_hw *hw, uint32_t needed_sz, uint32_t *empty_sz,
									 bool b_188_byte_pkts, uint8_t *flags);
bool crystalhd_link_tx_list0_handler(struct crystalhd_hw *hw, uint32_t err_sts);
bool crystalhd_link_tx_list1_handler(struct crystalhd_hw *hw, uint32_t err_sts);
void crystalhd_link_tx_isr(struct crystalhd_hw *hw, uint32_t int_sts);
void crystalhd_link_start_tx_dma_engine(struct crystalhd_hw *hw, uint8_t list_id, addr_64 desc_addr);
BC_STATUS crystalhd_link_stop_tx_dma_engine(struct crystalhd_hw *hw);
uint32_t crystalhd_link_get_pib_avail_cnt(struct crystalhd_hw *hw);
uint32_t crystalhd_link_get_addr_from_pib_Q(struct crystalhd_hw *hw);
bool crystalhd_link_rel_addr_to_pib_Q(struct crystalhd_hw *hw, uint32_t addr_to_rel);
void link_cpy_pib_to_app(struct C011_PIB *src_pib, BC_PIC_INFO_BLOCK *dst_pib);
void crystalhd_link_proc_pib(struct crystalhd_hw *hw);
void crystalhd_link_start_rx_dma_engine(struct crystalhd_hw *hw);
void crystalhd_link_stop_rx_dma_engine(struct crystalhd_hw *hw);
BC_STATUS crystalhd_link_hw_prog_rxdma(struct crystalhd_hw *hw, struct crystalhd_rx_dma_pkt *rx_pkt);
BC_STATUS crystalhd_link_hw_post_cap_buff(struct crystalhd_hw *hw, struct crystalhd_rx_dma_pkt *rx_pkt);
void crystalhd_link_get_dnsz(struct crystalhd_hw *hw, uint32_t list_index,
									uint32_t *y_dw_dnsz, uint32_t *uv_dw_dnsz);
void crystalhd_link_hw_finalize_pause(struct crystalhd_hw *hw);
bool crystalhd_link_rx_list0_handler(struct crystalhd_hw *hw,uint32_t int_sts,uint32_t y_err_sts,uint32_t uv_err_sts);
bool crystalhd_link_rx_list1_handler(struct crystalhd_hw *hw,uint32_t int_sts,uint32_t y_err_sts,uint32_t uv_err_sts);
void crystalhd_link_rx_isr(struct crystalhd_hw *hw, uint32_t intr_sts);
BC_STATUS crystalhd_link_hw_pause(struct crystalhd_hw *hw, bool state);
BC_STATUS crystalhd_link_fw_cmd_post_proc(struct crystalhd_hw *hw, BC_FW_CMD *fw_cmd);
BC_STATUS crystalhd_link_put_ddr2sleep(struct crystalhd_hw *hw);
BC_STATUS crystalhd_link_download_fw(struct crystalhd_hw* hw, uint8_t* buffer, uint32_t sz);
BC_STATUS crystalhd_link_do_fw_cmd(struct crystalhd_hw *hw, BC_FW_CMD *fw_cmd);
bool crystalhd_link_hw_interrupt_handle(struct crystalhd_adp *adp, struct crystalhd_hw *hw);
void crystalhd_link_notify_fll_change(struct crystalhd_hw *hw, bool bCleanupContext);
bool crystalhd_link_notify_event(struct crystalhd_hw *hw, enum BRCM_EVENT EventCode);
#endif
