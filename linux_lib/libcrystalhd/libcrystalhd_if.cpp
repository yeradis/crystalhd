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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "7411d.h"
#include "libcrystalhd_version.h"
#include "bc_decoder_regs.h"
#include "libcrystalhd_if.h"
#include "libcrystalhd_priv.h"
#include "libcrystalhd_int_if.h"
#include "libcrystalhd_fwcmds.h"
#include "libcrystalhd_fwload_if.h"

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

static __attribute__((aligned(4))) uint8_t ExtData[] =
{ 0x00, 0x00};


BC_STATUS DtsSetupHardware(HANDLE hDevice, BOOL IgnClkChk)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT *Ctx;

	DTS_GET_CTX(hDevice,Ctx);

	if( !IgnClkChk){
		if(Ctx->DevId == BC_PCI_DEVID_LINK || Ctx->DevId == BC_PCI_DEVID_FLEA){
			if(DtsGetHwInitSts() != BC_DIL_HWINIT_NOT_YET){
				DebugLog_Trace(LDIL_DBG," HW init already?\n");
				return BC_STS_SUCCESS;
			}
		}
	}

	DtsSetHwInitSts(BC_DIL_HWINIT_IN_PROGRESS);

    if (Ctx->DevId == BC_PCI_DEVID_LINK)
        sts = DtsPushAuthFwToLink(hDevice,NULL);
	else if (Ctx->DevId == BC_PCI_DEVID_FLEA)
		sts = DtsPushFwToFlea(hDevice,NULL);
	else {
		DebugLog_Trace(LDIL_DBG,"HW Type not found\n");
		return BC_STS_ERROR;
	}

	if(sts != BC_STS_SUCCESS){
		return sts;
	}

	/* Initialize Firmware interface */
	sts = DtsFWInitialize(hDevice,0);

	if (sts == BC_STS_SUCCESS)
		DtsSetHwInitSts(BC_DIL_HWINIT_DONE);
	else
		DtsSetHwInitSts(BC_DIL_HWINIT_NOT_YET);

	return sts;
}

static BC_STATUS DtsReleaseChannel(HANDLE  hDevice, uint32_t ChannelID, bool Stop)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	if(Stop){
		sts = DtsFWStopVideo(hDevice, ChannelID, true);
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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		sts = DtsFWOpenChannel(hDevice, StreamType, 0);
		if(sts == BC_STS_SUCCESS)
		{
			/*For Multiapplication support this will change.*/
			if(Ctx->OpenRsp.channelId != 0)
				sts = BC_STS_FW_CMD_ERR;
		}
		return sts;
	}

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
extern BOOL glob_mode_valid;

DRVIFLIB_API BC_STATUS
DtsDeviceOpen(
    HANDLE	*hDevice,
	uint32_t		mode
    )
{
	int 		drvHandle=1;
	BC_STATUS	Sts=BC_STS_SUCCESS;
	uint32_t globMode = 0;
	uint8_t	nTry=1;
	uint32_t		VendorID, DeviceID, RevID, FixFlags, drvMode;
	uint32_t		drvVer, dilVer;
	uint32_t		fwVer, decVer, hwVer;
	pid_t	processID;
	int shmid=0;

	DebugLog_Trace(LDIL_DBG,"Running DIL (%d.%d.%d) Version\n",
		DIL_MAJOR_VERSION,DIL_MINOR_VERSION,DIL_REVISION );

	processID = getpid();

	FixFlags = mode;
	mode &= 0xFF;

	Sts = DtsCreateShMem(&shmid);
	if(BC_STS_SUCCESS !=Sts)
		return Sts;

	if (mode != DTS_MONITOR_MODE && DtsIsDecOpened(processID))
	{
		DebugLog_Trace(LDIL_DBG, "DtsDeviceOpen: Decoder is already opened\n");
		DtsDelDilShMem();
		return BC_STS_DEC_EXIST_OPEN;
	}

	DebugLog_Trace(LDIL_DBG,"DtsDeviceOpen: Opening HW in mode %x\n", mode);

	/* For External API case, we support only Plyaback mode. */
	if( !(BC_DTS_DEF_CFG & BC_EN_DIAG_MODE) && (mode != DTS_PLAYBACK_MODE) ){
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: mode %d not supported\n",mode);
		DtsDelDilShMem();
		return BC_STS_INV_ARG;
	}

	if(!glob_mode_valid) {
		globMode = DtsGetOPMode();
		if(globMode&4) {
			globMode&=4;
		}
		DebugLog_Trace(LDIL_DBG,"DtsDeviceOpen: New globmode is %d \n",globMode);
	}
	else{
		globMode = DtsGetOPMode();
	}

	if (mode == DTS_HWINIT_MODE)
		DtsSetHwInitSts(BC_DIL_HWINIT_IN_PROGRESS);

	drvHandle =open(CRYSTALHD_API_DEV_NAME, O_RDWR);
	if(drvHandle < 0)
	{
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: Create File Failed\n");
		DtsDelDilShMem();
		return BC_STS_ERROR;
	}


	/* Initialize Internal Driver interfaces.. */
	if( (Sts = DtsInitInterface(drvHandle,hDevice, mode)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_ERR,"DtsDeviceOpen: Interface Init Failed:%x\n",Sts);
		DtsReleaseInterface(DtsGetContext(*hDevice));
		DtsDelDilShMem();
		return Sts;
	}
	if( (Sts = DtsGetHwType(*hDevice,&DeviceID,&VendorID,&RevID))!=BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"Get Hardware Type Failed\n");
		DtsReleaseInterface(DtsGetContext(*hDevice));
		DtsDelDilShMem();
		return Sts;
	}

	// set Ctx->DevId early, other depend on it
	DtsGetContext(*hDevice)->DevId = DeviceID;
	DtsSetgDevID(DeviceID);

	/*
	 * Old layout link cards have issues w/a core clock of 200, so we use
	 * 180 for all link cards, as we have no way to tell old layout from
	 * new layout cards.
	 */
	DtsSetCoreClock(*hDevice, 180);

	/*
	 * We have to specify the mode to the driver.
	 * So the driver can cleanup only in case of
	 * playback/Diag mode application close.
	 */
	if ((Sts = DtsGetVersion(*hDevice, &drvVer, &dilVer)) != BC_STS_SUCCESS) {
		DebugLog_Trace(LDIL_DBG,"Get drv ver failed\n");
		DtsReleaseInterface(DtsGetContext(*hDevice));
		DtsDelDilShMem();
		return Sts;
	}
	/* If driver minor version is more than 13, enable DTS_SKIP_TX_CHK_CPB feature */
	if (FixFlags & DTS_SKIP_TX_CHK_CPB) {
		if (((drvVer >> 16) & 0xFF) > 13)
			FixFlags |= DTS_SKIP_TX_CHK_CPB;
	}

	if (FixFlags & DTS_ADAPTIVE_OUTPUT_PER) {
        if(DtsGetContext(*hDevice)->DevId == BC_PCI_DEVID_FLEA)
            Sts = DtsGetFWVersion(*hDevice, &fwVer, &decVer, &hwVer, (char*)FWBINFILE_70015, 0);
        else
            Sts = DtsGetFWVersion(*hDevice, &fwVer, &decVer, &hwVer, (char*)FWBINFILE_70012, 0);
        if(Sts == BC_STS_SUCCESS) {
			if (fwVer >= ((14 << 16) | (8 << 8) | (1)))		// 2.14.8.1 (ignore 2)
				FixFlags |= DTS_ADAPTIVE_OUTPUT_PER;
			else
				FixFlags &= (~DTS_ADAPTIVE_OUTPUT_PER);
        }
	}

	/* only enable dropping of repeated pictures for Adobe mode and MFT mode */
	/* since we see corrupted video is some cases for non Adobe usage */
	/* NAREN - This is a major hack since we have not root caused the corrupted video */
	if(((FixFlags & DTS_SINGLE_THREADED_MODE) || (FixFlags & DTS_MFT_MODE)) && !(FixFlags & DTS_DIAG_TEST_MODE))
		FixFlags |= DTS_PLAYBACK_DROP_RPT_MODE;

	/* If driver minor version is newer (for major == 2), send the fullMode */
	if (((drvVer >> 24) & 0xFF) == 2) {
		if (((drvVer >> 16) & 0xFF) > 10) drvMode = FixFlags;
		else drvMode = mode;
	}
	else
		drvMode = FixFlags;

	if( (Sts = DtsNotifyOperatingMode(*hDevice,drvMode)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"Notify Operating Mode Failed\n");
		DtsReleaseInterface(DtsGetContext(*hDevice));
		DtsDelDilShMem();
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

	if (DeviceID == BC_PCI_DEVID_LINK || DeviceID == BC_PCI_DEVID_FLEA)
		nTry = HARDWARE_INIT_RETRY_LINK_CNT;
	else
		nTry = HARDWARE_INIT_RETRY_CNT;

	if((mode == DTS_PLAYBACK_MODE)||(mode == DTS_HWINIT_MODE))
	{
		while(nTry--)
		{
			Sts = 	DtsSetupHardware(*hDevice, FALSE);
			if(Sts == BC_STS_SUCCESS)
			{
				break;
			}
			else
			{
				DebugLog_Trace(LDIL_DBG,"DtsSetupHardware: Failed from Open\n");
				bc_sleep_ms(100);
			}
		}
		if(Sts != BC_STS_SUCCESS )
		{
			DtsReleaseInterface(DtsGetContext(*hDevice));
			DtsDelDilShMem();
			goto exit;
		}
	}

	if(mode == DTS_HWINIT_MODE){
		DtsSetHwInitSts(0);
	}

	// Clear all stats before we start play back
	if(mode == DTS_PLAYBACK_MODE) {
		DtsRstDrvStat(*hDevice);
	}

	DtsGetContext(*hDevice)->ProcessID = processID;

	//DtsDevRegisterWr( hDevice,UartSelectA, 3);
exit:
	return Sts;
}

