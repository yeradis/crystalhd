/***************************************************************************
 * Copyright (c) 2005-2010, Broadcom Corporation.
 *
 *  Name: crystalhd_flea_ddr . c
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

#include <linux/delay.h>
#include "crystalhd_hw.h"
#include "crystalhd_flea_ddr.h"

/*#include "bchp_ddr23_ctl_regs_0.h" */
/*#include "bchp_ddr23_phy_byte_lane_0.h" */
/*#include "bchp_ddr23_phy_byte_lane_1.h" */
/*#include "bchp_ddr23_phy_control_regs.h" */
/*#include "bchp_pri_arb_control_regs.h" */
/*#include "bchp_pri_client_regs.h" */

/* RTS Programming Values for all Clients */
/*   column legend */
/*   [0]: 1=Program, 0=Default; */
/*   [1]: Blockout Count; */
/*   [2]: Critical Period; */
/*   [3]: Priority; */
/*   [4]: Access Mode */
/* Default mode for clients is best effort */

uint32_t rts_prog_vals[21][5] = {
  {1,    130,    130,      6,      1}, /*       Deblock ( 0) */
  {1,   1469,   1469,      9,      1}, /*         Cabac ( 1) */
  {1,    251,    251,      4,      1}, /*         Iloop ( 2) */
  {1,    842,    842,      5,      1}, /*         Oloop ( 3) */
  {1,   1512,   1512,     10,      1}, /*      Symb_Int ( 4) */
  {1,     43,     43,     14,      1}, /*         Mcomp ( 5) */
  {1,   1318,   1318,     11,      1}, /*         XPT_0 ( 6) */
  {1,   4320,   4320,     16,      1}, /*         XPT_1 ( 7) */
  {1,   5400,   5400,     17,      0}, /*         XPT_2 ( 8) */
  {1,   1080,   1080,     18,      1}, /*           ARM ( 9) */
  {1,    691,    691,      7,      0}, /*       MEM_DMA (10) */
  {1,   1382,   1382,     15,      0}, /*         SHARF (11) */
  {1,    346,    346,      2,      0}, /*           BVN (12) */
  {1,   1728,   1728,     13,      1}, /*        RxDMA3 (13) */
  {1,    864,    864,      8,      1}, /*         TxDMA (14) */
  {1,    173,    173,      3,      1}, /*       MetaDMA (15) */
  {1,   2160,   2160,     19,      1}, /*     DirectDMA (16) */
  {1,  10800,  10800,     20,      1}, /*           MSA (17) */
  {1,    216,    216,      1,      1}, /*         TRACE (18) */
  {1,   1598,   1598,     12,      0}, /*      refresh1 (19) */
  { 0, 0, 0, 0, 0}, /*(20) */
};

