/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_if.cpp
 *
 *  Description: Driver Interface API.
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

/* Code-In fifo */
/* TODO: this comes from crystalhd_hw.h (driver include) */
#define REG_DecCA_RegCinCTL	0xa00
#define REG_DecCA_RegCinBase	0xa0c
#define REG_DecCA_RegCinEnd	0xa10
#define REG_DecCA_RegCinWrPtr	0xa04
#define REG_DecCA_RegCinRdPtr	0xa08
#define REG_Dec_TsUser0Base	0x100864
#define REG_Dec_TsUser0Rdptr	0x100868
#define REG_Dec_TsUser0Wrptr	0x10086C
#define REG_Dec_TsUser0End	0x100874
#define REG_Dec_TsAudCDB2Base	0x10036c
#define REG_Dec_TsAudCDB2Rdptr	0x100378
#define REG_Dec_TsAudCDB2Wrptr	0x100374
#define REG_Dec_TsAudCDB2End	0x100370

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "libcrystalhd_if.h"
#include "libcrystalhd_int_if.h"
#include "version_lnx.h"
#include "libcrystalhd_fwcmds.h"
#include "libcrystalhd_priv.h"

#if (!__STDC_WANT_SECURE_LIB__)
inline bool memcpy_s(void *dest, size_t sizeInBytes, void *src, size_t count)
{
	bool status = false;
	if (count > sizeInBytes) {
		DebugLog_Trace(LDIL_DBG,"memcpy_s: buffer overflow\n");
	} else {
		memcpy(dest, src, count);
		status = true;
	}

	return(status);
}

inline bool memmove_s(void *dest, size_t sizeInBytes, void *src, size_t count)
{
	bool status = false;
	if (count > sizeInBytes) {
		DebugLog_Trace(LDIL_DBG,"memmove_s: buffer overflow\n");
	} else {
		memmove(dest, src, count);
		status = true;
	}

	return(status);
}
#endif

// Full TS packet of EOS
static __attribute__((aligned(4))) uint8_t eos_mpeg[184] =
{
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7,
	0x00, 0x00, 0x01, 0xb7
};

static __attribute__((aligned(4))) uint8_t eos_avc_vc1[184] =
{
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a,
	0x00, 0x00, 0x01, 0x0a
};

static __attribute__((aligned(4))) uint8_t eos_vc1_spmp_link[32] =
{
	0x5a, 0x5a, 0x5a, 0x5a,
	0x00, 0x00, 0x00, 0x20,
	0x00, 0x00, 0x00, 0x05,
	0x5a, 0x5a, 0x5a, 0x5a,
	0x0d, 0x00, 0x00, 0x01,
	0x0a, 0xe0, 0x55, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};


static __attribute__((aligned(4))) uint8_t eos_divx[184] =
{
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1,
	0x00, 0x00, 0x01, 0xb1
};

static __attribute__((aligned(4))) uint8_t btp_video_done_es_private[] =
{
	/* 0x81, 0x01, 0x14, 0x80,*/
	0x42, 0x52, 0x43, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	/*, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,*/
};

static __attribute__((aligned(4))) uint8_t btp_video_done_es[] =
{
					    /*0x81, 0x01, 0x14, 0x80, 0x42, 0x52, 0x43, 0x4D, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,*/ 0x00, 0x00, 0x00,
	0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xBC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

#if 0
static __attribute__((aligned(4))) uint8_t btp_video_plunge_es[] =
{
					    /*0x81, 0x01, 0x14, 0x80, 0x42, 0x52, 0x43, 0x4D, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,*/ 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xBC, 0xAA, 0xBB, 0xCC, 0xDD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
#endif

static BC_STATUS DtsSetupHardware(HANDLE hDevice, BOOL IgnClkChk)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx;

	DTS_GET_CTX(hDevice,Ctx);

	if(!IgnClkChk)
	{
		if(Ctx->DevId == BC_PCI_DEVID_LINK)
		{
			if((DtsGetOPMode() & 0x08) && 
			   (DtsGetHwInitSts() != BC_DIL_HWINIT_IN_PROGRESS))
			{
				return BC_STS_SUCCESS;
			}
		}
		
	}
	 
	if(Ctx->DevId == BC_PCI_DEVID_LINK)
	{
		DebugLog_Trace(LDIL_ERR,"DtsSetupHardware: DtsPushAuthFwToLink\n");
		sts = DtsPushAuthFwToLink(hDevice,NULL);
	}

	if(sts != BC_STS_SUCCESS){
		return sts;
	}
	
	/* Initialize Firmware interface */
	sts = DtsFWInitialize(hDevice,0);
	if(sts != BC_STS_SUCCESS ){
		return sts;
	}

	return sts;
}


static BC_STATUS DtsReleaseChannel(HANDLE  hDevice,uint32_t ChannelID, BOOL Stop)
{
	BC_STATUS	sts = BC_STS_SUCCESS;


	if(Stop){
		sts = DtsFWStopVideo(hDevice, ChannelID, TRUE);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsReleaseChannel: StopVideoFailed Ignoring error\n");
		}
	}
	sts = DtsFWCloseChannel(hDevice, ChannelID);
	if(sts != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsReleaseChannel: DtsFWCloseChannel Failed\n");
	}

	return sts;
}

static BC_STATUS DtsRecoverableDecOpen(HANDLE  hDevice,uint32_t StreamType)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	uint32_t			retry = 3;

	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);
	
	while(retry--){

		sts = DtsFWOpenChannel(hDevice, StreamType, 0);
		if(sts == BC_STS_SUCCESS){
			if(Ctx->OpenRsp.channelId == 0){
				break;
			}else{
				DebugLog_Trace(LDIL_DBG,"DtsFWOpenChannel: ChannelID leakage..\n");
				/* First Release the Current Channel.*/
				DtsReleaseChannel(hDevice,Ctx->OpenRsp.channelId, FALSE);
				/* Fall through to release the previous Channel */
				sts = BC_STS_FW_CMD_ERR;
			}
		}
	
		if((sts == BC_STS_TIMEOUT) || (retry == 1) ){
			/* Do Full initialization */
			sts = DtsSetupHardware(hDevice,TRUE);
			if(sts != BC_STS_SUCCESS)
				break;
			/* Setup The Clock Again */
			sts = DtsSetVideoClock(hDevice,0);
			if(sts != BC_STS_SUCCESS )
				break;

		}else{
			sts = DtsReleaseChannel(hDevice,0,TRUE);
			if(sts != BC_STS_SUCCESS)
				break;
		}
	}

	return sts;

}

//===================================Externs ============================================
#ifdef _USE_SHMEM_
extern BOOL glob_mode_valid;
#endif

DRVIFLIB_API BC_STATUS 
DtsDeviceOpen(
    HANDLE	*hDevice,
	uint32_t		mode
    )
{
	int 		drvHandle=1;
	BC_STATUS	Sts=BC_STS_SUCCESS;
	uint32_t globMode = 0, cnt = 100;
	uint8_t	nTry=1;
	uint32_t		VendorID, DeviceID, RevID, FixFlags, drvMode;
	uint32_t		drvVer, dilVer;
	uint32_t		fwVer, decVer, hwVer;
#ifdef _USE_SHMEM_
	int shmid=0;
#endif
	
	DebugLog_Trace(LDIL_DBG,"Running DIL (%d.%d.%d) Version\n",
		DIL_MAJOR_VERSION,DIL_MINOR_VERSION,DIL_REVISION );

	FixFlags = mode;
	mode &= 0xFF;

	/* For External API case, we support only Plyaback mode. */
	if( !(BC_DTS_DEF_CFG & BC_EN_DIAG_MODE) && (mode != DTS_PLAYBACK_MODE) ){
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: mode %d not supported\n",mode);
		return BC_STS_INV_ARG;
	}
#ifdef _USE_SHMEM_
	Sts = DtsCreateShMem(&shmid);	
	if(BC_STS_SUCCESS !=Sts)
		return Sts;	


/*	Sts = DtsGetDilShMem(shmid);
	if(BC_STS_SUCCESS !=Sts)
		return Sts;*/

	if(!glob_mode_valid) {
		globMode = DtsGetOPMode();
		if(globMode&4) {
			globMode&=4;
		}
		DebugLog_Trace(LDIL_DBG,"DtsDeviceOpen:New  globmode is %d \n",globMode);
	}
	else{
		globMode = DtsGetOPMode();
	}
#else
	globMode = DtsGetOPMode();
#endif


	if(((globMode & 0x3) && (mode != DTS_MONITOR_MODE)) ||
	   ((globMode & 0x4) && (mode == DTS_MONITOR_MODE)) ||
	   ((globMode & 0x8) && (mode == DTS_HWINIT_MODE))){
		DebugLog_Trace(LDIL_DBG,"DtsDeviceOpen: mode %d already opened\n",mode);
#ifdef _USE_SHMEM_
		DtsDelDilShMem();
#endif
		return BC_STS_DEC_EXIST_OPEN;
	}

	if(mode == DTS_PLAYBACK_MODE){
		while(cnt--){
			if(DtsGetHwInitSts() != BC_DIL_HWINIT_IN_PROGRESS)
				break;
			bc_sleep_ms(100);
		}
		if(!cnt){
			return BC_STS_TIMEOUT;
		}
	}else if(mode == DTS_HWINIT_MODE){
		DtsSetHwInitSts(BC_DIL_HWINIT_IN_PROGRESS);
	}

	drvHandle =open(CRYSTALHD_API_DEV_NAME, O_RDWR);
	if(drvHandle < 0)
	{
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: Create File Failed\n");
		return BC_STS_ERROR;
	}


	/* Initialize Internal Driver interfaces.. */
	if( (Sts = DtsInitInterface(drvHandle,hDevice, mode)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: Interface Init Failed:%x\n",Sts);
		return Sts;
	}
	
	/* 
	 * We have to specify the mode to the driver.
	 * So the driver can cleanup only in case of 
	 * playback/Diag mode application close.
	 */
	if ((Sts = DtsGetVersion(*hDevice, &drvVer, &dilVer)) != BC_STS_SUCCESS) {
		DebugLog_Trace(LDIL_DBG,"Get drv ver failed\n");
		return Sts;
	}

	/* If driver minor version is more than 13, enable DTS_SKIP_TX_CHK_CPB feature */
	if (FixFlags & DTS_SKIP_TX_CHK_CPB) {
		if (((drvVer >> 16) & 0xFF) > 13) 
			FixFlags |= DTS_SKIP_TX_CHK_CPB;
	}

	if (FixFlags & DTS_ADAPTIVE_OUTPUT_PER) {

		if (!DtsGetFWVersion(*hDevice, &fwVer, &decVer, &hwVer, (char*)FWBINFILE_LNK, 0)) {
			if (fwVer >= ((14 << 16) | (8 << 8) | (1)))		// 2.14.8.1 (ignore 2)
				FixFlags |= DTS_ADAPTIVE_OUTPUT_PER;
			else
				FixFlags &= (~DTS_ADAPTIVE_OUTPUT_PER);
		}
	}

	/* If driver minor version is newer, send the fullMode */
	if (((drvVer >> 16) & 0xFF) > 10) drvMode = FixFlags; else drvMode = mode;

	if( (Sts = DtsNotifyOperatingMode(*hDevice,drvMode)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"Notify Operating Mode Failed\n");
		return Sts;
	}

	if( (Sts = DtsGetHwType(*hDevice,&DeviceID,&VendorID,&RevID))!=BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"Get Hardware Type Failed\n");
		return Sts;
	}

	/* Setup Hardware Specific Configuration */
	DtsSetupConfig(DtsGetContext(*hDevice), DeviceID, RevID, FixFlags);

	/* Enable single threaded mode in the context */
	if (FixFlags & DTS_SINGLE_THREADED_MODE) {
		DebugLog_Trace(LDIL_DBG,"Enable single threaded mode\n");
		DtsGetContext(*hDevice)->SingleThreadedAppMode = 1;
	}

	if(mode == DTS_PLAYBACK_MODE){
		globMode |= 0x1;
	} else if(mode == DTS_DIAG_MODE){
		globMode |= 0x2;
	} else if(mode == DTS_MONITOR_MODE) {
		globMode |= 0x4;
	} else if(mode == DTS_HWINIT_MODE) {
		globMode |= 0x8;
	} else {
		globMode = 0;
	}

	DtsSetOPMode(globMode);
 
	if((mode == DTS_PLAYBACK_MODE)||(mode == DTS_HWINIT_MODE))
	{	
		while(nTry--)
		{
			Sts = 	DtsSetupHardware(*hDevice, FALSE);	
			if(Sts == BC_STS_SUCCESS)
			{	
				DebugLog_Trace(LDIL_DBG,"DtsSetupHardware: Success\n");
				break;
			}
			else 
			{	
				DebugLog_Trace(LDIL_DBG,"DtsSetupHardware: Failed\n");
				bc_sleep_ms(100);				
			}
		}
		if(Sts != BC_STS_SUCCESS )
		{
			DtsReleaseInterface(DtsGetContext(*hDevice));
		}		
	}
	
	if(mode == DTS_HWINIT_MODE){
		DtsSetHwInitSts(0);
	}

	return Sts;
}