DRVIFLIB_API BC_STATUS
DtsDeviceClose(
    HANDLE hDevice
    )
{
	DTS_LIB_CONTEXT		*Ctx;
	uint32_t globMode = 0;

	if(hDevice == NULL)
		return BC_STS_SUCCESS;

	DTS_GET_CTX(hDevice,Ctx);

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if(Ctx->State != BC_DEC_STATE_CLOSE){
		DtsCloseDecoder(hDevice);
	}

	DtsCancelFetchOutInt(Ctx);
	/* Unmask the mode */
	globMode = DtsGetOPMode( );

	// Make sure we are in playback mode before freeing up playback resources
	if(Ctx->OpMode == DTS_PLAYBACK_MODE)
	{
		DtsFlushRxCapture(hDevice,false); // Make sure that all buffers and DMA engines are freed up
	}

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
	DtsReleasePESConverter(hDevice);

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
    HANDLE    hDevice,
    uint32_t  *StreamVer,
	uint32_t  *DecVer,
	char      *fname
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
        if(Ctx->DevId == BC_PCI_DEVID_FLEA)
            strncat(fwfile,FWBINFILE_70015,sizeof(FWBINFILE_70015));
        else
            strncat(fwfile,FWBINFILE_70012,sizeof(FWBINFILE_70012));
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
	/* Read the FW bin file */
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

	//There is 16k hole in the FW binary. Hnece start searching from 16k in the bin file
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
DtsOpenDecoder(
    HANDLE    hDevice,
    uint32_t  StreamType)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT	*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->State != BC_DEC_STATE_CLOSE)
	{
		if((sts = DtsCloseDecoder(hDevice)) != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG, "DtsOpenDecoder: DtsCloseDecoder Failed (sts:%d)\n", sts);
			return sts;
		}
	}

	Ctx->LastPicNum = -1;
	Ctx->LastSessNum = -1;
	Ctx->EOSCnt = 0;
	Ctx->DrvStatusEOSCnt = 0;
	Ctx->bEOSCheck = false;
	Ctx->bEOS = false;
	Ctx->CapState = 0;
	Ctx->hw_paused = false;
	Ctx->fw_cmd_issued = false;

	sts = DtsSetVideoClock(hDevice,0);
	if (sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"Failed to Set Video Clock:%x\n",sts);
		return sts;
	}

	if(Ctx->DevId == BC_PCI_DEVID_LINK)
	{
		// FIX_ME to support other stream types.
		sts = DtsSetTSMode(hDevice,0);
		if(sts != BC_STS_SUCCESS )
		{
			return sts;
		}
	}

	if (Ctx->VidParams.MediaSubType != BC_MSUBTYPE_INVALID)
		StreamType = Ctx->VidParams.StreamType;
	else if (Ctx->DevId == BC_PCI_DEVID_FLEA)
		StreamType = BC_STREAM_TYPE_PES;

	sts = DtsRecoverableDecOpen(hDevice,StreamType);
	if(sts != BC_STS_SUCCESS )
	{
		DebugLog_Trace(LDIL_DBG,"Dts Recoverable Open Failed:%x\n",sts);
		return sts;
	}

	sts = DtsFWSetVideoInput(hDevice);
	if(sts != BC_STS_SUCCESS )
	{
		DebugLog_Trace(LDIL_DBG,"DtsFWSetVideoInput Failed:%x\n",sts);
		return sts;
	}

	Ctx->State = BC_DEC_STATE_STOP;

	DtsSetDecStat(true, Ctx->ProcessID);

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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE) {
		DebugLog_Trace(LDIL_DBG,"DtsStartDecoder: Decoder is not opened\n");
		return BC_STS_DEC_NOT_OPEN;
	}

	if( Ctx->State == BC_DEC_STATE_START)
	{
		if ((sts = DtsStopDecoder(hDevice)) != BC_STS_SUCCESS)
		{
			DebugLog_Trace(LDIL_DBG, "DtsStartDecoder: DtsStopDecoder FAILED (sts:%d)\n", sts);
			return sts;
		}
	}

	if(Ctx->VidParams.Progressive){
		sts = DtsSetProgressive(hDevice,0);
		if(sts != BC_STS_SUCCESS ) {
			DebugLog_Trace(LDIL_DBG,"DtsSetProgressive: Failed [%x]\n",sts);
			return sts;
		}
	}

	sts = DtsFWActivateDecoder(hDevice);
	if(sts != BC_STS_SUCCESS ) {
		DebugLog_Trace(LDIL_DBG,"DtsFWActivateDecoder: Failed [%x]\n",sts);
		return sts;
	}

	sts = DtsFWStartVideo(hDevice,
							Ctx->VidParams.VideoAlgo,
							Ctx->VidParams.FGTEnable,
							Ctx->VidParams.MetaDataEnable,
							Ctx->VidParams.Progressive,
							Ctx->VidParams.OptFlags);
	if(sts != BC_STS_SUCCESS) {
		DebugLog_Trace(LDIL_DBG,"DtsFWStartVideo: Failed [%x]\n",sts);
		return sts;
	}

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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if(Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_SUCCESS;
	}

	if (Ctx->State != BC_DEC_STATE_STOP)
	{
		DtsStopDecoder(hDevice);
	}


	sts = DtsFWCloseChannel(hDevice,Ctx->OpenRsp.channelId);

	/*if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}*/

	DtsSetDecStat(false, Ctx->ProcessID);

	Ctx->State = BC_DEC_STATE_CLOSE;

	Ctx->LastPicNum = -1;
	Ctx->LastSessNum = -1;

	Ctx->EOSCnt = 0;
	Ctx->DrvStatusEOSCnt = 0;
	Ctx->bEOSCheck = false;
	Ctx->bEOS = false;

