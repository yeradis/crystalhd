/***************************************************************************
 * Copyright (c) 2005-2010, Broadcom Corporation.
 *
 *  Name: crystalhd_flea_ddr . h
 *
 *  Description:
 *		BCM70015 generic DDR routines
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

#undef  BRCM_ALIGN
#define BRCM_ALIGN(c,r,f)   0

#define MEM_SYS_NUM_DDR_PLLS 2;

/*extern uint32_t rts_prog_vals[][5]; */

enum eDDR2_SPEED_GRADE {
  DDR2_400MHZ = 0x0,
  DDR2_333MHZ = 0x1,
  DDR2_266MHZ = 0x2,
  DDR2_200MHZ = 0x3,
  DDR2_533MHZ = 0x4,
  DDR2_667MHZ = 0x5
};

enum eSD_COL_SIZE {
  COL_BITS_9  = 0x0,
  COL_BITS_10 = 0x1,
  COL_BITS_11 = 0x2,
};

enum eSD_BANK_SIZE {
  BANK_SIZE_4  = 0x0,
  BANK_SIZE_8  = 0x1,
};

enum eSD_ROW_SIZE {
  ROW_SIZE_8K  = 0x0,
  ROW_SIZE_16K = 0x1,
};

/*DDR PHY PLL init routine */
void crystalhd_flea_ddr_pll_config(struct crystalhd_hw* hw, int32_t *speed_grade, int32_t num_plls, uint32_t tmode);

/*DDR controller init routine */
void crystalhd_flea_ddr_ctrl_init(struct crystalhd_hw *hw,
						int32_t port,
                        int32_t ddr3,
                        int32_t speed_grade,
                        int32_t col,
                        int32_t bank,
                        int32_t row,
                        uint32_t tmode );

/*RTS Init routines */
void crystalhd_flea_ddr_arb_rts_init(struct crystalhd_hw *hw);
