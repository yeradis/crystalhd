/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_priv.cpp
 *
 *  Description: Driver Interface library Internal.
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

#include <link.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include "7411d.h"
#include "libcrystalhd_if.h"
#include "libcrystalhd_int_if.h"
#include "libcrystalhd_priv.h"
#include "libcrystalhd_parser.h"

/*============== Global shared area usage ======================*/
/* Global mode settings */
/*
	Bit 0 (LSB)	- Play Back mode
	Bit 1		- Diag mode
	Bit 2		- Monitor mode
	Bit 3		- HwInit mode
	bit 5       - Hwsetup in progress
*/

bc_dil_glob_s *bc_dil_glob_ptr=NULL;
bool glob_mode_valid=TRUE;

BC_STATUS DtsCreateShMem(int *shmem_id)
{
	int shmid=-1;
	key_t shmkey=BC_DIL_SHMEM_KEY;
	shmid_ds buf;
	uint32_t mode=0;


	if(shmem_id==NULL) {
		DebugLog_Trace(LDIL_DBG,"Invalid argument ...\n");
		return BC_STS_INSUFF_RES;
	}

	*shmem_id =shmid;
	//First Try to create it.
	if((shmid= shmget(shmkey, 1024, 0644|IPC_CREAT|IPC_EXCL))== -1 ) {
		if(errno==EEXIST) {
			//DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:shmem already exists :%d\n",errno);
			//shmem segment already exists so get the shmid
			if((shmid= shmget(shmkey, 1024, 0644))== -1 ) {
				DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:unable to get shmid :%d\n",errno);
				return BC_STS_INSUFF_RES;
			}

			//we got the shmid, see if any process is alreday attached to it
			if(shmctl(shmid,IPC_STAT,&buf)==-1){
				DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:shmctl failed ...\n");
				return BC_STS_ERROR;
			}


			if(buf.shm_nattch ==0) {
				//No process is currently attached to the shmem seg. go ahead and delete it as its contents are stale.
				if(-1!=shmctl(shmid,IPC_RMID,NULL)){
						DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:deleted shmem segment and creating a new one ...\n");
						//return BC_STS_ERROR;

				}
				//create a new shmem
				if((shmid= shmget(shmkey, 1024, 0644|IPC_CREAT|IPC_EXCL))== -1 ) {
					DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:unable to get shmid :%d\n",errno);
					return BC_STS_INSUFF_RES;
			    }
				//attach to it
				DtsGetDilShMem(shmid);

		   }

			else{ //someone is attached to it

					DtsGetDilShMem(shmid);
					mode = DtsGetOPMode();
					if(! ((mode==1)||(mode==2)||(mode==4))&&(buf.shm_nattch==1) ) {
						glob_mode_valid=FALSE;
						DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:globmode %d is invalid\n",mode);
					}

			    }

		   }else{

			DebugLog_Trace(LDIL_DBG,"shmcreate failed with err %d",errno);
			return BC_STS_ERROR;
		   }
	}else{
		//we created just attach to it
		DtsGetDilShMem(shmid);
	}

	*shmem_id =shmid;

	return BC_STS_SUCCESS;
}

BC_STATUS DtsGetDilShMem(uint32_t shmid)
{
	bc_dil_glob_ptr=(bc_dil_glob_s *)shmat(shmid,(void *)0,0);

	if((long)bc_dil_glob_ptr==-1) {
		DebugLog_Trace(LDIL_DBG,"Unable to open shared memory ...\n");
		return BC_STS_ERROR;
	}

	return BC_STS_SUCCESS;
}

BC_STATUS DtsDelDilShMem()
{
	int shmid =0;
	shmid_ds buf;
	//First dettach the shared mem segment

	if(shmdt(bc_dil_glob_ptr)==-1) {

		DebugLog_Trace(LDIL_DBG,"Unable to detach from Dil shared memory ...\n");
		//return BC_STS_ERROR;
	}

	//delete the shared mem segment if there are no other attachments
	if ((shmid =shmget((key_t)BC_DIL_SHMEM_KEY,0,0))==-1){
			DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:Unable get shmid ...\n");
			return BC_STS_ERROR;
	}

	if(shmctl(shmid,IPC_STAT,&buf)==-1){
		DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:shmctl failed ...\n");
		return BC_STS_ERROR;
	}


	if(buf.shm_nattch ==0) {
		//No process is currently attached to the shmem seg. go ahead and delete it
		if(-1!=shmctl(shmid,IPC_RMID,NULL)){
				// DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:deleted shmem segment ...\n");
				return BC_STS_ERROR;

		} else{
			DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:unable to delete shmem segment ...\n");
		}

	}
	return BC_STS_SUCCESS;


}

uint32_t DtsGetgDevID(void)
{
	if(bc_dil_glob_ptr == NULL)
		return BC_PCI_DEVID_INVALID;
	else
		return bc_dil_glob_ptr->DevID;
}

void DtsSetgDevID(uint32_t DevID)
{
	bc_dil_glob_ptr->DevID = DevID;
}

uint32_t DtsGetOPMode( void )
{
	return  bc_dil_glob_ptr->gDilOpMode;
}

void DtsSetOPMode( uint32_t value )
{
	bc_dil_glob_ptr->gDilOpMode = value;
}

uint32_t DtsGetHwInitSts( void )
{
	return bc_dil_glob_ptr->gHwInitSts;
}

void DtsSetHwInitSts( uint32_t value )
{
	bc_dil_glob_ptr->gHwInitSts = value;
}

void DtsRstStats( void )
{
	memset(&bc_dil_glob_ptr->stats, 0, sizeof(bc_dil_glob_ptr->stats));
}

BC_DTS_STATS * DtsGetgStats ( void )
{
	return &bc_dil_glob_ptr->stats;
}

bool DtsIsDecOpened(pid_t nNewPID)
{
	if(bc_dil_glob_ptr == NULL)
		return false;

	if (nNewPID == 0)
		return bc_dil_glob_ptr->g_bDecOpened;

	if (nNewPID == bc_dil_glob_ptr->g_nProcID)
		return false;

	return bc_dil_glob_ptr->g_bDecOpened;
}

bool DtsChkPID(pid_t nCurPID)
{
	if (bc_dil_glob_ptr->g_nProcID == 0)
		return true;

	return (nCurPID == bc_dil_glob_ptr->g_nProcID);
}

void DtsSetDecStat(bool bDecOpen, pid_t PID)
{
	if (bDecOpen == true)
		bc_dil_glob_ptr->g_nProcID = PID;
	else
		bc_dil_glob_ptr->g_nProcID = 0;

	bc_dil_glob_ptr->g_bDecOpened = bDecOpen;
}
/*============== Global shared area usage End.. ======================*/

#define TOP_FIELD_FLAG				0x01
#define BOTTOM_FIELD_FLAG			0x02
#define PROGRESSIVE_FRAME_FLAG	0x03

static void DtsGetMaxYUVSize(DTS_LIB_CONTEXT *Ctx, uint32_t *YbSz, uint32_t *UVbSz)
{
	if (Ctx->b422Mode) {
		*YbSz = (1920*1090)*2;
		*UVbSz = 0;
	} else {
		*YbSz  = 1920*1090;
		*UVbSz = 1920*1090/2;
	}
}
static void DtsGetMaxSize(DTS_LIB_CONTEXT *Ctx, uint32_t *Sz)
{
	*Sz = (1920*1090)*2;
}
static void DtsInitLock(DTS_LIB_CONTEXT	*Ctx)
{
	//Create mutex
	int ret;
	pthread_mutexattr_t thLockattr;
	ret = pthread_mutexattr_init(&thLockattr);
	if(ret)
		DebugLog_Trace(LDIL_DBG, "Error initializing attributes\n");
	ret = pthread_mutexattr_settype(&thLockattr, PTHREAD_MUTEX_RECURSIVE);
	if(ret)
		DebugLog_Trace(LDIL_DBG, "Error setting type of mutex\n");
	ret = pthread_mutex_init(&Ctx->thLock, &thLockattr);
	if(ret)
		DebugLog_Trace(LDIL_DBG, "Error initializing mutex\n");
}
static void DtsDelLock(DTS_LIB_CONTEXT	*Ctx)
{
	pthread_mutex_destroy(&Ctx->thLock);

}
void DtsLock(DTS_LIB_CONTEXT	*Ctx)
{
	pthread_mutex_lock(&Ctx->thLock);
}
void DtsUnLock(DTS_LIB_CONTEXT	*Ctx)
{
	pthread_mutex_unlock(&Ctx->thLock);
}

static void DtsIncPend(DTS_LIB_CONTEXT	*Ctx)
{
	DtsLock(Ctx);
	Ctx->ProcOutPending++;
	DtsUnLock(Ctx);
}
static void DtsDecPend(DTS_LIB_CONTEXT	*Ctx)
{
	DtsLock(Ctx);
	if(Ctx->ProcOutPending)
		Ctx->ProcOutPending--;
	DtsUnLock(Ctx);
}

void DtsGetFrameRate(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut)
{
	if (Ctx->VidParams.FrameRate == pOut->PicInfo.frame_rate)
	{
		pOut->PicInfo.frame_rate = 0;
		return;
	}

	// DebugLog_Trace(LDIL_DBG,"DtsGetFrameRate: frame_rate:%d (%d)\n", pOut->PicInfo.frame_rate, Ctx->VidParams.FrameRate);

	Ctx->VidParams.FrameRate = pOut->PicInfo.frame_rate;

	if (pOut->PicInfo.frame_rate == vdecFrameRate23_97)
		pOut->PicInfo.frame_rate = 23970;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate24)
		pOut->PicInfo.frame_rate = 24000;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate25)
		pOut->PicInfo.frame_rate = 25000;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate29_97)
		pOut->PicInfo.frame_rate = 29970;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate30)
		pOut->PicInfo.frame_rate = 30000;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate50)
		pOut->PicInfo.frame_rate = 50000;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate59_94)
		pOut->PicInfo.frame_rate = 59940;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate60)
		pOut->PicInfo.frame_rate = 60000;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate14_985)
		pOut->PicInfo.frame_rate = 14985;
	else if (pOut->PicInfo.frame_rate == vdecFrameRate7_496)
		pOut->PicInfo.frame_rate = 7496;
	else if (pOut->PicInfo.frame_rate == vdecFrameRateUnknown)
		pOut->PicInfo.frame_rate = 23970;
	else
		pOut->PicInfo.frame_rate = 23970;

}


uint32_t DtsGetHWOutputStride(DTS_LIB_CONTEXT *Ctx, C011_PIB *pPIBInfo)
{
	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		return pPIBInfo->ppb.width;
	}
	else
	{
		return pPIBInfo->resolution;
	}
}

uint32_t DtsGetWidthfromResolution(DTS_LIB_CONTEXT *Ctx, uint32_t Resolution)
{
	uint32_t Width;

	/* For Flea source width is always equal to video width */
	/* For Link translate from format container to actual source width */
	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
		return 0;

	switch (Resolution) {
	case vdecRESOLUTION_CUSTOM:
		Width = 0;
		break;
	case vdecRESOLUTION_1080i:
	case vdecRESOLUTION_1080i25:
    case vdecRESOLUTION_1080i29_97:
    case vdecRESOLUTION_1080p29_97:
    case vdecRESOLUTION_1080p30:
    case vdecRESOLUTION_1080p24:
    case vdecRESOLUTION_1080p25:
	case vdecRESOLUTION_1080p0:
	case vdecRESOLUTION_1080i0:
	case vdecRESOLUTION_1080p23_976:
		Width = 1920;
		break;
	case vdecRESOLUTION_240p29_97:
	case vdecRESOLUTION_240p30:
	case vdecRESOLUTION_288p25:
		Width = 1440;
		break;
	case vdecRESOLUTION_720p:
    case vdecRESOLUTION_720p50:
	case vdecRESOLUTION_720p59_94:
    case vdecRESOLUTION_720p24:
	case vdecRESOLUTION_720p29_97:
	case vdecRESOLUTION_720p0:
	case vdecRESOLUTION_720p23_976:
		Width = 1280;
		break;
	case vdecRESOLUTION_480i:
	case vdecRESOLUTION_NTSC:
	case vdecRESOLUTION_PAL1:
    case vdecRESOLUTION_SD_DVD:
    case vdecRESOLUTION_480p656:
	case vdecRESOLUTION_480p:
    case vdecRESOLUTION_576p:
	case vdecRESOLUTION_480p23_976:
	case vdecRESOLUTION_480p29_97:
	case vdecRESOLUTION_576p25:
	case vdecRESOLUTION_480p0:
	case vdecRESOLUTION_480i0:
	case vdecRESOLUTION_576p0:
		Width = 720;
		break;
	default:
		Width = 0;
		break;
	}
	return Width;
}