void crystalhd_flea_ddr_pll_config(struct crystalhd_hw* hw, int32_t *speed_grade, int32_t num_plls, uint32_t tmode)
{
	uint32_t PLL_NDIV_INT[2];
	uint32_t PLL_M1DIV[2];
	int32_t  i;
	uint32_t tmp;
	uint32_t config;
	uint32_t timeout;
	uint32_t skip_init[2];     /* completely skip initialization */
	/*uint32_t offset[2]; */
	uint32_t skip_pll_setup;
	uint32_t poll_cnt;

	skip_init[0] =  0;
	skip_init[1] =  0;

	/* If the test mode is not 0 then skip the PLL setups too. */
	if (tmode != 0){
		skip_pll_setup = 1;
	}
	else {
		skip_pll_setup = 0;
	}

	/* Use this scratch register in DDR0 - which should reset to 0 - as a simple symaphore for the test */
	/* to monitor if and when the Initialization of the DDR is complete */
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_SCRATCH, 0);

	if (!skip_pll_setup) {
		for(i=0;i<num_plls;i++) {
			switch(speed_grade[i])
			{
				case DDR2_333MHZ: PLL_NDIV_INT[i] = 49; PLL_M1DIV[i] = 2; break;
				case DDR2_400MHZ: PLL_NDIV_INT[i] = 59; PLL_M1DIV[i] = 2; break;
				case DDR2_533MHZ: PLL_NDIV_INT[i] = 39; PLL_M1DIV[i] = 1; break;
				case DDR2_667MHZ: PLL_NDIV_INT[i] = 49; PLL_M1DIV[i] = 1; break;
				default:
					printk("Undefined speed_grade of %d\n",speed_grade[i]);
					break;
			}
		}

		/*////////////////////////// */
		/*setup the PLLs */

		for(i=0;i<num_plls;i++) {
			if (skip_init[i]) continue;
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG,
					  (0 << 0) | /*PWRDWN */
					  (0 << 1) | /*REFCOMP_PWRDWN */
					  (1 << 2) | /*ARESET */
					  (1 << 3) | /*DRESET */
					  (0 << 4) | /*ENB_CLKOUT */
					  (0 << 5) | /*BYPEN ??? */
					  (0 << 6) | /*PWRDWN_CH1 */
					  (0 << 8) | /*DLY_CH1 */
					  (0 << 10)| /*VCO_RNG */
					  (1 << 31)  /*DIV2 CLK RESET */
					  );

			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_PRE_DIVIDER,
						(1 << 0) | /*P1DIV */
						(1 << 4) | /*P2DIV */
						(PLL_NDIV_INT[i] << 8) | /*NDIV_INT */
						(1 << 24) /*BYPASS_SDMOD */
						);

			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_DIVIDER,
						(PLL_M1DIV[i] << 24) /*M1DIV */
						);

			config = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG);
			config &= 0xfffffffb; /*clear ARESET */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG, config);
		}

		/*poll for lock */
		for(i=0;i<num_plls;i++){
			if (skip_init[i]) continue;
			timeout = 10;
			tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_STATUS);
			while((timeout>0) && ((tmp & 0x1) == 0)){
				msleep_interruptible(1);
				tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_STATUS);
				timeout--;
			}
			if (timeout<=0)
				printk("Timed out waiting for DDR Controller PLL %d to lock\n",i);
		}

		/*deassert PLL digital reset */
		for(i=0;i<num_plls;i++){
			if (skip_init[i]) continue;
			config = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG);
			config &= 0xfffffff7; /*clear DRESET */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG, config);
		}

		/*deassert reset of logic */
		for(i=0;i<num_plls;i++){
			if (skip_init[i]) continue;
			config = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG);
			config &= 0x7fffffff; /*clear logic reset */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_PLL_CONFIG, config);
		}
	} /* end (skip_pll_setup) */

	/*run VDL calibration for all byte lanes */
	for(i=0;i<num_plls;i++) {
		if (skip_init[i]) continue;
		tmp = 0;
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_CALIBRATE,tmp);
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_CALIBRATE,tmp);
		tmp = (
			(1 << 0) | /*calib_fast */
			(1 << 1)   /*calib_once */
			);
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_CALIBRATE,tmp);
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_CALIBRATE,tmp);


		if (!skip_pll_setup){  /*VDLs might not lock if clocks are bypassed */
			timeout=100;
			tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_STATUS);
			while((timeout>0) && ((tmp & 0x3) == 0x0)){
				msleep_interruptible(1);
				timeout--;
				tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_STATUS);
			}
			if ((tmp & 0x3) != 0x3)
				printk("VDL calibration did not finish or did not lock!\n");
			timeout=100;
			tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_STATUS);
			while((timeout>0) && ((tmp & 0x3) == 0x0)){
				msleep_interruptible(1);
				timeout--;
				tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_STATUS);
			}
			if ((tmp & 0x3) != 0x3)
				printk("VDL calibration did not finish or did not lock!\n");

			if(timeout<=0){
				printk("DDR PHY %d VDL Calibration failed\n",i);
			}
		}
		else {
			msleep_interruptible(1);
		}

		/*clear VDL calib settings */
		tmp = 0;
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_CALIBRATE,tmp);
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_CALIBRATE,tmp);

		/*override the ADDR/CTRL VDLs with results from Bytelane #0 */
		/*if tmode other than zero then set the VDL compensations to max values of 0x1ff. */
		tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_STATUS);
		tmp = (tmp >> 4) & 0x3ff;
		/* If in other than tmode 0 then set the VDL override settings to max. */
		if (tmode) {
			tmp = 0x3ff;
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_OVERRIDE_0, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_OVERRIDE_1, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_OVERRIDE_2, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_VDL_OVERRIDE_3, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_OVERRIDE_0, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_OVERRIDE_1, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_OVERRIDE_2, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_VDL_OVERRIDE_3, 0x1003f);
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_UPDATE_VDL, BCHP_DDR23_CTL_REGS_0_UPDATE_VDL_refresh_MASK);
		}
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_STATIC_VDL_OVERRIDE,
			(((tmp & 0x3f0) >> 4) << 0) |  /* step override value */
			(1 << 16)  |  /* override enable */
			(1 << 20)     /* override force ; no update vdl required */
			);

		/* NAREN added support for ZQ Calibration */
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL, 0);
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL_sample_en_MASK);
		tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL);

		poll_cnt = 0;
		while(1)
		{
			if(!(tmp & BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL_sample_done_MASK))
				tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL);
			else
				break;

			if(poll_cnt++ > 100)
				break;
		}

		if(tmode) {
			/* Set fields addr_ovr_en and dq_pvr_en to '1'.  Set all *_override_val fields to 0xf - ZQ_PVT_COMP_CTL */
			tmp = ( (  1 << 25) |     /*  addr_ovr_en */
					(  1 << 24) |     /*  dq_ovr_en */
					(0xf << 12) |     /* addr_pd_override_val */
					(0xf << 8)  |     /* addr_nd_override_val */
					(0xf << 4)  |     /* dq_pd_override_val */
					(0xf << 0)  );    /* dq_nd_override_val */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL, tmp);
			/* Drive_PAD_CTL register.  Set field selrxdrv and slew to 0; */
			tmp = hw->pfnReadDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_DRIVE_PAD_CTL);
			tmp &= (0xfffffffe);    /*clear bits 0 and 1. */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_DRIVE_PAD_CTL,tmp);
		}
	}/*for(i=0.. */
}