DRVIFLIB_API BC_STATUS 
DtsDeviceClose(
    HANDLE hDevice
    )
{
	DTS_LIB_CONTEXT		*Ctx;
	uint32_t globMode = 0;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->State != BC_DEC_STATE_INVALID){
		DebugLog_Trace(LDIL_DBG,"DtsDeviceClose: closing decoder ....\n");
		DtsCloseDecoder(hDevice);

	}

	DtsCancelFetchOutInt(Ctx);
	/* Unmask the mode */
	globMode = DtsGetOPMode( );
	if(Ctx->OpMode == DTS_PLAYBACK_MODE){
		globMode &= (~0x1);
	} else if(Ctx->OpMode == DTS_DIAG_MODE){
		globMode &= (~0x2);
	} else if(Ctx->OpMode == DTS_MONITOR_MODE) {
		globMode &= (~0x4);
	} else if(Ctx->OpMode == DTS_HWINIT_MODE) {
		globMode &= (~0x8);
	} else {
		globMode = 0;
	}
	DtsSetOPMode(globMode);
	


	return DtsReleaseInterface(Ctx);
	
}

DRVIFLIB_API BC_STATUS 
DtsGetVersion(
    HANDLE  hDevice,
    uint32_t     *DrVer,
    uint32_t     *DilVer
	)
{
	BC_VERSION_INFO *pVerInfo;
	BC_IOCTL_DATA *pIocData = NULL;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_STATUS	sts = BC_STS_SUCCESS;


	DTS_GET_CTX(hDevice,Ctx);
	
	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	pVerInfo = 	&pIocData->u.VerInfo;

	if( (sts=DtsDrvCmd(Ctx,BCM_IOC_GET_VERSION,0,pIocData,FALSE)) != BC_STS_SUCCESS){
		DtsRelIoctlData(Ctx,pIocData);
		DebugLog_Trace(LDIL_DBG,"DtsGetVersion: Ioctl failed: %d\n",sts);
		return sts;
	}

	*DrVer  = (pVerInfo->DriverMajor << 24) | (pVerInfo->DriverMinor<<16) | pVerInfo->DriverRevision;
	*DilVer = (DIL_MAJOR_VERSION <<24)| (DIL_MINOR_VERSION<<16) | DIL_REVISION;

	DtsRelIoctlData(Ctx,pIocData);

	return BC_STS_SUCCESS;
}

#define MAX_BIN_FILE_SZ 	0x300000

DRVIFLIB_API BC_STATUS 
DtsGetFWVersionFromFile(
    HANDLE  hDevice,
    uint32_t     *StreamVer,
	uint32_t		*DecVer,
	char	*fname
	)
{

	
	BC_STATUS sts = BC_STS_SUCCESS;
	uint8_t *buf;
	//uint32_t buflen=0;
	uint32_t sizeRead=0;
	uint32_t err=0;
	FILE *fhnd =NULL;
	char fwfile[MAX_PATH+1];

	DTS_LIB_CONTEXT		*Ctx = NULL;
	
	DTS_GET_CTX(hDevice,Ctx);	
	
	sts = DtsGetDILPath(hDevice, fwfile, sizeof(fwfile));

	if(sts != BC_STS_SUCCESS){
		return sts;
	}

	if(fname){
		strncat(fwfile,(const char*)fname,sizeof(fwfile));
	}else{
		//strncat(fwfile,"/",1);
		strncat(fwfile,FWBINFILE_LNK,sizeof(FWBINFILE_LNK));
	}
	



	
	if(!StreamVer){
		DebugLog_Trace(LDIL_DBG,"\nDtsGetFWVersionFromFile: Null Pointer argument");
		return BC_STS_INSUFF_RES;
	}

	fhnd = fopen((const char *)fwfile, "rb");
	
 	

	if(fhnd == NULL ){
		DebugLog_Trace(LDIL_DBG,"DtsGetFWVersionFromFile:Failed to Open file Err\n");
		return BC_STS_INSUFF_RES;
	}
	

	buf=(uint8_t *)malloc(MAX_BIN_FILE_SZ);
	if(buf==NULL) {
		DebugLog_Trace(LDIL_DBG,"DtsGetFWVersionFromFile:Failed to allocate memory\n");
		return BC_STS_INSUFF_RES;
	}

	err = fread(buf, sizeof(uint8_t), MAX_BIN_FILE_SZ, fhnd);
	if(!err)
		 {
			sizeRead = err;
	}

	if((err==0)&&(errno!=0)) 
	{ 
		DebugLog_Trace(LDIL_DBG,"DtsGetFWVersionFromFile:Failed to read bin file %d\n",errno);

		if(buf)free(buf);

		fclose(fhnd);
		return BC_STS_ERROR;
	}
	

	
	
	uint8_t *pSearchStr = &buf[0x4000];
	*StreamVer =0;
	for(uint32_t i=0; i <(sizeRead-0x4000);i++){
		if(NULL != strstr((char *)pSearchStr,(const char *)"Media_PC_FW_Rev")){
			//The actual FW versions are at searchstring - 4 bytes.	
			*StreamVer = ((*(pSearchStr-4)) << 16) | 
						 ((*(pSearchStr-3))<<8) | 
						 (*(pSearchStr-2));
			break;
		}
		pSearchStr++;
	}
	

	if(buf)
		free(buf);
	if(fhnd)
		fhnd=NULL;
	if(*StreamVer ==0)
		return BC_STS_ERROR;
	else
		return BC_STS_SUCCESS;

	
}

DRVIFLIB_API BC_STATUS 
DtsGetFWVersion(
    HANDLE  hDevice,
    uint32_t     *StreamVer,
	uint32_t		*DecVer,
    uint32_t     *HwVer,
	char	*fname,
	uint32_t	     flag
	)
{
	BC_STATUS sts;
	if(flag) //get runtime version by issuing a FWcmd
	{
		sts = DtsFWVersion(hDevice,StreamVer,DecVer,HwVer);
			if(sts == BC_STS_SUCCESS)
			{
				DebugLog_Trace(LDIL_DBG,"FW Version: Stream: %x, Dec: %x, HW:%x\n",*StreamVer,*DecVer,*HwVer);
			}
			else
			{
				DebugLog_Trace(LDIL_DBG,"DtsGetFWVersion: failed to get version fromFW at runtime: %d\n",sts);			
				return sts;
			}
	
	}
	else //read the stream arc version from binary
	{
		sts = DtsGetFWVersionFromFile(hDevice,StreamVer,DecVer,fname);
		if(sts == BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"FW Version: Stream: %x",*StreamVer);
			if(DecVer !=NULL)
				DebugLog_Trace(LDIL_DBG," Dec: %x\n",*DecVer);
		}
		else
		{
			DebugLog_Trace(LDIL_DBG,"DtsGetFWVersion: failed to get version from FW bin file: %d\n",sts);			
			return sts;
		}

	}
	
	return BC_STS_SUCCESS;

}


DRVIFLIB_API BC_STATUS 
DtsOpenDecoder(HANDLE  hDevice, uint32_t StreamType)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT	*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if (Ctx->State != BC_DEC_STATE_INVALID) {
		DebugLog_Trace(LDIL_DBG, "DtsOpenDecoder: Channel Already Open\n");
		return BC_STS_BUSY;
	}

	Ctx->LastPicNum = 0;
	Ctx->eosCnt = 0;
	Ctx->FlushIssued = FALSE;
	Ctx->CapState = 0;
	Ctx->picWidth = 0;
	Ctx->picHeight = 0;
	if (Ctx->SingleThreadedAppMode) {
		Ctx->cpbBase = 0;
		Ctx->cpbEnd = 0;
	}

	sts = DtsSetVideoClock(hDevice,0);
	if (sts != BC_STS_SUCCESS)
	{
		return sts;
	}

	// FIX_ME to support other stream types.
	sts = DtsSetTSMode(hDevice,0);
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}

	sts = DtsRecoverableDecOpen(hDevice,StreamType);
	if(sts != BC_STS_SUCCESS )
		return sts;

	sts = DtsFWSetVideoInput(hDevice);
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}

	Ctx->State = BC_DEC_STATE_OPEN;


	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS 
DtsStartDecoder(
    HANDLE  hDevice
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if( (Ctx->State == BC_DEC_STATE_START) || (Ctx->State == BC_DEC_STATE_INVALID) )
	{
		DebugLog_Trace(LDIL_DBG,"DtsStartDecoder: Decoder Not in correct State\n");
		return BC_STS_ERR_USAGE;
	}

	if(Ctx->VidParams.Progressive){
		sts = DtsSetProgressive(hDevice,0);
		if(sts != BC_STS_SUCCESS )
			return sts;
	}

	sts = DtsFWActivateDecoder(hDevice);
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}
	sts = DtsFWStartVideo(hDevice, Ctx->VidParams.VideoAlgo,
			      Ctx->VidParams.FGTEnable,
			      Ctx->VidParams.MetaDataEnable,
			      Ctx->VidParams.Progressive,
			      Ctx->VidParams.OptFlags);
	if(sts != BC_STS_SUCCESS )
		return sts;
	
	Ctx->State = BC_DEC_STATE_START;

	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsCloseDecoder(
    HANDLE  hDevice
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if( Ctx->State == BC_DEC_STATE_INVALID){
		DebugLog_Trace(LDIL_DBG,"DtsCloseDecoder: Decoder Not Opened Ignoring..\n");
		return BC_STS_SUCCESS;
	}

	sts = DtsFWCloseChannel(hDevice,Ctx->OpenRsp.channelId);
	
	/*if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}*/

	Ctx->State = BC_DEC_STATE_INVALID;
	
	Ctx->LastPicNum = 0;
	Ctx->eosCnt = 0;
	Ctx->FlushIssued = FALSE;

	/* Clear all pending lists.. */
	DtsClrPendMdataList(Ctx);

	/* Close the Input dump File */
	DumpInputSampleToFile(NULL,0);

	

	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsSetVideoParams(
    HANDLE  hDevice,
	uint32_t		videoAlg,
	BOOL	FGTEnable,
	BOOL	MetaDataEnable,
	BOOL	Progressive,
    uint32_t     OptFlags
	)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	
	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->State != BC_DEC_STATE_OPEN){
		DebugLog_Trace(LDIL_DBG,"DtsOpenDecoder: Channel Already Open\n");
		return BC_STS_ERR_USAGE;
	}
	Ctx->VidParams.VideoAlgo = videoAlg;
	Ctx->VidParams.FGTEnable = FGTEnable;
	Ctx->VidParams.MetaDataEnable = MetaDataEnable;
	Ctx->VidParams.Progressive = Progressive;
	/* Ctx->VidParams.Reserved = rsrv; */
	/* Ctx->VidParams.FrameRate = FrameRate; */
	Ctx->VidParams.OptFlags = OptFlags;
	/* SingleThreadedAppMode is bit 7 of OptFlags */
	if (OptFlags & 0x80)
		Ctx->SingleThreadedAppMode = 1;
	else
		Ctx->SingleThreadedAppMode = 0;

	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS
DtsSetInputFormat(HANDLE hDevice, BC_INPUT_FORMAT *pInputFormat )
{
	DTS_LIB_CONTEXT *Ctx = NULL;
	uint32_t videoAlgo = BC_VID_ALGO_H264;

	DTS_GET_CTX(hDevice,Ctx);

	Ctx->VidParams.MediaSubType = pInputFormat->mSubtype;
	Ctx->VidParams.WidthInPixels = pInputFormat->width;
	Ctx->VidParams.HeightInPixels = pInputFormat->height;
	if (pInputFormat->startCodeSz)
		Ctx->VidParams.StartCodeSz = pInputFormat->startCodeSz;
	else
		Ctx->VidParams.StartCodeSz = BRCM_START_CODE_SIZE;

	if (pInputFormat->metaDataSz) {
		if (Ctx->VidParams.pMetaData)
			delete Ctx->VidParams.pMetaData;
		Ctx->VidParams.pMetaData = new uint8_t[pInputFormat->metaDataSz];
		memcpy(Ctx->VidParams.pMetaData, pInputFormat->pMetaData, pInputFormat->metaDataSz);

		Ctx->VidParams.MetaDataSz = pInputFormat->metaDataSz;
	}

	if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_H264 || Ctx->VidParams.MediaSubType== BC_MSUBTYPE_AVC1)
		videoAlgo = BC_VID_ALGO_H264;
	else if (Ctx->VidParams.MediaSubType==BC_MSUBTYPE_DIVX)
		videoAlgo = BC_VID_ALGO_DIVX;
	else if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_MPEG2VIDEO )
		videoAlgo = BC_VID_ALGO_MPEG2;
	else if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1 || Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMVA ||Ctx->VidParams.MediaSubType == BC_MSUBTYPE_VC1)
		videoAlgo = BC_VID_ALGO_VC1;
	else  if (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3)
		videoAlgo = BC_VID_ALGO_VC1MP;	// Main Profile

	if (Ctx->DevId == BC_PCI_DEVID_FLEA || Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3)
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_PES;
	else
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_ES;

	DtsSetVideoParams(hDevice, videoAlgo, pInputFormat->FGTEnable, pInputFormat->MetaDataEnable, pInputFormat->Progressive, pInputFormat->OptFlags);
	DtsSetPESConverter(hDevice);

	//	if (Ctx->DevId == BC_PCI_DEVID_FLEA && !Ctx->adobeMode)
	// Always enable scaling to work around FP performance problem with YUY2
	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
		Ctx->bScaling = pInputFormat->bEnableScaling;
	return DtsCheckProfile(hDevice);
}

DRVIFLIB_API BC_STATUS 
DtsGetVideoParams(
    HANDLE  hDevice,
	uint32_t		*videoAlg,
	BOOL	*FGTEnable,
	BOOL	*MetaDataEnable,
	BOOL	*Progressive,
    uint32_t     rsrv
	)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(!videoAlg || !FGTEnable || !MetaDataEnable || !Progressive)
		return BC_STS_INV_ARG;
	
	if(Ctx->State != BC_DEC_STATE_OPEN){
		DebugLog_Trace(LDIL_DBG,"DtsOpenDecoder: Channel Already Open\n");
		return BC_STS_ERR_USAGE;
	}

	 *videoAlg = Ctx->VidParams.VideoAlgo;
	 *FGTEnable = Ctx->VidParams.FGTEnable;
	 *MetaDataEnable = Ctx->VidParams.MetaDataEnable;
	 *Progressive = Ctx->VidParams.Progressive;

	 return BC_STS_SUCCESS;

}


DRVIFLIB_API BC_STATUS 
DtsStopDecoder(
    HANDLE  hDevice
	)
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);


	if( (Ctx->State != BC_DEC_STATE_START) && (Ctx->State != BC_DEC_STATE_PAUSE) ) {
		DebugLog_Trace(LDIL_DBG,"DtsStopDecoder: Decoder Not Started\n");
		return BC_STS_ERR_USAGE;
	}

	DtsCancelFetchOutInt(Ctx);

	sts = DtsFWStopVideo(hDevice,Ctx->OpenRsp.channelId, FALSE);
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}

	Ctx->State = BC_DEC_STATE_STOP;

	return sts;
}

DRVIFLIB_API BC_STATUS 
	DtsPauseDecoder(
    HANDLE  hDevice	
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if( Ctx->State != BC_DEC_STATE_START ) {
		DebugLog_Trace(LDIL_DBG,"DtsPauseDecoder: Decoder Not Started\n");
		return BC_STS_ERR_USAGE;
	}

	sts = DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_ON);	
	if(sts != BC_STS_SUCCESS )
	{
		DebugLog_Trace(LDIL_DBG,"DtsPauseDecoder: Failed\n");
		return sts;
	}

	Ctx->State = BC_DEC_STATE_PAUSE;


	return sts;
}

DRVIFLIB_API BC_STATUS 
	DtsResumeDecoder(
    HANDLE  hDevice
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if( Ctx->State != BC_DEC_STATE_PAUSE ) {		
		return BC_STS_ERR_USAGE;
	}

	sts = DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_OFF);	
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}

	Ctx->State = BC_DEC_STATE_START;

	Ctx->totalPicsCounted = 0;
	Ctx->rptPicsCounted = 0;
	Ctx->nrptPicsCounted = 0;

	return sts;
}


DRVIFLIB_API BC_STATUS 
DtsStartCapture(HANDLE  hDevice)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_IOCTL_DATA *pIocData = NULL;
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_GET_CTX(hDevice,Ctx);

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	sts = DtsMapYUVBuffs(Ctx);
	if(sts != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsMapYUVBuffs failed Sts:%d\n",sts);
		return sts;
	}

	DebugLog_Trace(LDIL_DBG,"DbgOptions=%x\n", Ctx->RegCfg.DbgOptions);
	pIocData->u.RxCap.Rsrd = 0;
	pIocData->u.RxCap.StartDeliveryThsh = RX_START_DELIVERY_THRESHOLD;
	pIocData->u.RxCap.PauseThsh = PAUSE_DECODER_THRESHOLD;
	pIocData->u.RxCap.ResumeThsh = 	RESUME_DECODER_THRESHOLD;
	if( (sts=DtsDrvCmd(Ctx,BCM_IOC_START_RX_CAP,0,pIocData,FALSE)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsStartCapture: Failed: %d\n",sts);
	}

	DtsRelIoctlData(Ctx,pIocData);	

	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsFlushRxCapture(
				HANDLE  hDevice,	
				BOOL	bDiscardOnly)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_STATUS			Sts;
	BC_IOCTL_DATA *pIocData = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	pIocData->u.FlushRxCap.bDiscardOnly = bDiscardOnly;
	Sts = DtsDrvCmd(Ctx,BCM_IOC_FLUSH_RX_CAP, 0, pIocData, TRUE);
	return Sts;
}


DRVIFLIB_INT_API BC_STATUS
DtsCancelTxRequest(
	HANDLE	hDevice,
	uint32_t		Operation)
{
	return BC_STS_NOT_IMPL;
}


DRVIFLIB_API BC_STATUS 
DtsProcOutput(
    HANDLE  hDevice,
	uint32_t		milliSecWait,
	BC_DTS_PROC_OUT *pOut
)

{

	BC_STATUS	stRel,sts = BC_STS_SUCCESS;
#if 0
	BC_STATUS	stspwr = BC_STS_SUCCESS;
	BC_CLOCK *dynClock;
	BC_IOCTL_DATA *pIocData = NULL;
#endif
	BC_DTS_PROC_OUT OutBuffs;
	uint32_t	width=0, savFlags=0;
	
	

	DTS_LIB_CONTEXT		*Ctx = NULL;
	
	DTS_GET_CTX(hDevice,Ctx);
	

	if(!pOut){
		DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Invalid Arg!!\n");
		return BC_STS_INV_ARG;
	}

	savFlags = pOut->PoutFlags;
	pOut->discCnt = 0;
	pOut->b422Mode = Ctx->b422Mode;

	do
	{
		memset(&OutBuffs,0,sizeof(OutBuffs));

		sts = DtsFetchOutInterruptible(Ctx,&OutBuffs,milliSecWait);
		if(sts != BC_STS_SUCCESS)
		{
			/* In case of a peek..*/
			if((sts == BC_STS_TIMEOUT) && !(milliSecWait) )
				sts = BC_STS_NO_DATA;
			return sts;
		}

		/* Copying the discontinuity count */
		if(OutBuffs.discCnt)
			pOut->discCnt = OutBuffs.discCnt;

		pOut->PoutFlags |= OutBuffs.PoutFlags;

		if(pOut->PoutFlags & BC_POUT_FLAGS_FMT_CHANGE){
			if(pOut->PoutFlags & BC_POUT_FLAGS_PIB_VALID){
				pOut->PicInfo = OutBuffs.PicInfo;
#if 0
// Disable clock control for Linux for now.
				if(OutBuffs.PicInfo.width > 1280) {
					Ctx->minClk = 135;
					Ctx->maxClk = 165;
				} else if (OutBuffs.PicInfo.width > 720) {
					Ctx->minClk = 105;
					Ctx->maxClk = 135;
				} else {
					Ctx->minClk = 75;
					Ctx->maxClk = 100;
				}
				if(!(pIocData = DtsAllocIoctlData(Ctx)))
					/* don't do anything */
					DebugLog_Trace(LDIL_DBG,"Could not allocate IOCTL for power ctl\n");
				else {
					dynClock = 	&pIocData->u.clockValue;
					dynClock->clk = Ctx->minClk;
					DtsDrvCmd(Ctx, BCM_IOC_CHG_CLK, 0, pIocData, FALSE);
				}
				if(pIocData) DtsRelIoctlData(Ctx,pIocData);
				
				Ctx->curClk = dynClock->clk;	
				Ctx->totalPicsCounted = 0;
				Ctx->rptPicsCounted = 0;
				Ctx->nrptPicsCounted = 0;
				Ctx->numTimesClockChanged = 0;
#endif
			}

			/* Update Counters */
			DebugLog_Trace(LDIL_DBG,"Dtsprocout:update stats\n");
			DtsUpdateOutStats(Ctx,pOut);

			DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,TRUE);
			
		
			return BC_STS_FMT_CHANGE;
		}

		if (pOut->DropFrames)
		{
			/* We need to release the buffers even if we fail to copy..*/
			stRel = DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);

			if(sts != BC_STS_SUCCESS)
			{
				DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Failed to copy out buffs.. %x\n", sts);
				return sts;
			}			
			pOut->DropFrames--;
			DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Drop count.. %d\n", pOut->DropFrames);

			/* Get back the original flags */
			pOut->PoutFlags = savFlags;
		}
		else
			break;

	/* this can't be right, DropFrames is a uint8_t so it will always be greater than or equal to zero. */
	/* } while((pOut->DropFrames >= 0)); */
	} while((pOut->DropFrames > 0));

	if(pOut->AppCallBack && pOut->hnd && (OutBuffs.PoutFlags & BC_POUT_FLAGS_PIB_VALID))
	{
		/* Merge in and out flags */
		OutBuffs.PoutFlags |= pOut->PoutFlags;
		if(OutBuffs.PicInfo.width > 1280){
			width = 1920;
		}else if(OutBuffs.PicInfo.width > 720){
			width = 1280;
		}else{
			width = 720;
		}

		OutBuffs.b422Mode = Ctx->b422Mode;
		pOut->AppCallBack(	pOut->hnd,
							width,
							OutBuffs.PicInfo.height,
							0,
							&OutBuffs);
	}

	if(Ctx->b422Mode) {
		sts = DtsCopyRawDataToOutBuff(Ctx,pOut,&OutBuffs);	
	}else{
		if(pOut->PoutFlags & BC_POUT_FLAGS_YV12){
			sts = DtsCopyNV12ToYV12(Ctx,pOut,&OutBuffs);
		}else {
			sts = DtsCopyNV12(Ctx,pOut,&OutBuffs);
		}
	}

	if(pOut->PoutFlags & BC_POUT_FLAGS_PIB_VALID){
		pOut->PicInfo = OutBuffs.PicInfo;
	}
	
	if(Ctx->FlushIssued){
		if(Ctx->LastPicNum == pOut->PicInfo.picture_number){
			Ctx->eosCnt++;
		}else{
			Ctx->LastPicNum = pOut->PicInfo.picture_number;
			Ctx->eosCnt = 0;
			pOut->PicInfo.flags &= ~VDEC_FLAG_LAST_PICTURE;
		}
		if(Ctx->eosCnt >= BC_EOS_PIC_COUNT){
			/* Mark this picture as end of stream..*/
			pOut->PicInfo.flags |= VDEC_FLAG_LAST_PICTURE;
			Ctx->FlushIssued = 0;
			DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Last Picture Set\n");
		}
		else {
			pOut->PicInfo.flags &= ~VDEC_FLAG_LAST_PICTURE;
		}
	}

