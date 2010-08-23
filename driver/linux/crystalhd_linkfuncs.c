/***************************************************************************
 * Copyright (c) 2005-2009, Broadcom Corporation.
 *
 *  Name: crystalhd_hw . c
 *
 *  Description:
 *		BCM70010 Linux driver HW layer.
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

#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <asm/tsc.h>
#include "crystalhd_hw.h"
#include "crystalhd_lnx.h"
#include "crystalhd_linkfuncs.h"
#include "bc_defines.h"

#define OFFSETOF(_s_, _m_) ((size_t)(unsigned long)&(((_s_ *)0)->_m_))

/**
* link_dec_reg_rd - Read 70010's device register.
* @adp: Adapter instance
* @reg_off: Register offset.
*
* Return:
*	32bit value read
*
* 70010's device register read routine. This interface use
* 70010's device access range mapped from BAR-2 (4M) of PCIe
* configuration space.
*/
uint32_t link_dec_reg_rd(struct crystalhd_adp *adp, uint32_t reg_off)
{
	if (!adp) {
		printk(KERN_ERR "%s: Invalid args\n", __func__);
		return 0;
	}

	if (reg_off > adp->pci_mem_len) {
		dev_err(&adp->pdev->dev, "%s: reg_off out of range: 0x%08x\n",
			__func__, reg_off);
		return 0;
	}

	return readl(adp->mem_addr + reg_off);
}

/**
* link_dec_reg_wr - Write 70010's device register
* @adp: Adapter instance
* @reg_off: Register offset.
* @val: Dword value to be written.
*
* Return:
*	none.
*
* 70010's device register write routine. This interface use
* 70010's device access range mapped from BAR-2 (4M) of PCIe
* configuration space.
*/
void link_dec_reg_wr(struct crystalhd_adp *adp, uint32_t reg_off, uint32_t val)
{
	if (!adp) {
		printk(KERN_ERR "%s: Invalid args\n", __func__);
		return;
	}

	if (reg_off > adp->pci_mem_len) {
		dev_err(&adp->pdev->dev, "%s: reg_off out of range: 0x%08x\n",
			__func__, reg_off);
		return;
	}

	writel(val, adp->mem_addr + reg_off);

	/* the udelay is required for latest 70012, not for others... :( */
	udelay(8);
}

/**
* crystalhd_reg_rd - Read 70012's device register.
* @adp: Adapter instance
* @reg_off: Register offset.
*
* Return:
*	32bit value read
*
* 70012 device register  read routine. This interface use
* 70012's device access range mapped from BAR-1 (64K) of PCIe
* configuration space.
*
*/
uint32_t crystalhd_link_reg_rd(struct crystalhd_adp *adp, uint32_t reg_off)
{
	if (!adp) {
		printk(KERN_ERR "%s: Invalid args\n", __func__);
		return 0;
	}

	if (reg_off > adp->pci_i2o_len) {
		dev_err(&adp->pdev->dev, "%s: reg_off out of range: 0x%08x\n",
			__func__, reg_off);
		return 0;
	}

	return readl(adp->i2o_addr + reg_off);
}

/**
* crystalhd_reg_wr - Write 70012's device register
* @adp: Adapter instance
* @reg_off: Register offset.
* @val: Dword value to be written.
*
* Return:
*	none.
*
* 70012 device register  write routine. This interface use
* 70012's device access range mapped from BAR-1 (64K) of PCIe
* configuration space.
*
*/
void crystalhd_link_reg_wr(struct crystalhd_adp *adp, uint32_t reg_off, uint32_t val)
{
	if (!adp) {
		printk(KERN_ERR "%s: Invalid args\n", __func__);
		return;
	}

	if (reg_off > adp->pci_i2o_len) {
		dev_err(&adp->pdev->dev, "%s: reg_off out of range: 0x%08x\n",
				__func__, reg_off);
				return;
	}

	writel(val, adp->i2o_addr + reg_off);
}

inline uint32_t crystalhd_link_dram_rd(struct crystalhd_hw *hw, uint32_t mem_off)
{
	hw->pfnWriteFPGARegister(hw->adp,  DCI_DRAM_BASE_ADDR, (mem_off >> 19));
	return hw->pfnReadDevRegister(hw->adp,  (0x00380000 | (mem_off & 0x0007FFFF)));
}

inline void crystalhd_link_dram_wr(struct crystalhd_hw *hw, uint32_t mem_off, uint32_t val)
{
	hw->pfnWriteFPGARegister(hw->adp, DCI_DRAM_BASE_ADDR, (mem_off >> 19));
	hw->pfnWriteDevRegister(hw->adp, (0x00380000 | (mem_off & 0x0007FFFF)), val);
}

/**
* crystalhd_link_mem_rd - Read data from DRAM area.
* @adp: Adapter instance
* @start_off: Start offset.
* @dw_cnt: Count in dwords.
* @rd_buff: Buffer to copy the data from dram.
*
* Return:
*	Status.
*
* Dram read routine.
*/
BC_STATUS crystalhd_link_mem_rd(struct crystalhd_hw *hw, uint32_t start_off,
								uint32_t dw_cnt, uint32_t *rd_buff)
{
	uint32_t ix = 0;

	if (!hw || !rd_buff) {
		printk(KERN_ERR "%s: Invalid arg\n", __func__);
		return BC_STS_INV_ARG;
	}
	for (ix = 0; ix < dw_cnt; ix++)
		rd_buff[ix] = crystalhd_link_dram_rd(hw, (start_off + (ix * 4)));

	return BC_STS_SUCCESS;
}

/**
* crystalhd_link_mem_wr - Write data to DRAM area.
* @adp: Adapter instance
* @start_off: Start offset.
* @dw_cnt: Count in dwords.
* @wr_buff: Data Buffer to be written.
*
* Return:
*	Status.
*
* Dram write routine.
*/
BC_STATUS crystalhd_link_mem_wr(struct crystalhd_hw *hw, uint32_t start_off,
						uint32_t dw_cnt, uint32_t *wr_buff)
{
	uint32_t ix = 0;

	if (!hw || !wr_buff) {
		printk(KERN_ERR "%s: Invalid arg\n", __func__);
		return BC_STS_INV_ARG;
	}

	for (ix = 0; ix < dw_cnt; ix++)
		crystalhd_link_dram_wr(hw, (start_off + (ix * 4)), wr_buff[ix]);

	return BC_STS_SUCCESS;
}

void crystalhd_link_enable_uarts(struct crystalhd_hw *hw)
{
	hw->pfnWriteDevRegister(hw->adp, UartSelectA, BSVS_UART_STREAM);
	hw->pfnWriteDevRegister(hw->adp, UartSelectB, BSVS_UART_DEC_OUTER);
}

void crystalhd_link_start_dram(struct crystalhd_hw *hw)
{
	hw->pfnWriteDevRegister(hw->adp, SDRAM_PARAM, ((40 / 5 - 1) <<  0) |
	/* tras (40ns tras)/(5ns period) -1 ((15/5 - 1) <<  4) | // trcd */
		      ((15 / 5 - 1) <<  7) |	/* trp */
		      ((10 / 5 - 1) << 10) |	/* trrd */
		      ((15 / 5 + 1) << 12) |	/* twr */
		      ((2 + 1) << 16) |		/* twtr */
		      ((70 / 5 - 2) << 19) |	/* trfc */
		      (0 << 23));

	hw->pfnWriteDevRegister(hw->adp, SDRAM_PRECHARGE, 0);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_EXT_MODE, 2);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_MODE, 0x132);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_PRECHARGE, 0);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_REFRESH, 0);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_REFRESH, 0);
	hw->pfnWriteDevRegister(hw->adp, SDRAM_MODE, 0x32);
	/* setting the refresh rate here */
	hw->pfnWriteDevRegister(hw->adp, SDRAM_REF_PARAM, ((1 << 12) | 96));
}


bool crystalhd_link_bring_out_of_rst(struct crystalhd_hw *hw)
{
	link_misc_perst_deco_ctrl rst_deco_cntrl;
	link_misc_perst_clk_ctrl rst_clk_cntrl;
	uint32_t temp;

	/*
	 * Link clocks: MISC_PERST_CLOCK_CTRL Clear PLL power down bit,
	 * delay to allow PLL to lock Clear alternate clock, stop clock bits
	 */
	rst_clk_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL);
	rst_clk_cntrl.pll_pwr_dn = 0;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL, rst_clk_cntrl.whole_reg);
	msleep_interruptible(50);

	rst_clk_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL);
	rst_clk_cntrl.stop_core_clk = 0;
	rst_clk_cntrl.sel_alt_clk = 0;

	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL, rst_clk_cntrl.whole_reg);
	msleep_interruptible(50);

	/*
	 * Bus Arbiter Timeout: GISB_ARBITER_TIMER
	 * Set internal bus arbiter timeout to 40us based on core clock speed
	 * (63MHz * 40us = 0x9D8)
	 */
	hw->pfnWriteFPGARegister(hw->adp, GISB_ARBITER_TIMER, 0x9D8);

	/*
	 * Decoder clocks: MISC_PERST_DECODER_CTRL
	 * Enable clocks while 7412 reset is asserted, delay
	 * De-assert 7412 reset
	 */
	rst_deco_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL);
	rst_deco_cntrl.stop_bcm_7412_clk = 0;
	rst_deco_cntrl.bcm7412_rst = 1;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL, rst_deco_cntrl.whole_reg);
	msleep_interruptible(50);

	rst_deco_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL);
	rst_deco_cntrl.bcm7412_rst = 0;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL, rst_deco_cntrl.whole_reg);
	msleep_interruptible(50);

	/* Disable OTP_CONTENT_MISC to 0 to disable all secure modes */
	hw->pfnWriteFPGARegister(hw->adp, OTP_CONTENT_MISC, 0);

	/* Clear bit 29 of 0x404 */
	temp = hw->pfnReadFPGARegister(hw->adp, PCIE_TL_TRANSACTION_CONFIGURATION);
	temp &= ~BC_BIT(29);
	hw->pfnWriteFPGARegister(hw->adp, PCIE_TL_TRANSACTION_CONFIGURATION, temp);

	/* 2.5V regulator must be set to 2.6 volts (+6%) */
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_VREG_CTRL, 0xF3);

	return true;
}