void crystalhd_flea_ddr_ctrl_init(struct crystalhd_hw *hw,
								int32_t port,
								int32_t ddr3,
								int32_t speed_grade,
								int32_t col,
								int32_t bank,
								int32_t row,
								uint32_t tmode)
{
	/*uint32_t offset; */
	/*uint32_t arb_refresh_addr; */
	uint32_t port_int;

	uint32_t data;

	/*DDR2 Parameters */
	uint8_t tRCD = 0;
	uint8_t tRP = 0;
	uint8_t tRRD = 0;
	uint8_t tWR = 0;
	uint8_t tWTR = 0;
	uint8_t tCAS = 0;
	uint8_t tWL = 0;
	uint8_t tRTP = 0;
	uint8_t tRAS = 0;
	uint8_t tFAW = 0;
	uint8_t tRFC = 0;
	uint8_t INTLV_BYTES = 0;
	uint8_t INTLV_DISABLE = 0;
	uint8_t CTRL_BITS = 0;
	uint8_t ALLOW_PICTMEM_RD = 0;
	uint8_t DIS_DQS_ODT = 0;
	uint8_t CS0_ONLY = 0;
	uint8_t EN_ODT_EARLY = 0;
	uint8_t EN_ODT_LATE = 0;
	uint8_t USE_CHR_HGT = 0;
	uint8_t DIS_ODT = 0;
	uint8_t EN_2T_TIMING = 0;
	uint8_t CWL = 0;
	uint8_t DQ_WIDTH = 0;

	uint8_t DM_IDLE_MODE = 0;
	uint8_t CTL_IDLE_MODE = 0;
	uint8_t DQ_IDLE_MODE = 0;

	uint8_t DIS_LATENCY_CTRL = 0;

	uint8_t PSPLIT = 0;
	uint8_t DSPLIT = 0;

	/* For each controller port, 0 and 1. */
	for (port_int=0; port_int < 1; ++port_int)  {
#if 0
		printk("******************************************************\n");
		printk("* Configuring DDR23 at addr=0x%x, speed grade [%s]\n",0,
                ((speed_grade == DDR2_667MHZ) && (tmode == 0)) ? "667MHZ":
                ((speed_grade == DDR2_533MHZ) && (tmode == 0)) ? "533MHZ":
                ((speed_grade == DDR2_400MHZ) && (tmode == 0)) ? "400MHZ":
                ((speed_grade == DDR2_333MHZ) && (tmode == 0)) ? "333MHZ":
                ((speed_grade == DDR2_266MHZ) && (tmode == 0)) ? "266MHZ": "400MHZ" );
#endif
		/* Written in this manner to prevent table lookup in Memory for embedded MIPS code. */
		/* Cannot use memory until it is inited!  Case statements with greater than 5 cases use memory tables */
		/* when optimized.  Tony O 9/18/07 */
		/* Note if not in test mode 0, choose the slowest clock speed. */
		if (speed_grade == DDR2_200MHZ) {
			tRCD = 3;
			tRP = 3;
			tRRD = 2;
			tWR = 3;
			tWTR = 2;
			tCAS = 4;
			tWL = 3;
			tRTP = 2;
			tRAS = 8;
			tFAW = 10;
			if (bank == BANK_SIZE_4)
				tRFC = 21;
			else /*BANK_SIZE_8 */
				tRFC = 26;
		}
		else if (speed_grade == DDR2_266MHZ ) {
			tRCD = 4;
			tRP = 4;
			tRRD = 3;
			tWR = 4;
			tWTR = 2;
			tCAS = 4;
			tWL = 3;
			tRTP = 2;
			tRAS = 11;
			tFAW = 14;
			if (bank == BANK_SIZE_4)
				tRFC = 28;
			else /*BANK_SIZE_8 */
				tRFC = 34;
		}
		else if (speed_grade == DDR2_333MHZ) {
			tRCD = 4;
			tRP = 4;
			tRRD = 4;
			tWR = 5;
			tWTR = 3;
			tCAS = 4;
			tWL = 3;
			tRTP = 3;
			tRAS = 14;
			tFAW = 17;
			if (bank == BANK_SIZE_4)
				tRFC = 35;
			else /*BANK_SIZE_8 */
				tRFC = 43;
		}
		else if ((speed_grade == DDR2_400MHZ) || (tmode != 0)) { /* -25E timing */
			tRCD = 6;
			tRP = 6;
			tRRD = 4;
			tWR = 6;
			tWTR = 4;
			tCAS = ddr3 ? 6 : 5;
			tWL = ddr3 ? 5 : 4;
			tRTP = 3;
			tRAS = 18;
			tFAW = 20;
			if (bank == BANK_SIZE_4)
				tRFC = 42;
			else /*BANK_SIZE_8 */
				tRFC = 52;
			CWL = tWL - 5;
		}
		else if (speed_grade == DDR2_533MHZ) { /* -187E timing */
			tRCD = 7;
			tRP = 7;
			tRRD =  6;
			tWR = 8;
			tWTR = 4;
			tCAS = 7;
			tWL = tCAS - 1;
			tRTP = 4;
			tRAS = 22;
			tFAW = 24;
			tRFC = 68;
			CWL = tWL - 5;
		}
		else if (speed_grade == DDR2_667MHZ) { /* -15E  timing */
			tRCD = 9;
			tRP = 9;
			tRRD =  5;/* 4/5 */
			tWR = 10;
			tWTR = 5;
			tCAS = 9;
			tWL = 7;
			tRTP = 5;
			tRAS = 24;
			tFAW = 30; /* 20/30 */
			tRFC = 74;
			CWL = tWL - 5;
		}
		else
			printk("init: CANNOT HAPPEN - Memory DDR23 Ctrl_init failure. Incorrect speed grade type [%d]\n", speed_grade);

		CTRL_BITS = 0; /* Control Bit for CKE signal */
		EN_2T_TIMING = 0;
		INTLV_DISABLE = ddr3 ? 1:0;  /* disable for DDR3, enable for DDR2 */
		INTLV_BYTES = 0;
		ALLOW_PICTMEM_RD = 0;
		DIS_DQS_ODT = 0;
		CS0_ONLY = 0;
		EN_ODT_EARLY = 0;
		EN_ODT_LATE = 0;
		USE_CHR_HGT = 0;
		DIS_ODT = 0;

		/*Power Saving Controls */
		DM_IDLE_MODE = 0;
		CTL_IDLE_MODE = 0;
		DQ_IDLE_MODE = 0;

		/*Latency Control Setting */
		DIS_LATENCY_CTRL = 0;

		/* ****** Start of Grain/Flea specific fixed settings ***** */
		CS0_ONLY = 1 ;      /* 16-bit mode only */
		INTLV_DISABLE = 1 ; /* Interleave is always disabled */
		DQ_WIDTH = 16 ;
		/* ****** End of Grain specific fixed settings ***** */

#if 0
		printk("* DDR23 Config: CAS: %d, tRFC: %d, INTLV: %d, WIDTH: %d\n",
				tCAS,tRFC,INTLV_BYTES,DQ_WIDTH);
		printk("******************************************************\n");
#endif
		/*Disable refresh */
		data = ((0x68 << 0) |  /*Refresh period */
				(0x0 << 12)    /*disable refresh */
				);

		hw->pfnWriteDevRegister(hw->adp, BCHP_PRI_ARB_CONTROL_REGS_REFRESH_CTL_0, data);

		/* DecSd_Ddr2Param1 */
		data = 0;
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, trcd, tRCD); /* trcd */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, trp, tRP); /* trp */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, trrd, tRRD);  /* trrd */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, twr, tWR);  /* twr */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, twtr, tWTR);  /* twtr */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, tcas, tCAS);  /* tcas */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, twl, tWL); /* twl */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS1, trtp, tRTP); /* trtp */

		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PARAMS1,  data );

		/*DecSd_Ddr2Param3 - deassert reset only */
		data = 0;
		/*DEBUG_PRINT(PARAMS3, data); */
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PARAMS3,  data );

		/* Reset must be deasserted 500us before CKE. This needs */
		/* to be reflected in the CFE. (add delay here) */

		/*DecSd_Ddr2Param2 */
		data = 0;
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, tras, tRAS);  /* tras */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, tfaw, tFAW); /* tfaw */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, trfc, tRFC); /* trfc */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, bank_bits, bank & 1); /*  0 = bank size of 4K == 2bits, 1 = bank size of 8k == 3 bits */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, allow_pictmem_rd, ALLOW_PICTMEM_RD);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, cs0_only, CS0_ONLY);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, dis_itlv, INTLV_DISABLE); /* #disable interleave */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, il_sel, INTLV_BYTES); /* #bytes per interleave */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, sd_col_bits, col & 3); /* column bits, 0 = 9, 1= 10, 2 or 3 = 11 bits */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, clke, CTRL_BITS); /* Control Bit for CKE signal */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, use_chr_hgt, USE_CHR_HGT);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS2, row_bits, row & 1); /* row size 1 is 16K for 2GB device, otherwise 0 and 8k sized */

		/*DEBUG_PRINT(PARAMS2, data); */
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PARAMS2,  data );

		/*DecSd_Ddr2Param3. */
		data = 0;
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, wr_odt_en, DIS_ODT ? 0 : 1);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, wr_odt_le_adj, EN_ODT_EARLY ? 1 : 0);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, wr_odt_te_adj, EN_ODT_LATE ? 1 : 0);
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, cmd_2t, EN_2T_TIMING ? 1: 0);  /* 2T timing is disabled */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, ddr_bl, ddr3 ? 1: 0);  /* 0 for DDR2, 1 for DDR3 */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, wr_odt_mode, ddr3 ? 1:0);  /* ddr3 preamble */
		SET_FIELD(data, BCHP_DDR23_CTL_REGS_0, PARAMS3, ddr3_reset, ddr3 ? 0:1);  /* ddr3 reset */

		/*DEBUG_PRINT(PARAMS3, data); */
		hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PARAMS3,  data );
	} /* for( port_int......) */

	data = 0;
	SET_FIELD(data, BCHP_DDR23_PHY_CONTROL_REGS, DRIVE_PAD_CTL, slew, 1);
	SET_FIELD(data, BCHP_DDR23_PHY_CONTROL_REGS, DRIVE_PAD_CTL, seltxdrv_ci, 1);
	SET_FIELD(data, BCHP_DDR23_PHY_CONTROL_REGS, DRIVE_PAD_CTL, sel_sstl18, ddr3 ? 0 : 1);
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_CONTROL_REGS_DRIVE_PAD_CTL, data );

	data = 0;
	SET_FIELD(data, BCHP_DDR23_PHY_BYTE_LANE_0, DRIVE_PAD_CTL, slew, 0);
	SET_FIELD(data, BCHP_DDR23_PHY_BYTE_LANE_0, DRIVE_PAD_CTL, selrxdrv, 0);
	SET_FIELD(data, BCHP_DDR23_PHY_BYTE_LANE_0, DRIVE_PAD_CTL, seltxdrv_ci, 0);
	SET_FIELD(data, BCHP_DDR23_PHY_BYTE_LANE_0, DRIVE_PAD_CTL, sel_sstl18, ddr3 ? 0 : 1);
	SET_FIELD(data, BCHP_DDR23_PHY_BYTE_LANE_0, DRIVE_PAD_CTL, rt60b, 0);
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_DRIVE_PAD_CTL, data );
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_DRIVE_PAD_CTL, data );

	data = 0;

	if (speed_grade == DDR2_667MHZ) {
		data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_MASK) | ((2 << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_MASK));
	} else {
		data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_MASK) | ((1 << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_data_dly_MASK));
	}

	data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_enable_MASK) | ((((DIS_DQS_ODT || DIS_ODT) ? 0:1) << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_enable_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_enable_MASK));
	data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_adj_MASK) | (((EN_ODT_EARLY ? 1 : 0) << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_adj_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_dq_odt_adj_MASK));
	data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_enable_MASK) | (((DIS_ODT ? 0:1) << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_enable_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_enable_MASK));
	data = ((data & ~BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_adj_MASK) | (((EN_ODT_EARLY ? 1 : 0) << BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_adj_SHIFT) & BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL_rd_enb_odt_adj_MASK));

	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_READ_CONTROL, data);
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_READ_CONTROL, data);

	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_0_WR_PREAMBLE_MODE, ddr3 ? 1 : 0);
	hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_PHY_BYTE_LANE_1_WR_PREAMBLE_MODE, ddr3 ? 1 : 0);

	/* Disable unused clocks */

	for (port_int=0; port_int<1; ++port_int) { /* For Grain */
		/* Changes for Grain/Flea */
		/*offset = 0; */
		/*arb_refresh_addr = BCHP_PRI_ARB_CONTROL_REGS_REFRESH_CTL_0; */
		/*offset += GLOBAL_REG_RBUS_START; */
		/* Changes for Grain - till here */

		if (ddr3) {
			data = (CWL & 0x07) << 3;
			/*DEBUG_PRINT(LOAD_EMODE2_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE2_CMD,  data );

			data = 0;
			/*DEBUG_PRINT(LOAD_EMODE3_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE3_CMD,  data );

			data = 6;  /* was 4; */
			/*DEBUG_PRINT(LOAD_EMODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE_CMD,  data );

			data = 0x1100;   /* Reset DLL */
			data += ((tWR-4) << 9);
			data += ((tCAS-4) << 4);
			/*DEBUG_PRINT(LOAD_MODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_MODE_CMD,  data );


			data = 0x0400;  /* long calibration */
			/*DEBUG_PRINT(ZQ_CALIBRATE, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_ZQ_CALIBRATE,  data );

			msleep_interruptible(1);
		}
		else
		{
			/*DecSd_RegSdPrechCmd    // Precharge */
			data = 0;
			/*DEBUG_PRINT(PRECHARGE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PRECHARGE_CMD,  data );

			/*DEBUG_PRINT(PRECHARGE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PRECHARGE_CMD,  data );

			/*DecSd_RegSdLdModeCmd   //Clear EMODE 2,3 */
			data = 0;
			/*DEBUG_PRINT(LOAD_EMODE2_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE2_CMD,  data );

			/*DEBUG_PRINT(LOAD_EMODE3_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE3_CMD,  data );

			/*DecSd_RegSdLdEmodeCmd  // Enable DLL ; Rtt ; enable OCD */
			data = 0x3C0;
			/*DEBUG_PRINT(LOAD_EMODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE_CMD,  data );

			/*DecSd_RegSdLdModeCmd */
			data = 0x102;   /* Reset DLL */
			data += ((tWR-1) << 9);
			data += (tCAS << 4);
			/*DEBUG_PRINT(LOAD_MODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_MODE_CMD,  data );

			/*DecSd_RegSdPrechCmd    // Precharge */
			data = 0;
			/*DEBUG_PRINT(PRECHARGE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PRECHARGE_CMD,  data );

			/*DEBUG_PRINT(PRECHARGE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_PRECHARGE_CMD,  data );

			/*DecSd_RegSdRefCmd      // Refresh */
			data = 0x69;
			/*DEBUG_PRINT(REFRESH_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_REFRESH_CMD,  data );

			/*DEBUG_PRINT(REFRESH_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_REFRESH_CMD,  data );

			/*DecSd_RegSdLdModeCmd */
			data = 0x002;   /* Un-Reset DLL */
			data += ((tWR-1) << 9);
			data += (tCAS << 4);
			/*DEBUG_PRINT(LOAD_MODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_MODE_CMD,  data );

			/*DecSd_RegSdLdEmodeCmd  // Enable DLL ; Rtt ; enable OCD */
			data = 0x3C0;
			/*DEBUG_PRINT(LOAD_EMODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE_CMD,  data );

			/*DecSd_RegSdLdEmodeCmd  // Enable DLL ; Rtt ; disable OCD */
			data = 0x40;
			/*DEBUG_PRINT(LOAD_EMODE_CMD, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LOAD_EMODE_CMD,  data );
		}

		/*Enable refresh */
		data = ((0x68 << 0) |  /*Refresh period */
				(0x1 << 12)    /*enable refresh */
				);
		if (tmode == 0) {
			/*MemSysRegWr(arb_refresh_addr + GLOBAL_REG_RBUS_START,data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_PRI_ARB_CONTROL_REGS_REFRESH_CTL_0,data);
		}

		/*offset = 0; */
		/*offset += GLOBAL_REG_RBUS_START; */

		/* Use this scratch register in DDR0 as a simple symaphore for the test */
		/* to monitor if and when the Initialization of the DDR is complete. Seeing a non zero value */
		/* indicates DDR init complete.  This code is ONLY for the MIPS. It has no affect in init.c */
		/* The MIPS executes this code and we wait until DDR 1 is inited before setting the semaphore. */
		if ( port_int == 1)
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_SCRATCH, 0xff);

		/*Setup the Arbiter Data and Pict Buffer split if specified */
		if (port_int==0) { /*only need to do this once */
			/*where is the pict buff split (2 bits) */
			/*0 = always mem_a, 1 = (<128 is mem_a), 2 = (<64 is mem_a), 3 = always mem_b */
			PSPLIT = 0;

			/*0 = 32MB, 1 = 64MB, 2 = 128 MB, 3 = 256MB, 4=512MB */
			DSPLIT = 4;

			data = 0;
			data += DSPLIT;
			data += PSPLIT<< 4;
			/* MemSysRegWr (PRI_ARB_CONTROL_REGS_CONC_CTL + offset,  data ); */
		}

		if (DIS_LATENCY_CTRL == 1){
			/*set the work limit to the maximum */
			/*DEBUG_PRINT(LATENCY, data); */
			hw->pfnWriteDevRegister(hw->adp, BCHP_DDR23_CTL_REGS_0_LATENCY,  0x3ff );
		}
	} /* for (port_int=0......  ) */

 return;
}

void crystalhd_flea_ddr_arb_rts_init(struct crystalhd_hw *hw)
{
	uint32_t addr_cnt;
	uint32_t addr_ctrl;
	uint32_t i;

	addr_cnt = BCHP_PRI_CLIENT_REGS_CLIENT_00_COUNT;
	addr_ctrl = BCHP_PRI_CLIENT_REGS_CLIENT_00_CONTROL;

	/*Go through the various clients and program them */
	for(i=0;i<21;i++){
		if (rts_prog_vals[i][0] > 0) {
			hw->pfnWriteDevRegister(hw->adp, addr_cnt,
				(rts_prog_vals[i][1]) |        /*Blockout Count */
				(rts_prog_vals[i][2] << 16)    /*Critical Period */
				);
			hw->pfnWriteDevRegister(hw->adp, addr_ctrl,
				(rts_prog_vals[i][3]) |        /*Priority Level */
				(rts_prog_vals[i][4] << 8)     /*Access Mode */
				);
		}
		addr_cnt+=8;
		addr_ctrl+=8;
	}
}