#if 0
	/* Dynamic power control. If we are not repeating pictures then slow down the clock. 
	If we are repeating pictures, speed up the clock */
	if(Ctx->FlushIssued) {
		Ctx->totalPicsCounted = 0;
		Ctx->rptPicsCounted = 0;
		Ctx->nrptPicsCounted = 0;
		Ctx->numTimesClockChanged = 0;
	}
	else {
		Ctx->totalPicsCounted++;
		
		if(pOut->PicInfo.picture_number != Ctx->LastPicNum)
			Ctx->nrptPicsCounted++;
		else
			Ctx->rptPicsCounted++;
		
		if (Ctx->totalPicsCounted > 100) {
			Ctx->totalPicsCounted = 0;
			Ctx->rptPicsCounted = 0;
			Ctx->nrptPicsCounted = 0;
		}
	}
	
	/* start dynamic power mangement only after the first 10 pictures are decoded */
	/* algorithm used is to check if for every 20 pictures decoded greater than 6 are repeating. 
		Then we are too slow. If we are not repeating > 80% of the pictures, then slow down the clock.
		Slow down the clock only 3 times for the clip. This is an arbitrary number until we figure out the long 
		run problem. */
	if(pOut->PicInfo.picture_number >= 10) {			
		if((Ctx->totalPicsCounted >= 20) && (Ctx->totalPicsCounted % 20) == 0) {
			if(!(pIocData = DtsAllocIoctlData(Ctx)))
				/* don't do anything */
				DebugLog_Trace(LDIL_DBG,"Could not allocate IOCTL for power ctl\n");
			else {
				dynClock = 	&pIocData->u.clockValue;
				if (Ctx->rptPicsCounted >= 6) {
					dynClock->clk = Ctx->curClk + 5;
					if (dynClock->clk >= Ctx->maxClk)
						dynClock->clk = Ctx->curClk;
					Ctx->rptPicsCounted = 0;
				}
				else if (Ctx->nrptPicsCounted > 80) {
					dynClock->clk = Ctx->curClk - 5;
					if (dynClock->clk <= Ctx->minClk)
						dynClock->clk = Ctx->curClk;
					else
						Ctx->numTimesClockChanged++;
					if(Ctx->numTimesClockChanged > 3)
						dynClock->clk = Ctx->curClk;
				}
				else
					dynClock->clk = Ctx->curClk;
				
				if(dynClock->clk != Ctx->curClk)
					stspwr = DtsDrvCmd(Ctx, BCM_IOC_CHG_CLK, 0, pIocData, FALSE);
			}
		
			if(pIocData) DtsRelIoctlData(Ctx,pIocData);
			
			Ctx->curClk = dynClock->clk;	
			if (stspwr != BC_STS_SUCCESS)
				DebugLog_Trace(LDIL_DBG,"Could not change frequency for power ctl\n");
		}
	}
#endif

	Ctx->LastPicNum = pOut->PicInfo.picture_number;

	/* Update Counters */
	DtsUpdateOutStats(Ctx,pOut);

	/* We need to release the buffers even if we fail to copy..*/
	stRel = DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);

	if(sts != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Failed to copy out buffs.. %x\n", sts);
		return sts;
	}

	return stRel;
}

#ifdef _ENABLE_CODE_INSTRUMENTATION_
static void dts_swap_buffer(uint32_t *dst, uint32_t *src, uint32_t cnt)
{
	uint32_t i=0;

	for (i=0; i < cnt; i++){
		dst[i] = BC_SWAP32(src[i]);
	}

}
#endif

DRVIFLIB_API BC_STATUS 
DtsProcOutputNoCopy(
    HANDLE  hDevice,
	uint32_t		milliSecWait,
	BC_DTS_PROC_OUT *pOut
)
{

	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	
	DTS_GET_CTX(hDevice,Ctx);

	if(!pOut){
		return BC_STS_INV_ARG;
	}
	/* Init device params */
	if(Ctx->DevId == BC_PCI_DEVID_LINK){
		pOut->bPibEnc = TRUE;
	}else{
		pOut->bPibEnc = FALSE;
	}
	pOut->b422Mode = Ctx->b422Mode;

	while(1){

		if( (sts = DtsFetchOutInterruptible(Ctx,pOut,milliSecWait)) != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsProcOutput: No Active Channels\n");
				/* In case of a peek..*/
			if((sts == BC_STS_TIMEOUT) && !(milliSecWait) ){
				sts = BC_STS_NO_DATA;
				break;
			}	
		}

		/*
		 * If the PIB is not encrypted then we can direclty go
		 * to the PIB and get the information weather the Picture 
		 * is encrypted or not.
		 */
		
#ifdef _ENABLE_CODE_INSTRUMENTATION_

		if( (sts == BC_STS_SUCCESS) && 
			(!(pOut->PoutFlags & BC_POUT_FLAGS_FMT_CHANGE)) && 
			(FALSE == pOut->b422Mode))
		{
			uint32_t						Width=0,Height=0,PicInfoLineNum=0,CntrlFlags = 0; 
			uint8_t*						pPicInfoLine;

			Height = Ctx->picHeight;
			Width = Ctx->picWidth;
			
			PicInfoLineNum = (ULONG)(*(pOut->Ybuff + 3)) & 0xff
			| ((ULONG)(*(pOut->Ybuff + 2)) << 8) & 0x0000ff00
			| ((ULONG)(*(pOut->Ybuff + 1)) << 16) & 0x00ff0000
			| ((ULONG)(*(pOut->Ybuff + 0)) << 24) & 0xff000000;

			//DebugLog_Trace(LDIL_DBG,"PicInfoLineNum:%x, Width: %d, Height: %d\n",PicInfoLineNum, Width, Height);
			pOut->PoutFlags &= ~BC_POUT_FLAGS_PIB_VALID;
			if( (PicInfoLineNum == Height) || (PicInfoLineNum == Height/2))
			{
				pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;
				pPicInfoLine = pOut->Ybuff + PicInfoLineNum * Width;
				CntrlFlags  = (ULONG)(*(pPicInfoLine + 3)) & 0xff
							| ((ULONG)(*(pPicInfoLine + 2)) << 8) & 0x0000ff00
							| ((ULONG)(*(pPicInfoLine + 1)) << 16) & 0x00ff0000
							| ((ULONG)(*(pPicInfoLine + 0)) << 24) & 0xff000000;


				pOut->PicInfo.picture_number = 0xc0000000 & CntrlFlags;
				DebugLog_Trace(LDIL_DBG," =");
				if(CntrlFlags & BC_BIT(30)){
					pOut->PoutFlags |= BC_POUT_FLAGS_FLD_BOT;
					DebugLog_Trace(LDIL_DBG,"B+");
				}else{
					pOut->PoutFlags &= ~BC_POUT_FLAGS_FLD_BOT;
					DebugLog_Trace(LDIL_DBG,"T+");
				}

				if(CntrlFlags & BC_BIT(31)) {
					DebugLog_Trace(LDIL_DBG,"E");
				} else {
					DebugLog_Trace(LDIL_DBG,"Ne");
				}
				DebugLog_Trace(LDIL_DBG,"=");

				dts_swap_buffer((uint32_t*)&pOut->PicInfo,(uint32_t*)(pPicInfoLine + 4), 32);
			}
		}

#endif
		/* Update Counters.. */
		DtsUpdateOutStats(Ctx,pOut);

		if( (sts == BC_STS_SUCCESS) && (pOut->PoutFlags & BC_POUT_FLAGS_FMT_CHANGE) ){	
			DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,TRUE);	
			sts = BC_STS_FMT_CHANGE;
			break;
		}

		if(pOut->DropFrames){
			/* We need to release the buffers even if we fail to copy..*/
			sts = DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);

			if(sts != BC_STS_SUCCESS)
			{
				DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Failed to release buffs.. %x\n", sts);
				return sts;
			}			
			pOut->DropFrames--;
			DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Drop count.. %d\n", pOut->DropFrames);

		}
		else
			break;

	}


	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsReleaseOutputBuffs(
    HANDLE  hDevice,
	PVOID   Reserved,
	BOOL	fChange)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	
	DTS_GET_CTX(hDevice,Ctx);

	if(fChange)
		return BC_STS_SUCCESS;

	return DtsRelRxBuff(Ctx, &Ctx->pOutData->u.RxBuffs, FALSE);
}

