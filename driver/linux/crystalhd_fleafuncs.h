/***************************************************************************
 * Copyright (c) 2005-2009, Broadcom Corporation.
 *
 *  Name: crystalhd_fleafuncs . h
 *
 *  Description:
 *		BCM70015 Linux driver hardware layer.
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

#ifndef _CRYSTALHD_FLEAFUNCS_H_
#define _CRYSTALHD_FLEAFUNCS_H_

#include "FleaDefs.h"

#define FW_CMD_BUFF_SZ		64

bool crystalhd_flea_start_device(struct crystalhd_hw *hw);
bool crystalhd_flea_stop_device(struct crystalhd_hw *hw);
bool crystalhd_flea_hw_interrupt_handle(struct crystalhd_adp *adp, struct crystalhd_hw *hw);
uint32_t crystalhd_flea_reg_rd(struct crystalhd_adp *adp, uint32_t reg_off);											/* Done */
void crystalhd_flea_reg_wr(struct crystalhd_adp *adp, uint32_t reg_off, uint32_t val);									/* Done */
bool crystalhd_flea_check_input_full(struct crystalhd_hw *hw, uint32_t needed_sz, uint32_t *empty_sz, bool b_188_byte_pkts, uint8_t *flags);
BC_STATUS crystalhd_flea_mem_rd(struct crystalhd_hw *hw, uint32_t start_off, uint32_t dw_cnt, uint32_t *rd_buff);		/* Done */
BC_STATUS crystalhd_flea_mem_wr(struct crystalhd_hw *hw, uint32_t start_off, uint32_t dw_cnt, uint32_t *wr_buff);		/* Done */
BC_STATUS crystalhd_flea_do_fw_cmd(struct crystalhd_hw *hw, BC_FW_CMD *fw_cmd);
BC_STATUS crystalhd_flea_download_fw(struct crystalhd_hw* hw, uint8_t* buffer, uint32_t sz);
void crystalhd_flea_get_dnsz(struct crystalhd_hw *hw, uint32_t list_index, uint32_t *y_dw_dnsz, uint32_t *uv_dw_dnsz);
BC_STATUS crystalhd_flea_hw_pause(struct crystalhd_hw *hw, bool state);
bool crystalhd_flea_peek_next_decoded_frame(struct crystalhd_hw *hw, uint64_t *meta_payload, uint32_t *picNumFlags, uint32_t PicWidth);
BC_STATUS crystalhd_flea_hw_post_cap_buff(struct crystalhd_hw *hw, struct crystalhd_rx_dma_pkt *rx_pkt);
void crystalhd_flea_start_tx_dma_engine(struct crystalhd_hw *hw, uint8_t list_id, addr_64 desc_addr);
void crystalhd_flea_stop_rx_dma_engine(struct crystalhd_hw *hw);
BC_STATUS crystalhd_flea_stop_tx_dma_engine(struct crystalhd_hw *hw);
bool crystalhd_flea_tx_list0_handler(struct crystalhd_hw *hw, uint32_t err_sts);
bool crystalhd_flea_tx_list1_handler(struct crystalhd_hw *hw, uint32_t err_sts);
void crystalhd_flea_tx_isr(struct crystalhd_hw *hw, union FLEA_INTR_BITS_COMMON int_sts);
bool crystalhd_flea_rx_list0_handler(struct crystalhd_hw *hw,union FLEA_INTR_BITS_COMMON int_sts,uint32_t y_err_sts,uint32_t uv_err_sts);
bool crystalhd_flea_rx_list1_handler(struct crystalhd_hw *hw,union FLEA_INTR_BITS_COMMON int_sts,uint32_t y_err_sts,uint32_t uv_err_sts);
void crystalhd_flea_rx_isr(struct crystalhd_hw *hw, union FLEA_INTR_BITS_COMMON intr_sts);
void crystalhd_flea_notify_fll_change(struct crystalhd_hw *hw, bool bCleanupContext);
bool crystalhd_flea_notify_event(struct crystalhd_hw *hw, enum BRCM_EVENT EventCode);

bool flea_GetPictureInfo(struct crystalhd_hw *hw, struct crystalhd_rx_dma_pkt * rx_pkt,
						 uint32_t *PicNumber, uint64_t *PicMetaData);
#endif