//	Ctx->InSampleCount = 0;

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

	Ctx->VidParams.VideoAlgo = videoAlg;
	Ctx->VidParams.FGTEnable = FGTEnable;
	Ctx->VidParams.MetaDataEnable = MetaDataEnable;
	Ctx->VidParams.Progressive = Progressive;
	//Ctx->VidParams.Reserved = rsrv;
	//Ctx->VidParams.FrameRate = FrameRate;
	Ctx->VidParams.OptFlags = OptFlags;
	// SingleThreadedAppMode is bit 7 of OptFlags
	if(OptFlags & 0x80) {
		Ctx->SingleThreadedAppMode = 1;
	// Hard code BD mode as well as max frame rate mode for Link
		Ctx->VidParams.OptFlags |= 0xD1;
	}
	else
		Ctx->SingleThreadedAppMode = 0;
	if (Ctx->DevId == BC_PCI_DEVID_FLEA || Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_PES;
	else
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_ES;
	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS
DtsSetInputFormat(
    HANDLE hDevice,
    BC_INPUT_FORMAT *pInputFormat
    )
{
	DTS_LIB_CONTEXT *Ctx = NULL;
	uint32_t videoAlgo = BC_VID_ALGO_H264;
	uint32_t	ScaledWidth = 0;

	DTS_GET_CTX(hDevice,Ctx);

	Ctx->VidParams.MediaSubType = pInputFormat->mSubtype;
	Ctx->VidParams.WidthInPixels = pInputFormat->width;
	Ctx->VidParams.HeightInPixels = pInputFormat->height;
	if (pInputFormat->startCodeSz)
		Ctx->VidParams.StartCodeSz = pInputFormat->startCodeSz;
	else
		Ctx->VidParams.StartCodeSz = BRCM_START_CODE_SIZE;

	if (pInputFormat->metaDataSz)
	{
		if(Ctx->VidParams.pMetaData){
			DebugLog_Trace(LDIL_DBG,"deleting buffer\n");
			free(Ctx->VidParams.pMetaData);
		}
		Ctx->VidParams.pMetaData = (uint8_t*)malloc(pInputFormat->metaDataSz);
		memcpy(Ctx->VidParams.pMetaData, pInputFormat->pMetaData, pInputFormat->metaDataSz);

		Ctx->VidParams.MetaDataSz = pInputFormat->metaDataSz;
	}

	if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_H264 || Ctx->VidParams.MediaSubType== BC_MSUBTYPE_AVC1)
	{
		videoAlgo = BC_VID_ALGO_H264;
	}
	else if (Ctx->VidParams.MediaSubType==BC_MSUBTYPE_DIVX)
	{
		videoAlgo = BC_VID_ALGO_DIVX;
	}
	else if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_MPEG2VIDEO )
	{
		videoAlgo = BC_VID_ALGO_MPEG2;
	}
	else if(Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1 || Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMVA ||Ctx->VidParams.MediaSubType == BC_MSUBTYPE_VC1)
	{
		videoAlgo = BC_VID_ALGO_VC1;
	}
	else  if (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3)
	{
		videoAlgo = BC_VID_ALGO_VC1MP;	// Main Profile
	}

	if (Ctx->DevId == BC_PCI_DEVID_FLEA || Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3)
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_PES;
	else
		Ctx->VidParams.StreamType = BC_STREAM_TYPE_ES;

	DtsSetVideoParams(hDevice, videoAlgo, pInputFormat->FGTEnable, pInputFormat->MetaDataEnable, pInputFormat->Progressive, pInputFormat->OptFlags);
	DtsSetPESConverter(hDevice);

	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		if(Ctx->SingleThreadedAppMode) {
			pInputFormat->bEnableScaling = true;
			pInputFormat->ScalingParams.sWidth = 1280;
		}
		if(pInputFormat->bEnableScaling) {
			if((pInputFormat->ScalingParams.sWidth > 1920)||
			   (pInputFormat->ScalingParams.sWidth < 128))
				ScaledWidth = 1280;
			else
				ScaledWidth = pInputFormat->ScalingParams.sWidth;

			Ctx->EnableScaling = (ScaledWidth << 20) | (ScaledWidth << 8) |
					     pInputFormat->bEnableScaling;
		} else {
			Ctx->EnableScaling = 0;
		}

		Ctx->bEnable720pDropHalf = 0;
	}

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

	*videoAlg = Ctx->VidParams.VideoAlgo;
	*FGTEnable = Ctx->VidParams.FGTEnable;
	*MetaDataEnable = Ctx->VidParams.MetaDataEnable;
	*Progressive = Ctx->VidParams.Progressive;

	 return BC_STS_SUCCESS;

}

DRVIFLIB_API BC_STATUS
DtsFormatChange(
    HANDLE  hDevice,
	uint32_t videoAlg,
	BOOL	FGTEnable,
	BOOL	MetaDataEnable,
	BOOL	Progressive,
    uint32_t rsrv
	)
{
	return BC_STS_NOT_IMPL;
}

DRVIFLIB_API BC_STATUS
DtsStopDecoder(
    HANDLE  hDevice
	)
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE || Ctx->State == BC_DEC_STATE_STOP)
	{
		return BC_STS_SUCCESS;
	}

	// On LINK if the decoder is paused due to the RLL being full, un pause it before flush
	if(Ctx->DevId == BC_PCI_DEVID_LINK && Ctx->hw_paused) {
		DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_OFF);
		Ctx->hw_paused = false;
	}
	DtsCancelFetchOutInt(Ctx);

	sts = DtsFWStopVideo(hDevice,Ctx->OpenRsp.channelId, FALSE);

	sts = DtsFlushRxCapture(hDevice, false);

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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		DebugLog_Trace(LDIL_DBG, "DtsPauseDecoder: Decoder is not opened\n");
		return BC_STS_DEC_NOT_OPEN;
	}
	if (Ctx->State == BC_DEC_STATE_STOP)
	{
		DebugLog_Trace(LDIL_DBG, "DtsPauseDecoder: Decoder is not started\n");
		return BC_STS_DEC_NOT_STARTED;
	}

	if( Ctx->State == BC_DEC_STATE_PAUSE || Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		Ctx->State = BC_DEC_STATE_PAUSE;
		return BC_STS_SUCCESS;
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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		DebugLog_Trace(LDIL_DBG, "DtsResumeDecoder: Decoder is not opened\n");
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP)
	{
		DebugLog_Trace(LDIL_DBG, "DtsResumeDecoder: Decoder is not started\n");
		return BC_STS_DEC_NOT_STARTED;
	}

	if(Ctx->State == BC_DEC_STATE_START || Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		Ctx->State = BC_DEC_STATE_START;
		return BC_STS_SUCCESS;
	}

	sts = DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_OFF);
	if(sts != BC_STS_SUCCESS )
	{
		return sts;
	}

	Ctx->State = BC_DEC_STATE_START;

	return sts;
}