static void DtsCopyAppPIB(DTS_LIB_CONTEXT *Ctx, BC_DEC_OUT_BUFF *decOut, BC_DTS_PROC_OUT *pOut)
{
	C011_PIB			*srcPib = &decOut->PibInfo;
	BC_PIC_INFO_BLOCK	*dstPib = &pOut->PicInfo;
	//uint16_t					sNum = 0;
	//BC_STATUS			sts = BC_STS_SUCCESS;

	Ctx->FormatInfo.timeStamp		= dstPib->timeStamp	= 0;
	Ctx->FormatInfo.picture_number	= dstPib->picture_number = srcPib->ppb.picture_number;
	Ctx->FormatInfo.width			= dstPib->width			 = srcPib->ppb.width;
	Ctx->FormatInfo.height			= dstPib->height		 = srcPib->ppb.height;
	Ctx->FormatInfo.chroma_format	= dstPib->chroma_format	 = srcPib->ppb.chroma_format;
	Ctx->FormatInfo.pulldown		= dstPib->pulldown		 = srcPib->ppb.pulldown;
	Ctx->FormatInfo.flags			= dstPib->flags			 = srcPib->ppb.flags;
	Ctx->FormatInfo.sess_num		= dstPib->sess_num		 = srcPib->ptsStcOffset;
	Ctx->FormatInfo.aspect_ratio	= dstPib->aspect_ratio	 = srcPib->ppb.aspect_ratio;
	Ctx->FormatInfo.colour_primaries = dstPib->colour_primaries = srcPib->ppb.colour_primaries;
	Ctx->FormatInfo.picture_meta_payload= dstPib->picture_meta_payload	= srcPib->ppb.picture_meta_payload;

	/* FIX_ME:: Add extensions part.. */

	/* Retrieve Timestamp */
	// NAREN - FIXME - We should not copy the timestamp for Format Change since it is a dummy picture with no timestamp
#if 0
	if(srcPib->ppb.flags & VDEC_FLAG_PICTURE_META_DATA_PRESENT){
		sNum = (U16) ( ((srcPib->ppb.picture_meta_payload & 0xFF) << 8) |
		((srcPib->ppb.picture_meta_payload& 0xFF00) >> 8) );
		DtsFetchMdata(Ctx,sNum,pOut);
}
#endif
}
static void dts_swap_buffer(uint32_t *dst, uint32_t *src, uint32_t cnt)
{
	uint32_t i=0;

	for (i=0; i < cnt; i++){
		dst[i] = BC_SWAP32(src[i]);
	}

}

static void DtsGetPibFrom422(uint8_t *pibBuff, uint8_t mode422)
{
	uint32_t		i;

	//First stripe has PIB data and second one has Extension PB. so total 256 bytes
	if (mode422 == OUTPUT_MODE422_YUY2) {
		for(i=0; i<256; i++) {
			pibBuff[i] = pibBuff[i*2];
		}
	} else if (mode422 == OUTPUT_MODE422_UYVY) {
		for(i=0; i<256; i++) {
			pibBuff[i] = pibBuff[(i*2)+1];
		}
	}
}

static BC_STATUS DtsGetPictureInfo(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut)
{

	uint16_t			sNum = 0;
	uint8_t*			pPicInfoLine = NULL;
	uint32_t			PictureNumber = 0;
	uint32_t			PicInfoLineNum;
	bool				bInterlaced = false;

	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		PicInfoLineNum = *(uint32_t *)(pOut->Ybuff);
	}
	else if (Ctx->b422Mode == OUTPUT_MODE422_YUY2)
	{
		PicInfoLineNum = ((uint32_t)(*(pOut->Ybuff + 6)) & 0xff)
						| (((uint32_t)(*(pOut->Ybuff + 4)) << 8)  & 0x0000ff00)
						| (((uint32_t)(*(pOut->Ybuff + 2)) << 16) & 0x00ff0000)
						| (((uint32_t)(*(pOut->Ybuff + 0)) << 24) & 0xff000000);
	}
	else if (Ctx->b422Mode == OUTPUT_MODE422_UYVY)
	{
		PicInfoLineNum = ((uint32_t)(*(pOut->Ybuff + 7)) & 0xff)
						| (((uint32_t)(*(pOut->Ybuff + 5)) << 8)  & 0x0000ff00)
						| (((uint32_t)(*(pOut->Ybuff + 3)) << 16) & 0x00ff0000)
						| (((uint32_t)(*(pOut->Ybuff + 1)) << 24) & 0xff000000);
	}
	else
	{
		PicInfoLineNum = ((uint32_t)(*(pOut->Ybuff + 3)) & 0xff)
						| (((uint32_t)(*(pOut->Ybuff + 2)) << 8)  & 0x0000ff00)
						| (((uint32_t)(*(pOut->Ybuff + 1)) << 16) & 0x00ff0000)
						| (((uint32_t)(*(pOut->Ybuff + 0)) << 24) & 0xff000000);
	}

	if (PicInfoLineNum == BC_EOS_DETECTED)  // EOS
	{
		memcpy((uint32_t*)&pOut->PicInfo,(uint32_t*)(pOut->Ybuff + 4), sizeof(BC_PIC_INFO_BLOCK));
		if (pOut->PicInfo.flags & VDEC_FLAG_EOS)
		{
			Ctx->bEOS = true;
			Ctx->pOutData->RetSts = BC_STS_NO_DATA;
			DebugLog_Trace(LDIL_DBG, "Found EOS \n");
			return BC_STS_NO_DATA;
		}
	}
	/*
	-- To take care of 16 byte alignment the firmware might put extra
	-- line so that the PIB starts with a line boundary. We will need to
	-- have additional checks for the following condition to take care of
	-- extra lines.
	*/

	if( ( (PicInfoLineNum != Ctx->HWOutPicHeight) && (PicInfoLineNum != (Ctx->HWOutPicHeight+1))) &&
		( (PicInfoLineNum != Ctx->HWOutPicHeight/2) && (PicInfoLineNum != (Ctx->HWOutPicHeight+1)/2)) )
	{
		return BC_STS_IO_XFR_ERROR;
	}

	if (Ctx->b422Mode) {
		pPicInfoLine = pOut->Ybuff + PicInfoLineNum * Ctx->HWOutPicWidth * 2;
	} else {
		pPicInfoLine = pOut->Ybuff + PicInfoLineNum * Ctx->HWOutPicWidth;
	}

	if (Ctx->DevId != BC_PCI_DEVID_FLEA)
	{
		DtsGetPibFrom422(pPicInfoLine, Ctx->b422Mode);
		PictureNumber = ((ULONG)(*(pPicInfoLine + 3)) & 0xff)
						| (((ULONG)(*(pPicInfoLine + 2)) << 8)  & 0x0000ff00)
						| (((ULONG)(*(pPicInfoLine + 1)) << 16) & 0x00ff0000)
						| (((ULONG)(*(pPicInfoLine + 0)) << 24) & 0xff000000);

	}else{
		/*The Metadata Is Linear in Flea.*/
		PictureNumber = ((ULONG)(*(pPicInfoLine + 0)) & 0xff)
						| (((ULONG)(*(pPicInfoLine + 1)) << 8)  & 0x0000ff00)
						| (((ULONG)(*(pPicInfoLine + 2)) << 16) & 0x00ff0000)
						| (((ULONG)(*(pPicInfoLine + 3)) << 24) & 0xff000000);
	}

	pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;

	if (Ctx->DevId != BC_PCI_DEVID_FLEA)
	{
		dts_swap_buffer((uint32_t*)&pOut->PicInfo,(uint32_t*)(pPicInfoLine + 4), 32);
		//copy ext PIB
		dts_swap_buffer((uint32_t*)&pOut->PicInfo.other,(uint32_t*)(pPicInfoLine +128), 32);
	}
	else
	{
		memcpy((uint32_t*)&pOut->PicInfo,(uint32_t*)(pPicInfoLine + 4), sizeof(BC_PIC_INFO_BLOCK));
	}

	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		if (pOut->PicInfo.flags & VDEC_FLAG_BOTTOMFIELD)
			bInterlaced = true;
	}
	else
	{
		if (pOut->PicInfo.flags & VDEC_FLAG_INTERLACED_SRC)
			bInterlaced = true;
	}

	if(bInterlaced)
	{
		Ctx->VidParams.Progressive = FALSE;
		pOut->PicInfo.flags |= VDEC_FLAG_INTERLACED_SRC;
		if (PictureNumber & 0x40000000)
		{
			pOut->PoutFlags |= BC_POUT_FLAGS_FLD_BOT;
			pOut->PicInfo.flags |= VDEC_FLAG_BOTTOMFIELD;
		}
		else
		{
			pOut->PicInfo.flags &= ~VDEC_FLAG_BOTTOMFIELD;
			pOut->PicInfo.flags |= VDEC_FLAG_TOPFIELD;
		}
	}
	else
	{
		Ctx->VidParams.Progressive = TRUE;
		pOut->PicInfo.flags &= ~(VDEC_FLAG_BOTTOMFIELD | VDEC_FLAG_INTERLACED_SRC);
	}

	if(PictureNumber & 0x80000000)
	{
		pOut->PoutFlags |= BC_POUT_FLAGS_ENCRYPTED;
	}

	DtsGetFrameRate(Ctx, pOut);
	//DILDbg_Trace(BC_DIL_DBG_DETAIL, TEXT("DtsGetPictureInfo: PicInfo (W,H):(%d,%d) FR:%ld Flags:0x%x\n"),pOut->PicInfo.width, pOut->PicInfo.height,pOut->PicInfo.frame_rate, pOut->PicInfo.flags);

	/* Replace Y Component data*/
	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		// In Flea from the HW we are getting Y and UV directly in the y component
			*((uint32_t *)&pOut->Ybuff[0]) = pOut->PicInfo.ycom;
	}
	else
	{
		//Replace Data Back by 422 Mode
		if (Ctx->b422Mode == OUTPUT_MODE422_YUY2)
		{
			//For YUY2
			*(pOut->Ybuff + 6) = ((char *)&pOut->PicInfo.ycom)[3];
			*(pOut->Ybuff + 4) = ((char *)&pOut->PicInfo.ycom)[2];
			*(pOut->Ybuff + 2) = ((char *)&pOut->PicInfo.ycom)[1];
			*(pOut->Ybuff + 0) = ((char *)&pOut->PicInfo.ycom)[0];
		}
		else if (Ctx->b422Mode == OUTPUT_MODE422_UYVY)
		{
			//For UYVY
			*(pOut->Ybuff + 7) = ((char *)&pOut->PicInfo.ycom)[3];
			*(pOut->Ybuff + 5) = ((char *)&pOut->PicInfo.ycom)[2];
			*(pOut->Ybuff + 3) = ((char *)&pOut->PicInfo.ycom)[1];
			*(pOut->Ybuff + 1) = ((char *)&pOut->PicInfo.ycom)[0];
		}
		else
		{
			//For NV12 or YV12
			*((uint32_t*)&pOut->Ybuff[0]) = pOut->PicInfo.ycom;
		}
	}

	if(Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		//Flea Mode
		if(pOut->PicInfo.timeStamp == 0xFFFFFFFF)
		{
			//For Pre-Load
			pOut->PicInfo.timeStamp = 0xFFFFFFFFFFFFFFFFLL;
		}
		else
		{
			//Normal PTS
			//Change PTS becuase of Shift PTS Issue in FW and 32-bit (ms) and 64-bit (100 ns) Scaling
			pOut->PicInfo.timeStamp = pOut->PicInfo.timeStamp * 2 * 10000;
		}
	}
	else
	{
		/* Retrieve Timestamp */
		if(pOut->PicInfo.flags & VDEC_FLAG_PICTURE_META_DATA_PRESENT)
		{
			sNum = (uint16_t) ( ( (pOut->PicInfo.picture_meta_payload & 0xFF) << 8) |
                                ((pOut->PicInfo.picture_meta_payload& 0xFF00) >> 8) );
			DtsFetchMdata(Ctx,sNum,pOut);
		}
	}
	return BC_STS_SUCCESS;
}

BC_STATUS DtsUpdateVidParams(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut)
{
	Ctx->VidParams.WidthInPixels = pOut->PicInfo.width;
	Ctx->VidParams.HeightInPixels = pOut->PicInfo.height;

	if (pOut->PicInfo.flags & VDEC_FLAG_INTERLACED_SRC)
		Ctx->VidParams.Progressive = FALSE;
	else
		Ctx->VidParams.Progressive = TRUE;

	return BC_STS_SUCCESS;
}