bool crystalhd_link_put_in_reset(struct crystalhd_hw *hw)
{
	link_misc_perst_deco_ctrl rst_deco_cntrl;
	link_misc_perst_clk_ctrl  rst_clk_cntrl;
	uint32_t                  temp;

	/*
	 * Decoder clocks: MISC_PERST_DECODER_CTRL
	 * Assert 7412 reset, delay
	 * Assert 7412 stop clock
	 */
	rst_deco_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL);
	rst_deco_cntrl.stop_bcm_7412_clk = 1;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_DECODER_CTRL, rst_deco_cntrl.whole_reg);
	msleep_interruptible(50);

	/* Bus Arbiter Timeout: GISB_ARBITER_TIMER
	 * Set internal bus arbiter timeout to 40us based on core clock speed
	 * (6.75MHZ * 40us = 0x10E)
	 */
	hw->pfnWriteFPGARegister(hw->adp, GISB_ARBITER_TIMER, 0x10E);

	/* Link clocks: MISC_PERST_CLOCK_CTRL
	 * Stop core clk, delay
	 * Set alternate clk, delay, set PLL power down
	 */
	rst_clk_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL);
	rst_clk_cntrl.stop_core_clk = 1;
	rst_clk_cntrl.sel_alt_clk = 1;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL, rst_clk_cntrl.whole_reg);
	msleep_interruptible(50);

	rst_clk_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL);
	rst_clk_cntrl.pll_pwr_dn = 1;
	hw->pfnWriteFPGARegister(hw->adp, MISC_PERST_CLOCK_CTRL, rst_clk_cntrl.whole_reg);

	/*
	 * Read and restore the Transaction Configuration Register
	 * after core reset
	 */
	temp = hw->pfnReadFPGARegister(hw->adp, PCIE_TL_TRANSACTION_CONFIGURATION);

	/*
	 * Link core soft reset: MISC3_RESET_CTRL
	 * - Write BIT[0]=1 and read it back for core reset to take place
	 */
	hw->pfnWriteFPGARegister(hw->adp, MISC3_RESET_CTRL, 1);
	rst_deco_cntrl.whole_reg = hw->pfnReadFPGARegister(hw->adp, MISC3_RESET_CTRL);
	msleep_interruptible(50);

	/* restore the transaction configuration register */
	hw->pfnWriteFPGARegister(hw->adp, PCIE_TL_TRANSACTION_CONFIGURATION, temp);

	return true;
}

void crystalhd_link_disable_interrupts(struct crystalhd_hw *hw)
{
	intr_mask_reg   intr_mask;
	intr_mask.whole_reg = hw->pfnReadFPGARegister(hw->adp, INTR_INTR_MSK_STS_REG);
	intr_mask.mask_pcie_err = 1;
	intr_mask.mask_pcie_rbusmast_err = 1;
	intr_mask.mask_pcie_rgr_bridge   = 1;
	intr_mask.mask_rx_done = 1;
	intr_mask.mask_rx_err  = 1;
	intr_mask.mask_tx_done = 1;
	intr_mask.mask_tx_err  = 1;
	hw->pfnWriteFPGARegister(hw->adp, INTR_INTR_MSK_SET_REG, intr_mask.whole_reg);

	return;
}

void crystalhd_link_enable_interrupts(struct crystalhd_hw *hw)
{
	intr_mask_reg   intr_mask;
	intr_mask.whole_reg = hw->pfnReadFPGARegister(hw->adp, INTR_INTR_MSK_STS_REG);
	intr_mask.mask_pcie_err = 1;
	intr_mask.mask_pcie_rbusmast_err = 1;
	intr_mask.mask_pcie_rgr_bridge   = 1;
	intr_mask.mask_rx_done = 1;
	intr_mask.mask_rx_err  = 1;
	intr_mask.mask_tx_done = 1;
	intr_mask.mask_tx_err  = 1;
	hw->pfnWriteFPGARegister(hw->adp, INTR_INTR_MSK_CLR_REG, intr_mask.whole_reg);

	return;
}

void crystalhd_link_clear_errors(struct crystalhd_hw *hw)
{
	uint32_t reg;

	/* Writing a 1 to a set bit clears that bit */
	reg = hw->pfnReadFPGARegister(hw->adp, MISC1_Y_RX_ERROR_STATUS);
	if (reg)
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_ERROR_STATUS, reg);

	reg = hw->pfnReadFPGARegister(hw->adp, MISC1_UV_RX_ERROR_STATUS);
	if (reg)
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_ERROR_STATUS, reg);

	reg = hw->pfnReadFPGARegister(hw->adp, MISC1_TX_DMA_ERROR_STATUS);
	if (reg)
		hw->pfnWriteFPGARegister(hw->adp, MISC1_TX_DMA_ERROR_STATUS, reg);
}

void crystalhd_link_clear_interrupts(struct crystalhd_hw *hw)
{
	uint32_t intr_sts =  hw->pfnReadFPGARegister(hw->adp, INTR_INTR_STATUS);

	if (intr_sts) {
		hw->pfnWriteFPGARegister(hw->adp, INTR_INTR_CLR_REG, intr_sts);

		/* Write End Of Interrupt for PCIE */
		hw->pfnWriteFPGARegister(hw->adp, INTR_EOI_CTRL, 1);
	}
}

void crystalhd_link_soft_rst(struct crystalhd_hw *hw)
{
	uint32_t val;

	/* Assert c011 soft reset*/
	hw->pfnWriteDevRegister(hw->adp, DecHt_HostSwReset, 0x00000001);
	msleep_interruptible(50);

	/* Release c011 soft reset*/
	hw->pfnWriteDevRegister(hw->adp, DecHt_HostSwReset, 0x00000000);

	/* Disable Stuffing..*/
	val = hw->pfnReadFPGARegister(hw->adp, MISC2_GLOBAL_CTRL);
	val |= BC_BIT(8);
	hw->pfnWriteFPGARegister(hw->adp, MISC2_GLOBAL_CTRL, val);
}

bool crystalhd_link_load_firmware_config(struct crystalhd_hw *hw)
{
	uint32_t i = 0, reg;

	hw->pfnWriteFPGARegister(hw->adp, DCI_DRAM_BASE_ADDR, (BC_DRAM_FW_CFG_ADDR >> 19));

	hw->pfnWriteFPGARegister(hw->adp, AES_CMD, 0);
	hw->pfnWriteFPGARegister(hw->adp, AES_CONFIG_INFO, (BC_DRAM_FW_CFG_ADDR & 0x7FFFF));
	hw->pfnWriteFPGARegister(hw->adp, AES_CMD, 0x1);

	for (i = 0; i < 100; ++i) {
		reg = hw->pfnReadFPGARegister(hw->adp, AES_STATUS);
		if (reg & 0x1)
			return true;
		msleep_interruptible(10);
	}

	return false;
}


bool crystalhd_link_start_device(struct crystalhd_hw *hw)
{
	uint32_t dbg_options, glb_cntrl = 0, reg_pwrmgmt = 0;
	struct device *dev;

	if (!hw)
		return -EINVAL;

	dev = &hw->adp->pdev->dev;

	dev_dbg(dev, "Starting Crystal HD Device\n");

	if (!crystalhd_link_bring_out_of_rst(hw)) {
		dev_err(dev, "Failed To Bring BCM70012 Out Of Reset\n");
		return false;
	}

	crystalhd_link_disable_interrupts(hw);

	crystalhd_link_clear_errors(hw);

	crystalhd_link_clear_interrupts(hw);

	crystalhd_link_enable_interrupts(hw);

	/* Enable the option for getting the total no. of DWORDS
	 * that have been transfered by the RXDMA engine
	 */
	dbg_options = hw->pfnReadFPGARegister(hw->adp, MISC1_DMA_DEBUG_OPTIONS_REG);
	dbg_options |= 0x10;
	hw->pfnWriteFPGARegister(hw->adp, MISC1_DMA_DEBUG_OPTIONS_REG, dbg_options);

	/* Enable PCI Global Control options */
	glb_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC2_GLOBAL_CTRL);
	glb_cntrl |= 0x100;
	glb_cntrl |= 0x8000;
	hw->pfnWriteFPGARegister(hw->adp, MISC2_GLOBAL_CTRL, glb_cntrl);

	crystalhd_link_enable_interrupts(hw);

	crystalhd_link_soft_rst(hw);
	crystalhd_link_start_dram(hw);
	crystalhd_link_enable_uarts(hw);

	// Disable L1 ASPM while video is playing as this causes performance problems otherwise
	reg_pwrmgmt = hw->pfnReadFPGARegister(hw->adp, PCIE_DLL_DATA_LINK_CONTROL);
	reg_pwrmgmt &= ~ASPM_L1_ENABLE;

	hw->pfnWriteFPGARegister(hw->adp, PCIE_DLL_DATA_LINK_CONTROL, reg_pwrmgmt);

	return true;
}

bool crystalhd_link_stop_device(struct crystalhd_hw *hw)
{
	uint32_t reg;
	BC_STATUS sts;

	dev_dbg(&hw->adp->pdev->dev, "Stopping Crystal HD Device\n");
	sts = crystalhd_link_put_ddr2sleep(hw);
	if (sts != BC_STS_SUCCESS) {
		dev_err(&hw->adp->pdev->dev, "Failed to Put DDR To Sleep!!\n");
		return BC_STS_ERROR;
	}

	/* Clear and disable interrupts */
	crystalhd_link_disable_interrupts(hw);
	crystalhd_link_clear_errors(hw);
	crystalhd_link_clear_interrupts(hw);

	if (!crystalhd_link_put_in_reset(hw))
		dev_err(&hw->adp->pdev->dev, "Failed to Put Link To Reset State\n");

	reg = hw->pfnReadFPGARegister(hw->adp, PCIE_DLL_DATA_LINK_CONTROL);
	reg |= ASPM_L1_ENABLE;
	hw->pfnWriteFPGARegister(hw->adp, PCIE_DLL_DATA_LINK_CONTROL, reg);

	/* Set PCI Clk Req */
	reg = hw->pfnReadFPGARegister(hw->adp, PCIE_CLK_REQ_REG);
	reg |= PCI_CLK_REQ_ENABLE;
	hw->pfnWriteFPGARegister(hw->adp, PCIE_CLK_REQ_REG, reg);

	return true;
}