DRVIFLIB_API BC_STATUS
DtsSetVideoPID(
    HANDLE hDevice,
	uint32_t pid
    )
{
	return BC_STS_NOT_IMPL;
}


DRVIFLIB_API BC_STATUS
DtsStartCaptureImmidiate(HANDLE		hDevice,
						 uint32_t   Reserved)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_IOCTL_DATA		*pIocData = NULL;
	BC_STATUS			sts = BC_STS_SUCCESS;
	//unused uint32_t			Sz=0;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->State != BC_DEC_STATE_START)
	{
		DebugLog_Trace(LDIL_DBG, "DtsStartCaptureImmidiate: Decoder is not started\n");
		return BC_STS_DEC_NOT_STARTED;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	if(Ctx->CfgFlags & BC_ADDBUFF_MOVE){
		sts = DtsMapYUVBuffs(Ctx);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsMapYUVBuffs failed Sts:%d\n",sts);
			return sts;
		}
	}

//	DebugLog_Trace(LDIL_DBG,"DbgOptions=%x\n", Ctx->RegCfg.DbgOptions);

	pIocData->u.RxCap.Rsrd = ST_CAP_IMMIDIATE;
	pIocData->u.RxCap.StartDeliveryThsh = RX_START_DELIVERY_THRESHOLD;
	pIocData->u.RxCap.PauseThsh = PAUSE_DECODER_THRESHOLD;
	pIocData->u.RxCap.ResumeThsh = 	RESUME_DECODER_THRESHOLD;

	// NAREN for Flea change the values dynamically for pause and resume
	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		pIocData->u.RxCap.PauseThsh = Ctx->MpoolCnt - 2;
		pIocData->u.RxCap.ResumeThsh = 	FLEA_RT_PU_THRESHOLD;
	}

	if( (sts=DtsDrvCmd(Ctx,BCM_IOC_START_RX_CAP,0,pIocData,FALSE)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsStartCapture: Failed: %d\n",sts);
	}

	DtsRelIoctlData(Ctx,pIocData);

	return sts;
}

DRVIFLIB_API BC_STATUS
DtsStartCapture(HANDLE  hDevice)
{
	DTS_LIB_CONTEXT		*Ctx = NULL;
	BC_IOCTL_DATA *pIocData = NULL;
	BC_STATUS	sts = BC_STS_SUCCESS;
	//unused uint32_t	Sz=0;

	DTS_GET_CTX(hDevice,Ctx);

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State != BC_DEC_STATE_START)
	{
		DebugLog_Trace(LDIL_DBG, "DtsStartCapture: Decoder is not started\n");
		return BC_STS_DEC_NOT_STARTED;
	}

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	if(Ctx->CfgFlags & BC_ADDBUFF_MOVE){
		sts = DtsMapYUVBuffs(Ctx);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsMapYUVBuffs failed Sts:%d\n",sts);
			return sts;
		}
	}

//	DebugLog_Trace(LDIL_DBG,"DbgOptions=%x\n", Ctx->RegCfg.DbgOptions);

	pIocData->u.RxCap.Rsrd = NO_PARAM;
	pIocData->u.RxCap.StartDeliveryThsh = RX_START_DELIVERY_THRESHOLD;
	pIocData->u.RxCap.PauseThsh = PAUSE_DECODER_THRESHOLD;
	pIocData->u.RxCap.ResumeThsh = 	RESUME_DECODER_THRESHOLD;

	// NAREN for Flea change the values dynamically for pause and resume
	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		pIocData->u.RxCap.PauseThsh = Ctx->MpoolCnt - 2;
		pIocData->u.RxCap.ResumeThsh = 	FLEA_RT_PU_THRESHOLD;
	}

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

	if (!Ctx->bMapOutBufDone)
	{
		return BC_STS_SUCCESS;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	pIocData->u.FlushRxCap.bDiscardOnly = bDiscardOnly;
	Sts = DtsDrvCmd(Ctx,BCM_IOC_FLUSH_RX_CAP, 0, pIocData, TRUE);
	//If it is not Discard Only, RX Buffer will be un-mapped
	if(bDiscardOnly == false)
	{
		Ctx->bMapOutBufDone = false;
    }

	return Sts;
}


DRVIFLIB_INT_API BC_STATUS
DtsCancelTxRequest(
	HANDLE	hDevice,
	uint32_t Operation)
{
	return BC_STS_SUCCESS; // Since we always check before TX, there can never be a TX holding   in the Driver. FIXME
}