// NAREN this function is used to send data buffered to the HW
// This is initially for FP since it needs buffering to avoid stalling the CPU
// and excessive polling from the single thread plugin due to the HW returning 0 size
// buffers when it is absorbing data
// FP tries to send bursts of data to return from hide or minimized operation to full visible
// operation and this will allow bigger bursts
DRVIFLIB_API BC_STATUS
DtsSendDataFPMode( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	uint32_t	DramOff;
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_DTS_STATUS pStat;
	uint32_t TransferSz = 0;
	//unused uint32_t i_cnt=0, p_cnt=0;

	//unused BC_DTS_STATS *pDtsStat = DtsGetgStats();

	DTS_GET_CTX(hDevice,Ctx);

	// Not really needed for FP since it only used H.264. But leave it just in case
	if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		encrypted|=0x2;

	if(ulSizeInBytes > FP_TX_BUF_SIZE)
		return BC_STS_INSUFF_RES; //Error

	// This hack only works because FP is single threaded and we are sure that no one else is using this function
	// For other cases we will have to use synchronization
	Ctx->SingleThreadedAppMode |= 0x8; // get the HW free size always in this function
	// First get the available size from the HW
	sts = DtsGetDriverStatus(hDevice, &pStat);
	Ctx->SingleThreadedAppMode &= 0x7;
	if(sts != BC_STS_SUCCESS)
		return sts;

	/* If we are aborting TX then clear the buffer and return */
	if(pStat.cpbEmptySize & 0x80000000)
	{
		Ctx->nPendFPBufInd = 0;
		return BC_STS_SUCCESS;
	}

	// Check for Drain condition. In case of FP Drain will be posted from ProcOutput
	if((ulSizeInBytes == 0) && (pUserData == NULL))
	{
		// first make sure the HW can deal with the data
		if(pStat.cpbEmptySize == 0)
			return BC_STS_SUCCESS;

		if(Ctx->nPendFPBufInd <= pStat.cpbEmptySize)
			TransferSz = Ctx->nPendFPBufInd;
		else
			TransferSz = pStat.cpbEmptySize;

		sts = DtsTxDmaText(hDevice, Ctx->FPpendingBuf, TransferSz, &DramOff, encrypted);
		if(sts == BC_STS_SUCCESS)
			DtsUpdateInStats(Ctx, TransferSz);
		else
			return sts;

		Ctx->nPendFPBufInd -= TransferSz;
		if (Ctx->nPendFPBufInd == 0)
			return BC_STS_SUCCESS;
		// Then clean up the buffer for the next time
		if(memmove_s(Ctx->FPpendingBuf, FP_TX_BUF_SIZE, Ctx->FPpendingBuf + TransferSz, Ctx->nPendFPBufInd))
			return BC_STS_INSUFF_RES;
		else
			return BC_STS_SUCCESS;
	}

	// We can safely buffer if we can't send because FP has checked for enough space before the TX
	// if the cpb returns empty - we are either in PD or don't have space
	// just buffer and return
	if(pStat.cpbEmptySize == 0)
	{
		if(memcpy_s(Ctx->FPpendingBuf + Ctx->nPendFPBufInd, FP_TX_BUF_SIZE - Ctx->nPendFPBufInd, pUserData, ulSizeInBytes))
			return BC_STS_INSUFF_RES;
		Ctx->nPendFPBufInd += ulSizeInBytes;
		return BC_STS_SUCCESS;
	}

	// We can send some data. If we can send without buffering data send directly
	if((Ctx->nPendFPBufInd == 0) && (ulSizeInBytes <= pStat.cpbEmptySize))
	{
		sts = DtsTxDmaText(hDevice, pUserData, ulSizeInBytes, &DramOff, encrypted);
		if(sts == BC_STS_SUCCESS)
			DtsUpdateInStats(Ctx,ulSizeInBytes);
		return sts;
	}

	// We were unable to send all the data. So buffer the rest.
	// Do a poor man's queue here. Ugly, so fix this if it is a performance problem
	// First add to the buffer
	if(memcpy_s(Ctx->FPpendingBuf + Ctx->nPendFPBufInd, FP_TX_BUF_SIZE - Ctx->nPendFPBufInd, pUserData, ulSizeInBytes))
		return BC_STS_INSUFF_RES;
	Ctx->nPendFPBufInd += ulSizeInBytes;
	if(Ctx->nPendFPBufInd <= pStat.cpbEmptySize)
		TransferSz = Ctx->nPendFPBufInd;
	else
		TransferSz = pStat.cpbEmptySize;

	sts = DtsTxDmaText(hDevice, Ctx->FPpendingBuf, TransferSz, &DramOff, encrypted);
	if(sts == BC_STS_SUCCESS)
		DtsUpdateInStats(Ctx, TransferSz);
	else
		return sts;
	Ctx->nPendFPBufInd -= TransferSz;
	if (Ctx->nPendFPBufInd == 0)
		return BC_STS_SUCCESS;
	// Then clean up the buffer for the next time
	if(memmove_s(Ctx->FPpendingBuf, FP_TX_BUF_SIZE, Ctx->FPpendingBuf + TransferSz, Ctx->nPendFPBufInd))
		return BC_STS_INSUFF_RES;
	else
		return BC_STS_SUCCESS;

}

DRVIFLIB_API BC_STATUS
DtsSendDataBuffer( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	uint32_t	DramOff;
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_IOCTL_DATA		*pIocData = NULL;
	uint8_t bForceDeliver = false;
	uint8_t bFlushing = false;

	//unused uint8_t* pData;
	uint32_t	ulSize;

	uint32_t ulTxBufAvailSize;
	//unused BC_DTS_STATS *pDtsStat = DtsGetgStats( );

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		encrypted|=0x2;

	// ulSizeInBytes = 0 && pUserData = NULL ==> Drain input buffer

	if (pUserData == NULL && ulSizeInBytes == 0)
		bFlushing = true;

#ifdef TX_CIRCULAR_BUFFER
	if (Ctx->nTxHoldBufWrite >= Ctx->nTxHoldBufRead)
	{
		ulTxBufAvailSize = TX_HOLD_BUF_SIZE - (Ctx->nTxHoldBufWrite - Ctx->nTxHoldBufRead);
	}
	else
	{
		ulTxBufAvailSize = Ctx->nTxHoldBufRead - Ctx->nTxHoldBufWrite;
	}
	if (ulSizeInBytes && ulTxBufAvailSize > ulSizeInBytes)
	{
		if ((TX_HOLD_BUF_SIZE - Ctx->nTxHoldBufWrite) >= ulSizeInBytes)
		{
			memcpy(Ctx->pendingBuf+Ctx->nTxHoldBufWrite, pUserData, ulSizeInBytes);
			Ctx->nTxHoldBufWrite = (Ctx->nTxHoldBufWrite + ulSizeInBytes) % TX_HOLD_BUF_SIZE;
			ulSizeInBytes = 0;
		}
		else
		{
			ulSize = TX_HOLD_BUF_SIZE - Ctx->nTxHoldBufWrite;
			memcpy(Ctx->pendingBuf+Ctx->nTxHoldBufWrite, pUserData, ulSize);
			pUserData += ulSize;
			memcpy(Ctx->pendingBuf, pUserData, ulSizeInBytes - ulSize);
			Ctx->nTxHoldBufWrite = ulSizeInBytes - ulSize;
			ulSizeInBytes = 0;
		}
	}

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	while (Ctx->nTxHoldBufWrite != Ctx->nTxHoldBufRead)
	{
		bForceDeliver = bFlushing;
		if (Ctx->nTxHoldBufWrite >= Ctx->nTxHoldBufRead)
		{
			ulSize = Ctx->nTxHoldBufWrite - Ctx->nTxHoldBufRead;
		}
		else
		{
			bForceDeliver = true;
			ulSize = TX_HOLD_BUF_SIZE - Ctx->nTxHoldBufRead;
		}

		if ((!bForceDeliver) && (ulSize < TX_BUF_THRESHOLD) && (Ctx->nTxHoldBufCounter < TX_BUF_DELIVER_THRESHOLD))
		{
			Ctx->nTxHoldBufCounter ++;
			break;
		}
		Ctx->nTxHoldBufCounter = 0;
		if (DtsDrvCmd(Ctx, BCM_IOC_GET_DRV_STAT, 0, pIocData, FALSE) == BC_STS_SUCCESS)
		{
			if (pIocData->u.drvStat.DrvcpbEmptySize & 0x80000000)
			{
				//Aborting Tx
				Ctx->nTxHoldBufWrite = Ctx->nTxHoldBufRead = 0;
				DtsRelIoctlData(Ctx,pIocData);
				return BC_STS_IO_USER_ABORT;
			}
			if (pIocData->u.drvStat.DrvcpbEmptySize == 0)
			{
				bc_sleep_ms(5);
				break;
			}
			if (pIocData->u.drvStat.DrvcpbEmptySize < ulSize)
				ulSize = pIocData->u.drvStat.DrvcpbEmptySize;

			if(!bForceDeliver)
				ulSize = (ulSize >> 2) << 2;

			if (ulSize)
			{

				sts = DtsTxDmaText(hDevice, Ctx->pendingBuf+Ctx->nTxHoldBufRead,ulSize, &DramOff, encrypted);
				if(sts == BC_STS_SUCCESS)
				{
					Ctx->nTxHoldBufRead = ( Ctx->nTxHoldBufRead + ulSize ) % TX_HOLD_BUF_SIZE;
					if(Ctx->nTxHoldBufRead == Ctx->nTxHoldBufWrite)
						Ctx->nTxHoldBufWrite = Ctx->nTxHoldBufRead = 0;
					DtsUpdateInStats(Ctx,ulSize);
				}
			}
		}
	}
	DtsRelIoctlData(Ctx,pIocData);

	if (ulSizeInBytes > 0)
		return BC_STS_BUSY;
	if (bFlushing && (Ctx->nTxHoldBufWrite != Ctx->nTxHoldBufRead))
		return BC_STS_BUSY;
	return BC_STS_SUCCESS;
#else
	if (ulSizeInBytes > 0 && pUserData)
	{
		if (ulSizeInBytes > TX_HOLD_BUF_SIZE)
		{
			if (Ctx->nPendBufInd)
			{
				sts = DtsTxDmaText(hDevice,Ctx->pendingBuf,Ctx->nPendBufInd,&DramOff, encrypted);
				if (sts != BC_STS_SUCCESS)
					return sts;
				DtsUpdateInStats(Ctx,Ctx->nPendBufInd);
				Ctx->nPendBufInd = 0;
			}
			sts = DtsTxDmaText(hDevice,pUserData,ulSizeInBytes,&DramOff, encrypted);
			if(sts == BC_STS_SUCCESS)
				DtsUpdateInStats(Ctx,ulSizeInBytes);
			return sts;
		}
		else if ((Ctx->nPendBufInd + ulSizeInBytes) < TX_HOLD_BUF_SIZE)
		{
			memcpy(Ctx->pendingBuf+Ctx->nPendBufInd, pUserData, ulSizeInBytes);
			Ctx->nPendBufInd += ulSizeInBytes;
			return BC_STS_SUCCESS;
		}
	}
	pData = Ctx->pendingBuf;
	ulSize = Ctx->nPendBufInd;

	if (ulSize == 0)
		return BC_STS_SUCCESS;

	sts = DtsTxDmaText(hDevice,pData,ulSize,&DramOff, encrypted);
	if(sts == BC_STS_SUCCESS)
		DtsUpdateInStats(Ctx,ulSize);

	if (ulSizeInBytes && pUserData)
		memcpy(Ctx->pendingBuf, pUserData, ulSizeInBytes);

	Ctx->nPendBufInd = ulSizeInBytes;
	return sts;

#endif
}