BOOL DtsCheckRptPic(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut)
{
	BOOL bRepeat = FALSE;
	uint8_t nCheckFlag = TOP_FIELD_FLAG;

	if (pOut->PicInfo.picture_number  <3)
		return FALSE;

	if (Ctx->bEOS == TRUE)
	{
		pOut->PicInfo.flags |= VDEC_FLAG_LAST_PICTURE;
		return TRUE;
	}

	if (Ctx->LastPicNum == pOut->PicInfo.picture_number && Ctx->LastSessNum == pOut->PicInfo.sess_num)
	{
		if (Ctx->VidParams.Progressive)
			nCheckFlag = PROGRESSIVE_FRAME_FLAG;
		else if((pOut->PicInfo.flags & VDEC_FLAG_BOTTOMFIELD) == VDEC_FLAG_BOTTOMFIELD)
			nCheckFlag = BOTTOM_FIELD_FLAG;
		else
			nCheckFlag = TOP_FIELD_FLAG;

		//Discard for PullDown
		int nShift = 2;
		uint8_t nFlag = Ctx->PullDownFlag;
		bool bFound = false;

		while(nFlag)
		{
			if((nFlag & 0x03) == nCheckFlag)
			{
				bFound = true;
				break;
			}
			nFlag = nFlag >> 2;
			nShift += 2;
		}

		if(!bFound)
			bRepeat = true;

		Ctx->PullDownFlag = Ctx->PullDownFlag >> nShift;
	}
	else
	{
		if (Ctx->VidParams.Progressive)
		{
			switch(pOut->PicInfo.pulldown)
			{
				case vdecFrame_X1:
					Ctx->PullDownFlag = 0x0003;  //Frame x 1 ==> 00000011
					break;
				case vdecFrame_X2:
					Ctx->PullDownFlag = 0x000f;  //Frame x 2 ==> 00001111
					break;
				case vdecFrame_X3:
					Ctx->PullDownFlag = 0x003f;  //Frame x 3 ==> 00111111
					break;
				case vdecFrame_X4:
					Ctx->PullDownFlag = 0x00ff;  //Frame x 4 ==> 11111111
					break;
				default:
					Ctx->PullDownFlag = 0x0003;  //Frame x 1 ==> 00000011
					break;
			}
		}
		else
		{
			switch(pOut->PicInfo.pulldown)
			{
				case vdecTop:
					Ctx->PullDownFlag = 0x0001;  //Top ==> 00000001
					break;
				case vdecBottom:
					Ctx->PullDownFlag = 0x0002;  //Bottom ==> 00000010
					break;
				case vdecTopBottom:
					Ctx->PullDownFlag = 0x0009;  //TopBottom ==> 00001001
					break;
				case vdecBottomTop:
					Ctx->PullDownFlag = 0x0006;  //BottomTop ==> 00000110
					break;
				case vdecTopBottomTop:
					Ctx->PullDownFlag = 0x0019;  //TopBottomTop ==> 00011001
					break;
				case vdecBottomTopBottom:
					Ctx->PullDownFlag = 0x0026;  //BottomTopBottom ==> 00100110
					break;
				default:
					Ctx->PullDownFlag = 0x0009;  //TopBottom ==> 00001001
					break;
			}
		}
		Ctx->EOSCnt = 0;
	}


	if (Ctx->bEOSCheck && !Ctx->bEOS)
	{
		if (bRepeat)
			Ctx->EOSCnt ++;

		if (Ctx->EOSCnt >= BC_EOS_PIC_COUNT)
		{
			Ctx->bEOS = true;
			pOut->PicInfo.flags |= VDEC_FLAG_LAST_PICTURE;
		}
	}
	Ctx->LastPicNum = pOut->PicInfo.picture_number;
	Ctx->LastSessNum = pOut->PicInfo.sess_num;

	return bRepeat;
}

static void DtsSetupProcOutInfo(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut, BC_IOCTL_DATA *pIo)
{
	if(!pOut || !pIo)
		return; // This is an internal function should never happen..

	if(Ctx->RegCfg.DbgOptions & BC_BIT(6))
	{
	/* Decoder PIC_INFO_ON mode, PIB is NOT embedded in frame */
		if(pIo->u.DecOutData.Flags & COMP_FLAG_PIB_VALID)
		{
			pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;
			DtsCopyAppPIB(Ctx, &pIo->u.DecOutData, pOut);
		}
		if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_ENC)
			pOut->PoutFlags |= BC_POUT_FLAGS_ENCRYPTED;

		if (pOut->PicInfo.flags & VDEC_FLAG_BOTTOMFIELD)
			pOut->PicInfo.flags |= VDEC_FLAG_INTERLACED_SRC;

		if (pOut->PicInfo.flags & VDEC_FLAG_INTERLACED_SRC)
			Ctx->VidParams.Progressive = FALSE;
		else
			Ctx->VidParams.Progressive = TRUE;
	}

	/* No change in Format Change method */
	if(pIo->u.DecOutData.Flags & COMP_FLAG_FMT_CHANGE)
	{
		if(pIo->u.DecOutData.Flags & COMP_FLAG_PIB_VALID)
		{
			pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;
			DtsCopyAppPIB(Ctx, &pIo->u.DecOutData, pOut);
		}else{
			DebugLog_Trace(LDIL_DBG,"Error: Can't handle F/C w/o PIB_VALID \n");
			return;
		}
		Ctx->HWOutPicHeight = pOut->PicInfo.height;
		// FW returns output picture's stride in PPB.resolution when Format changes.
		Ctx->HWOutPicWidth = DtsGetHWOutputStride(Ctx,(C011_PIB *)&(pIo->u.DecOutData.PibInfo));

		if (pOut->PicInfo.flags & VDEC_FLAG_BOTTOMFIELD)
			pOut->PicInfo.flags |= VDEC_FLAG_INTERLACED_SRC;

		pOut->PoutFlags |= BC_POUT_FLAGS_FMT_CHANGE;
		if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_VALID){
			DebugLog_Trace(LDIL_DBG,"Error: Data not expected with F/C \n");
			return;
		}
	}

	if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_VALID)
	{
		pOut->Ybuff = pIo->u.DecOutData.OutPutBuffs.YuvBuff;
		pOut->YBuffDoneSz = pIo->u.DecOutData.OutPutBuffs.YBuffDoneSz;
		pOut->YbuffSz = pIo->u.DecOutData.OutPutBuffs.UVbuffOffset;

		pOut->UVbuff = pIo->u.DecOutData.OutPutBuffs.YuvBuff +
					pIo->u.DecOutData.OutPutBuffs.UVbuffOffset;
		pOut->UVBuffDoneSz = pIo->u.DecOutData.OutPutBuffs.UVBuffDoneSz;
		pOut->UVbuffSz = (pIo->u.DecOutData.OutPutBuffs.YuvBuffSz -
					pIo->u.DecOutData.OutPutBuffs.UVbuffOffset);

		pOut->discCnt = pIo->u.DecOutData.BadFrCnt;
		if(Ctx->FixFlags & DTS_LOAD_FILE_PLAY_FW){
			/* Decoder PIC_INFO_OFF mode, PIB is embedded in frame */
			DtsGetPictureInfo(Ctx, pOut);
		}
	}
}

// Input Meta Data related funtions..
static BC_STATUS	DtsCreateMdataPool(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t		i, mpSz =0;
	DTS_INPUT_MDATA		*temp=NULL;

	mpSz = BC_INPUT_MDATA_POOL_SZ * sizeof(DTS_INPUT_MDATA);

	if( (Ctx->MdataPoolPtr = malloc(mpSz)) == NULL){
		DebugLog_Trace(LDIL_DBG,"Failed to Alloc mem\n");
		return BC_STS_INSUFF_RES;
	}

	memset(Ctx->MdataPoolPtr,0,mpSz);

	temp = (DTS_INPUT_MDATA*)Ctx->MdataPoolPtr;

	Ctx->MDFreeHead = Ctx->MDPendHead = Ctx->MDPendTail = NULL;

	for(i=0; i<BC_INPUT_MDATA_POOL_SZ; i++){
		temp->flink = Ctx->MDFreeHead;
		Ctx->MDFreeHead = temp;
		temp++;
	}

	/* Initialize MData Pending list Params */
	Ctx->MDPendHead = DTS_MDATA_PEND_LINK(Ctx);
	Ctx->MDPendTail = DTS_MDATA_PEND_LINK(Ctx);
	Ctx->InMdataTag = 0;

	return BC_STS_SUCCESS;
}

static void DtsMdataSetIntTag(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*temp)
{
	uint16_t stemp=0;
	DtsLock(Ctx);

	if(Ctx->InMdataTag == 0xFFFF){
		// Skip zero seqNum
		Ctx->InMdataTag = 0;
	}
	temp->IntTag = ++Ctx->InMdataTag & (DTS_MDATA_MAX_TAG|DTS_MDATA_TAG_MASK);
	stemp = (uint16_t)( temp->IntTag & DTS_MDATA_MAX_TAG );
	temp->Spes.SeqNum[0] = (uint8_t)(stemp & 0xFF);
	temp->Spes.SeqNum[1] = (uint8_t)((stemp & 0xFF00) >> 8);
	DtsUnLock(Ctx);
}

static uint32_t DtsMdataGetIntTag(DTS_LIB_CONTEXT *Ctx, uint16_t snum)
{
	uint32_t retTag=0;

	DtsLock(Ctx);
	retTag = ((Ctx->InMdataTag & DTS_MDATA_TAG_MASK) | snum) ;
	DtsUnLock(Ctx);

	return retTag;
}


static BC_STATUS DtsDeleteMdataPool(DTS_LIB_CONTEXT *Ctx)
{
	DTS_INPUT_MDATA		*temp=NULL;

	if(!Ctx || !Ctx->MdataPoolPtr){
		return BC_STS_INV_ARG;
	}
	DtsLock(Ctx);
	/* Remove all Pending elements */
	temp = Ctx->MDPendHead;

	while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)){
		DtsRemoveMdata(Ctx,temp,FALSE);
		temp = Ctx->MDPendHead;
	}

	/* Delete Free Pool */
	Ctx->MDFreeHead = NULL;

	if(Ctx->MdataPoolPtr){
		free(Ctx->MdataPoolPtr);
		Ctx->MdataPoolPtr = NULL;
	}

	DtsUnLock(Ctx);

	return BC_STS_SUCCESS;
}

/*===================== Externs =================================*/

//-----------------------------------------------------------------------
// Name: DtsGetContext
// Description: Get internal context structure from user handle.
//-----------------------------------------------------------------------
DTS_LIB_CONTEXT	*	DtsGetContext(HANDLE userHnd)
{
	DTS_LIB_CONTEXT	*AppCtx = (DTS_LIB_CONTEXT *)userHnd;

	if(!AppCtx)
		return NULL;

	if(AppCtx->Sig != LIB_CTX_SIG)
		return NULL;

	return AppCtx;
}

//-----------------------------------------------------------------------
// Name: DtsIsPend
// Description: Check for Pending request..
//-----------------------------------------------------------------------
BOOL DtsIsPend(DTS_LIB_CONTEXT	*Ctx)
{
	BOOL res =FALSE;
	DtsLock(Ctx);
	res = (Ctx->ProcOutPending);
	DtsUnLock(Ctx);

	return res;
}

//-----------------------------------------------------------------------
// Name: DtsDrvIoctl
// Description: Wrapper for windows IOCTL.
//-----------------------------------------------------------------------
BOOL DtsDrvIoctl
(
	  HANDLE	userHandle,
	  DWORD		dwIoControlCode,
	  LPVOID	lpInBuffer,
	  DWORD		nInBufferSize,
	  LPVOID	lpOutBuffer,
	  DWORD		nOutBufferSize,
	  LPDWORD	lpBytesReturned,
	  BOOL		Async
)
{

	DTS_LIB_CONTEXT	*	Ctx = DtsGetContext(userHandle);
	//unused DWORD	dwTimeout = 0;
	BC_STATUS sts;

	if( !Ctx )
		return FALSE;

	if(Ctx->Sig != LIB_CTX_SIG)
		return FALSE;

	// == FIX ME ==
	// We need to take care of Async ioctl.
	// WILL need to take care of lb bytes returned.
	// Check the existing code.
	//
	if(BC_STS_SUCCESS != (sts = DtsDrvCmd(Ctx,dwIoControlCode,Async,(BC_IOCTL_DATA *)lpInBuffer,FALSE)))
	{
		DebugLog_Trace(LDIL_DBG, "DtsDrvCmd Failed with status %d\n", sts);
		return FALSE;
	}

	return TRUE;
}