DRVIFLIB_API BC_STATUS
DtsProcOutput(
    HANDLE  hDevice,
	uint32_t milliSecWait,
	BC_DTS_PROC_OUT *pOut)
{
	BC_STATUS	stRel,sts = BC_STS_SUCCESS;
	BC_DTS_PROC_OUT OutBuffs;
	uint32_t	width=0, savFlags=0;

	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if(!pOut){
		DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Invalid Arg!!\n");
		return BC_STS_INV_ARG;
	}

	if(!(Ctx->FixFlags & DTS_LOAD_FILE_PLAY_FW)){
		if(!(Ctx->RegCfg.DbgOptions & BC_BIT(6))){
			DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Use NoCopy Interface for PIB encryption scheme\n");
			return BC_STS_ERR_USAGE;
		}
	}

	savFlags = pOut->PoutFlags;
	pOut->discCnt = 0;

	do
	{
		memset(&OutBuffs,0,sizeof(OutBuffs));

		sts = DtsFetchOutInterruptible(Ctx,&OutBuffs,milliSecWait);

		if(sts != BC_STS_SUCCESS)
		{
			if(sts == BC_STS_TIMEOUT)
			{
				if (Ctx->bEOSCheck == true && Ctx->bEOS == false)
				{
					if(milliSecWait)
						Ctx->EOSCnt = BC_EOS_PIC_COUNT;
					else
						Ctx->EOSCnt ++;

					if(Ctx->EOSCnt >= BC_EOS_PIC_COUNT)
					{
						/* Mark this picture as end of stream..*/
						pOut->PicInfo.flags |= VDEC_FLAG_LAST_PICTURE;
						Ctx->bEOS = TRUE;
						DebugLog_Trace(LDIL_DBG,"HIT EOS with counter\n");
					}
				}

				sts = BC_STS_NO_DATA;
			}
			// Have to make sure EOS is returned correctly for FLEA
			// In case of Flea the EOS picture has no data and hence the status will be STS_NO_DATA
			if(OutBuffs.PicInfo.flags & VDEC_FLAG_EOS)
				pOut->PicInfo.flags |= (VDEC_FLAG_EOS|VDEC_FLAG_LAST_PICTURE);
			DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,TRUE);
			return sts;
		}

		Ctx->bEOS = FALSE;
		pOut->PoutFlags |= OutBuffs.PoutFlags;
		/* Copying the discontinuity count */
		if(OutBuffs.discCnt)
			pOut->discCnt = OutBuffs.discCnt;

		if(pOut->PoutFlags & BC_POUT_FLAGS_FMT_CHANGE){
			if(pOut->PoutFlags & BC_POUT_FLAGS_PIB_VALID){
				pOut->PicInfo = OutBuffs.PicInfo;
			}

			/* Update Counters */
			DtsUpdateOutStats(Ctx,pOut);
			DtsUpdateVidParams(Ctx, pOut);

			DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,true);

			return BC_STS_FMT_CHANGE;
		}

		if (Ctx->DevId == BC_PCI_DEVID_FLEA)
		{
			if (Ctx->bEOSCheck == true && Ctx->bEOS == true && (OutBuffs.PicInfo.flags & VDEC_FLAG_EOS))
			{
				Ctx->bEOS = true;
				pOut->PicInfo.flags |= (VDEC_FLAG_LAST_PICTURE|VDEC_FLAG_EOS);
				DebugLog_Trace(LDIL_DBG,"HIT EOS with PIB tag\n");
			}
		}
		else
		{
			if (DtsCheckRptPic(Ctx, &OutBuffs) == true)
			{
				DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);
				return BC_STS_NO_DATA;
			}
		}

		if(pOut->DropFrames)
		{
			/* We need to release the buffers even if we fail to copy..*/
			stRel = DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);

			if(sts != BC_STS_SUCCESS)
			{
				DebugLog_Trace(LDIL_DBG,"DtsProcOutput: Failed to copy out buffs.. %x\n", sts);
				return sts;
			}
			pOut->DropFrames--;

			/* Get back the original flags */
			pOut->PoutFlags = savFlags;

			// Added support for dropping of single pictures by Adobe FP
			if(Ctx->SingleThreadedAppMode && (pOut->DropFrames == 0))
			{
				pOut->PicInfo.timeStamp = 0;
				pOut->PicInfo.picture_number = OutBuffs.PicInfo.picture_number;
				if(OutBuffs.PicInfo.flags & VDEC_FLAG_EOS)
					pOut->PicInfo.flags |= (VDEC_FLAG_EOS|VDEC_FLAG_LAST_PICTURE);
				return BC_STS_SUCCESS;
			}
		}
		else
			break;

	// this can't be right, DropFrames is a uint8_t so it will always be greater than or equal to zero.
  //} while((pOut->DropFrames >= 0));
	} while((pOut->DropFrames > 0));


	if(pOut->AppCallBack && pOut->hnd && (OutBuffs.PoutFlags & BC_POUT_FLAGS_PIB_VALID))
	{
		/* Merge in and out flags */
		OutBuffs.PoutFlags |= pOut->PoutFlags;
		width = Ctx->HWOutPicWidth;
		OutBuffs.b422Mode = Ctx->b422Mode;
		pOut->AppCallBack(pOut->hnd,
					width,
					OutBuffs.PicInfo.height,
					0,
					&OutBuffs);
	}

	if (pOut->PoutFlags & BC_POUT_FLAGS_MODE) {
		sts = DtsCopyFormat(Ctx,pOut,&OutBuffs);
	} else {
		pOut->b422Mode = Ctx->b422Mode;
		if(Ctx->b422Mode) {
			sts = DtsCopyRawDataToOutBuff(Ctx,pOut,&OutBuffs);
		}else{
			if(pOut->PoutFlags & BC_POUT_FLAGS_YV12){
				sts = DtsCopyNV12ToYV12(Ctx,pOut,&OutBuffs);
			}else {
				sts = DtsCopyNV12(Ctx,pOut,&OutBuffs);
			}
		}
	}

	if(pOut->PoutFlags & BC_POUT_FLAGS_PIB_VALID){
		pOut->PicInfo = OutBuffs.PicInfo;
	}

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

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

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

	while(Ctx->State == BC_DEC_STATE_START || Ctx->State == BC_DEC_STATE_PAUSE){

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

DRVIFLIB_INT_API BC_STATUS
DtsSendData( HANDLE  hDevice ,
				 uint8_t *pUserData,
				 uint32_t ulSizeInBytes,
				 uint64_t timeStamp,
				 BOOL encrypted
			    )
{
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	// for now check the sizes here and wait if there is not enough space
	while(ulSizeInBytes > Ctx->circBuf.freeSize) {
		usleep(5 * 1000);
		if (Ctx->State !=  BC_DEC_STATE_START && Ctx->State != BC_DEC_STATE_PAUSE)
			return BC_STS_IO_USER_ABORT;
	}
	return txBufPush(&Ctx->circBuf, pUserData, ulSizeInBytes);
}

DRVIFLIB_API uint32_t
DtsTxFreeSize( HANDLE hDevice )
{
	DTS_LIB_CONTEXT                *Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	return Ctx->circBuf.freeSize;
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

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

	while (i <20)
	{
		sts = DtsPrepareMdata(Ctx, timeStamp, &im, &pSPESPkt, &ulSize);
		if (sts == BC_STS_SUCCESS)
			break;
		i++;
		bc_sleep_ms(2);
	}
	if (sts == BC_STS_SUCCESS)
	{
		if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		{
			if(posix_memalign((void**)&pSPESPkt, 8, 32+sizeof(BC_PES_HDR_FORMAT))){
				DebugLog_Trace(LDIL_DBG, "DtsProcInput: Failed to alloc mem for  ASFHdr for SPES:%x\n", sts);
				return BC_STS_INSUFF_RES;
			}

			sts =DtsPrepareMdataASFHdr(Ctx, im, pSPESPkt);

			if(sts != BC_STS_SUCCESS){
				free(pSPESPkt);
				DebugLog_Trace(LDIL_DBG, "DtsProcInput: Failed to Prepare ASFHdr for SPES:%x\n", sts);
				return sts;
			}
			ulSize = 32+sizeof(BC_PES_HDR_FORMAT);
		}
		sts = DtsSendData(hDevice, pSPESPkt, ulSize, 0, encrypted);
		if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
			free(pSPESPkt);

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

	uint8_t* alignBuf;

	DTS_GET_CTX(hDevice,Ctx);

	alignBuf = Ctx->alignBuf;

	uint32_t ulRestBytes = ulSizeInBytes;
	uint32_t ulDeliverBytes = 0;
	uint32_t ulPktLength;
	uint32_t ulPktHeaderSz;
	uint32_t ulUsedDataBytes = 0;

	uint8_t* pDeliverBuf = NULL;
	uint8_t* pRestDataPt = pUserData;
	uint8_t bAddPTS = 1;
	uint8_t bPrivData = Ctx->PESConvParams.m_bPESPrivData;
	uint8_t bExtData = Ctx->PESConvParams.m_bPESExtField;
	uint8_t bStuffing = Ctx->PESConvParams.m_bStuffing;

	uint8_t* pPrivData = Ctx->PESConvParams.m_pPESPrivData;
	uint8_t* pExtData = Ctx->PESConvParams.m_pPESExtField;
	uint32_t nExtDataLen = Ctx->PESConvParams.m_nPESExtLen;
	uint32_t	nStuffingBytes = Ctx->PESConvParams.m_nStuffingBytes;


	uint8_t j = 0;
	uint8_t k = 0;

	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	if ((timeStamp || Ctx->PESConvParams.m_bSoftRave))
	{
		bAddPTS = 1;
	}
	else
	{
		bAddPTS = 0;
	}
	while ((Ctx->State == BC_DEC_STATE_START || Ctx->State == BC_DEC_STATE_PAUSE) && ulRestBytes )
	{
		sts = BC_STS_SUCCESS;

		pDeliverBuf = pRestDataPt;

		if (Ctx->State == BC_DEC_STATE_PAUSE)
		{
			sleep(5);
			continue;
		}

		/* Copy the non-DWORD aligned buffer first */
		oddBytes = (uint8_t)((uintptr_t)pDeliverBuf % 4);

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


			memcpy(alignBuf,(uint8_t *)b_pes_header, 9);
			*((uint16_t *)(alignBuf + 4)) = WORD_SWAP((uint16_t)ulPktLength);
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
				if (Ctx->State == BC_DEC_STATE_STOP)
				{
					break;
				}
				sleep(2);
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
			}
			else if(sts == BC_STS_IO_USER_ABORT)
			{
				sts = BC_STS_SUCCESS;
				break;
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
				 BOOL  encrypted
			    )
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	uint32_t Offset = 0;

	DTS_GET_CTX(hDevice,Ctx);

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if (Ctx->State == BC_DEC_STATE_FLUSH)
		Ctx->State = BC_DEC_STATE_START;

	if (Ctx->State != BC_DEC_STATE_START)
	{
		if (!DtsIsDecOpened(0))
		{
			DtsLock(Ctx);
			sts = DtsOpenDecoder(hDevice, Ctx->VidParams.StreamType);
			if (sts == BC_STS_SUCCESS)
			{
				sts = DtsStartDecoder(hDevice);
				if (sts == BC_STS_SUCCESS)
					sts = DtsStartCapture(hDevice);
			}
			DtsUnLock(Ctx);
			if (sts != BC_STS_SUCCESS)
				return sts;
		}
	}

	Ctx->bEOSCheck = false;
	Ctx->bEOS = false;

	// According to ASF spec special timestamps can be 0x1FFFFFFFF or 0x1FFFFFFFE
	// NAREN - FIXME - should we add support for these pre-roll timestamps
	if (Ctx->DevId == BC_PCI_DEVID_FLEA && timeStamp != 0xFFFFFFFFFFFFFFFFULL)
	{
		timeStamp /= 10000;
	}

	if((Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WVC1) || (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMV3) || (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_WMVA))
		DtsCheckKeyFrame(hDevice, pUserData);

	if (Ctx->PESConvParams.m_bAddSpsPps)
	{

		if  (Ctx->PESConvParams.m_pSpsPpsBuf != NULL && Ctx->PESConvParams.m_iSpsPpsLen != 0)
		{
			//Check if SequenceHeader is already delivered within input data.
			//Unnecessary SpsPps input will cause no timestamp output.
			if (!Ctx->SingleThreadedAppMode && !DtsCheckSpsPps(hDevice, pUserData, ulSizeInBytes))
			{
				// Used to flag softRAVE special handling of Sequence level information as well as preroll samples
				if (Ctx->PESConvParams.m_bSoftRave)
					sts = DtsAlignSendData(hDevice, Ctx->PESConvParams.m_pSpsPpsBuf, Ctx->PESConvParams.m_iSpsPpsLen, 0xFFFFFFFFFFFFFFFFULL, 0);
				else
					sts = DtsAlignSendData(hDevice, Ctx->PESConvParams.m_pSpsPpsBuf, Ctx->PESConvParams.m_iSpsPpsLen, 0, 0);
				if (sts != BC_STS_SUCCESS)
					return sts;
			}
		}
		Ctx->PESConvParams.m_bAddSpsPps = false;
	}

	//Use 0xFFFFFFFD for SoftRave context instead of zero
	// For pre-roll samples for softRAVE context we send the timestamp as 0 to the softRAVE to handle.
	// The FW is responsible to add the timestamp back.
	//Both 0x1FFFFFFFF and 0x1FFFFFFFE when right shifted by XPT become 0xFFFFFFFF which will be treated as special PTS by softrave to replace the PTS entry with Start code entry
	if (Ctx->PESConvParams.m_bSoftRave && (((timeStamp&0x1FFFFFFFFULL) == 0x1FFFFFFFFULL) || ((timeStamp&0x1FFFFFFFFULL) == 0x1FFFFFFFEULL)))
	{
		//timeStamp = 0x0;
		timeStamp = 0xFFFFFFFDULL;
	}

	if ((sts = DtsAddStartCode(hDevice, &pUserData, &ulSizeInBytes, &timeStamp)) != BC_STS_SUCCESS)
	{
		if (sts == BC_STS_IO_XFR_ERROR)
			return BC_STS_SUCCESS;
		return BC_STS_ERROR;
	}

	if (Ctx->VidParams.StreamType == BC_STREAM_TYPE_PES || timeStamp == 0)
	{
		return DtsAlignSendData(hDevice, pUserData, ulSizeInBytes, timeStamp, encrypted);
	}
	else
	{
		if(!Ctx->SingleThreadedAppMode && (DtsFindStartCode(hDevice, pUserData, ulSizeInBytes, &Offset) != BC_STS_SUCCESS))
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
DtsGetColorPrimaries( HANDLE  hDevice ,
				 uint32_t *colorPrimaries
				 )
{
	return BC_STS_NOT_IMPL;
}

DRVIFLIB_API BC_STATUS
DtsSendEOS( HANDLE  hDevice, uint32_t Op
			  )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	uint8_t	*pEOS;
	uint32_t nEOSLen;
	uint32_t	nTag;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

	Ctx->PESConvParams.m_bPESExtField = false;
	Ctx->PESConvParams.m_bPESPrivData = false;

	Ctx->PESConvParams.m_pPESExtField = NULL;
	Ctx->PESConvParams.m_pPESPrivData = NULL;

	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker

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
	else if (Ctx->VidParams.MediaSubType == BC_MSUBTYPE_DIVX || Ctx->VidParams.MediaSubType == BC_MSUBTYPE_DIVX311)
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
	if (Op == 0)
	{
		if (Ctx->DevId == BC_PCI_DEVID_FLEA)
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
		Ctx->bEOSCheck = true;
	}

	//Reset
	Ctx->PESConvParams.m_bPESExtField = false;
	Ctx->PESConvParams.m_pPESExtField = NULL;
	Ctx->PESConvParams.m_bPESPrivData = false;
	Ctx->PESConvParams.m_pPESPrivData = NULL;
	Ctx->PESConvParams.m_bStuffing = false;
	Ctx->PESConvParams.m_nStuffingBytes = 0;
	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker

	return sts;
}

DRVIFLIB_API BC_STATUS
DtsFlushInput( HANDLE  hDevice ,
				 uint32_t Op )
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);
	DebugLog_Trace(LDIL_DBG, "Flush called with opcode %u\n", Op);

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	if(Op == 0 || Op == 5) // DRAIN
	{
		DtsSendEOS(hDevice, Op);
		return BC_STS_SUCCESS;
	}
	else
	{
		Ctx->PESConvParams.m_bAddSpsPps = true;
		Ctx->State = BC_DEC_STATE_FLUSH;
		txBufFlush(&Ctx->circBuf);
		Ctx->bEOSCheck = false;
		bc_sleep_ms(30); // For the cancel to take place in case we are looping
		sts = DtsCancelTxRequest(hDevice, Op);
		if((Op == 3) || (sts != BC_STS_SUCCESS))
		{
			return sts;
		}
		DtsClrPendMdataList(Ctx);
	}

	// On LINK if the decoder is paused due to the RLL being full, un pause it before flush
	if(Ctx->DevId == BC_PCI_DEVID_LINK && Ctx->hw_paused) {
		DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_OFF);
		Ctx->hw_paused = false;
	}

	if(Op == 4)
		sts = DtsFWDecFlushChannel(hDevice,2);
	else if (Op != 0 && Op != 5)
		sts = DtsFWDecFlushChannel(hDevice,Op);

	if(Op != 0 && Op != 5)
	{
		if (Ctx->State != BC_DEC_STATE_CLOSE)
		{
			DtsLock(Ctx);
			sts = DtsStopDecoder(hDevice);
			sts	= DtsCloseDecoder(hDevice);
			DtsUnLock(Ctx);
		}
	}

	Ctx->LastPicNum = -1;
	Ctx->LastSessNum = -1;
	Ctx->EOSCnt = 0;
	Ctx->DrvStatusEOSCnt = 0;
	Ctx->bEOS = FALSE;
//	Ctx->InSampleCount = 0;

	Ctx->PESConvParams.m_lStartCodeDataSize = 0;

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

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

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
		if(Ctx->DevId == BC_PCI_DEVID_FLEA)
		{
			//Flea
			if((Rate <= 2) && (direction == 0))
			{
				DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Normal Speed\n");
				mode = eC011_SKIP_PIC_IPB_DECODE;
				HostTrickModeEnable = 0;
			}
			else if((Rate <= 3) && (direction == 0))
			{
				DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Fast Speed for IP-Frame Only\n");
				mode = eC011_SKIP_PIC_IP_DECODE;
				HostTrickModeEnable = 1;
			}
			else
			{
				//I-Frame Only for Fast Forward and All Speed Backward
				DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Very Fast Speed for I-Frame Only\n");
				mode = eC011_SKIP_PIC_I_DECODE;
				HostTrickModeEnable = 1;
			}
		}
		else
		{
			//Link
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

		if(Ctx->DevId != BC_PCI_DEVID_FLEA)
		{
			sts = DtsFWSetFFRate(hDevice, Rate);
			if(sts != BC_STS_SUCCESS)
			{
				DebugLog_Trace(LDIL_DBG,"DtsSetRateChange: Set Fast Forward Failed\n");
				return sts;
			}
		}
	}

	return BC_STS_SUCCESS;
}