DRVIFLIB_API BC_STATUS
DtsSendDataDirect( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	uint32_t	DramOff;
	BC_STATUS	sts = BC_STS_SUCCESS;
	uint32_t     CpbSize=0;
	uint32_t		CpbFullness=0;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	uint32_t base, end, readp, writep;
	int		FifoSize;

	BC_DTS_STATS *pDtsStat = DtsGetgStats( );

	DTS_GET_CTX(hDevice,Ctx);

	if (pUserData == NULL || ulSizeInBytes == 0)
		return BC_STS_INV_ARG;

	if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		encrypted|=0x2;

	if (!(Ctx->FixFlags & DTS_SKIP_TX_CHK_CPB) && (Ctx->DevId == BC_PCI_DEVID_LINK))
	{

		if(encrypted&0x2){
			DtsDevRegisterRead(hDevice, REG_Dec_TsAudCDB2Base, &base);
			DtsDevRegisterRead(hDevice, REG_Dec_TsAudCDB2End,	&end);
			DtsDevRegisterRead(hDevice, REG_Dec_TsAudCDB2Wrptr, &writep);
			DtsDevRegisterRead(hDevice, REG_Dec_TsAudCDB2Rdptr, &readp);
		}
		else if(encrypted&0x1){
			DtsDevRegisterRead(hDevice, REG_Dec_TsUser0Base, &base);
			DtsDevRegisterRead(hDevice, REG_Dec_TsUser0End, &end);
			DtsDevRegisterRead(hDevice, REG_Dec_TsUser0Wrptr, &writep);
			DtsDevRegisterRead(hDevice, REG_Dec_TsUser0Rdptr, &readp);
		}else{
			DtsDevRegisterRead(hDevice, REG_DecCA_RegCinBase, &base);
			DtsDevRegisterRead(hDevice, REG_DecCA_RegCinEnd, &end);
			DtsDevRegisterRead(hDevice, REG_DecCA_RegCinWrPtr, &writep);
			DtsDevRegisterRead(hDevice, REG_DecCA_RegCinRdPtr, &readp);
		}

		CpbSize = end - base;

		if(writep >= readp) {
			CpbFullness = writep - readp;
		} else {
			CpbFullness = (end - base) - (readp - writep);
		}

		FifoSize = CpbSize - CpbFullness;

		if(FifoSize < BC_INFIFO_THRESHOLD) {
			//DebugLog_Trace(LDIL_DBG, "Bsy0:Base:0x%08x End:0x%08x Wr:0x%08x Rd:0x%08x FifoSz:0x%08x\n",
			//						base,end, writep, readp, FifoSize);
			pDtsStat->TxFifoBsyCnt++;
			return BC_STS_BUSY;
		}

		if((signed int)ulSizeInBytes > (FifoSize - BC_INFIFO_THRESHOLD)) {
			//DebugLog_Trace(LDIL_DBG, "Bsy1:Base:0x%08x End:0x%08x Wr:0x%08x Rd:0x%08x FifoSz:0x%08x XfrReq:0x%08x\n",
			//					base,end, writep, readp, FifoSize, ulSizeInBytes);
			pDtsStat->TxFifoBsyCnt++;
			return BC_STS_BUSY;
		}
	}
	sts = DtsTxDmaText(hDevice, pUserData, ulSizeInBytes, &DramOff, encrypted);

	if(sts == BC_STS_SUCCESS)
	{
		DtsUpdateInStats(Ctx,ulSizeInBytes);
	}
	return sts;
}

DRVIFLIB_INT_API BC_STATUS
DtsSendData( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	//unused BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if ((Ctx->DevId == BC_PCI_DEVID_LINK) || (Ctx->FixFlags&DTS_DIAG_TEST_MODE))
	{
		return DtsSendDataDirect(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
	}
	else if (Ctx->SingleThreadedAppMode)
	{
		return DtsSendDataFPMode(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
	}
	else
	{
		return DtsSendDataBuffer(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
	}
}

DRVIFLIB_API BC_STATUS
DtsSendSPESPkt(HANDLE  hDevice ,
			   uint64_t timeStamp,
			   BOOL encrypted
			   )
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_INPUT_MDATA		*im = NULL;
	//unused uint8_t *temp=NULL;
	uint32_t ulSize = 0;
	uint8_t* pSPESPkt = NULL;
	uint8_t	i = 0;

	DTS_GET_CTX(hDevice,Ctx);
	while (i <20)
	{
		sts = DtsPrepareMdata(Ctx, timeStamp, &im, &pSPESPkt, &ulSize);
		if (sts == BC_STS_SUCCESS)
			break;
		i++;
		bc_sleep_ms(2000);
	}
	if (sts == BC_STS_SUCCESS)
	{
		if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		{
			pSPESPkt = (uint8_t*)DtsAlignedMalloc(32+sizeof(BC_PES_HDR_FORMAT), 4);
			if(pSPESPkt==NULL){
				DebugLog_Trace(LDIL_DBG, "DtsProcInput: Failed to alloc mem for  ASFHdr for SPES:%x\n", sts);
				return BC_STS_INSUFF_RES;
			}

			sts =DtsPrepareMdataASFHdr(Ctx, im, pSPESPkt);

			if(sts != BC_STS_SUCCESS){
                DtsAlignedFree(pSPESPkt);
				DebugLog_Trace(LDIL_DBG, "DtsProcInput: Failed to Prepare ASFHdr for SPES:%x\n", sts);
                return sts;
			}
			ulSize = 32+sizeof(BC_PES_HDR_FORMAT);
		}
		sts = DtsSendData(hDevice, pSPESPkt, ulSize, 0, encrypted);
        DtsAlignedFree(pSPESPkt);

		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG, "DtsProcInput: Failed to send Spes hdr:%x\n", sts);
			DtsFreeMdata(Ctx,im,TRUE);
			return sts;
		}

		sts = DtsInsertMdata(Ctx,im);

		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG, "DtsProcInput: DtsInsertMdata failed\n");
		}
	}
	return sts;
}

DRVIFLIB_API BC_STATUS
DtsAlignSendData( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	uint8_t	oddBytes = 0;
	//unused uint32_t lRestSize =0;
	//unused uint8_t *pRestBuff = NULL;

	uint8_t* alignBuf;
	//unused DTS_INPUT_MDATA		*im = NULL;
	//unused uint8_t *temp=NULL;
	//unused uint32_t ulSize = 0;

	DTS_GET_CTX(hDevice,Ctx);

	alignBuf = Ctx->alignBuf;

	LONG ulRestBytes = ulSizeInBytes;
	uint32_t ulDeliverBytes = 0;
	uint32_t ulPktLength;
	uint32_t ulPktHeaderSz;
	uint32_t ulUsedDataBytes = 0;

	uint8_t* pDeliverBuf = NULL;
	uint8_t* pRestDataPt = pUserData;
	uint32_t bAddPTS = 1;
	uint8_t bPrivData = Ctx->PESConvParams.m_bPESPrivData;
	uint8_t bExtData = Ctx->PESConvParams.m_bPESExtField;
	uint8_t bStuffing = Ctx->PESConvParams.m_bStuffing;

	uint8_t* pPrivData = Ctx->PESConvParams.m_pPESPrivData;
	uint8_t* pExtData = Ctx->PESConvParams.m_pPESExtField;
	uint32_t nExtDataLen = Ctx->PESConvParams.m_nPESExtLen;
	uint32_t	nStuffingBytes = Ctx->PESConvParams.m_nStuffingBytes;

	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	uint8_t bSoftRaveEOS = Ctx->PESConvParams.m_bSoftRaveEOS;

	uint8_t j = 0;
	uint8_t k = 0;

	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	if ((timeStamp ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_DIVX))
		&& (bSoftRaveEOS == false))
	{
		bAddPTS = 1;
	}
	else
	{
		bAddPTS = 0;
	}

	while (Ctx->State == BC_DEC_STATE_START && ulRestBytes )
	{
		sts = BC_STS_SUCCESS;

		pDeliverBuf = pRestDataPt;

		if (Ctx->State != BC_DEC_STATE_START && Ctx->State != BC_DEC_STATE_PAUSE)
			break;

		/* Copy the non-DWORD aligned buffer first */
		oddBytes = (uint8_t)((DWORD)pDeliverBuf % 4);

		if (Ctx->VidParams.StreamType == BC_STREAM_TYPE_ES)
		{
			// SPES Mode
			ulDeliverBytes = ulRestBytes;
			if (timeStamp)
			{
				sts = DtsSendSPESPkt(hDevice, timeStamp, encrypted);
			}
			timeStamp = 0;

			if(oddBytes)
			{
				if (ulRestBytes > ALIGN_BUF_SIZE)
					ulDeliverBytes = ALIGN_BUF_SIZE - oddBytes;
				else
					ulDeliverBytes = ulRestBytes;

				memcpy(alignBuf, pDeliverBuf, ulDeliverBytes);
				pDeliverBuf = alignBuf;
			}
			ulUsedDataBytes = ulDeliverBytes;
		}
		else if (Ctx->VidParams.StreamType == BC_STREAM_TYPE_PES)
		{
			if (Ctx->DevId == BC_PCI_DEVID_LINK && timeStamp)
			{
				sts = DtsSendSPESPkt(hDevice, timeStamp, encrypted);
				timeStamp = 0;
				bAddPTS =0;
			}

			if (bAddPTS)
				ulPktHeaderSz = 5;
			else
				ulPktHeaderSz = 0;
			if (bPrivData || bExtData)
			     ulPktHeaderSz += 1;

			if (bPrivData)
				ulPktHeaderSz += 16;

			if (bExtData)
				ulPktHeaderSz += nExtDataLen + 1;

			if (bStuffing)
				ulPktHeaderSz += nStuffingBytes;
			ulPktLength = ulRestBytes + 3 + ulPktHeaderSz;

			if (ulPktLength > MAX_RE_PES_BOUND)
			{
				ulPktLength = MAX_RE_PES_BOUND;
			}
			ulUsedDataBytes = ulPktLength - ulPktHeaderSz - 3;
			ulDeliverBytes = ulPktLength + 6;


			memcpy(alignBuf,(uint8_t*)b_pes_header, 9);
			*((uint16_t*)(alignBuf + 4)) = WORD_SWAP((uint16_t)ulPktLength);
			*(alignBuf + 8) = (uint8_t)ulPktHeaderSz;

			j = 9;
			if (bAddPTS)
			{
				*(alignBuf + 7) = 0x80;
				PTS2MakerBit5Bytes(alignBuf + j, timeStamp);
				j += 5;
			}

			if (bPrivData || bExtData)
			{
				*(alignBuf + 7) |= 0x01;
				*(alignBuf + j) = 0x00;
				if (bPrivData)
					*(alignBuf + j) |= 0x80;
				if (bExtData)
					*(alignBuf + j) |= 0x01;
				j++;
			}

			if (bPrivData)
			{
				memcpy(alignBuf + j, pPrivData, 16);
				j += 16;
			}
			if (bExtData)
			{
				*(alignBuf + j) = 0x80 | nExtDataLen;
				j++;
				memcpy(alignBuf + j, pExtData, nExtDataLen);
				j += nExtDataLen;
			}

			if (bStuffing)
			{
				for (k = 0; k < nStuffingBytes; k ++, j++)
				{
					*(alignBuf + j) = 0xFF;
				}
			}
			memcpy(alignBuf + j, pDeliverBuf, ulUsedDataBytes);
			pDeliverBuf = alignBuf;

		}

		if (ulDeliverBytes)
		{
			sts = DtsSendData(hDevice,pDeliverBuf ,ulDeliverBytes, 0, encrypted);

			if(sts == BC_STS_BUSY )
			{
				if (Ctx->State == BC_DEC_STATE_PAUSE)
				{
					return sts;
				}
				bc_sleep_ms(2000);
			}
			else if(sts == BC_STS_SUCCESS)
			{
				ulRestBytes -= ulUsedDataBytes;
				pRestDataPt += ulUsedDataBytes;
				bAddPTS = 0;
				timeStamp = 0;
				bPrivData = 0;
				bExtData = 0;
				pPrivData = NULL;
				pExtData = NULL;
				nExtDataLen = 0;
				bStuffing = 0;
				nStuffingBytes = 0;
			}else if(sts == BC_STS_IO_USER_ABORT){
				return BC_STS_SUCCESS;
			} else
				break; // On any other error condition
		}
	}
	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsProcInput( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	uint8_t	*pSpsPpsBuf;
	uint32_t ulSpsPpsSize;
	uint32_t Offset;

	DTS_GET_CTX(hDevice,Ctx);

	Ctx->FlushIssued = FALSE;
	Ctx->bEOS = FALSE;

#ifdef _CHECK_PI_
	WCHAR pi[512];
	static unsigned int pinum=1;
	wsprintf(pi, L"PI%d sz %ld TS %ld\n", pinum++, ulSizeInBytes, timeStamp);
	OutputDebugString(pi);
#endif

	if (Ctx->DevId == BC_PCI_DEVID_FLEA && timeStamp != 0xFFFFFFFFFFFFFFFFLL)
	{
		timeStamp /= 10000;
	}
	Ctx->bEOS = false;
	if((Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1) || (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) || (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMVA))
		DtsCheckKeyFrame(hDevice, pUserData);
	if (Ctx->PESConvParams.m_bAddSpsPps)
	{
		pSpsPpsBuf = Ctx->PESConvParams.m_pSpsPpsBuf;
		ulSpsPpsSize = Ctx->PESConvParams.m_iSpsPpsLen;

		if  (pSpsPpsBuf != NULL && ulSpsPpsSize != 0)
		{
			if ((sts = DtsAlignSendData(hDevice, pSpsPpsBuf, ulSpsPpsSize, 0, 0)) != BC_STS_SUCCESS)
				return sts;
		}
		Ctx->PESConvParams.m_bAddSpsPps = false;
	}

	if ((sts = DtsAddStartCode(hDevice, &pUserData, &ulSizeInBytes, &timeStamp)) != BC_STS_SUCCESS)
	{
		if (sts == BC_STS_IO_XFR_ERROR)
			return BC_STS_SUCCESS;
		return BC_STS_ERROR;
	}
	if (Ctx->VidParams.StreamType == BC_STREAM_TYPE_PES || timeStamp == 0)
		return DtsAlignSendData(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
	else
	{
		if(DtsFindStartCode(hDevice, pUserData, ulSizeInBytes, &Offset) != BC_STS_SUCCESS)
		{
			timeStamp = 0;
			Offset = 0;
		}
		if(Offset == 0)
		{
			return DtsAlignSendData(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
		}
		else
		{
			if ((sts=DtsAlignSendData(hDevice, pUserData, Offset, 0, encrypted)) != BC_STS_SUCCESS)
				return sts;
			if(ulSizeInBytes > Offset)
				return DtsAlignSendData(hDevice, pUserData+Offset, ulSizeInBytes-Offset, timeStamp, encrypted);
		}
	}
	return BC_STS_ERROR;
}

DRVIFLIB_API BC_STATUS
DtsGetColorPrimaries(HANDLE  hDevice, uint32_t *colorPrimaries)
{
	return BC_STS_NOT_IMPL;
}

DRVIFLIB_API BC_STATUS
DtsSendEOS(HANDLE  hDevice, uint32_t Op)
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	uint8_t	*pEOS;
	uint32_t nEOSLen;
	uint32_t	nTag;
	__attribute__((aligned(4))) uint8_t	ExtData[2] = {0x00, 0x00};
	//unused __attribute__((aligned(4))) uint8_t	Plunge[200];

	//unused uint8_t	*alignBuf = Ctx->alignBuf;

	Ctx->PESConvParams.m_bPESExtField = false;
	Ctx->PESConvParams.m_bPESPrivData = false;

	Ctx->PESConvParams.m_pPESExtField = NULL;
	Ctx->PESConvParams.m_pPESPrivData = NULL;

	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	if ((Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_DIVX))
	{
		Ctx->PESConvParams.m_bSoftRaveEOS = true;
	}
	else
	{
		Ctx->PESConvParams.m_bSoftRaveEOS = false;
	}

	if ((Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMVA) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_VC1)
		)
	{
		Ctx->PESConvParams.m_bPESExtField = true;
		Ctx->PESConvParams.m_nPESExtLen = 2;
		Ctx->PESConvParams.m_pPESExtField = ExtData;
	}

	if (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_MPEG1VIDEO ||
		Ctx->VidParams.MediaSubType == BC_MSUBTYPE_MPEG2VIDEO)
	{
		pEOS = eos_mpeg;
		nEOSLen = 4;
	}
	else if (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_DIVX)
	{
		pEOS = eos_divx;
		nEOSLen = 8;
	}
	else if (Ctx->DevId == BC_PCI_DEVID_LINK && Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3)
	{
		pEOS = eos_vc1_spmp_link;
		nEOSLen = 32;
	}
	else
	{
		pEOS = eos_avc_vc1;
		nEOSLen = 8;
	}

	sts = DtsAlignSendData(hDevice, pEOS, nEOSLen, 0, 0);

	/* Only send timing marker if this is FLEA */
	/* LINK Support LAST_PICTURE and does not need timing marker */
	if (Op == 0 && Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		Ctx->PESConvParams.m_bPESPrivData = true;
		Ctx->PESConvParams.m_pPESPrivData = btp_video_done_es_private;
		if (Ctx->PESConvParams.m_bPESExtField == true)
		{
			Ctx->PESConvParams.m_bStuffing = false;
			Ctx->PESConvParams.m_nStuffingBytes = 0;
		}
		else
		{
			Ctx->PESConvParams.m_bStuffing = true;
			Ctx->PESConvParams.m_nStuffingBytes = 3;
		}

		nTag = 0xffff0000 | (0xffff & 0x0001);
		btp_video_done_es[0]   = 0x00;
		btp_video_done_es[13]   = (nTag >> 24) & 0xff;
		btp_video_done_es[14]   = (nTag >> 16) & 0xff;
		btp_video_done_es[15]   = (nTag >> 8)  & 0xff;
		btp_video_done_es[16]   = nTag & 0xff;

		sts = DtsAlignSendData(hDevice, btp_video_done_es, sizeof(btp_video_done_es), 0, 0);
		Ctx->PESConvParams.m_bPESPrivData = false;
		Ctx->PESConvParams.m_pPESPrivData = NULL;
		Ctx->PESConvParams.m_bStuffing = false;
		Ctx->PESConvParams.m_nStuffingBytes = 0;

		sts = DtsAlignSendData(hDevice, pEOS, nEOSLen, 0, 0);
		sts = DtsAlignSendData(hDevice, pEOS, nEOSLen, 0, 0);
	}
#if 0
	/* NAREN the following code is only needed in I-Frame Only mode trick modes */
	/* This code is needed for all codecs */
	/* In I-frame only mode, if the the source filter ever gives a partial I-Frame in the input */
	/* then this code will push that partial I-Frame aside for the decoder to continue correctly */
	/* Needed mainly for error handling */
	/* Uncomment and condition this with I-Frame only mode when we do enable I-Frame only trick modes */
	Ctx->PESConvParams.m_bPESExtField = false;
	Ctx->PESConvParams.m_pPESExtField = NULL;
	//Send N-Marker
	if ((Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) ||
		(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_VC1))
	{
		Ctx->PESConvParams.m_bPESExtField = true;
		Ctx->PESConvParams.m_nPESExtLen = 2;
		Ctx->PESConvParams.m_pPESExtField = ExtData;
	}

	memcpy(Plunge, btp_video_plunge_es, sizeof(btp_video_plunge_es));
	Plunge[4] = 0x0a;
	sts = DtsAlignSendData(hDevice, Plunge, sizeof(btp_video_plunge_es), 0, 0);

	Plunge[4] = 0x09;
    Plunge[37]   = 0;
    Plunge[38]   = 0;
    Plunge[39]   = 0;
    Plunge[40]   = 1;
	sts = DtsAlignSendData(hDevice, Plunge, sizeof(btp_video_plunge_es), 0, 0);
#endif

	//Reset
	Ctx->PESConvParams.m_bPESExtField = false;
	Ctx->PESConvParams.m_pPESExtField = NULL;
	Ctx->PESConvParams.m_bPESPrivData = false;
	Ctx->PESConvParams.m_pPESPrivData = NULL;
	Ctx->PESConvParams.m_bStuffing = false;
	Ctx->PESConvParams.m_nStuffingBytes = 0;
	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	Ctx->PESConvParams.m_bSoftRaveEOS = false;

	return sts;
}