//------------------------------------------------------------------------
// Name: DtsDrvCmd
// Description: Wrapper for windows IOCTL using the internal pre-allocated
//              IOCTL_DATA structure. And waits for the completion incase
//				Async path.
//------------------------------------------------------------------------
BC_STATUS DtsDrvCmd(DTS_LIB_CONTEXT	*Ctx,
					DWORD Code,
					BOOL Async,
					BC_IOCTL_DATA *pIoData,
					BOOL Rel)
{
	int rc;
	//DWORD	BytesReturned=0;
	BOOL	locRel=FALSE;//,bRes=0;
	//DWORD	dwTimeout = 0;
	BC_IOCTL_DATA *pIo = NULL;
	BC_STATUS	Sts = BC_STS_SUCCESS ;
	int i = 30;

	if(!Ctx || !Ctx->DevHandle){
		DebugLog_Trace(LDIL_DBG,"Invalid arg..%p \n",Ctx);
		return BC_STS_INV_ARG;
	}

	if(!pIoData){
		if(! (pIo = DtsAllocIoctlData(Ctx)) ){
			return BC_STS_INSUFF_RES;
		}
		locRel = TRUE;
	}else{
		pIo = pIoData;
	}

	pIo->RetSts = BC_STS_SUCCESS;

	//  We need to take care of async completion.
	// == FIX ME ==
	// We allow only one FW command at a time for LINK
	// prevent additional fw commands from other threads
	if(Ctx->DevId == BC_PCI_DEVID_LINK && Code == BCM_IOC_FW_CMD) {
		while(Ctx->fw_cmd_issued && (i > 0)) {
			usleep(100);
			i--;
		}
		if (i == 0)
			return BC_STS_ERROR; // cannot issue second FW command while one is pending
		Ctx->fw_cmd_issued = true;
	}
	rc = ioctl(Ctx->DevHandle, Code, pIo);
	Sts = pIo->RetSts;

	if(Ctx->DevId == BC_PCI_DEVID_LINK && Code == BCM_IOC_FW_CMD) {
		Ctx->fw_cmd_issued = false; // FW commands complete synchronously
	}

	if (locRel || Rel)
		DtsRelIoctlData(Ctx, pIo);

	if (rc < 0) {
		DebugLog_Trace(LDIL_DBG,"IOCTL Command Failed %d cmd %x sts %d\n", rc, Code, Sts);
		return BC_STS_ERROR;
	}

	return Sts;
}

//------------------------------------------------------------------------
// Name: DtsRelIoctlData
// Description: Release IOCTL_DATA back to the pool.
//------------------------------------------------------------------------
void DtsRelIoctlData(DTS_LIB_CONTEXT *Ctx, BC_IOCTL_DATA *pIoData)
{
	DtsLock(Ctx);

	pIoData->next = Ctx->pIoDataFreeHd;
    Ctx->pIoDataFreeHd = pIoData;

	DtsUnLock(Ctx);
}

//------------------------------------------------------------------------
// Name: DtsAllocIoctlData
// Description: Acquire IOCTL_DATA From pool.
//------------------------------------------------------------------------
BC_IOCTL_DATA *DtsAllocIoctlData(DTS_LIB_CONTEXT *Ctx)
{
	BC_IOCTL_DATA *temp=NULL;
	DtsLock(Ctx);
    if((temp=Ctx->pIoDataFreeHd) != NULL){
        Ctx->pIoDataFreeHd = Ctx->pIoDataFreeHd->next;
        memset(temp,0,sizeof(*temp));
    }
	DtsUnLock(Ctx);
	if(!temp){
		DebugLog_Trace(LDIL_DBG,"DtsAllocIoctlData Error\n");
	}

    return temp;
}
//------------------------------------------------------------------------
// Name: DtsAllocMemPools
// Description: Allocate memory for application specific configs and RxBuffs
//------------------------------------------------------------------------
BC_STATUS DtsAllocMemPools(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t	i, Sz;
	DTS_MPOOL_TYPE	*mp;
	BC_IOCTL_DATA *pIoData;
	BC_STATUS	sts = BC_STS_SUCCESS;

	if(!Ctx){
		return BC_STS_INV_ARG;
	}

	DtsInitLock(Ctx);

	for(i=0; i< BC_IOCTL_DATA_POOL_SIZE; i++){
		pIoData = (BC_IOCTL_DATA *) malloc(sizeof(BC_IOCTL_DATA));
		if(!pIoData){
			DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: ioctlData pool Alloc Failed\n");
			return BC_STS_INSUFF_RES;
		}
		DtsRelIoctlData(Ctx,pIoData);
	}

	Ctx->pOutData = (BC_IOCTL_DATA *) malloc(sizeof(BC_IOCTL_DATA));
	if(!Ctx->pOutData){
		DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: pOutData \n");
		return BC_STS_INSUFF_RES;
	}

	if((Ctx->OpMode != DTS_PLAYBACK_MODE) && (Ctx->OpMode != DTS_DIAG_MODE))
		return BC_STS_SUCCESS;

	sts = DtsCreateMdataPool(Ctx);
	if(sts != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"InMdata PoolCreation Failed:%x\n",sts);
		return sts;
	}

	if(!(Ctx->CfgFlags & BC_MPOOL_INCL_YUV_BUFFS)){
		return BC_STS_SUCCESS;
	}
	Ctx->MpoolCnt	= BC_MAX_SW_VOUT_BUFFS;

	Ctx->Mpools = (DTS_MPOOL_TYPE*)malloc(Ctx->MpoolCnt * sizeof(DTS_MPOOL_TYPE));
	if(!Ctx->Mpools){
		DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: Mpool alloc failed\n");
		return BC_STS_INSUFF_RES;
	}

	memset(Ctx->Mpools,0,(Ctx->MpoolCnt * sizeof(DTS_MPOOL_TYPE)));

	DtsGetMaxSize(Ctx,&Sz);

	for(i=0; i<BC_MAX_SW_VOUT_BUFFS; i++){
		mp = &Ctx->Mpools[i];
		mp->type = BC_MEM_DEC_YUVBUFF |BC_MEM_USER_MODE_ALLOC;
		mp->sz = Sz;
		mp->buff = (uint8_t *)malloc(mp->sz);
		if(!mp->buff){
			DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: Mpool %x failed\n",mp->type);
			return BC_STS_INSUFF_RES;
		}
		//DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: Alloc Mpool %x Buff:%p\n",mp->type,mp->buff);

		memset(mp->buff,0,mp->sz);
	}

	return BC_STS_SUCCESS;
}
BC_STATUS DtsAllocMemPools_dbg(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t	i;
	BC_IOCTL_DATA *pIoData;

	if(!Ctx){
		return BC_STS_INV_ARG;
	}

	DtsInitLock(Ctx);

	for(i=0; i< BC_IOCTL_DATA_POOL_SIZE; i++){
		pIoData = (BC_IOCTL_DATA *) malloc(sizeof(BC_IOCTL_DATA));
		if(!pIoData){
			DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: ioctlData pool Alloc Failed\n");
			return BC_STS_INSUFF_RES;
		}
		DtsRelIoctlData(Ctx,pIoData);
	}

	Ctx->pOutData = (BC_IOCTL_DATA *) malloc(sizeof(BC_IOCTL_DATA));
	if(!Ctx->pOutData){
		DebugLog_Trace(LDIL_DBG,"DtsInitMemPools: pOutData \n");
		return BC_STS_INSUFF_RES;
	}

	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsReleaseMemPools
// Description: Release application specific allocations.
//------------------------------------------------------------------------
void DtsReleaseMemPools(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t	i,cnt=0;
	DTS_MPOOL_TYPE	*mp;
	BC_IOCTL_DATA *pIoData = NULL;


	if (!Ctx) //vgd
		return;

	/* need to release any user buffers mapped in driver
	 * or free(mp->buff) can hang under Linux and Mac OS X */
	pIoData = DtsAllocIoctlData(Ctx);
	if (pIoData) {
		pIoData->u.FlushRxCap.bDiscardOnly = TRUE;
		DtsDrvCmd(Ctx, BCM_IOC_FLUSH_RX_CAP, 0, pIoData, TRUE);
	}
	if (Ctx->MpoolCnt) {
		for (i = 0; i < Ctx->MpoolCnt; i++){
			mp = &Ctx->Mpools[i];
			if (mp->buff){
				//DebugLog_Trace(LDIL_DBG,"DtsReleaseMemPools: Free Mpool %x Buff:%p\n",mp->type,mp->buff);
				free(mp->buff);
			}
		}
		free(Ctx->Mpools);
	}

	/* Release IOCTL_DATA pool */
    while((pIoData=DtsAllocIoctlData(Ctx))!=NULL){
		free(pIoData);
		cnt++;
	}

	if(cnt != BC_IOCTL_DATA_POOL_SIZE){
		DebugLog_Trace(LDIL_DBG,"DtsReleaseMemPools: pIoData MemPool Leak: %d..\n",cnt);
	}

	if(Ctx->pOutData)
		free(Ctx->pOutData);

	if(Ctx->MdataPoolPtr)
		DtsDeleteMdataPool(Ctx);

	if (Ctx->VidParams.pMetaData)
		free(Ctx->VidParams.pMetaData);

	DtsDelLock(Ctx);
}
void DtsReleaseMemPools_dbg(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t	cnt=0;
	BC_IOCTL_DATA *pIoData = NULL;


	if(!Ctx || !Ctx->Mpools){
		return;
	}

	/* Release IOCTL_DATA pool */
    while((pIoData=DtsAllocIoctlData(Ctx))!=NULL){
		free(pIoData);
		cnt++;
	}

	if(cnt != BC_IOCTL_DATA_POOL_SIZE){
		DebugLog_Trace(LDIL_DBG,"DtsReleaseMemPools: pIoData MemPool Leak: %d..\n",cnt);
	}

	if(Ctx->pOutData)
		free(Ctx->pOutData);

}
//------------------------------------------------------------------------
// Name: DtsAddOutBuff
// Description: Pass on user mode allocated Rx buffs to driver.
//------------------------------------------------------------------------
BC_STATUS DtsAddOutBuff(DTS_LIB_CONTEXT *Ctx, uint8_t *buff, uint32_t BuffSz, uint32_t flags)
{

	uint32_t YbSz, UVbSz;
	BC_IOCTL_DATA	*pIocData = NULL;

	if(!Ctx || !buff)
		return BC_STS_INV_ARG;

	if(!(pIocData = DtsAllocIoctlData(Ctx))) {
		DebugLog_Trace(LDIL_DBG,"Cannot Allocate IOCTL data\n");
		return BC_STS_INSUFF_RES;
	}

	DtsGetMaxYUVSize(Ctx, &YbSz, &UVbSz);

	pIocData->u.RxBuffs.YuvBuff = buff;
	pIocData->u.RxBuffs.YuvBuffSz = YbSz + UVbSz;

	if(Ctx->b422Mode)
	{
		pIocData->u.RxBuffs.b422Mode = Ctx->b422Mode;
		pIocData->u.RxBuffs.UVbuffOffset = 0;
	}else{
		pIocData->u.RxBuffs.b422Mode = FALSE;
		pIocData->u.RxBuffs.UVbuffOffset = YbSz;
	}

	return DtsDrvCmd(Ctx,BCM_IOC_ADD_RXBUFFS,0, pIocData, TRUE);
}
//------------------------------------------------------------------------
// Name: DtsRelRxBuff
// Description: Release Rx buffers back to driver.
//------------------------------------------------------------------------
BC_STATUS DtsRelRxBuff(DTS_LIB_CONTEXT *Ctx, BC_DEC_YUV_BUFFS *buff, BOOL SkipAddBuff)
{
	BC_STATUS	sts;

	if(!Ctx || !buff)
	{
		DebugLog_Trace(LDIL_DBG,"DtsRelRxBuff: Invalid Arguments\n");
		return BC_STS_INV_ARG;
	}

	if(SkipAddBuff){
		DtsDecPend(Ctx);
		return BC_STS_SUCCESS;
	}

	Ctx->pOutData->u.RxBuffs.b422Mode = Ctx->b422Mode;
	Ctx->pOutData->u.RxBuffs.UVBuffDoneSz =0;
	Ctx->pOutData->u.RxBuffs.YBuffDoneSz=0;

	sts = DtsDrvCmd(Ctx,BCM_IOC_ADD_RXBUFFS,0, Ctx->pOutData, FALSE);

	if(sts != BC_STS_SUCCESS)
	{
		DebugLog_Trace(LDIL_DBG,"DtsRelRxBuff: Failed Sts:%x .. \n",sts);
	}

	if(sts == BC_STS_SUCCESS)
		DtsDecPend(Ctx);

	return sts;
}

//------------------------------------------------------------------------
// Name: DtsFetchOutInterruptible
// Description: Get uncompressed video data from hardware.
//              This function is interruptable procOut for
//              multi-threaded scenerios ONLY..
//------------------------------------------------------------------------
BC_STATUS DtsFetchOutInterruptible(DTS_LIB_CONTEXT *Ctx, BC_DTS_PROC_OUT *pOut, uint32_t dwTimeout)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	if(!Ctx ||  !pOut)
		return BC_STS_INV_ARG;

	if(DtsIsPend(Ctx)){
		DebugLog_Trace(LDIL_DBG,"DtsFetchOutInterruptible: ProcOutput Pending.. \n");
		return BC_STS_BUSY;
	}

	DtsIncPend(Ctx);

	memset(Ctx->pOutData,0,sizeof(*Ctx->pOutData));

	sts=DtsDrvCmd(Ctx,BCM_IOC_FETCH_RXBUFF,0,Ctx->pOutData,FALSE);

	if(sts == BC_STS_SUCCESS)
	{
		DtsSetupProcOutInfo(Ctx,pOut,Ctx->pOutData);
		sts = Ctx->pOutData->RetSts;
		if(sts != BC_STS_SUCCESS)
		{
			DtsDecPend(Ctx);
		}

	}else{
		DebugLog_Trace(LDIL_DBG,"DtsFetchOutInterruptible: Failed:%x\n",sts);
		DtsDecPend(Ctx);
	}

	if(!Ctx->CancelWaiting)
		return sts;

	/* Cancel request waiting.. Release Buffer back
	 * to driver and trigger Cancel wait.
	 */
	if(sts == BC_STS_SUCCESS){
		sts = BC_STS_IO_USER_ABORT;
		DtsRelRxBuff(Ctx,&Ctx->pOutData->u.RxBuffs,FALSE);
	}

	return sts;
}
//------------------------------------------------------------------------
// Name: DtsCancelFetchOutInt
// Description: Cancel Pending ProcOut Request..
//------------------------------------------------------------------------
BC_STATUS DtsCancelFetchOutInt(DTS_LIB_CONTEXT *Ctx)
{
	bool pend = false;
	uint32_t cnt;

	if(!(DtsIsPend(Ctx))){
		return BC_STS_SUCCESS;
	}

	Ctx->CancelWaiting = 1;

	/* Worst case scenerio the timeout should happen.. */
	cnt = BC_PROC_OUTPUT_TIMEOUT / 100;

	do{
		usleep(100 * 1000);
		pend = DtsIsPend(Ctx);
	}while( (pend) && (cnt--) );

	if(pend){
		DebugLog_Trace(LDIL_DBG,"DtsCancelFetchOutInt: TimeOut\n");
		Ctx->CancelWaiting = 0;
		return BC_STS_TIMEOUT;
	}

	Ctx->CancelWaiting = 0;

	return BC_STS_SUCCESS;
}