uint32_t link_GetPicInfoLineNum(crystalhd_dio_req *dio, uint8_t *base)
{
	uint32_t PicInfoLineNum = 0;

	if (dio->uinfo.b422mode == MODE422_YUY2) {
		PicInfoLineNum = ((uint32_t)(*(base + 6)) & 0xff)
			| (((uint32_t)(*(base + 4)) << 8)  & 0x0000ff00)
			| (((uint32_t)(*(base + 2)) << 16) & 0x00ff0000)
			| (((uint32_t)(*(base + 0)) << 24) & 0xff000000);
	} else if (dio->uinfo.b422mode == MODE422_UYVY) {
		PicInfoLineNum = ((uint32_t)(*(base + 7)) & 0xff)
			| (((uint32_t)(*(base + 5)) << 8)  & 0x0000ff00)
			| (((uint32_t)(*(base + 3)) << 16) & 0x00ff0000)
			| (((uint32_t)(*(base + 1)) << 24) & 0xff000000);
	} else {
		PicInfoLineNum = ((uint32_t)(*(base + 3)) & 0xff)
			| (((uint32_t)(*(base + 2)) << 8)  & 0x0000ff00)
			| (((uint32_t)(*(base + 1)) << 16) & 0x00ff0000)
			| (((uint32_t)(*(base + 0)) << 24) & 0xff000000);
	}

	return PicInfoLineNum;
}

uint32_t link_GetMode422Data(crystalhd_dio_req *dio,
			       PBC_PIC_INFO_BLOCK pPicInfoLine, int type)
{
	int i;
	uint32_t offset = 0, val = 0;
	uint8_t *tmp;
	tmp = (uint8_t *)&val;

	if (type == 1)
		offset = OFFSETOF(BC_PIC_INFO_BLOCK, picture_meta_payload);
	else if (type == 2)
		offset = OFFSETOF(BC_PIC_INFO_BLOCK, height);
	else
		offset = 0;

	if (dio->uinfo.b422mode == MODE422_YUY2) {
		for (i = 0; i < 4; i++)
			((uint8_t*)tmp)[i] =
				((uint8_t*)pPicInfoLine)[(offset + i) * 2];
	} else if (dio->uinfo.b422mode == MODE422_UYVY) {
		for (i = 0; i < 4; i++)
			((uint8_t*)tmp)[i] =
				((uint8_t*)pPicInfoLine)[(offset + i) * 2 + 1];
	}

	return val;
}

uint32_t link_GetMetaDataFromPib(crystalhd_dio_req *dio,
				   PBC_PIC_INFO_BLOCK pPicInfoLine)
{
	uint32_t picture_meta_payload = 0;

	if (dio->uinfo.b422mode)
		picture_meta_payload = link_GetMode422Data(dio, pPicInfoLine, 1);
	else
		picture_meta_payload = pPicInfoLine->picture_meta_payload;

	return BC_SWAP32(picture_meta_payload);
}

uint32_t link_GetHeightFromPib(crystalhd_dio_req *dio,
				 PBC_PIC_INFO_BLOCK pPicInfoLine)
{
	uint32_t height = 0;

	if (dio->uinfo.b422mode)
		height = link_GetMode422Data(dio, pPicInfoLine, 2);
	else
		height = pPicInfoLine->height;

	return BC_SWAP32(height);
}

/* This function cannot be called from ISR context since it uses APIs that can sleep */
bool link_GetPictureInfo(struct crystalhd_hw *hw, uint32_t picHeight, uint32_t picWidth, crystalhd_dio_req *dio,
			   uint32_t *PicNumber, uint64_t *PicMetaData)
{
	uint32_t PicInfoLineNum = 0, HeightInPib = 0, offset = 0, size = 0;
	PBC_PIC_INFO_BLOCK pPicInfoLine = NULL;
	uint32_t pic_number = 0;
	uint8_t *tmp = (uint8_t *)&pic_number;
	int i;
	unsigned long res = 0;

	*PicNumber = 0;
	*PicMetaData = 0;

	if (!dio || !picWidth)
		goto getpictureinfo_err_nosem;

// 	if(down_interruptible(&hw->fetch_sem))
// 		goto getpictureinfo_err_nosem;

	dio->pib_va = kmalloc(2 * sizeof(BC_PIC_INFO_BLOCK) + 16, GFP_KERNEL); // since copy_from_user can sleep anyway
	if(dio->pib_va == NULL)
		goto getpictureinfo_err;

	res = copy_from_user(dio->pib_va, (void *)dio->uinfo.xfr_buff, 8);
	if (res != 0)
		goto getpictureinfo_err;

	/*
	 * -- Ajitabh[01-16-2009]: Strictly check against done size.
	 * -- we have seen that the done size sometimes comes less without
	 * -- any error indicated to the driver. So we change the limit
	 * -- to check against the done size rather than the full buffer size
	 * -- this way we will always make sure that the PIB is recieved by
	 * -- the driver.
	 */
	/* Limit = Base + pRxDMAReq->RxYDMADesc.RxBuffSz; */
	/* Limit = Base + (pRxDMAReq->RxYDoneSzInDword * 4); */
// 	Limit = dio->uinfo.xfr_buff + dio->uinfo.xfr_len;

	PicInfoLineNum = link_GetPicInfoLineNum(dio, dio->pib_va);
	if (PicInfoLineNum > 1092) {
		printk("Invalid Line Number[%x]\n",	(int)PicInfoLineNum);
		goto getpictureinfo_err;
	}

	/*
	 * -- Ajitabh[01-16-2009]: Added the check for validating the
	 * -- PicInfoLine Number. This function is only called for link so we
	 * -- do not have to check for height+1 or (Height+1)/2 as we are doing
	 * -- in DIL. In DIL we need that because for flea firmware is padding
	 * -- the data to make it 16 byte aligned. This Validates the reception
	 * -- of PIB itself.
	 */
	if (picHeight) {
		if ((PicInfoLineNum != picHeight) &&
		    (PicInfoLineNum != picHeight/2)) {
			printk("PicInfoLineNum[%d] != PICHeight "
				"Or PICHeight/2 [%d]\n",
				(int)PicInfoLineNum, picHeight);
			goto getpictureinfo_err;
		}
	}

	/* calc pic info line offset */
	if (dio->uinfo.b422mode) {
		size = 2 * sizeof(BC_PIC_INFO_BLOCK);
		offset = (PicInfoLineNum * picWidth * 2) + 8;
	} else {
		size = sizeof(BC_PIC_INFO_BLOCK);
		offset = (PicInfoLineNum * picWidth) + 4;
	}

	res = copy_from_user(dio->pib_va, (void *)(dio->uinfo.xfr_buff+offset), size);
	if (res != 0)
		goto getpictureinfo_err;
	pPicInfoLine = (PBC_PIC_INFO_BLOCK)(dio->pib_va);

// 	if (((uint8_t *)pPicInfoLine < Base) ||
// 	    ((uint8_t *)pPicInfoLine > Limit)) {
// 		dev_err(dev, "Base Limit Check Failed for Extracting "
// 			"the PIB\n");
// 		goto getpictureinfo_err;
// 	}

	/*
	 * -- Ajitabh[01-16-2009]:
	 * We have seen that the data gets shifted for some repeated frames.
	 * To detect those we use PicInfoLineNum and compare it with height.
	 */

	HeightInPib = link_GetHeightFromPib(dio, pPicInfoLine);
	if ((PicInfoLineNum != HeightInPib) &&
	    (PicInfoLineNum != HeightInPib / 2)) {
		printk("Height Match Failed: HeightInPIB[%d] "
			"PicInfoLineNum[%d]\n",
			(int)HeightInPib, (int)PicInfoLineNum);
		goto getpictureinfo_err;
	}

	/* get pic meta data from pib */
	*PicMetaData = link_GetMetaDataFromPib(dio, pPicInfoLine);
	/* get pic number from pib */
	/* calc pic info line offset */
	if (dio->uinfo.b422mode)
		offset = (PicInfoLineNum * picWidth * 2);
	else
		offset = (PicInfoLineNum * picWidth);

	res = copy_from_user(dio->pib_va, (void *)(dio->uinfo.xfr_buff+offset), 12);
	if (res != 0)
		goto getpictureinfo_err;

	if (dio->uinfo.b422mode == MODE422_YUY2) {
		for (i = 0; i < 4; i++)
			((uint8_t *)tmp)[i] = ((uint8_t *)dio->pib_va)[i * 2];
	} else if (dio->uinfo.b422mode == MODE422_UYVY) {
		for (i = 0; i < 4; i++)
			((uint8_t *)tmp)[i] = ((uint8_t *)dio->pib_va)[(i * 2) + 1];
	} else
		pic_number = *(uint32_t *)(dio->pib_va);

	*PicNumber =  BC_SWAP32(pic_number);

	if(dio->pib_va)
		kfree(dio->pib_va);

// 	up(&hw->fetch_sem);

	return true;

getpictureinfo_err:
// 	up(&hw->fetch_sem);

getpictureinfo_err_nosem:
	if(dio->pib_va)
		kfree(dio->pib_va);
	*PicNumber = 0;
	*PicMetaData = 0;

	return false;
}

uint32_t link_GetRptDropParam(struct crystalhd_hw *hw, uint32_t picHeight, uint32_t picWidth, void* pRxDMAReq)
{
	uint32_t PicNumber = 0, result = 0;
	uint64_t PicMetaData = 0;

	if(link_GetPictureInfo(hw, picHeight, picWidth, ((crystalhd_rx_dma_pkt *)pRxDMAReq)->dio_req,
				&PicNumber, &PicMetaData))
		result = PicNumber;

	return result;
}