DRVIFLIB_API BC_STATUS
DtsFlushInput( HANDLE  hDevice, uint32_t Op)
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

#ifdef _CHECK_FLUSH_
	WCHAR fl[512];
	wsprintf(fl, L"Fl#%d\n", Op);
	OutputDebugString(fl);
#endif

	DTS_GET_CTX(hDevice,Ctx);
	if(Op == 0 || Op == 5) // DRAIN
	{
		Ctx->PESConvParams.m_bAddSpsPps = true;
#ifdef TX_CIRCULAR_BUFFER
		if ((Ctx->InSampleCount == 0) && (Ctx->nTxHoldBufRead == Ctx->nTxHoldBufWrite))
			return BC_STS_SUCCESS;
#else
		if ((Ctx->InSampleCount == 0) && (Ctx->nPendBufInd == 0))
			return BC_STS_SUCCESS;
#endif
		DtsSendEOS(hDevice, Op);
#ifdef TX_CIRCULAR_BUFFER
		while (Ctx->State == BC_DEC_STATE_START && Ctx->nTxHoldBufRead != Ctx->nTxHoldBufWrite)
		{
			if ((sts = DtsSendData(hDevice, 0, 0, 0, 0))== BC_STS_SUCCESS)
				break;
		}
		Ctx->nTxHoldBufRead = Ctx->nTxHoldBufWrite = 0;
#else
		if (Ctx->nPendBufInd)
			DtsSendData(hDevice, 0, 0, 0, 0);
		Ctx->nPendBufInd = 0;
#endif
		if (Ctx->SingleThreadedAppMode && Ctx->nPendFPBufInd)
		{
			DtsSendData(hDevice, 0, 0, 0, 0);
			Ctx->FPDrain = TRUE;
		}
		Ctx->FlushIssued = TRUE;
		if (!Ctx->SingleThreadedAppMode)
			bc_sleep_ms(50000);
	}
	else
	{
#ifdef TX_CIRCULAR_BUFFER
		Ctx->nTxHoldBufRead = Ctx->nTxHoldBufWrite = 0;
		Ctx->nTxHoldBufCounter = 0;
#else
		Ctx->nPendBufInd = 0;
#endif
		if (Ctx->InSampleCount == 0)
			return BC_STS_SUCCESS;
		Ctx->nPendFPBufInd = 0;
		Ctx->FPDrain = FALSE;
		bc_sleep_ms(30000); // For the cancel to take place in case we are looping
		sts = DtsCancelTxRequest(hDevice, Op);
		if((Op == 3) || (sts != BC_STS_SUCCESS))
		{
			return sts;
		}
		DtsClrPendMdataList(Ctx);
	}

	if(Op == 4)
		sts = DtsFWDecFlushChannel(hDevice,2);
	else if (Op != 0 && Op != 5)
		sts = DtsFWDecFlushChannel(hDevice,Op);

	if(sts != BC_STS_SUCCESS)
		return sts;

	/* Issue a flush to the driver RX every time we flush the decoder */
	if(Op != 0 && Op != 5) {
		sts = DtsFlushRxCapture(hDevice,true);
		if(sts != BC_STS_SUCCESS)
			return sts;
	}

	Ctx->LastPicNum = -1;
	Ctx->LastSessNum = -1;
	Ctx->eosCnt = 0;
	Ctx->bEOS = FALSE;
	Ctx->InSampleCount = 0;

	// Clear the saved cpb base and end for SingleThreadedAppMode
	if(Ctx->SingleThreadedAppMode) {
		Ctx->cpbBase = 0;
		Ctx->cpbEnd = 0;
	}
	Ctx->PESConvParams.m_lStartCodeDataSize = 0;

	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS
OldDtsProcInput( HANDLE  hDevice ,
				uint8_t *pUserData,
				uint32_t ulSizeInBytes,
				uint64_t timeStamp,
				BOOL encrypted)