//Set FF Rate for Catching Up
DRVIFLIB_API BC_STATUS
DtsSetFFRate(HANDLE  hDevice ,
				 uint32_t rate)
{
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	//For Rate Change
	uint32_t mode = 0;
	uint32_t HostTrickModeEnable = 0;

	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

	if (!DtsChkPID(Ctx->ProcessID))
		return BC_STS_ERROR;

	//Change Rate Value for Version 1.1
	//Rate: Specifies the new rate x 10000
	//Rate is the inverse of speed. For example, if the playback speed is 2x, the rate is 1/2, so the Rate member is set to 5000.

	float fRate = float(1) / ((float)rate / (float)10000);

	//Mode Decision
	if(fRate < 1)
	{
		//Error
		//Only for FF

		DebugLog_Trace(LDIL_DBG,"DtsSetFFRate: NOT Support Slow Motion\n");

		return BC_STS_INV_ARG;
	}
	else
	{
		//Fast
		LONG Rate = (LONG)fRate;

		if(Ctx->DevId == BC_PCI_DEVID_FLEA)
		{
			if(Rate > FLEA_MAX_TRICK_MODE_SPEED)
			{
				Rate = FLEA_MAX_TRICK_MODE_SPEED;
			}
		}

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

#if 0
DRVIFLIB_API BC_STATUS
	DtsIs422Supported(
    HANDLE  hDevice,
	uint8_t*		bSupported)
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->DevId == BC_PCI_DEVID_LINK || Ctx->DevId == BC_PCI_DEVID_FLEA)
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
DtsSet422Mode(HANDLE hDevice, uint8_t	Mode422)
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

#endif

DRVIFLIB_API BC_STATUS
DtsIsEndOfStream(
    HANDLE  hDevice,
    uint8_t*	bEOS
)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	DTS_LIB_CONTEXT		*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	*bEOS = Ctx->bEOS;

	return sts;
}