//------------------------------------------------------------------------
// Name: DtsMapYUVBuffs
// Description: Pass user mode pre-allocated buffers to driver for mapping.
//------------------------------------------------------------------------
BC_STATUS DtsMapYUVBuffs(DTS_LIB_CONTEXT *Ctx)
{
	uint32_t i;
	BC_STATUS	sts;
	DTS_MPOOL_TYPE	*mp;

	if (Ctx->bMapOutBufDone)
		return BC_STS_SUCCESS;
	if(!Ctx->Mpools || !(Ctx->CfgFlags & BC_MPOOL_INCL_YUV_BUFFS)){
		return BC_STS_SUCCESS;
	}

	for(i=0; i<Ctx->MpoolCnt; i++){
		mp = &Ctx->Mpools[i];
		if(mp->type & BC_MEM_DEC_YUVBUFF){
			sts = DtsAddOutBuff(Ctx, mp->buff,mp->sz, mp->type);
			if(sts != BC_STS_SUCCESS) {
				DebugLog_Trace(LDIL_DBG,"Map YUV buffs Failed [%x]\n",sts);
				return sts;
			}
		}
	}

	Ctx->bMapOutBufDone = true;
	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsInitInterface
// Description: Do application specific allocation and other initialization.
//------------------------------------------------------------------------
BC_STATUS DtsInitInterface(int hDevice, HANDLE *RetCtx, uint32_t mode)
{

	DTS_LIB_CONTEXT *Ctx = NULL;
	BC_STATUS	sts = BC_STS_SUCCESS;
	pthread_attr_t thread_attr;
	int ret = 0;

	Ctx = (DTS_LIB_CONTEXT*)malloc(sizeof(*Ctx));
	if(!Ctx){
		DebugLog_Trace(LDIL_DBG,"DtsInitInterface: Ctx alloc failed\n");
		return BC_STS_INSUFF_RES;
	}

	memset(Ctx,0,sizeof(*Ctx));

	/* Initialize Application specific params. */
	Ctx->Sig		= LIB_CTX_SIG;
	Ctx->DevHandle  = hDevice;
	Ctx->OpMode		= mode;
	Ctx->CfgFlags	= BC_DTS_DEF_CFG;
	Ctx->b422Mode	= OUTPUT_MODE420;

	Ctx->VidParams.MediaSubType = BC_MSUBTYPE_INVALID;
	Ctx->VidParams.StartCodeSz = 0;
	Ctx->VidParams.StreamType = BC_STREAM_TYPE_ES;
//	Ctx->InSampleCount = 0;
	/* Set Pixel height & width */
	if(Ctx->CfgFlags & BC_PIX_WID_1080){
		Ctx->VidParams.HeightInPixels = 1080;
		Ctx->VidParams.WidthInPixels = 1920;
	}else{
		Ctx->VidParams.HeightInPixels = 720;
		Ctx->VidParams.WidthInPixels = 1280;
	}
	Ctx->VidParams.pMetaData = NULL;

	sts = DtsAllocMemPools(Ctx);
	if(sts != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsAllocMemPools failed Sts:%d\n",sts);
		*RetCtx = (HANDLE)Ctx;
		return sts;
	}

	if(!(Ctx->CfgFlags & BC_ADDBUFF_MOVE)){
		sts = DtsMapYUVBuffs(Ctx);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsMapYUVBuffs failed Sts:%d\n",sts);
			*RetCtx = (HANDLE)Ctx;
			return sts;
		}
	}

	// Allocate circular buffer
	if(BC_STS_SUCCESS != txBufInit(&Ctx->circBuf, CIRC_TX_BUF_SIZE))
		sts = BC_STS_INSUFF_RES;

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&Ctx->htxThread, &thread_attr, txThreadProc, Ctx);
	pthread_attr_destroy(&thread_attr);

	ret = posix_memalign((void**)&Ctx->alignBuf, 128, ALIGN_BUF_SIZE);
	if(ret)
		sts = BC_STS_INSUFF_RES;

	*RetCtx = (HANDLE)Ctx;

	return sts;
}

//------------------------------------------------------------------------
// Name: DtsReleaseInterface
// Description: Do application specific Release and other initialization.
//------------------------------------------------------------------------
BC_STATUS DtsReleaseInterface(DTS_LIB_CONTEXT *Ctx)
{

	if(!Ctx)
		return BC_STS_INV_ARG;

	// Exit TX thread
	Ctx->txThreadExit = true;
	// wait to make sure the thread exited
	pthread_join(Ctx->htxThread, NULL);
	// de-Allocate circular buffer
	txBufFree(&Ctx->circBuf);
	Ctx->htxThread = 0;
	if(Ctx->alignBuf)
		free(Ctx->alignBuf);

	DtsReleaseMemPools(Ctx);

	if(Ctx->DevHandle != 0) //Zero if success
	{
		DtsReleaseUserHandle(Ctx);

		if(0 != close(Ctx->DevHandle))
			DebugLog_Trace(LDIL_DBG,"DtsDeviceClose: Close Handle Failed with error %d\n",errno);
	}

	DtsSetHwInitSts(BC_DIL_HWINIT_NOT_YET);

	DtsDelDilShMem();

	free(Ctx);

	return BC_STS_SUCCESS;

}

//------------------------------------------------------------------------
// Name: DtsReleaseUserHandle
// Description: Notfiy the driver to release the user handle
// Should be called right before close
//------------------------------------------------------------------------
BC_STATUS DtsReleaseUserHandle(DTS_LIB_CONTEXT* Ctx)
{
	BC_IOCTL_DATA	pIo;
	BC_STATUS		sts = BC_STS_SUCCESS;

	memset(&pIo, 0, sizeof(BC_IOCTL_DATA));

	if( (sts=DtsDrvCmd(Ctx,BCM_IOC_RELEASE,0,&pIo,FALSE)) != BC_STS_SUCCESS){
		DebugLog_Trace(LDIL_DBG,"DtsReleaseUserHandle: Ioctl failed: %d\n",sts);
		return sts;
	}

	return sts;
}

//------------------------------------------------------------------------
// Name: DtsNotifyOperatingMode
// Description: Notfiy the operating mode to driver.
//------------------------------------------------------------------------
BC_STATUS DtsNotifyOperatingMode(HANDLE hDevice,uint32_t Mode)
{
	BC_IOCTL_DATA			*pIocData = NULL;
	DTS_LIB_CONTEXT			*Ctx = NULL;
	BC_STATUS				sts = BC_STS_SUCCESS;

	DTS_GET_CTX(hDevice,Ctx);

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

	pIocData->u.NotifyMode.Mode = Mode ; /* Setting the 31st bit to indicate that this is not the timeout value */

	if( (sts=DtsDrvCmd(Ctx,BCM_IOC_NOTIFY_MODE,0,pIocData,FALSE)) != BC_STS_SUCCESS){
		DtsRelIoctlData(Ctx,pIocData);
		DebugLog_Trace(LDIL_DBG,"DtsNotifyMode: Ioctl failed: %d\n",sts);
		return sts;
	}

	DtsRelIoctlData(Ctx,pIocData);
	return sts;
}
//------------------------------------------------------------------------
// Name: DtsSetupConfiguration
// Description: Setup HW specific Configuration.
//------------------------------------------------------------------------
BC_STATUS DtsSetupConfig(DTS_LIB_CONTEXT *Ctx, uint32_t did, uint32_t rid, uint32_t FixFlags)
{
	Ctx->DevId = did;
	Ctx->hwRevId = rid;
	Ctx->FixFlags = FixFlags;

	if(Ctx->DevId == BC_PCI_DEVID_LINK){
		Ctx->RegCfg.DbgOptions = BC_DTS_DEF_OPTIONS_LINK;
	}else{
		Ctx->RegCfg.DbgOptions = BC_DTS_DEF_OPTIONS;
	}
	DtsGetBCRegConfig(Ctx);

	//Forcing the use of certificate in case of Link.

	if( (Ctx->DevId == BC_PCI_DEVID_LINK)&& (!(Ctx->RegCfg.DbgOptions & BC_BIT(5))) ){

		Ctx->RegCfg.DbgOptions |= BC_BIT(5);
	}


	Ctx->capInfo.ColorCaps.Count = 0;
	if (Ctx->DevId == BC_PCI_DEVID_LINK)
	{
		Ctx->capInfo.ColorCaps.Count =3;
		Ctx->capInfo.ColorCaps.OutFmt[0] = OUTPUT_MODE420;
		Ctx->capInfo.ColorCaps.OutFmt[1] = OUTPUT_MODE422_YUY2;
		Ctx->capInfo.ColorCaps.OutFmt[2] = OUTPUT_MODE422_UYVY;
		Ctx->capInfo.flags = PES_CONV_SUPPORT;

		//Decoder Capability
		Ctx->capInfo.DecCaps = BC_DEC_FLAGS_H264 | BC_DEC_FLAGS_MPEG2 | BC_DEC_FLAGS_VC1;
	}
	else if(Ctx->DevId == BC_PCI_DEVID_DOZER)
	{
		Ctx->capInfo.ColorCaps.Count =1;
		Ctx->capInfo.ColorCaps.OutFmt[0] = OUTPUT_MODE420;
		Ctx->capInfo.flags = PES_CONV_SUPPORT;

		//Decoder Capability
		Ctx->capInfo.DecCaps = BC_DEC_FLAGS_H264 | BC_DEC_FLAGS_MPEG2 | BC_DEC_FLAGS_VC1;
	}

	Ctx->capInfo.Reserved1 = NULL;

	return BC_STS_SUCCESS;
}