/*
* This function gets the next picture metadata payload
* from the decoded picture in ReadyQ (if there was any)
* and returns it. THIS IS ONLY USED FOR LINK.
*/
bool crystalhd_link_peek_next_decoded_frame(struct crystalhd_hw *hw,
					  uint64_t *meta_payload, uint32_t *picNumFlags,
					  uint32_t PicWidth)
{
	uint32_t PicNumber = 0;
	unsigned long flags = 0;
	crystalhd_dioq_t *ioq;
	crystalhd_elem_t *tmp;
	crystalhd_rx_dma_pkt *rpkt;

	*meta_payload = 0;

	ioq = hw->rx_rdyq;
	spin_lock_irqsave(&ioq->lock, flags);

	if ((ioq->count > 0) && (ioq->head != (crystalhd_elem_t *)&ioq->head)) {
		tmp = ioq->head;
		spin_unlock_irqrestore(&ioq->lock, flags);
		rpkt = (crystalhd_rx_dma_pkt *)tmp->data;
		if (rpkt) {
			// We are in process context here and have to check if we have repeated pictures
			// Drop repeated pictures or garbabge pictures here
			// This is because if we advertize a valid picture here, but later drop it
			// It will cause single threaded applications to hang, or errors in applications that expect
			// pictures not to be dropped once we have advertized their availability

			// If format change packet, then return with out checking anything
			if (!(rpkt->flags & (COMP_FLAG_PIB_VALID | COMP_FLAG_FMT_CHANGE))) {
				link_GetPictureInfo(hw, hw->PICHeight, hw->PICWidth, rpkt->dio_req,
									&PicNumber, meta_payload);
				if(!PicNumber || (PicNumber == hw->LastPicNo) || (PicNumber == hw->LastTwoPicNo)) {
					// discard picture
					if(PicNumber != 0) {
						hw->LastTwoPicNo = hw->LastPicNo;
						hw->LastPicNo = PicNumber;
					}
					rpkt = crystalhd_dioq_fetch(hw->rx_rdyq);
					if (rpkt) {
						crystalhd_dioq_add(hw->rx_freeq, rpkt, false, rpkt->pkt_tag);
						rpkt = NULL;
					}
					*meta_payload = 0;
				}
				return true;
				// Do not update the picture numbers here since they will be updated on the actual fetch of a valid picture
			}
			else
				return false; // don't use the meta_payload information
		}
		else
			return false;
	}
	spin_unlock_irqrestore(&ioq->lock, flags);

	return false;
}

bool crystalhd_link_check_input_full(struct crystalhd_hw *hw,
				   uint32_t needed_sz, uint32_t *empty_sz,
				   bool b_188_byte_pkts, uint8_t *flags)
{
	uint32_t base, end, writep, readp;
	uint32_t cpbSize, cpbFullness, fifoSize;

	if (*flags & 0x02) { /* ASF Bit is set */
		base   = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsAudCDB2Base);
		end    = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsAudCDB2End);
		writep = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsAudCDB2Wrptr);
		readp  = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsAudCDB2Rdptr);
	} else if (b_188_byte_pkts) { /*Encrypted 188 byte packets*/
		base   = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsUser0Base);
		end    = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsUser0End);
		writep = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsUser0Wrptr);
		readp  = hw->pfnReadDevRegister(hw->adp, REG_Dec_TsUser0Rdptr);
	} else {
		base   = hw->pfnReadDevRegister(hw->adp, REG_DecCA_RegCinBase);
		end    = hw->pfnReadDevRegister(hw->adp, REG_DecCA_RegCinEnd);
		writep = hw->pfnReadDevRegister(hw->adp, REG_DecCA_RegCinWrPtr);
		readp  = hw->pfnReadDevRegister(hw->adp, REG_DecCA_RegCinRdPtr);
	}

	cpbSize = end - base;
	if (writep >= readp)
		cpbFullness = writep - readp;
	else
		cpbFullness = (end - base) - (readp - writep);

	fifoSize = cpbSize - cpbFullness;


	if (fifoSize < BC_INFIFO_THRESHOLD)
	{
		*empty_sz = 0;
		return true;
	}

	if (needed_sz > (fifoSize - BC_INFIFO_THRESHOLD))
	{
		*empty_sz = 0;
		return true;
	}
	*empty_sz = fifoSize - BC_INFIFO_THRESHOLD;

	return false;
}

bool crystalhd_link_tx_list0_handler(struct crystalhd_hw *hw, uint32_t err_sts)
{
	uint32_t err_mask, tmp;

	err_mask = MISC1_TX_DMA_ERROR_STATUS_TX_L0_DESC_TX_ABORT_ERRORS_MASK |
		MISC1_TX_DMA_ERROR_STATUS_TX_L0_DMA_DATA_TX_ABORT_ERRORS_MASK |
		MISC1_TX_DMA_ERROR_STATUS_TX_L0_FIFO_FULL_ERRORS_MASK;

	if (!(err_sts & err_mask))
		return false;

	dev_err(&hw->adp->pdev->dev, "Error on Tx-L0 %x\n", err_sts);

	tmp = err_mask;

	if (err_sts & MISC1_TX_DMA_ERROR_STATUS_TX_L0_FIFO_FULL_ERRORS_MASK)
		tmp &= ~MISC1_TX_DMA_ERROR_STATUS_TX_L0_FIFO_FULL_ERRORS_MASK;

	if (tmp) {
		/* reset list index.*/
		hw->tx_list_post_index = 0;
	}

	tmp = err_sts & err_mask;
	hw->pfnWriteFPGARegister(hw->adp, MISC1_TX_DMA_ERROR_STATUS, tmp);

	return true;
}

bool crystalhd_link_tx_list1_handler(struct crystalhd_hw *hw, uint32_t err_sts)
{
	uint32_t err_mask, tmp;

	err_mask = MISC1_TX_DMA_ERROR_STATUS_TX_L1_DESC_TX_ABORT_ERRORS_MASK |
		MISC1_TX_DMA_ERROR_STATUS_TX_L1_DMA_DATA_TX_ABORT_ERRORS_MASK |
		MISC1_TX_DMA_ERROR_STATUS_TX_L1_FIFO_FULL_ERRORS_MASK;

	if (!(err_sts & err_mask))
		return false;

	dev_err(&hw->adp->pdev->dev, "Error on Tx-L1 %x\n", err_sts);

	tmp = err_mask;

	if (err_sts & MISC1_TX_DMA_ERROR_STATUS_TX_L1_FIFO_FULL_ERRORS_MASK)
		tmp &= ~MISC1_TX_DMA_ERROR_STATUS_TX_L1_FIFO_FULL_ERRORS_MASK;

	if (tmp) {
		/* reset list index.*/
		hw->tx_list_post_index = 0;
	}

	tmp = err_sts & err_mask;
	hw->pfnWriteFPGARegister(hw->adp, MISC1_TX_DMA_ERROR_STATUS, tmp);

	return true;
}

void crystalhd_link_tx_isr(struct crystalhd_hw *hw, uint32_t int_sts)
{
	uint32_t err_sts;

	if (int_sts & INTR_INTR_STATUS_L0_TX_DMA_DONE_INTR_MASK)
		crystalhd_hw_tx_req_complete(hw, hw->tx_ioq_tag_seed + 0,
					   BC_STS_SUCCESS);

	if (int_sts & INTR_INTR_STATUS_L1_TX_DMA_DONE_INTR_MASK)
		crystalhd_hw_tx_req_complete(hw, hw->tx_ioq_tag_seed + 1,
					     BC_STS_SUCCESS);

	if (!(int_sts & (INTR_INTR_STATUS_L0_TX_DMA_ERR_INTR_MASK |
			 INTR_INTR_STATUS_L1_TX_DMA_ERR_INTR_MASK)))
		/* No error mask set.. */
		return;

	/* Handle Tx errors. */
	err_sts = hw->pfnReadFPGARegister(hw->adp, MISC1_TX_DMA_ERROR_STATUS);

	if (crystalhd_link_tx_list0_handler(hw, err_sts))
		crystalhd_hw_tx_req_complete(hw, hw->tx_ioq_tag_seed + 0,
					     BC_STS_ERROR);

	if (crystalhd_link_tx_list1_handler(hw, err_sts))
		crystalhd_hw_tx_req_complete(hw, hw->tx_ioq_tag_seed + 1,
					     BC_STS_ERROR);

	hw->stats.tx_errors++;
}

void crystalhd_link_start_tx_dma_engine(struct crystalhd_hw *hw, uint8_t list_id, addr_64 desc_addr)
{
	uint32_t dma_cntrl;
	uint32_t first_desc_u_addr, first_desc_l_addr;

	if (list_id == 0) {
		first_desc_u_addr = MISC1_TX_FIRST_DESC_U_ADDR_LIST0;
		first_desc_l_addr = MISC1_TX_FIRST_DESC_L_ADDR_LIST0;
	} else {
		first_desc_u_addr = MISC1_TX_FIRST_DESC_U_ADDR_LIST1;
		first_desc_l_addr = MISC1_TX_FIRST_DESC_L_ADDR_LIST1;
	}

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp,MISC1_TX_SW_DESC_LIST_CTRL_STS);
	if (!(dma_cntrl & DMA_START_BIT)) {
		dma_cntrl |= DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_TX_SW_DESC_LIST_CTRL_STS,
			       dma_cntrl);
	}

	hw->pfnWriteFPGARegister(hw->adp, first_desc_u_addr, desc_addr.high_part);

	hw->pfnWriteFPGARegister(hw->adp, first_desc_l_addr, desc_addr.low_part | 0x01);
						/* Be sure we set the valid bit ^^^^ */
	return;
}

/* _CHECK_THIS_
 *
 * Verify if the Stop generates a completion interrupt or not.
 * if it does not generate an interrupt, then add polling here.
 */