DRVIFLIB_API BC_STATUS
DtsSetColorSpace(
	HANDLE  hDevice,
	BC_OUTPUT_FORMAT	Mode422
)
{
	BC_STATUS	sts = BC_STS_SUCCESS;
	//unused uint32_t    Val = 0;
	DTS_LIB_CONTEXT		*Ctx = NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->DevId == BC_PCI_DEVID_LINK)
	{
		Ctx->b422Mode = Mode422;
		sts = DtsSetLinkIn422Mode(hDevice);
	}
	else if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		Ctx->b422Mode = Mode422;
		sts = DtsSetFleaIn422Mode(hDevice);
	}

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
	if (Ctx->State == BC_DEC_STATE_CLOSE)
	{
		return BC_STS_DEC_NOT_OPEN;
	}

	if (Ctx->State == BC_DEC_STATE_STOP || Ctx->State == BC_DEC_STATE_FLUSH)
	{
		return BC_STS_DEC_NOT_STARTED;
	}

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
	BOOL realHWCPBSize = false; // Always report DIL buffer size unless explicitly asked to report HW size
	BOOL readTXinfoOnly = false; // Report only TX information

	uint64_t NextTimeStamp = 0;

	DTS_LIB_CONTEXT			*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	temp.DrvNextMDataPLD = Ctx->HWOutPicWidth | (0x1 << 31);

	// If bit 31 of the input cpbEmptySize is set, then report the real HW size
	// Else report the buffered size
	// If Bit 30 of the input cpbEmptySize is set, then only report TX information
	// and not probe for anything else
	// Use Bit 29 to indicate to the driver to read the VC1 fifo status
	if(pStatus->cpbEmptySize >> 31)
		realHWCPBSize = true;
	if((pStatus->cpbEmptySize >> 30) & 0x1) {
		readTXinfoOnly = true;
		temp.DrvcpbEmptySize |= (1 << 30);
	}

	if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
		temp.DrvcpbEmptySize |= (1 << 29);

	ret = DtsGetDrvStat(hDevice, &temp);

	if (ret != BC_STS_SUCCESS)
	{
		return ret;
	}

	pStatus->FreeListCount      = temp.drvFLL;
	pStatus->ReadyListCount     = temp.drvRLL;
	pStatus->FramesCaptured     = temp.opFrameCaptured;
	pStatus->FramesDropped      = temp.opFrameDropped;
	pStatus->FramesRepeated     = temp.reptdFrames;
	pStatus->PIBMissCount       = temp.pibMisses;
	pStatus->InputCount         = temp.ipSampleCnt;
	pStatus->InputBusyCount     = temp.TxFifoBsyCnt;
	pStatus->InputTotalSize     = temp.ipTotalSize;
	pStatus->cpbEmptySize		= temp.DrvcpbEmptySize;
	pStatus->picNumFlags		= temp.picNumFlags;
	pStatus->PowerStateChange	= temp.pwr_state_change;

	if(temp.eosDetected)
	{
		Ctx->bEOS = true;
	}

	if (Ctx->bEOSCheck == true && Ctx->bEOS == true)
	{
		if (pStatus->ReadyListCount == 0)
		{
			Ctx->DrvStatusEOSCnt ++;

			if(Ctx->DrvStatusEOSCnt >= BC_EOS_PIC_COUNT)
			{
				/* Mark this picture as end of stream..*/
				Ctx->bEOS = TRUE;
			}
		}
		else
			Ctx->DrvStatusEOSCnt = 0;
	}

	if(!realHWCPBSize)
		pStatus->cpbEmptySize = Ctx->circBuf.freeSize;

	if(!readTXinfoOnly)
	{
		/* return the timestamp of the next picture to be returned by ProcOutput */
		if((pStatus->ReadyListCount > 0) && Ctx->SingleThreadedAppMode) {
			if(Ctx->DevId == BC_PCI_DEVID_FLEA)
			{
				if(temp.DrvNextMDataPLD == 0xFFFFFFFF){
					//For Pre-Load
					pStatus->NextTimeStamp = 0xFFFFFFFFFFFFFFFFLL;
				}else{
					//Normal PTS
					//Change PTS becuase of Shift PTS Issue in FW and 32-bit (ms) and 64-bit (100 ns) Scaling
					pStatus->NextTimeStamp = (temp.DrvNextMDataPLD * 2 * 10000);
				}
			}else{
				DtsFetchTimeStampMdata(Ctx, ((temp.DrvNextMDataPLD & 0xFF) << 8) | ((temp.DrvNextMDataPLD & 0xFF00) >> 8), &NextTimeStamp);
				pStatus->NextTimeStamp = NextTimeStamp;
			}
		}
	}
	// For LINK Pause HW if the RLL is too full. Prevent overflows
	// Hard coded values for now
	if(Ctx->DevId == BC_PCI_DEVID_LINK && Ctx->SingleThreadedAppMode) {
		if(pStatus->ReadyListCount > 10 && !Ctx->hw_paused && !Ctx->fw_cmd_issued) {
			DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_ON);
			Ctx->hw_paused = true;
		}
		else if (pStatus->ReadyListCount  < 6 && Ctx->hw_paused && !Ctx->fw_cmd_issued) {
			DtsFWPauseVideo(hDevice,eC011_PAUSE_MODE_OFF);
			Ctx->hw_paused = false;
		}
	}

	return ret;
}