//------------------------------------------------------------------------
// Name: DtsGetBCRegConfig
// Description: Setup Register Sub-Key values.
//------------------------------------------------------------------------
BC_STATUS DtsGetBCRegConfig(DTS_LIB_CONTEXT	*Ctx)
{

	return BC_STS_NOT_IMPL;
}

int dtscallback(struct dl_phdr_info *info, size_t size, void *data)
{
    int ret=0,dilpath_len=0;

	//char* pSearchStr = (char*)info->dlpi_name;
	char * temp=NULL;
	//temp = strstr((char *)pSearchStr,(const char *)"/libdil.so");
	if(NULL != (temp= (char *)strstr(info->dlpi_name,(const char *)"/libcrystal.so"))){
		/*we found the loaded dil, set teh return value to non-zero so that
		 the callback won't be called anymore*/
		ret = 1;

	}

	if(ret!=0){
		dilpath_len = (temp-info->dlpi_name)+1; //we want the slash also to be copied
		strncpy((char*)data, info->dlpi_name,dilpath_len);

	}

    return dilpath_len;
}


//------------------------------------------------------------------------
// Name: DtsGetFirmwareFiles
// Description: Setup Firmware Filenames.
//------------------------------------------------------------------------
BC_STATUS DtsGetFirmwareFiles(DTS_LIB_CONTEXT *Ctx)
{
    int fwfile_len;
	char fwfile[MAX_PATH + 1];
	char fwfilepath[MAX_PATH + 1];
#ifndef __APPLE__
	const char fwdir[] = "/lib/firmware/";
#else
	const char fwdir[] = "/usr/lib/";
#endif

	if(Ctx->DevId == BC_PCI_DEVID_FLEA) {
        fwfile_len = strlen(FWBINFILE_70015);
        strncpy(fwfile, FWBINFILE_70015, fwfile_len);
    } else {
        fwfile_len = strlen(FWBINFILE_70012);
        strncpy(fwfile, FWBINFILE_70012, fwfile_len);
    }

	if ((strlen(fwdir) + fwfile_len) > (MAX_PATH + 1)) {
		DebugLog_Trace(LDIL_DATA,"DtsGetFirmwareFiles:Path is too large ....");
		return BC_STS_ERROR;
	}

	strncpy(fwfilepath, fwdir, strlen(fwdir) + 1);
    strncat(fwfilepath, fwfile, fwfile_len);
    fwfilepath[strlen(fwdir) + fwfile_len] = '\0';
    strncpy(Ctx->FwBinFile, fwfilepath, strlen(fwdir) + fwfile_len);

	return BC_STS_SUCCESS;

}

//------------------------------------------------------------------------
// Name: DtsAllocMdata
// Description: Get Mdata from free pool.
//------------------------------------------------------------------------
DTS_INPUT_MDATA	*DtsAllocMdata(DTS_LIB_CONTEXT *Ctx)
{
	DTS_INPUT_MDATA *temp = NULL;

	if(!Ctx)
		return temp;

	DtsLock(Ctx);
	if((temp=Ctx->MDFreeHead) != NULL)
	{
		Ctx->MDFreeHead = Ctx->MDFreeHead->flink;
		memset(temp, 0, sizeof(*temp));
	}
	else
	{
		//Use the Last Un-Fetched One
		DTS_INPUT_MDATA *last = NULL;
		last = Ctx->MDPendHead;

		//Check the Last Fetch Tag
		if((last) && (Ctx->MDLastFetchTag > (last->IntTag + MAX_DISORDER_GAP)))
		{
			//Remove
			DtsRemoveMdata(Ctx, last, FALSE);

			if((temp = Ctx->MDFreeHead) != NULL)
			{
				Ctx->MDFreeHead = Ctx->MDFreeHead->flink;
				memset(temp, 0, sizeof(*temp));
			}
		}
	}
	DtsUnLock(Ctx);

	return temp;
}
//------------------------------------------------------------------------
// Name: DtsFreeMdata
// Description: Free Mdata from to pool.
//------------------------------------------------------------------------
BC_STATUS DtsFreeMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata, BOOL sync)
{
	if(!Ctx || !Mdata){
		return BC_STS_INV_ARG;
	}
	if(sync)
		DtsLock(Ctx);
	Mdata->flink = Ctx->MDFreeHead;
	Ctx->MDFreeHead = Mdata;
	if(sync)
		DtsUnLock(Ctx);

	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsClrPendMdataList
// Description: Release all pending list back to free pool.
//------------------------------------------------------------------------
BC_STATUS DtsClrPendMdataList(DTS_LIB_CONTEXT *Ctx)
{
	DTS_INPUT_MDATA		*temp=NULL;
	int mdata_count = 0;

	if(!Ctx || !Ctx->MdataPoolPtr){
		return BC_STS_INV_ARG;
	}

	DtsLock(Ctx);
	/* Remove all Pending elements */
	temp = Ctx->MDPendHead;

	while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)){
		//DebugLog_Trace(LDIL_DBG,"Clearing PendMdata %p %x \n", temp->Spes.SeqNum, temp->IntTag);
		DtsRemoveMdata(Ctx,temp,FALSE);
		temp = Ctx->MDPendHead;
		mdata_count++;
	}

	if (mdata_count)
		DebugLog_Trace(LDIL_DBG,"Clearing %d PendMdata entries \n", mdata_count);

	DtsUnLock(Ctx);

	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsPendMdataGarbageCollect
// Description: Garbage Collect Meta Data. This funtion is only called
//              internel when we run out of Mdata.
//------------------------------------------------------------------------
BC_STATUS DtsPendMdataGarbageCollect(DTS_LIB_CONTEXT *Ctx)
{
	DTS_INPUT_MDATA		*temp=NULL;
	int mdata_count = 0;

	if(!Ctx || !Ctx->MdataPoolPtr){
		return BC_STS_INV_ARG;
	}

	DtsLock(Ctx);

	/* Collect garbage it */
	temp = Ctx->MDPendHead;
	while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)){
		//DebugLog_Trace(LDIL_DBG,"Clearing PendMdata %p %x \n", temp->Spes.SeqNum, temp->IntTag);
		if(mdata_count > (BC_INPUT_MDATA_POOL_SZ - BC_INPUT_MDATA_POOL_SZ_COLLECT)) {
			break;
		}
		DtsRemoveMdata(Ctx,temp,FALSE);
		temp = Ctx->MDPendHead;
		mdata_count++;
	}

	if (mdata_count)
		DebugLog_Trace(LDIL_DBG,"Clearing %d PendMdata entries \n", mdata_count);

	DtsUnLock(Ctx);

	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsInsertMdata
// Description: Insert Meta Data into list.
//------------------------------------------------------------------------
BC_STATUS DtsInsertMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata)
{
	if(!Ctx || !Mdata){
		return BC_STS_INV_ARG;
	}
	DtsLock(Ctx);
	Mdata->flink = DTS_MDATA_PEND_LINK(Ctx);
	Mdata->blink = Ctx->MDPendTail;
	Mdata->flink->blink = Mdata;
	Mdata->blink->flink = Mdata;
	DtsUnLock(Ctx);

	return BC_STS_SUCCESS;

}
//------------------------------------------------------------------------
// Name: DtsRemoveMdata
// Description: Remove Meta Data From List.
//------------------------------------------------------------------------
BC_STATUS DtsRemoveMdata(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA	*Mdata, BOOL sync)
{
	if(!Ctx || !Mdata){
		return BC_STS_INV_ARG;
	}

	if(sync)
		DtsLock(Ctx);
	if(Ctx->MDPendHead != DTS_MDATA_PEND_LINK(Ctx))
	{
		Mdata->flink->blink = Mdata->blink;
		Mdata->blink->flink = Mdata->flink;
	}
	if(sync)
		DtsUnLock(Ctx);

	return DtsFreeMdata(Ctx,Mdata,sync);
}

//------------------------------------------------------------------------
// Name: DtsFetchMdata
// Description: Get Input Meta Data.
//
// FIX_ME:: Fill the pout part after FW upgrade with SeqNum feature..
//------------------------------------------------------------------------
BC_STATUS DtsFetchMdata(DTS_LIB_CONTEXT *Ctx, uint16_t snum, BC_DTS_PROC_OUT *pout)
{
	uint32_t		InTag;
	DTS_INPUT_MDATA		*temp=NULL;
	BC_STATUS	sts = BC_STS_NO_DATA;
	int16_t tsnum = 0;
	uint32_t i = 0;

	if(!Ctx || !pout){
		return BC_STS_INV_ARG;
	}

	if(!snum){
		// Zero is not a valid SeqNum.
		pout->PicInfo.timeStamp = 0;
		DebugLog_Trace(LDIL_DBG,"ZeroSnum \n");
		return BC_STS_NO_DATA;
	}

	InTag = DtsMdataGetIntTag(Ctx,snum);
	temp = Ctx->MDPendHead;

	DtsLock(Ctx);
	while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)){
		if(temp->IntTag == InTag){
			pout->PicInfo.timeStamp = temp->appTimeStamp;
			sts = BC_STS_SUCCESS;
			DtsRemoveMdata(Ctx, temp, FALSE);

			//Reserve the Last Fetch Tag
			Ctx->MDLastFetchTag = InTag;
			break;
		}
		temp = temp->flink;
	}
	DtsUnLock(Ctx);
	// If we found a tag, clear out all the old entries - from (tag - 10) to (tag-110)
	// This is to work around the issue of lost pictures for which tags will never get freed
	if(sts == BC_STS_SUCCESS) {
		for(i = 0; i < 100; i++) {
			tsnum = snum - (10 + i);
			if(tsnum < 0)
				break;
			InTag = DtsMdataGetIntTag(Ctx, tsnum);
			temp = Ctx->MDPendHead;
			DtsLock(Ctx);
			while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)){
				if(temp->IntTag == InTag){
					DtsRemoveMdata(Ctx, temp, FALSE);
					break;
				}
				temp = temp->flink;
			}
			DtsUnLock(Ctx);
		}
	}

	return sts;
}

//------------------------------------------------------------------------
// Name: DtsFetchTimeStampMdata
// Description: Get the Timestamp from the Meta Data field with the specifc picture number.
//				Do not change the mdata list in any way.
//
//------------------------------------------------------------------------
BC_STATUS DtsFetchTimeStampMdata(DTS_LIB_CONTEXT *Ctx, uint16_t snum, uint64_t *TimeStamp)
{
	uint32_t InTag;
	DTS_INPUT_MDATA *temp=NULL;
	BC_STATUS	sts = BC_STS_NO_DATA;

	if(!Ctx) {
		return BC_STS_INV_ARG;
	}

	if(!snum) {
		/* Zero is not a valid SeqNum. */
		*TimeStamp = 0;
		return BC_STS_NO_DATA;
	}

	InTag = DtsMdataGetIntTag(Ctx, snum);
	temp = Ctx->MDPendHead;

	DtsLock(Ctx);
	while(temp && temp != DTS_MDATA_PEND_LINK(Ctx)) {
		if(temp->IntTag == InTag) {

			*TimeStamp = temp->appTimeStamp;
			/* DebugLog_Trace(LDIL_DBG, "Found entry for %x %x tstamp: %x\n", */
			/*	snum, temp->IntTag, pout->PicInfo.timeStamp); */
			sts = BC_STS_SUCCESS;
			break;
		}
		temp = temp->flink;
	}
	DtsUnLock(Ctx);

	return sts;
}

//------------------------------------------------------------------------
// Name: DtsPrepareMdata
// Description: Insert Meta Data..
//------------------------------------------------------------------------
BC_STATUS DtsPrepareMdata(DTS_LIB_CONTEXT *Ctx, uint64_t timeStamp, DTS_INPUT_MDATA **mData, uint8_t** ppData, uint32_t *pSize)
{
	DTS_INPUT_MDATA		*temp = NULL;
	BC_STATUS		ret;

	if( !mData || !Ctx)
		return BC_STS_INV_ARG;

	/* Alloc clears all fields */
	if( (temp = DtsAllocMdata(Ctx)) == NULL)
	{
		DebugLog_Trace(LDIL_DBG,"COULD not find free MDATA try again\n");
		ret = DtsPendMdataGarbageCollect(Ctx);
		if(ret != BC_STS_SUCCESS) {
			return BC_STS_BUSY;
		}
		if( (temp = DtsAllocMdata(Ctx)) == NULL) {
			DebugLog_Trace(LDIL_DBG,"COULD not find free MDATA finaly failed\n");
			return BC_STS_BUSY;
		}
	}
	/* Store all app data */
	DtsMdataSetIntTag(Ctx,temp);
	temp->appTimeStamp = timeStamp;

	/* Fill spes data.. */
	temp->Spes.StartCode[0] = 0;
	temp->Spes.StartCode[1] = 0;
	temp->Spes.StartCode[2] = 01;
	temp->Spes.StartCode[3] = 0xBD;
	temp->Spes.PacketLen = 0x07;
	temp->Spes.StartCodeEnd = 0x40;
	temp->Spes.Command = 0x0A;

	*mData = temp;
	*ppData = (uint8_t*)(&temp->Spes);
	*pSize = sizeof(temp->Spes);
	return BC_STS_SUCCESS;
}