BC_STATUS crystalhd_link_stop_tx_dma_engine(struct crystalhd_hw *hw)
{
	struct device *dev;
	uint32_t dma_cntrl, cnt = 30;
	uint32_t l1 = 1, l2 = 1;

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_TX_SW_DESC_LIST_CTRL_STS);

	dev = &hw->adp->pdev->dev;

	dev_dbg(dev, "Stopping TX DMA Engine..\n");

	if (!(dma_cntrl & DMA_START_BIT)) {
		dev_dbg(dev, "Already Stopped\n");
		return BC_STS_SUCCESS;
	}

	crystalhd_link_disable_interrupts(hw);

	/* Issue stop to HW */
	dma_cntrl &= ~DMA_START_BIT;
	hw->pfnWriteFPGARegister(hw->adp, MISC1_TX_SW_DESC_LIST_CTRL_STS, dma_cntrl);

	dev_dbg(dev, "Cleared the DMA Start bit\n");

	/* Poll for 3seconds (30 * 100ms) on both the lists..*/
	while ((l1 || l2) && cnt) {

		if (l1) {
			l1 = hw->pfnReadFPGARegister(hw->adp,
				MISC1_TX_FIRST_DESC_L_ADDR_LIST0);
			l1 &= DMA_START_BIT;
		}

		if (l2) {
			l2 = hw->pfnReadFPGARegister(hw->adp,
				MISC1_TX_FIRST_DESC_L_ADDR_LIST1);
			l2 &= DMA_START_BIT;
		}

		msleep_interruptible(100);

		cnt--;
	}

	if (!cnt) {
		dev_err(dev, "Failed to stop TX DMA.. l1 %d, l2 %d\n", l1, l2);
		crystalhd_link_enable_interrupts(hw);
		return BC_STS_ERROR;
	}

	hw->tx_list_post_index = 0;
	dev_dbg(dev, "stopped TX DMA..\n");
	crystalhd_link_enable_interrupts(hw);

	return BC_STS_SUCCESS;
}

uint32_t crystalhd_link_get_pib_avail_cnt(struct crystalhd_hw *hw)
{
	/*
	* Position of the PIB Entries can be found at
	* 0th and the 1st location of the Circular list.
	*/
	uint32_t Q_addr;
	uint32_t pib_cnt, r_offset, w_offset;

	Q_addr = hw->pib_del_Q_addr;

	/* Get the Read Pointer */
	crystalhd_link_mem_rd(hw, Q_addr, 1, &r_offset);

	/* Get the Write Pointer */
	crystalhd_link_mem_rd(hw, Q_addr + sizeof(uint32_t), 1, &w_offset);

	if (r_offset == w_offset)
		return 0;	/* Queue is empty */

	if (w_offset > r_offset)
		pib_cnt = w_offset - r_offset;
	else
		pib_cnt = (w_offset + MAX_PIB_Q_DEPTH) -
			  (r_offset + MIN_PIB_Q_DEPTH);

	if (pib_cnt > MAX_PIB_Q_DEPTH) {
		dev_err(&hw->adp->pdev->dev, "Invalid PIB Count (%u)\n", pib_cnt);
		return 0;
	}

	return pib_cnt;
}

uint32_t crystalhd_link_get_addr_from_pib_Q(struct crystalhd_hw *hw)
{
	uint32_t Q_addr;
	uint32_t addr_entry, r_offset, w_offset;

	Q_addr = hw->pib_del_Q_addr;

	/* Get the Read Pointer 0Th Location is Read Pointer */
	crystalhd_link_mem_rd(hw, Q_addr, 1, &r_offset);

	/* Get the Write Pointer 1st Location is Write pointer */
	crystalhd_link_mem_rd(hw, Q_addr + sizeof(uint32_t), 1, &w_offset);

	/* Queue is empty */
	if (r_offset == w_offset)
		return 0;

	if ((r_offset < MIN_PIB_Q_DEPTH) || (r_offset >= MAX_PIB_Q_DEPTH))
		return 0;

	/* Get the Actual Address of the PIB */
	crystalhd_link_mem_rd(hw, Q_addr + (r_offset * sizeof(uint32_t)),
		       1, &addr_entry);

	/* Increment the Read Pointer */
	r_offset++;

	if (MAX_PIB_Q_DEPTH == r_offset)
		r_offset = MIN_PIB_Q_DEPTH;

	/* Write back the read pointer to It's Location */
	crystalhd_link_mem_wr(hw, Q_addr, 1, &r_offset);

	return addr_entry;
}

bool crystalhd_link_rel_addr_to_pib_Q(struct crystalhd_hw *hw, uint32_t addr_to_rel)
{
	uint32_t Q_addr;
	uint32_t r_offset, w_offset, n_offset;

	Q_addr = hw->pib_rel_Q_addr;

	/* Get the Read Pointer */
	crystalhd_link_mem_rd(hw, Q_addr, 1, &r_offset);

	/* Get the Write Pointer */
	crystalhd_link_mem_rd(hw, Q_addr + sizeof(uint32_t), 1, &w_offset);

	if ((r_offset < MIN_PIB_Q_DEPTH) ||
	    (r_offset >= MAX_PIB_Q_DEPTH))
		return false;

	n_offset = w_offset + 1;

	if (MAX_PIB_Q_DEPTH == n_offset)
		n_offset = MIN_PIB_Q_DEPTH;

	if (r_offset == n_offset)
		return false; /* should never happen */

	/* Write the DRAM ADDR to the Queue at Next Offset */
	crystalhd_link_mem_wr(hw, Q_addr + (w_offset * sizeof(uint32_t)),
		       1, &addr_to_rel);

	/* Put the New value of the write pointer in Queue */
	crystalhd_link_mem_wr(hw, Q_addr + sizeof(uint32_t), 1, &n_offset);

	return true;
}

void link_cpy_pib_to_app(C011_PIB *src_pib, BC_PIC_INFO_BLOCK *dst_pib)
{
	if (!src_pib || !dst_pib) {
		printk(KERN_ERR "%s: Invalid Arguments\n", __func__);
		return;
	}

	dst_pib->timeStamp		= 0;
	dst_pib->picture_number		= src_pib->ppb.picture_number;
	dst_pib->width			= src_pib->ppb.width;
	dst_pib->height			= src_pib->ppb.height;
	dst_pib->chroma_format		= src_pib->ppb.chroma_format;
	dst_pib->pulldown		= src_pib->ppb.pulldown;
	dst_pib->flags			= src_pib->ppb.flags;
	dst_pib->sess_num		= src_pib->ptsStcOffset;
	dst_pib->aspect_ratio		= src_pib->ppb.aspect_ratio;
	dst_pib->colour_primaries	= src_pib->ppb.colour_primaries;
	dst_pib->picture_meta_payload	= src_pib->ppb.picture_meta_payload;
	dst_pib->frame_rate		= src_pib->resolution ;
	return;
}

void crystalhd_link_proc_pib(struct crystalhd_hw *hw)
{
	unsigned int cnt;
	C011_PIB src_pib;
	uint32_t pib_addr, pib_cnt;
	BC_PIC_INFO_BLOCK *AppPib;
	crystalhd_rx_dma_pkt *rx_pkt = NULL;

	pib_cnt = crystalhd_link_get_pib_avail_cnt(hw);

	if (!pib_cnt)
		return;

	for (cnt = 0; cnt < pib_cnt; cnt++) {
		pib_addr = crystalhd_link_get_addr_from_pib_Q(hw);
		crystalhd_link_mem_rd(hw, pib_addr, sizeof(C011_PIB) / 4,
				 (uint32_t *)&src_pib);

		if (src_pib.bFormatChange) {
			rx_pkt = (crystalhd_rx_dma_pkt *)
					crystalhd_dioq_fetch(hw->rx_freeq);
			if (!rx_pkt)
				return;

			rx_pkt->flags = 0;
			rx_pkt->flags |= COMP_FLAG_PIB_VALID |
					 COMP_FLAG_FMT_CHANGE;
			AppPib = &rx_pkt->pib;
			link_cpy_pib_to_app(&src_pib, AppPib);

			hw->PICHeight = rx_pkt->pib.height;
			if (rx_pkt->pib.width > 1280)
				hw->PICWidth = 1920;
			else if (rx_pkt->pib.width > 720)
				hw->PICWidth = 1280;
			else
				hw->PICWidth = 720;

			dev_info(&hw->adp->pdev->dev,
				"[FMT CH] PIB:%x %x %x %x %x %x %x %x %x %x\n",
				rx_pkt->pib.picture_number,
				rx_pkt->pib.aspect_ratio,
				rx_pkt->pib.chroma_format,
				rx_pkt->pib.colour_primaries,
				rx_pkt->pib.frame_rate,
				rx_pkt->pib.height,
				rx_pkt->pib.width,
				rx_pkt->pib.n_drop,
				rx_pkt->pib.pulldown,
				rx_pkt->pib.ycom);

			crystalhd_dioq_add(hw->rx_rdyq, (void *)rx_pkt,
					   true, rx_pkt->pkt_tag);

		}

		crystalhd_link_rel_addr_to_pib_Q(hw, pib_addr);
	}
}

void crystalhd_link_start_rx_dma_engine(struct crystalhd_hw *hw)
{
	uint32_t        dma_cntrl;

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS);
	if (!(dma_cntrl & DMA_START_BIT)) {
		dma_cntrl |= DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS);
	if (!(dma_cntrl & DMA_START_BIT)) {
		dma_cntrl |= DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}

	return;
}

void crystalhd_link_stop_rx_dma_engine(struct crystalhd_hw *hw)
{
	struct device *dev = &hw->adp->pdev->dev;
	uint32_t dma_cntrl = 0, count = 30;
	uint32_t l0y = 1, l0uv = 1, l1y = 1, l1uv = 1;

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS);
	if ((dma_cntrl & DMA_START_BIT)) {
		dma_cntrl &= ~DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS);
	if ((dma_cntrl & DMA_START_BIT)) {
		dma_cntrl &= ~DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}

	/* Poll for 3seconds (30 * 100ms) on both the lists..*/
	while ((l0y || l0uv || l1y || l1uv) && count) {

		if (l0y) {
			l0y = hw->pfnReadFPGARegister(hw->adp,
				MISC1_Y_RX_FIRST_DESC_L_ADDR_LIST0);
			l0y &= DMA_START_BIT;
			if (!l0y)
				hw->rx_list_sts[0] &= ~rx_waiting_y_intr;
		}

		if (l1y) {
			l1y = hw->pfnReadFPGARegister(hw->adp,
				MISC1_Y_RX_FIRST_DESC_L_ADDR_LIST1);
			l1y &= DMA_START_BIT;
			if (!l1y)
				hw->rx_list_sts[1] &= ~rx_waiting_y_intr;
		}

		if (l0uv) {
			l0uv = hw->pfnReadFPGARegister(hw->adp,
				MISC1_UV_RX_FIRST_DESC_L_ADDR_LIST0);
			l0uv &= DMA_START_BIT;
			if (!l0uv)
				hw->rx_list_sts[0] &= ~rx_waiting_uv_intr;
		}

		if (l1uv) {
			l1uv = hw->pfnReadFPGARegister(hw->adp,
				MISC1_UV_RX_FIRST_DESC_L_ADDR_LIST1);
			l1uv &= DMA_START_BIT;
			if (!l1uv)
				hw->rx_list_sts[1] &= ~rx_waiting_uv_intr;
		}
		msleep_interruptible(100);
		count--;
	}

	hw->rx_list_post_index = 0;

	dev_dbg(dev, "Capture Stop: %d List0:Sts:%x List1:Sts:%x\n",
		count, hw->rx_list_sts[0], hw->rx_list_sts[1]);
}