DRVIFLIB_API BC_STATUS DtsGetCapabilities (HANDLE  hDevice, PBC_HW_CAPS	pCapsBuffer)
{
	DTS_LIB_CONTEXT *Ctx;
	BC_STATUS sts = BC_STS_SUCCESS;
	uint32_t pciids = 0;
	int shmid = 0;

//	DebugLog_Trace(LDIL_DBG,"DtsGetCapabilities: Called\n");

	if(hDevice != NULL) {
		DTS_GET_CTX(hDevice,Ctx); // Called after the HW has been opened
		pciids = Ctx->DevId;
	}
	else {
		// called before HW has been opened
		// First make sure no one else has the HW open already
		if(BC_STS_SUCCESS == DtsCreateShMem(&shmid)) {
			pciids = DtsGetgDevID();
			DtsDelDilShMem();
			if(pciids == BC_PCI_DEVID_INVALID) {
				sts = DtsGetHWFeatures(&pciids);
				pciids >>= 16;
				if(sts != BC_STS_SUCCESS)
					return sts;
			}
		}
		else
			return BC_STS_INSUFF_RES;
	}

	if (pciids == BC_PCI_DEVID_INVALID)
	{
		return BC_STS_ERROR;
	}

	// Should check with driver/FW if current video is supported or not, and output supported format
	if(pciids == BC_PCI_DEVID_LINK)
	{
		pCapsBuffer->flags = PES_CONV_SUPPORT;
		pCapsBuffer->ColorCaps.Count = 3;

		pCapsBuffer->ColorCaps.OutFmt[0] = OUTPUT_MODE420;
		pCapsBuffer->ColorCaps.OutFmt[1] = OUTPUT_MODE422_YUY2;
		pCapsBuffer->ColorCaps.OutFmt[2] = OUTPUT_MODE422_UYVY;
		pCapsBuffer->Reserved1 = NULL;

		//Decoder Capability
		pCapsBuffer->DecCaps = BC_DEC_FLAGS_H264 | BC_DEC_FLAGS_MPEG2 | BC_DEC_FLAGS_VC1;
	}
	if(pciids == BC_PCI_DEVID_FLEA)
	{
		pCapsBuffer->flags = PES_CONV_SUPPORT;
		pCapsBuffer->ColorCaps.Count = 1;
		pCapsBuffer->ColorCaps.OutFmt[0] = OUTPUT_MODE422_YUY2;
		pCapsBuffer->ColorCaps.OutFmt[1] = OUTPUT_MODE_INVALID;
		pCapsBuffer->ColorCaps.OutFmt[2] = OUTPUT_MODE_INVALID;
		pCapsBuffer->Reserved1 = NULL;

		//Decoder Capability
		pCapsBuffer->DecCaps = BC_DEC_FLAGS_H264 | BC_DEC_FLAGS_MPEG2 | BC_DEC_FLAGS_VC1 | BC_DEC_FLAGS_M4P2;
	}

	return BC_STS_SUCCESS;
}

DRVIFLIB_API BC_STATUS DtsSetScaleParams(HANDLE hDevice, PBC_SCALING_PARAMS pScaleParams)
{
	DTS_LIB_CONTEXT *Ctx = NULL;
	DTS_GET_CTX(hDevice, Ctx);
	uint32_t ScaledWidth = 0;

	if (Ctx->DevId == BC_PCI_DEVID_FLEA) {
		if ((pScaleParams->sWidth > 1920) || (pScaleParams->sWidth < 128))
			ScaledWidth = 1280;
		else
			ScaledWidth = pScaleParams->sWidth;

		Ctx->EnableScaling = (ScaledWidth << 20) | (ScaledWidth << 8) | 1;

	} else {
		DebugLog_Trace(LDIL_ERR,"DtsSetScaleParams: not supported\n");
		return BC_STS_INV_ARG;
	}

	return DtsCheckProfile(hDevice);
}

DRVIFLIB_API BC_STATUS DtsCrystalHDVersion(HANDLE  hDevice, PBC_INFO_CRYSTAL bCrystalInfo)
{
	DTS_LIB_CONTEXT			*Ctx = NULL;
	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->DevId == BC_PCI_DEVID_LINK)
		bCrystalInfo->device = 0;
	else if(Ctx->DevId == BC_PCI_DEVID_FLEA)
		bCrystalInfo->device = 1;

	bCrystalInfo->dilVersion.dilRelease = DIL_MAJOR_VERSION;
	bCrystalInfo->dilVersion.dilMajor = DIL_MINOR_VERSION;
	bCrystalInfo->dilVersion.dilMinor = DIL_REVISION;

	bCrystalInfo->drvVersion.drvRelease = DRIVER_MAJOR_VERSION;
	bCrystalInfo->drvVersion.drvMajor = DRIVER_MINOR_VERSION;
	bCrystalInfo->drvVersion.drvMinor = DRIVER_REVISION;

	bCrystalInfo->fwVersion.fwRelease = FW_MAJOR_VERSION;
	bCrystalInfo->fwVersion.fwMajor = FW_MINOR_VERSION;
	bCrystalInfo->fwVersion.fwMinor = FW_REVISION;

	return BC_STS_SUCCESS;
}