{
	uint32_t					DramOff;
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_INPUT_MDATA		*im = NULL;
	uint8_t					BDPktflag=1;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP){
		uint32_t start_code=0x0;
		start_code = *(uint32_t *)(pUserData+pUserData[8]+9);
		
		/*In WMV ASF the PICTURE PAYLOAD can come in multiple PES PKTS as each payload pkt
		needs to be less than 64K. Filter sets NO_DISPLAY_PTS with the continuation pkts
		also, but only one SPES PKT needs to be inserted per picture. Only the first picture
		payload PES pkt will have the ASF header and rest of the continuation pkts do not
		have the ASF hdr. so, inser the SPES PKT only before the First picture payload pkt
		which has teh ASF hdr. The ASF HDr starts with teh magic number 0x5a5a5a5a.
		*/
		
		if(start_code !=0x5a5a5a5a){
			BDPktflag=0;
		}
	}

	if (timeStamp && BDPktflag){
		sts = OldDtsPrepareMdata(Ctx, timeStamp, &im);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_ERR,"DtsProcInput: Failed to Prepare Spes hdr:%x\n",sts);
			return sts;
		}

		uint8_t *temp=NULL;
		uint32_t sz=0;
		uint8_t AsfHdr[32+sizeof(BC_PES_HDR_FORMAT) + 4];

		if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP){
			temp = (uint8_t*) &AsfHdr;
			temp = (uint8_t*) ((uintptr_t)temp & ~0x3); /*Take care of alignment*/

			if(temp==NULL){
				DebugLog_Trace(LDIL_DBG,"DtsProcInput: Failed to alloc mem for  ASFHdr for SPES:%x\n",sts);
				return BC_STS_INSUFF_RES;
			}

			sts =DtsPrepareMdataASFHdr(Ctx, im, temp);
			if(sts != BC_STS_SUCCESS){
				DebugLog_Trace(LDIL_DBG,"DtsProcInput: Failed to Prepare ASFHdr for SPES:%x\n",sts);
				return sts;
			}
			
			sz = 32+sizeof(BC_PES_HDR_FORMAT);
		}else{
			temp = (uint8_t*)&im->Spes;
			sz = sizeof(im->Spes);
		}

		//sts = DtsTxDmaText(hDevice,(uint8_t*)&im->Spes,sizeof(im->Spes),&DramOff, encrypted);
		sts = DtsTxDmaText(hDevice, temp, sz, &DramOff, encrypted);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsProcInput: Failed to send Spes hdr:%x\n",sts);
			DtsFreeMdata(Ctx,im,TRUE);
			return sts;
		}

		sts = DtsInsertMdata(Ctx,im);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsProcInput: DtsInsertMdata failed\n",sts);
		}
	}
	
	sts = DtsTxDmaText(hDevice,pUserData,ulSizeInBytes,&DramOff, encrypted);

	if(sts == BC_STS_SUCCESS)
		DtsUpdateInStats(Ctx,ulSizeInBytes);

	return sts;
}

DRVIFLIB_API BC_STATUS 
OldDtsFlushInput( HANDLE  hDevice ,
				 uint32_t Op )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if (Op == 3) {
		return DtsCancelTxRequest(hDevice, Op);
	}

	if(Op == 0)
		bc_sleep_ms(100);
	sts = DtsFWDecFlushChannel(hDevice,Op);

	if(sts != BC_STS_SUCCESS)
		return sts;

	Ctx->LastPicNum = 0;
	Ctx->eosCnt = 0;

	if(Op == 0)//If mode is drain.
		Ctx->FlushIssued = TRUE;
	else
		DtsClrPendMdataList(Ctx);

	/* Clear the saved cpb base and end for SingleThreadedAppMode */
	if(Ctx->SingleThreadedAppMode) {
		Ctx->cpbBase = 0;
		Ctx->cpbEnd =0;
	}

	return BC_STS_SUCCESS;
}
DRVIFLIB_API BC_STATUS 
DtsSetRateChange(HANDLE  hDevice ,
				 uint32_t rate,
				 uint8_t direction )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	//For Rate Change
	uint32_t mode = 0;
	uint32_t HostTrickModeEnable = 0;

	//Change Rate Value for Version 1.1
	//Rate: Specifies the new rate x 10000
	//Rate is the inverse of speed. For example, if the playback speed is 2x, the rate is 1/2, so the Rate member is set to 5000.

	float fRate = float(1) / ((float)rate / (float)10000);

	//Mode Decision
	if(fRate < 1)
	{
		//Slow
		LONG Rate = LONG(float(1) / fRate);

		mode = eC011_SKIP_PIC_IPB_DECODE;
		HostTrickModeEnable = 1;

		sts = DtsFWSetHostTrickMode(hDevice,HostTrickModeEnable);
		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange:DtsFWSetHostTrickMode Failed\n");
			return sts;
		}

		sts = DtsFWSetSkipPictureMode(hDevice,mode);
		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: DtsFWSetSkipPictureMode Failed\n");
			return sts;
		}

		sts = DtsFWSetSlowMotionRate(hDevice, Rate);

		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Slow Forward Failed\n");

			return sts;
		}
	}
	else
	{
		//Fast
		LONG Rate = (LONG)fRate;

		//For I-Frame Only Trick Mode
		//Direction: 0: Forward, 1: Backward
		if((Rate == 1) && (direction == 0))
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Normal Speed\n");
			mode = eC011_SKIP_PIC_IPB_DECODE;
			HostTrickModeEnable = 0;			
			if(fRate > 1 && fRate < 2)
			{
				//Nav is giving I instead of IBP for 1.4x or 1.6x
				DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set 1.x I only\n");
				mode = eC011_SKIP_PIC_I_DECODE;
				HostTrickModeEnable = 1;		
			}
		}
		else
		{
			//I-Frame Only for Fast Forward and All Speed Backward
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Very Fast Speed for I-Frame Only\n");
			mode = eC011_SKIP_PIC_I_DECODE;
			HostTrickModeEnable = 1;			
		}

		sts = DtsFWSetHostTrickMode(hDevice,HostTrickModeEnable);
		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange:DtsFWSetHostTrickMode Failed\n");
			return sts;
		}

		sts = DtsFWSetSkipPictureMode(hDevice,mode);
		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: DtsFWSetSkipPictureMode Failed\n");
			return sts;
		}

		sts = DtsFWSetFFRate(hDevice, Rate);
		if(sts != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Fast Forward Failed\n");
			return sts;
		}
	}

	return BC_STS_SUCCESS;
}

//Set FF Rate for Catching Up
DRVIFLIB_API BC_STATUS 
DtsSetFFRate(HANDLE  hDevice ,
				 uint32_t Rate)
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	//For Rate Change
	uint32_t mode = 0;
	uint32_t HostTrickModeEnable = 0;
	

	//IPB Mode for Catching Up
	mode = eC011_SKIP_PIC_IPB_DECODE;

	if(Rate == 1)
	{
		//Normal Mode
		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate: Set Normal Speed\n");
		
		HostTrickModeEnable = 0;
	}
	else
	{
		//Fast Forward
		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate: Set Fast Forward\n");

		HostTrickModeEnable = 1;			
	}

	sts = DtsFWSetHostTrickMode(hDevice, HostTrickModeEnable);
	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate:DtsFWSetHostTrickMode Failed\n");
		return sts;
	}

	sts = DtsFWSetSkipPictureMode(hDevice, mode);
	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate: DtsFWSetSkipPictureMode Failed\n");
		return sts;
	}

	sts = DtsFWSetFFRate(hDevice, Rate);
	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate: Set Fast Forward Failed\n");
		return sts;
	}


	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS 
DtsSetSkipPictureMode( HANDLE  hDevice ,
						uint32_t SkipMode )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	sts = DtsFWSetSkipPictureMode(hDevice,SkipMode);

	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetSkipPictureMode: Set Picture Mode Failed, %d\n",SkipMode);	
		return sts;
	}

	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS 
	DtsSetIFrameTrickMode(
	HANDLE  hDevice
	)
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	sts = DtsFWSetHostTrickMode(hDevice,1);
	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetIFrameTrickMode: DtsFWSetHostTrickMode Failed\n");
		return sts;
	}

	sts = DtsFWSetSkipPictureMode(hDevice,eC011_SKIP_PIC_I_DECODE);
	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsSetIFrameTrickMode: DtsFWSetSkipPictureMode Failed\n");
		return sts;
	}
	return BC_STS_SUCCESS;
}


DRVIFLIB_API BC_STATUS 
	DtsStepDecoder(
    HANDLE  hDevice	
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if( Ctx->State != BC_DEC_STATE_PAUSE ) {
		DebugLog_Trace(LDIL_DBG,"DtsStepDecoder: Failed because Decoder is Not Paused\n");
		return BC_STS_ERR_USAGE;
	}

	sts = DtsFWFrameAdvance(hDevice);	
	if(sts != BC_STS_SUCCESS )
	{
		DebugLog_Trace(LDIL_DBG,"DtsStepDecoder: Failed \n");
		return sts;
	}

	return sts;
}


DRVIFLIB_API BC_STATUS 
	DtsIs422Supported(
    HANDLE  hDevice,
	uint8_t*		bSupported
    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->DevId == BC_PCI_DEVID_LINK)
	{
		*bSupported = 1;
	}
	else
	{
		*bSupported = 0;
	}
	return sts;
}

DRVIFLIB_API BC_STATUS 
DtsSet422Mode(HANDLE hDevice, uint8_t Mode422)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->DevId != BC_PCI_DEVID_LINK)
	{
		return sts;
	}

	Ctx->b422Mode = (BC_OUTPUT_FORMAT)Mode422;

	sts = DtsSetLinkIn422Mode(hDevice);

	return sts;
}

DRVIFLIB_API BC_STATUS
DtsGetDILPath(
    HANDLE  hDevice,
	char		*DilPath,
	uint32_t		size
    )
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	uint32_t *ptemp=NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if(!DilPath || (size < sizeof(Ctx->DilPath)) ){
		return BC_STS_INV_ARG;
	}

	/* if the first 4 bytes are zero, then the dil path in the context
	is not yet set. Hence go ahead and look at registry and update the
	context. and then just copy the dil path from context.
	*/
	ptemp = (uint32_t*)&Ctx->DilPath[0];
	if(!(*ptemp))
		DtsGetFirmwareFiles(Ctx);

	strncpy(DilPath, Ctx->DilPath, sizeof(Ctx->DilPath));


	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS 
DtsDropPictures( HANDLE  hDevice ,
                uint32_t Pictures )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	sts = DtsFWDrop(hDevice,Pictures);

	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsDropPictures: Set Picture Mode Failed, %d\n",Pictures);	
		return sts;
	}

	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS 
DtsGetDriverStatus( HANDLE  hDevice,
	                BC_DTS_STATUS *pStatus)
{
    BC_DTS_STATS temp;
    BC_STATUS ret;
    DTS_LIB_CONTEXT *Ctx = NULL;
    uint64_t NextTimeStamp = 0;

    DTS_GET_CTX(hDevice,Ctx);

    if (Ctx->SingleThreadedAppMode)
        temp.DrvNextMDataPLD = Ctx->picWidth | (0x1 << 31);

    ret = DtsGetDrvStat(hDevice, &temp);

    pStatus->FreeListCount      = temp.drvFLL;
    pStatus->ReadyListCount     = temp.drvRLL;
    pStatus->FramesCaptured     = temp.opFrameCaptured;
    pStatus->FramesDropped      = temp.opFrameDropped;
    pStatus->FramesRepeated     = temp.reptdFrames;
    pStatus->PIBMissCount       = temp.pibMisses;
    pStatus->InputCount         = temp.ipSampleCnt;
    pStatus->InputBusyCount     = temp.TxFifoBsyCnt;
    pStatus->InputTotalSize     = temp.ipTotalSize;
    pStatus->cpbEmptySize       = temp.DrvcpbEmptySize;

    pStatus->PowerStateChange   = temp.pwr_state_change;

    /* return the timestamp of the next picture to be returned by ProcOutput */
    if((pStatus->ReadyListCount > 0) && Ctx->SingleThreadedAppMode) {
        DtsFetchTimeStampMdata(Ctx, ((temp.DrvNextMDataPLD & 0xFF) << 8) | ((temp.DrvNextMDataPLD & 0xFF00) >> 8), &NextTimeStamp);
        pStatus->NextTimeStamp = NextTimeStamp;
    } else {
        pStatus->NextTimeStamp = 0;
    }

	return ret;
}