BC_STATUS crystalhd_link_hw_prog_rxdma(struct crystalhd_hw *hw,
					 crystalhd_rx_dma_pkt *rx_pkt)
{
	struct device *dev;
	uint32_t y_low_addr_reg, y_high_addr_reg;
	uint32_t uv_low_addr_reg, uv_high_addr_reg;
	addr_64 desc_addr;
	unsigned long flags;

	if (!hw || !rx_pkt) {
		printk(KERN_ERR "%s: Invalid Arguments\n", __func__);
		return BC_STS_INV_ARG;
	}

	dev = &hw->adp->pdev->dev;

	if (hw->rx_list_post_index >= DMA_ENGINE_CNT) {
		dev_err(dev, "List Out Of bounds %x\n", hw->rx_list_post_index);
		return BC_STS_INV_ARG;
	}

	spin_lock_irqsave(&hw->rx_lock, flags);
	if (hw->rx_list_sts[hw->rx_list_post_index]) {
		spin_unlock_irqrestore(&hw->rx_lock, flags);
		return BC_STS_BUSY;
	}

	if (!hw->rx_list_post_index) {
		y_low_addr_reg   = MISC1_Y_RX_FIRST_DESC_L_ADDR_LIST0;
		y_high_addr_reg  = MISC1_Y_RX_FIRST_DESC_U_ADDR_LIST0;
		uv_low_addr_reg  = MISC1_UV_RX_FIRST_DESC_L_ADDR_LIST0;
		uv_high_addr_reg = MISC1_UV_RX_FIRST_DESC_U_ADDR_LIST0;
	} else {
		y_low_addr_reg   = MISC1_Y_RX_FIRST_DESC_L_ADDR_LIST1;
		y_high_addr_reg  = MISC1_Y_RX_FIRST_DESC_U_ADDR_LIST1;
		uv_low_addr_reg  = MISC1_UV_RX_FIRST_DESC_L_ADDR_LIST1;
		uv_high_addr_reg = MISC1_UV_RX_FIRST_DESC_U_ADDR_LIST1;
	}
	rx_pkt->pkt_tag = hw->rx_pkt_tag_seed + hw->rx_list_post_index;
	hw->rx_list_sts[hw->rx_list_post_index] |= rx_waiting_y_intr;
	if (rx_pkt->uv_phy_addr)
		hw->rx_list_sts[hw->rx_list_post_index] |= rx_waiting_uv_intr;
	hw->rx_list_post_index = (hw->rx_list_post_index + 1) % DMA_ENGINE_CNT;

	crystalhd_dioq_add(hw->rx_actq, (void *)rx_pkt, false, rx_pkt->pkt_tag);

	crystalhd_link_start_rx_dma_engine(hw);
	/* Program the Y descriptor */
	desc_addr.full_addr = rx_pkt->desc_mem.phy_addr;
	hw->pfnWriteFPGARegister(hw->adp, y_high_addr_reg, desc_addr.high_part);
	hw->pfnWriteFPGARegister(hw->adp, y_low_addr_reg, desc_addr.low_part | 0x01);

	if (rx_pkt->uv_phy_addr) {
		/* Program the UV descriptor */
		desc_addr.full_addr = rx_pkt->uv_phy_addr;
		hw->pfnWriteFPGARegister(hw->adp, uv_high_addr_reg, desc_addr.high_part);
		hw->pfnWriteFPGARegister(hw->adp, uv_low_addr_reg, desc_addr.low_part | 0x01);
	}

	spin_unlock_irqrestore(&hw->rx_lock, flags);

	return BC_STS_SUCCESS;
}

BC_STATUS crystalhd_link_hw_post_cap_buff(struct crystalhd_hw *hw,
					  crystalhd_rx_dma_pkt *rx_pkt)
{
	BC_STATUS sts = crystalhd_link_hw_prog_rxdma(hw, rx_pkt);

	if (sts == BC_STS_BUSY)
		crystalhd_dioq_add(hw->rx_freeq, (void *)rx_pkt,
				 false, rx_pkt->pkt_tag);

	return sts;
}

void crystalhd_link_get_dnsz(struct crystalhd_hw *hw, uint32_t list_index,
			     uint32_t *y_dw_dnsz, uint32_t *uv_dw_dnsz)
{
	uint32_t y_dn_sz_reg, uv_dn_sz_reg;

	if (!list_index) {
		y_dn_sz_reg  = MISC1_Y_RX_LIST0_CUR_BYTE_CNT;
		uv_dn_sz_reg = MISC1_UV_RX_LIST0_CUR_BYTE_CNT;
	} else {
		y_dn_sz_reg  = MISC1_Y_RX_LIST1_CUR_BYTE_CNT;
		uv_dn_sz_reg = MISC1_UV_RX_LIST1_CUR_BYTE_CNT;
	}

	*y_dw_dnsz  = hw->pfnReadFPGARegister(hw->adp, y_dn_sz_reg);
	*uv_dw_dnsz = hw->pfnReadFPGARegister(hw->adp, uv_dn_sz_reg);
}

/*
 * This function should be called only after making sure that the two DMA
 * lists are free. This function does not check if DMA's are active, before
 * turning off the DMA.
 */
void crystalhd_link_hw_finalize_pause(struct crystalhd_hw *hw)
{
	uint32_t dma_cntrl;

	hw->stop_pending = 0;

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS);
	if (dma_cntrl & DMA_START_BIT) {
		dma_cntrl &= ~DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}

	dma_cntrl = hw->pfnReadFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS);
	if (dma_cntrl & DMA_START_BIT) {
		dma_cntrl &= ~DMA_START_BIT;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_SW_DESC_LIST_CTRL_STS,
				 dma_cntrl);
	}
	hw->rx_list_post_index = 0;

// 	aspm = crystalhd_reg_rd(hw->adp, PCIE_DLL_DATA_LINK_CONTROL);
// 	aspm |= ASPM_L1_ENABLE;
// 	dev_info(&hw->adp->pdev->dev, "aspm on\n");
// 	crystalhd_reg_wr(hw->adp, PCIE_DLL_DATA_LINK_CONTROL, aspm);
}

bool crystalhd_link_rx_list0_handler(struct crystalhd_hw *hw,
				       uint32_t int_sts,
				       uint32_t y_err_sts,
				       uint32_t uv_err_sts)
{
	uint32_t tmp;
	list_sts tmp_lsts;

	if (!(y_err_sts & GET_Y0_ERR_MSK) && !(uv_err_sts & GET_UV0_ERR_MSK))
		return false;

	tmp_lsts = hw->rx_list_sts[0];

	/* Y0 - DMA */
	tmp = y_err_sts & GET_Y0_ERR_MSK;
	if (int_sts & INTR_INTR_STATUS_L0_Y_RX_DMA_DONE_INTR_MASK)
		hw->rx_list_sts[0] &= ~rx_waiting_y_intr;

	if (y_err_sts & MISC1_Y_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK) {
		hw->rx_list_sts[0] &= ~rx_waiting_y_intr;
		tmp &= ~MISC1_Y_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK;
	}

	if (y_err_sts & MISC1_Y_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK) {
		hw->rx_list_sts[0] &= ~rx_y_mask;
		hw->rx_list_sts[0] |= rx_y_error;
		tmp &= ~MISC1_Y_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK;
	}

	if (tmp) {
		hw->rx_list_sts[0] &= ~rx_y_mask;
		hw->rx_list_sts[0] |= rx_y_error;
		hw->rx_list_post_index = 0;
	}

	/* UV0 - DMA */
	tmp = uv_err_sts & GET_UV0_ERR_MSK;
	if (int_sts & INTR_INTR_STATUS_L0_UV_RX_DMA_DONE_INTR_MASK)
		hw->rx_list_sts[0] &= ~rx_waiting_uv_intr;

	if (uv_err_sts & MISC1_UV_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK) {
		hw->rx_list_sts[0] &= ~rx_waiting_uv_intr;
		tmp &= ~MISC1_UV_RX_ERROR_STATUS_RX_L0_UNDERRUN_ERROR_MASK;
	}

	if (uv_err_sts & MISC1_UV_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK) {
		hw->rx_list_sts[0] &= ~rx_uv_mask;
		hw->rx_list_sts[0] |= rx_uv_error;
		tmp &= ~MISC1_UV_RX_ERROR_STATUS_RX_L0_FIFO_FULL_ERRORS_MASK;
	}

	if (tmp) {
		hw->rx_list_sts[0] &= ~rx_uv_mask;
		hw->rx_list_sts[0] |= rx_uv_error;
		hw->rx_list_post_index = 0;
	}

	if (y_err_sts & GET_Y0_ERR_MSK) {
		tmp = y_err_sts & GET_Y0_ERR_MSK;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_ERROR_STATUS, tmp);
	}

	if (uv_err_sts & GET_UV0_ERR_MSK) {
		tmp = uv_err_sts & GET_UV0_ERR_MSK;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_ERROR_STATUS, tmp);
	}

	return (tmp_lsts != hw->rx_list_sts[0]);
}