//------------------------------------------------------------------------
// Name: DtsPrepareMdataASFHdr
// Description: Insert Meta Data..
//------------------------------------------------------------------------
BC_STATUS DtsPrepareMdataASFHdr(DTS_LIB_CONTEXT *Ctx, DTS_INPUT_MDATA *mData, uint8_t* buf)
{


		if(buf==NULL)
			return BC_STS_INSUFF_RES;

			buf[0]=0;
			buf[1] = 0;
			buf[2] = 01;
			buf[3] = 0xE0;
			buf[4] = 0x0;
			buf[5]=35;
			buf[6] =0x80;
			buf[7]=0;
			buf[8]= 0;
			buf[9]=0x5a;buf[10]=0x5a;buf[11]=0x5a;buf[12]=0x5a;
			buf[13]=0x0; buf[14]=0x0;buf[15]=0x0;buf[16]=0x20;
			buf[17]=0x0; buf[18]=0x0;buf[19]=0x0;buf[20]=0x9;
			buf[21]=0x5a; buf[22]=0x5a;buf[23]=0x5a;buf[24]=0x5a;
			buf[25]=0xBD;
			buf[26]=0x40;
			buf[27]=mData->Spes.SeqNum[0];buf[28]=mData->Spes.SeqNum[1];
			buf[29]=mData->Spes.Command;
			buf[30]=buf[31]=buf[32]=buf[33]=buf[34]=buf[35]=buf[36]=buf[37]=buf[38]=buf[39]=buf[40]=0x0;
			return BC_STS_SUCCESS;
}

void DtsUpdateInStats(DTS_LIB_CONTEXT	*Ctx, uint32_t	size)
{
	BC_DTS_STATS *pDtsStat = DtsGetgStats( );

	pDtsStat->ipSampleCnt++;
	pDtsStat->ipTotalSize += size;
	pDtsStat->TxFifoBsyCnt = 0;

// 	Ctx->InSampleCount ++;
// 	if (Ctx->InSampleCount > 65530)
// 		Ctx->InSampleCount = 1;

	return;
}

void DtsUpdateOutStats(DTS_LIB_CONTEXT	*Ctx, BC_DTS_PROC_OUT *pOut)
{
	uint32_t fr23_976 = 0;
	BOOL	rptFrmCheck = TRUE;

	BC_DTS_STATS *pDtsStat = DtsGetgStats( );

	if(pOut->PicInfo.flags & VDEC_FLAG_LAST_PICTURE)
	{
		pDtsStat->eosDetected = 1;
	}

	if(!Ctx->CapState){
		/* Capture did not started yet..*/
		memset(pDtsStat,0,sizeof(*pDtsStat));
		if(pOut->PoutFlags & BC_POUT_FLAGS_FMT_CHANGE){

			/* Get the frame rate from vdecResolution enums */
			switch(pOut->PicInfo.frame_rate)
			{
			case vdecRESOLUTION_1080p23_976:
				fr23_976 = 1;
				break;
			case vdecRESOLUTION_1080i29_97:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_1080i25:
				fr23_976 = 0;
				break;
			//For Zero Frame Rate
			case vdecRESOLUTION_480p0:
			case vdecRESOLUTION_480i0:
			case vdecRESOLUTION_576p0:
			case vdecRESOLUTION_720p0:
			case vdecRESOLUTION_1080i0:
			case vdecRESOLUTION_1080p0:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_1080i:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_720p59_94:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_720p50:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_720p:
				fr23_976 = 0;
				break;
			case vdecRESOLUTION_720p23_976:
				fr23_976 = 1;
				break;
			case vdecRESOLUTION_CUSTOM ://no change in frame rate
				fr23_976 = Ctx->prevFrameRate;
				break;
			default:
				fr23_976 = 1;
				break;
			}

			/* Get the present frame rate */
			Ctx->prevFrameRate = fr23_976;

			if( ((pOut->PicInfo.flags & VDEC_FLAG_FIELDPAIR) ||
				(pOut->PicInfo.flags & VDEC_FLAG_INTERLACED_SRC)) && (!fr23_976)){
				/* Interlaced.*/
				Ctx->CapState = 1;
			}else{
				/* Progressive */
				Ctx->CapState = 2;
			}
		}
		return;
	}

	if((!pOut->UVBuffDoneSz && !pOut->b422Mode) || (!pOut->YBuffDoneSz)) {
		pDtsStat->opFrameDropped++;
	}else{
		pDtsStat->opFrameCaptured++;
	}

	if(pOut->discCnt){
		pDtsStat->opFrameDropped += pOut->discCnt;
	}

#ifdef _ENABLE_CODE_INSTRUMENTATION_

	/*
	 * For code instrumentation to be enabled
	 * we have to have unencripted PIBs.
	 */

	//if(!(Ctx->RegCfg.DbgOptions & BC_BIT(6))){
	//	/* New Scheme PIB is NOT available to DIL */
	//	return;
	//}
#else
	if(!(Ctx->RegCfg.DbgOptions & BC_BIT(6))){
		/* New Scheme PIB is NOT available to DIL */
		return;
	}
#endif

	/* PIB is not valid.. */
	if(!(pOut->PoutFlags & BC_POUT_FLAGS_PIB_VALID))
	{
		pDtsStat->pibMisses++;
		return;
	}

  	/* detect Even Odd field consistancy for interlaced..*/
	if(Ctx->CapState == 1){
		if( !Ctx->PibIntToggle && !(pOut->PoutFlags & BC_POUT_FLAGS_FLD_BOT))	{
			// Previous ODD & Current Even --- OK
			Ctx->PibIntToggle = 1;
		} else if (Ctx->PibIntToggle && (pOut->PoutFlags & BC_POUT_FLAGS_FLD_BOT)){
			//Previous Even and Current ODD --- OK
			rptFrmCheck = FALSE;
			Ctx->PibIntToggle = 0;
		} else if(!Ctx->PibIntToggle && (pOut->PoutFlags & BC_POUT_FLAGS_FLD_BOT)) {
            // Successive ODD
			if(!pOut->discCnt){
				if(Ctx->prevPicNum == pOut->PicInfo.picture_number)	{
					DebugLog_Trace(LDIL_DBG,"Succesive Odd=%d\n", pOut->PicInfo.picture_number);
					pDtsStat->reptdFrames++;
					rptFrmCheck = FALSE;
				}
			}
		} else if(Ctx->PibIntToggle && !(pOut->PoutFlags & BC_POUT_FLAGS_FLD_BOT)) {
			//Successive EVEN..
			 if(!pOut->discCnt){
				if(Ctx->prevPicNum == pOut->PicInfo.picture_number)	{
					DebugLog_Trace(LDIL_DBG,"Succesive Even=%d\n", pOut->PicInfo.picture_number);
					pDtsStat->reptdFrames++;
					rptFrmCheck = FALSE;
				}

			}
		}
	}

	if(!rptFrmCheck){
		Ctx->prevPicNum = pOut->PicInfo.picture_number;
		return;
	}

	if(Ctx->prevPicNum == pOut->PicInfo.picture_number) {
		/* Picture Number repetetion..*/
		DebugLog_Trace(LDIL_DBG,"Repetition=%d\n", pOut->PicInfo.picture_number);
		pDtsStat->reptdFrames++;
	}

	if(((Ctx->prevPicNum +1) != pOut->PicInfo.picture_number)&& !(pOut->discCnt) ){
			/* Discontguous Picture Numbers Frames/PIBs got dropped..*/
			pDtsStat->discCounter = pOut->PicInfo.picture_number - Ctx->prevPicNum ;
	}

	Ctx->prevPicNum = pOut->PicInfo.picture_number;

	return;
}

/********************************************************************************/
/* TX Circular Buffer routines */
// Init the circular buffer to be on 128 byte boundary
BC_STATUS txBufInit(pTXBUFFER txBuf, uint32_t sizeInit)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	int ret = 0;
	if(txBuf->buffer != NULL)
		return BC_STS_INV_ARG;
	ret = posix_memalign((void**)&txBuf->buffer, 128, sizeInit);
	if(ret)
		return BC_STS_INSUFF_RES;
	if(txBuf->buffer != NULL)
	{
		txBuf->basePointer = txBuf->buffer;
		txBuf->endPointer = txBuf->basePointer + sizeInit - 1;
		txBuf->readPointer = txBuf->writePointer = 0;
		txBuf->freeSize = txBuf->totalSize = sizeInit;
		txBuf->busySize = 0;
		pthread_mutex_init(&txBuf->flushLock, NULL);
		pthread_mutex_init(&txBuf->pushpopLock, NULL);
	}
	else
		sts = BC_STS_INSUFF_RES;
	return sts;
}

// Push the number of bytes specified on to the circular buffer
// This routine copies the data so that the orginial buffer can be released

// Assume here that Flush of this buffer always happens from the same thread that does procinput
// So don't lock pushing of new data against flush

BC_STATUS txBufPush(pTXBUFFER txBuf, uint8_t* bufToPush, uint32_t sizeToPush)
{
	uint32_t mcpySz = 0, sizeTop = 0;
	uint8_t* bufRemain = bufToPush;

	if(txBuf == NULL || bufToPush == NULL)
		return BC_STS_INV_ARG;

	if(txBuf->freeSize < sizeToPush)
		return BC_STS_INSUFF_RES;

	// How much will fit before we need to wrap
	sizeTop = (uint32_t)(txBuf->endPointer - (txBuf->basePointer + txBuf->writePointer) + 1);
	if(sizeToPush <= sizeTop)
		mcpySz = sizeToPush;
	else
		mcpySz = sizeTop;

	memcpy(txBuf->basePointer + txBuf->writePointer, bufToPush, mcpySz);

	txBuf->writePointer = (txBuf->writePointer + mcpySz) % txBuf->totalSize;

	pthread_mutex_lock(&txBuf->pushpopLock);
	txBuf->busySize += mcpySz;
	txBuf->freeSize -= mcpySz;
	pthread_mutex_unlock(&txBuf->pushpopLock);

	if((sizeToPush - mcpySz) != 0)
	{
		// Can only get here if we wrap at the top
		// writePointer should be 0
		bufRemain += mcpySz;
		mcpySz = sizeToPush - mcpySz;
		sizeTop = txBuf->readPointer;
		memcpy(txBuf->basePointer, bufRemain, mcpySz);
		txBuf->writePointer = mcpySz;
		pthread_mutex_lock(&txBuf->pushpopLock);
		txBuf->busySize += mcpySz;
		txBuf->freeSize -= mcpySz;
		pthread_mutex_unlock(&txBuf->pushpopLock);
	}

	return BC_STS_SUCCESS;
}

// Pops data from the circular buffer
// Returns the number of bytes popped
// We have to do a copy since we require the HW buffer to be 128 byte aligned for Flea
BC_STATUS txBufPop(pTXBUFFER txBuf, uint8_t* bufToPop, uint32_t sizeToPop)
{
	uint32_t popSz = 0, sizeTop = 0;
	uint8_t* bufRemain = bufToPop;

	if(txBuf == NULL)
		return BC_STS_INV_ARG;

	pthread_mutex_lock(&txBuf->flushLock);

	if(sizeToPop > txBuf->busySize)
		return BC_STS_INV_ARG;

	sizeTop = (uint32_t)(txBuf->endPointer - (txBuf->basePointer + txBuf->readPointer) + 1);
	if(sizeToPop <= sizeTop)
		popSz = sizeToPop;
	else
		popSz = sizeTop;

	memcpy(bufToPop, txBuf->basePointer + txBuf->readPointer, popSz);

	txBuf->readPointer = (txBuf->readPointer + popSz) % txBuf->totalSize;

	pthread_mutex_lock(&txBuf->pushpopLock);
	txBuf->busySize -= popSz;
	txBuf->freeSize += popSz;
	pthread_mutex_unlock(&txBuf->pushpopLock);

	if((sizeToPop - popSz) != 0)
	{
		// Can only get here if we wrap at the top
		// readPointer should be 0
		bufRemain += popSz;
		popSz = sizeToPop - popSz;
		sizeTop = txBuf->writePointer;
		memcpy(bufRemain, txBuf->basePointer, popSz);
		txBuf->readPointer = popSz;
		pthread_mutex_lock(&txBuf->pushpopLock);
		txBuf->busySize -= popSz;
		txBuf->freeSize += popSz;
		pthread_mutex_unlock(&txBuf->pushpopLock);
	}

	pthread_mutex_unlock(&txBuf->flushLock);
	return BC_STS_SUCCESS;
}