bool crystalhd_link_rx_list1_handler(struct crystalhd_hw *hw,
				       uint32_t int_sts, uint32_t y_err_sts,
				       uint32_t uv_err_sts)
{
	uint32_t tmp;
	list_sts tmp_lsts;

	if (!(y_err_sts & GET_Y1_ERR_MSK) && !(uv_err_sts & GET_UV1_ERR_MSK))
		return false;

	tmp_lsts = hw->rx_list_sts[1];

	/* Y1 - DMA */
	tmp = y_err_sts & GET_Y1_ERR_MSK;
	if (int_sts & INTR_INTR_STATUS_L1_Y_RX_DMA_DONE_INTR_MASK)
		hw->rx_list_sts[1] &= ~rx_waiting_y_intr;

	if (y_err_sts & MISC1_Y_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK) {
		hw->rx_list_sts[1] &= ~rx_waiting_y_intr;
		tmp &= ~MISC1_Y_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK;
	}

	if (y_err_sts & MISC1_Y_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK) {
		/* Add retry-support..*/
		hw->rx_list_sts[1] &= ~rx_y_mask;
		hw->rx_list_sts[1] |= rx_y_error;
		tmp &= ~MISC1_Y_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK;
	}

	if (tmp) {
		hw->rx_list_sts[1] &= ~rx_y_mask;
		hw->rx_list_sts[1] |= rx_y_error;
		hw->rx_list_post_index = 0;
	}

	/* UV1 - DMA */
	tmp = uv_err_sts & GET_UV1_ERR_MSK;
	if (int_sts & INTR_INTR_STATUS_L1_UV_RX_DMA_DONE_INTR_MASK)
		hw->rx_list_sts[1] &= ~rx_waiting_uv_intr;

	if (uv_err_sts & MISC1_UV_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK) {
		hw->rx_list_sts[1] &= ~rx_waiting_uv_intr;
		tmp &= ~MISC1_UV_RX_ERROR_STATUS_RX_L1_UNDERRUN_ERROR_MASK;
	}

	if (uv_err_sts & MISC1_UV_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK) {
		/* Add retry-support*/
		hw->rx_list_sts[1] &= ~rx_uv_mask;
		hw->rx_list_sts[1] |= rx_uv_error;
		tmp &= ~MISC1_UV_RX_ERROR_STATUS_RX_L1_FIFO_FULL_ERRORS_MASK;
	}

	if (tmp) {
		hw->rx_list_sts[1] &= ~rx_uv_mask;
		hw->rx_list_sts[1] |= rx_uv_error;
		hw->rx_list_post_index = 0;
	}

	if (y_err_sts & GET_Y1_ERR_MSK) {
		tmp = y_err_sts & GET_Y1_ERR_MSK;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_Y_RX_ERROR_STATUS, tmp);
	}

	if (uv_err_sts & GET_UV1_ERR_MSK) {
		tmp = uv_err_sts & GET_UV1_ERR_MSK;
		hw->pfnWriteFPGARegister(hw->adp, MISC1_UV_RX_ERROR_STATUS, tmp);
	}

	return (tmp_lsts != hw->rx_list_sts[1]);
}

void crystalhd_link_rx_isr(struct crystalhd_hw *hw, uint32_t intr_sts)
{
	unsigned long flags;
	uint32_t i, list_avail = 0;
	BC_STATUS comp_sts = BC_STS_NO_DATA;
	uint32_t y_err_sts, uv_err_sts, y_dn_sz = 0, uv_dn_sz = 0;
	bool ret = 0;

	if (!hw) {
		printk(KERN_ERR "%s: Invalid Arguments\n", __func__);
		return;
	}

	if (!(intr_sts & GET_RX_INTR_MASK))
		return;

	y_err_sts = hw->pfnReadFPGARegister(hw->adp, MISC1_Y_RX_ERROR_STATUS);
	uv_err_sts = hw->pfnReadFPGARegister(hw->adp, MISC1_UV_RX_ERROR_STATUS);

	for (i = 0; i < DMA_ENGINE_CNT; i++) {
		/* Update States..*/
		spin_lock_irqsave(&hw->rx_lock, flags);
		if (i == 0)
			ret = crystalhd_link_rx_list0_handler(hw, intr_sts, y_err_sts, uv_err_sts);
		else
			ret = crystalhd_link_rx_list1_handler(hw, intr_sts, y_err_sts, uv_err_sts);
		if (ret) {
			switch (hw->rx_list_sts[i]) {
			case sts_free:
				comp_sts = BC_STS_SUCCESS;
				list_avail = 1;
				hw->stats.rx_success++;
				break;
			case rx_y_error:
			case rx_uv_error:
			case rx_sts_error:
				/* We got error on both or Y or uv. */
				hw->stats.rx_errors++;
				hw->pfnHWGetDoneSize(hw, i, &y_dn_sz, &uv_dn_sz);
				dev_info(&hw->adp->pdev->dev, "list_index:%x "
					"rx[%d] rxtot[%d] Y:%x UV:%x Int:%x YDnSz:%x "
					"UVDnSz:%x\n", i, hw->stats.rx_errors,
					hw->stats.rx_errors + hw->stats.rx_success,
					y_err_sts, uv_err_sts, intr_sts,
					y_dn_sz, uv_dn_sz);
				hw->rx_list_sts[i] = sts_free;
				comp_sts = BC_STS_ERROR;
				break;
			default:
				/* Wait for completion..*/
				comp_sts = BC_STS_NO_DATA;
				break;
			}
		}
		spin_unlock_irqrestore(&hw->rx_lock, flags);

		/* handle completion...*/
		if (comp_sts != BC_STS_NO_DATA) {
			crystalhd_rx_pkt_done(hw, i, comp_sts);
			comp_sts = BC_STS_NO_DATA;
		}
	}

	if (list_avail) {
		if (hw->stop_pending) {
			if ((hw->rx_list_sts[0] == sts_free) &&
			    (hw->rx_list_sts[1] == sts_free))
				crystalhd_link_hw_finalize_pause(hw);
		} else {
			if(!hw->hw_pause_issued)
				crystalhd_hw_start_capture(hw);
		}
	}
}

BC_STATUS crystalhd_link_hw_pause(struct crystalhd_hw *hw, bool state)
{
	uint32_t pause = 0;
	BC_STATUS sts = BC_STS_SUCCESS;

	if(state) {
		pause = 1;
		hw->stats.pause_cnt++;
		hw->stop_pending = 1;
		hw->pfnWriteDevRegister(hw->adp, HW_PauseMbx, pause);

		if ((hw->rx_list_sts[0] == sts_free) &&
			(hw->rx_list_sts[1] == sts_free))
			crystalhd_link_hw_finalize_pause(hw);
	} else {
		pause = 0;
		hw->stop_pending = 0;
		sts = crystalhd_hw_start_capture(hw);
		hw->pfnWriteDevRegister(hw->adp, HW_PauseMbx, pause);
	}
	return sts;
}

BC_STATUS crystalhd_link_fw_cmd_post_proc(struct crystalhd_hw *hw,
					  BC_FW_CMD *fw_cmd)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	DecRspChannelStartVideo *st_rsp = NULL;

	switch (fw_cmd->cmd[0]) {
	case eCMD_C011_DEC_CHAN_START_VIDEO:
		st_rsp = (DecRspChannelStartVideo *)fw_cmd->rsp;
		hw->pib_del_Q_addr = st_rsp->picInfoDeliveryQ;
		hw->pib_rel_Q_addr = st_rsp->picInfoReleaseQ;
		dev_dbg(&hw->adp->pdev->dev, "DelQAddr:%x RelQAddr:%x\n",
		       hw->pib_del_Q_addr, hw->pib_rel_Q_addr);
		break;
	case eCMD_C011_INIT:
		if (!(crystalhd_link_load_firmware_config(hw))) {
			dev_err(&hw->adp->pdev->dev, "Invalid Params\n");
			sts = BC_STS_FW_AUTH_FAILED;
		}
		break;
	default:
		break;
	}
	return sts;
}

BC_STATUS crystalhd_link_put_ddr2sleep(struct crystalhd_hw *hw)
{
	uint32_t reg;
	link_misc_perst_decoder_ctrl rst_cntrl_reg;

	/* Pulse reset pin of 7412 (MISC_PERST_DECODER_CTRL) */
	rst_cntrl_reg.whole_reg = hw->pfnReadDevRegister(hw->adp, MISC_PERST_DECODER_CTRL);

	rst_cntrl_reg.bcm_7412_rst = 1;
	hw->pfnWriteDevRegister(hw->adp, MISC_PERST_DECODER_CTRL, rst_cntrl_reg.whole_reg);
	msleep_interruptible(50);

	rst_cntrl_reg.bcm_7412_rst = 0;
	hw->pfnWriteDevRegister(hw->adp, MISC_PERST_DECODER_CTRL, rst_cntrl_reg.whole_reg);

	/* Close all banks, put DDR in idle */
	hw->pfnWriteDevRegister(hw->adp, SDRAM_PRECHARGE, 0);

	/* Set bit 25 (drop CKE pin of DDR) */
	reg = hw->pfnReadDevRegister(hw->adp, SDRAM_PARAM);
	reg |= 0x02000000;
	hw->pfnWriteDevRegister(hw->adp, SDRAM_PARAM, reg);

	/* Reset the audio block */
	hw->pfnWriteDevRegister(hw->adp, AUD_DSP_MISC_SOFT_RESET, 0x1);

	/* Power down Raptor PLL */
	reg = hw->pfnReadDevRegister(hw->adp, DecHt_PllCCtl);
	reg |= 0x00008000;
	hw->pfnWriteDevRegister(hw->adp, DecHt_PllCCtl, reg);

	/* Power down all Audio PLL */
	hw->pfnWriteDevRegister(hw->adp, AIO_MISC_PLL_RESET, 0x1);

	/* Power down video clock (75MHz) */
	reg = hw->pfnReadDevRegister(hw->adp, DecHt_PllECtl);
	reg |= 0x00008000;
	hw->pfnWriteDevRegister(hw->adp, DecHt_PllECtl, reg);

	/* Power down video clock (75MHz) */
	reg = hw->pfnReadDevRegister(hw->adp, DecHt_PllDCtl);
	reg |= 0x00008000;
	hw->pfnWriteDevRegister(hw->adp, DecHt_PllDCtl, reg);

	/* Power down core clock (200MHz) */
	reg = hw->pfnReadDevRegister(hw->adp, DecHt_PllACtl);
	reg |= 0x00008000;
	hw->pfnWriteDevRegister(hw->adp, DecHt_PllACtl, reg);

	/* Power down core clock (200MHz) */
	reg = hw->pfnReadDevRegister(hw->adp, DecHt_PllBCtl);
	reg |= 0x00008000;
	hw->pfnWriteDevRegister(hw->adp, DecHt_PllBCtl, reg);

	return BC_STS_SUCCESS;
}

/************************************************
**
*************************************************/

BC_STATUS crystalhd_link_download_fw(struct crystalhd_hw *hw,
				uint8_t *buffer, uint32_t sz)
{
	struct device *dev;
	uint32_t reg_data, cnt, *temp_buff;
	uint32_t fw_sig_len = 36;
	uint32_t dram_offset = BC_FWIMG_ST_ADDR, sig_reg;

	if (!hw || !buffer || !sz) {
		printk(KERN_ERR "%s: Invalid Params\n", __func__);
		return BC_STS_INV_ARG;
	}

	dev = &hw->adp->pdev->dev;

	dev_dbg(dev, "%s entered\n", __func__);

	reg_data = hw->pfnReadFPGARegister(hw->adp, OTP_CMD);
	if (!(reg_data & 0x02)) {
		dev_err(dev, "Invalid hw config.. otp not programmed\n");
		return BC_STS_ERROR;
	}

	reg_data = 0;
	hw->pfnWriteFPGARegister(hw->adp, DCI_CMD, 0);
	reg_data |= BC_BIT(0);
	hw->pfnWriteFPGARegister(hw->adp, DCI_CMD, reg_data);

	reg_data = 0;
	cnt = 1000;
	msleep_interruptible(10);

	while (reg_data != BC_BIT(4)) {
		reg_data = hw->pfnReadFPGARegister(hw->adp, DCI_STATUS);
		reg_data &= BC_BIT(4);
		if (--cnt == 0) {
			dev_err(dev, "Firmware Download RDY Timeout.\n");
			return BC_STS_TIMEOUT;
		}
	}

	msleep_interruptible(10);
	/*  Load the FW to the FW_ADDR field in the DCI_FIRMWARE_ADDR */
	hw->pfnWriteFPGARegister(hw->adp, DCI_FIRMWARE_ADDR, dram_offset);
	temp_buff = (uint32_t *)buffer;
	for (cnt = 0; cnt < (sz - fw_sig_len); cnt += 4) {
		hw->pfnWriteFPGARegister(hw->adp, DCI_DRAM_BASE_ADDR, (dram_offset >> 19));
		hw->pfnWriteFPGARegister(hw->adp, DCI_FIRMWARE_DATA, *temp_buff);
		dram_offset += 4;
		temp_buff++;
	}
	msleep_interruptible(10);

	temp_buff++;

	sig_reg = (uint32_t)DCI_SIGNATURE_DATA_7;
	for (cnt = 0; cnt < 8; cnt++) {
		uint32_t swapped_data = *temp_buff;
		swapped_data = bswap_32_1(swapped_data);
		hw->pfnWriteFPGARegister(hw->adp, sig_reg, swapped_data);
		sig_reg -= 4;
		temp_buff++;
	}
	msleep_interruptible(10);

	reg_data = 0;
	reg_data |= BC_BIT(1);
	hw->pfnWriteFPGARegister(hw->adp, DCI_CMD, reg_data);
	msleep_interruptible(10);

	reg_data = 0;
	reg_data = hw->pfnReadFPGARegister(hw->adp, DCI_STATUS);

	if ((reg_data & BC_BIT(9)) == BC_BIT(9)) {
		cnt = 1000;
		while ((reg_data & BC_BIT(0)) != BC_BIT(0)) {
			reg_data = hw->pfnReadFPGARegister(hw->adp, DCI_STATUS);
			reg_data &= BC_BIT(0);
			if (!(--cnt))
				break;
			msleep_interruptible(10);
		}
		reg_data = 0;
		reg_data = hw->pfnReadFPGARegister(hw->adp, DCI_CMD);
		reg_data |= BC_BIT(4);
		hw->pfnWriteFPGARegister(hw->adp, DCI_CMD, reg_data);

	} else {
		dev_err(dev, "F/w Signature mismatch\n");
		return BC_STS_FW_AUTH_FAILED;
	}

	dev_info(dev, "Firmware Downloaded Successfully\n");

	// Load command response addresses
	hw->fwcmdPostAddr = TS_Host2CpuSnd;
	hw->fwcmdPostMbox = Hst2CpuMbx1;
	hw->fwcmdRespMbox = Cpu2HstMbx1;

	return BC_STS_SUCCESS;;
}

BC_STATUS crystalhd_link_do_fw_cmd(struct crystalhd_hw *hw, BC_FW_CMD *fw_cmd)
{
	struct device *dev;
	uint32_t cnt = 0, cmd_res_addr;
	uint32_t *cmd_buff, *res_buff;
	wait_queue_head_t fw_cmd_event;
	int rc = 0;
	BC_STATUS sts;
	unsigned long flags;

	crystalhd_create_event(&fw_cmd_event);

	if (!hw || !fw_cmd) {
		printk(KERN_ERR "%s: Invalid Arguments\n", __func__);
		return BC_STS_INV_ARG;
	}

	dev = &hw->adp->pdev->dev;

	dev_dbg(dev, "%s entered\n", __func__);

	cmd_buff = fw_cmd->cmd;
	res_buff = fw_cmd->rsp;

	if (!cmd_buff || !res_buff) {
		dev_err(dev, "Invalid Parameters for F/W Command\n");
		return BC_STS_INV_ARG;
	}

	hw->fwcmd_evt_sts = 0;
	hw->pfw_cmd_event = &fw_cmd_event;

	spin_lock_irqsave(&hw->lock, flags);

	/*Write the command to the memory*/
	hw->pfnDevDRAMWrite(hw, hw->fwcmdPostAddr, FW_CMD_BUFF_SZ, cmd_buff);

	/*Memory Read for memory arbitrator flush*/
	hw->pfnDevDRAMRead(hw, hw->fwcmdPostAddr, 1, &cnt);

	/* Write the command address to mailbox */
	hw->pfnWriteDevRegister(hw->adp, hw->fwcmdPostMbox, hw->fwcmdPostAddr);

	spin_unlock_irqrestore(&hw->lock, flags);

	msleep_interruptible(50);

	crystalhd_wait_on_event(&fw_cmd_event, hw->fwcmd_evt_sts,
				20000, rc, false);

	if (!rc) {
		sts = BC_STS_SUCCESS;
	} else if (rc == -EBUSY) {
		dev_err(dev, "Firmware command T/O\n");
		sts = BC_STS_TIMEOUT;
	} else if (rc == -EINTR) {
		dev_dbg(dev, "FwCmd Wait Signal int.\n");
		sts = BC_STS_IO_USER_ABORT;
	} else {
		dev_err(dev, "FwCmd IO Error.\n");
		sts = BC_STS_IO_ERROR;
	}

	if (sts != BC_STS_SUCCESS) {
		dev_err(dev, "FwCmd Failed.\n");
		return sts;
	}

	spin_lock_irqsave(&hw->lock, flags);

	/*Get the Responce Address*/
	cmd_res_addr = hw->pfnReadDevRegister(hw->adp, hw->fwcmdRespMbox);

	/*Read the Response*/
	hw->pfnDevDRAMRead(hw, cmd_res_addr, FW_CMD_BUFF_SZ, res_buff);

	spin_unlock_irqrestore(&hw->lock, flags);

	if (res_buff[2] != C011_RET_SUCCESS) {
		dev_err(dev, "res_buff[2] != C011_RET_SUCCESS\n");
		return BC_STS_FW_CMD_ERR;
	}

	sts = crystalhd_link_fw_cmd_post_proc(hw, fw_cmd);
	if (sts != BC_STS_SUCCESS)
		dev_err(dev, "crystalhd_fw_cmd_post_proc Failed.\n");

	return sts;
}

bool crystalhd_link_hw_interrupt_handle(struct crystalhd_adp *adp, struct crystalhd_hw *hw)
{
	uint32_t intr_sts = 0;
	uint32_t deco_intr = 0;
	bool rc = false;

	if (!adp || !hw->dev_started)
		return rc;

	hw->stats.num_interrupts++;

	deco_intr = hw->pfnReadDevRegister(hw->adp, Stream2Host_Intr_Sts);
	intr_sts  = hw->pfnReadFPGARegister(hw->adp, INTR_INTR_STATUS);

	if (intr_sts) {
		/* let system know we processed interrupt..*/
		rc = true;
		hw->stats.dev_interrupts++;
	}

	if (deco_intr && (deco_intr != 0xdeaddead)) {

		if (deco_intr & 0x80000000) {
			/*Set the Event and the status flag*/
			if (hw->pfw_cmd_event) {
				hw->fwcmd_evt_sts = 1;
				crystalhd_set_event(hw->pfw_cmd_event);
			}
		}

		if (deco_intr & BC_BIT(1))
			crystalhd_link_proc_pib(hw);

		hw->pfnWriteDevRegister(hw->adp, Stream2Host_Intr_Sts, deco_intr);
		hw->pfnWriteDevRegister(hw->adp, Stream2Host_Intr_Sts, 0);
		rc = 1;
	}

	/* Rx interrupts */
	crystalhd_link_rx_isr(hw, intr_sts);

	/* Tx interrupts*/
	crystalhd_link_tx_isr(hw, intr_sts);

	/* Clear interrupts */
	if (rc) {
		if (intr_sts)
			hw->pfnWriteFPGARegister(hw->adp, INTR_INTR_CLR_REG, intr_sts);

		hw->pfnWriteFPGARegister(hw->adp, INTR_EOI_CTRL, 1);
	}

	return rc;
}

// Dummy private function
void crystalhd_link_notify_fll_change(struct crystalhd_hw *hw, bool bCleanupContext)
{
	return;
}

bool crystalhd_link_notify_event(struct crystalhd_hw *hw, BRCM_EVENT EventCode)
{
	return true;
}