// Assume here that Flush of this buffer always happens from the same thread that does procinput
// So don't lock pushing of new data against flush
BC_STATUS txBufFlush(pTXBUFFER txBuf)
{
	if(txBuf->buffer == NULL)
		return BC_STS_INV_ARG;
	pthread_mutex_lock(&txBuf->flushLock);
	txBuf->readPointer = txBuf->writePointer = 0;
	txBuf->freeSize = txBuf->totalSize;
	txBuf->busySize = 0;
	pthread_mutex_unlock(&txBuf->flushLock);
	return BC_STS_SUCCESS;
}

BC_STATUS txBufFree(pTXBUFFER txBuf)
{
	if(txBuf->buffer == NULL)
		return BC_STS_INV_ARG;
	txBuf->basePointer = NULL;
	txBuf->endPointer = NULL;
	txBuf->readPointer = txBuf->writePointer = 0;
	txBuf->freeSize = 0;
	txBuf->busySize = 0;
	free(txBuf->buffer);
	pthread_mutex_destroy(&txBuf->flushLock);
	pthread_mutex_destroy(&txBuf->pushpopLock);
	return BC_STS_SUCCESS;
}

// TX Thread
// This thread has dual purpose. First is to send TX data. Second is to detect if we have restarted from any suspend/hibernate action
// and to restore the HW state
void * txThreadProc(void *ctx)
{
	DTS_LIB_CONTEXT* Ctx = (DTS_LIB_CONTEXT*)ctx;
	uint8_t* localBuffer;
	uint32_t szDataToSend;
	BC_STATUS sts;
	uint32_t dramOff;
	uint8_t encrypted = 0;
	HANDLE hDevice = (HANDLE)Ctx;
	BC_DTS_STATUS pStat;
	int ret = 0;
	uint32_t waitForPictCount = 0;
	uint32_t numPicCaptured = 0;

	ret = posix_memalign((void**)&localBuffer, 128, CIRC_TX_BUF_SIZE);
	if(ret)
		return FALSE;

	while(!Ctx->txThreadExit)
	{
		// First check the status of the HW
		// Get the real HW free size and also mark as we want TX information only
		pStat.cpbEmptySize = (0x3 << 31);

		sts = DtsGetDriverStatus(hDevice, &pStat);
		if(sts != BC_STS_SUCCESS)
		{
			pStat.cpbEmptySize = 0;
			DebugLog_Trace(LDIL_ERR,"txThreadProc: Got status %d from GetDriverStatus\n", sts);
			usleep(2 * 1000);
			continue;
		}

		//DebugLog_Trace(LDIL_ERR,"txThreadProc: Got hw size %u and data size %u\n", pStat.cpbEmptySize, Ctx->circBuf.busySize);

		if(pStat.PowerStateChange == BC_HW_SUSPEND)
		{
			// HW is in suspend mode, sleep 30 ms and then try again
			usleep(30 * 1000);
			continue;
		}

		// hack for indicating EOS when the HW does not signal one
		// We will check if the HW does not produce a picture for 1s and does not signal EOS either
		// This way exit maximum in 1s
		if(Ctx->bEOSCheck)
		{
			if(numPicCaptured == pStat.FramesCaptured)
				waitForPictCount++;

			if(waitForPictCount >= BC_EOS_PIC_COUNT)
				Ctx->bEOS = true;

			usleep(30 * 1000);
		}
		else
			waitForPictCount = 0;

		if(numPicCaptured != pStat.FramesCaptured)
		{
			waitForPictCount = 0;
			numPicCaptured = pStat.FramesCaptured;
		}

		if(pStat.PowerStateChange == BC_HW_RESUME)
		{
			DebugLog_Trace(LDIL_ERR,"Trying to resume from S3/S5\n");
			// HW is up, but needs to be initialized
			DtsSetCoreClock(hDevice, 180); // For LINK
			sts = DtsSetupHardware(hDevice, true);
			if(sts != BC_STS_SUCCESS)
			{
				// At this point we are dead. Can't do much'
				DebugLog_Trace(LDIL_ERR,"Cannot Recover from S3/S5 RESUME SetupHardware failed %d\n", sts);
				usleep(1000 * 1000);
				continue; // Try again and pray for the best
			}
			Ctx->State = BC_DEC_STATE_CLOSE; // Because the HW was reset below us
			sts = DtsOpenDecoder(hDevice, 0);
			if(sts != BC_STS_SUCCESS)
			{
				// At this point we are dead. Can't do much'
				DebugLog_Trace(LDIL_ERR,"Cannot Recover from S3/S5 RESUME OpenDecoder failed %d\n", sts);
				usleep(1000 * 1000);
				continue; // Try again and pray for the best
			}
			sts = DtsStartDecoder(hDevice);
			if(sts != BC_STS_SUCCESS)
			{
				// At this point we are dead. Can't do much'
				DebugLog_Trace(LDIL_ERR,"Cannot Recover from S3/S5 RESUME StartDecoder failed %d\n", sts);
				usleep(1000 * 1000);
				continue; // Try again and pray for the best
			}
			sts = DtsStartCapture(hDevice);
			if(sts != BC_STS_SUCCESS)
			{
				// At this point we are dead. Can't do much'
				DebugLog_Trace(LDIL_ERR,"Cannot Recover from S3/S5 RESUME StartCapture failed %d\n", sts);
				usleep(1000 * 1000);
				continue; // Try again and pray for the best
			}
			// Force sending SPS/PPS previously stored
			DtsClrPendMdataList(Ctx);
			Ctx->LastPicNum = -1;
			Ctx->LastSessNum = -1;
			Ctx->EOSCnt = 0;
			Ctx->DrvStatusEOSCnt = 0;
			Ctx->bEOS = FALSE;
			Ctx->PESConvParams.m_lStartCodeDataSize = 0;

			Ctx->PESConvParams.m_bAddSpsPps = true;
			// Throw away any potential partial data, since we need a complete picture to start decoding
			txBufFlush(&Ctx->circBuf);
			// But in case we were already in the mode to be hunting for EOS
			// and did not send it to HW, resend it so the playback can end gracefully
			if(Ctx->bEOSCheck)
				DtsSendEOS(hDevice, 0);
			DebugLog_Trace(LDIL_ERR,"Resume from S3/S5 Done\n");
		}

		// Check if we have data to send.
		if(Ctx->circBuf.busySize != 0)
		{
			if(pStat.cpbEmptySize == 0)
			{
				usleep(3000);
				continue;
			}

			if(Ctx->circBuf.busySize < pStat.cpbEmptySize)
				szDataToSend = Ctx->circBuf.busySize;
			else
				szDataToSend = pStat.cpbEmptySize;
			if(BC_STS_SUCCESS != txBufPop(&Ctx->circBuf, localBuffer, szDataToSend)) {
				usleep(5 * 1000);
				continue;
			}
			if(Ctx->VidParams.VideoAlgo == BC_VID_ALGO_VC1MP)
				encrypted |= 0x2;
			sts = DtsTxDmaText(hDevice, localBuffer, szDataToSend, &dramOff, encrypted);
			if(sts == BC_STS_SUCCESS)
				DtsUpdateInStats(Ctx, szDataToSend);
			else
			{
				// signal error to the next procinput
				DebugLog_Trace(LDIL_ERR,"txThreadProc: Got status %d from TxDmaText\n", sts);
			}
		} else
			usleep(5 * 1000);
	}

	free(localBuffer);
	localBuffer = NULL;
	return FALSE;
}

DRVIFLIB_INT_API BC_STATUS DtsGetHWFeatures(uint32_t *pciids)
{
	int drvHandle = -1;
	BC_IOCTL_DATA pIo;
	int rc;

	memset(&pIo, 0, sizeof(BC_IOCTL_DATA));

	drvHandle =open(CRYSTALHD_API_DEV_NAME, O_RDWR);
	if(drvHandle < 0)
	{
		DebugLog_Trace(LDIL_ERR,"DtsGetHWFeatures: Create File Failed\n");
		return BC_STS_ERROR;
	}

	pIo.u.pciCfg.Offset = 0;
	pIo.u.pciCfg.Size = 4;

	rc = ioctl(drvHandle, BCM_IOC_RD_PCI_CFG, &pIo);
	if(rc < 0){
		DebugLog_Trace(LDIL_ERR,"ioctl to get HW features failed\n");
		close(drvHandle);
		return BC_STS_ERROR;
	}

	if(pIo.RetSts == BC_STS_SUCCESS) {
		*pciids = pIo.u.pciCfg.pci_cfg_space[0] |
					(pIo.u.pciCfg.pci_cfg_space[1] << 8) |
					(pIo.u.pciCfg.pci_cfg_space[2] << 16) |
					(pIo.u.pciCfg.pci_cfg_space[3] << 24);
		//*pciids = *(uint32_t*)pIo.u.pciCfg.pci_cfg_space;
		close(drvHandle);
		return BC_STS_SUCCESS;
	}
	else {
		DebugLog_Trace(LDIL_ERR, "error in getting pciids\n");
		close(drvHandle);
		return BC_STS_ERROR;
	}
}

/*====================== Debug Routines ========================================*/
//
// Test routine to verify mdata functions ..
//
void DtsTestMdata(DTS_LIB_CONTEXT	*gCtx)
{
	uint32_t					i;
	BC_STATUS			sts = BC_STS_SUCCESS;
	DTS_INPUT_MDATA		*im = NULL;
	uint8_t 					*temp;
	uint32_t					ulSize;


	//sts = DtsCreateMdataPool(gCtx);
	//if(sts != BC_STS_SUCCESS){
	//	DebugLog_Trace(LDIL_DBG,"PoolCreation Failed:%x\n",sts);
	//	return;
	//}
	//DebugLog_Trace(LDIL_DBG,"PoolCreation done\n");

	DebugLog_Trace(LDIL_DBG,"Inserting Elements for Sequential Fetch..\n");
	//gCtx->InMdataTag = 0x11;
	//for(i=0x11; i < 0x21; i++){
	//gCtx->InMdataTag = 0;
	for(i=0; i < 64; i++){
		sts = DtsPrepareMdata(gCtx,i, &im, &temp, &ulSize);
		if(sts != BC_STS_SUCCESS){
			DtsFreeMdata(gCtx,im,TRUE);
			DebugLog_Trace(LDIL_DBG,"DtsPrepareMdata Failed:%x\n",sts);
			return;
		}
		DtsInsertMdata(gCtx,im);
	}

//	DtsFetchMdata(gCtx,10,&gpout);
#if 1
	BC_DTS_PROC_OUT		gpout;

	DebugLog_Trace(LDIL_DBG,"Fetch Begin\n");
	//for(i=0x12; i < 0x22; i++){
	for(i=1; i < 64; i++){
		sts = DtsFetchMdata(gCtx,i,&gpout);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsFetchMdata Failed:%x SNum:%x \n",sts,i);
			//return;
		}
	}
#endif
//	DebugLog_Trace(LDIL_DBG,"Deleting Pool...\n");
//	DtsDeleteMdataPool(gCtx);

}

BOOL DtsDbgCheckPointers(DTS_LIB_CONTEXT *Ctx,BC_IOCTL_DATA *pIo)
{
	uint32_t		i;
	BOOL	bRet = FALSE;
	DTS_MPOOL_TYPE	*mp;

	if(!Ctx->Mpools || !(Ctx->CfgFlags & BC_MPOOL_INCL_YUV_BUFFS)){
		return FALSE;
	}

	for(i=0; i<Ctx->MpoolCnt; i++){
		mp = &Ctx->Mpools[i];
		if(mp->type & BC_MEM_DEC_YUVBUFF){
			if(mp->buff == pIo->u.DecOutData.OutPutBuffs.YuvBuff){
				bRet = TRUE;
				break;
			}
		}
	}

	return bRet;

}
