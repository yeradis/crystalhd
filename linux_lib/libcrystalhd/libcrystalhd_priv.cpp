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
#include "libcrystalhd_priv.h"

#define	 BC_EOS_DETECTED		0xffffffff

/*============== Global shared area usage ======================*/
/* Global mode settings */
/*	
	Bit 0 (LSB)	- Play Back mode 
	Bit 1		- Diag mode
	Bit 2		- Monitor mode 
	Bit 3		- HwInit mode 
	bit 5       - Hwsetup in progress
*/

/* struct _bc_dil_glob_s{
	uint32_t gDilOpMode;
	uint32_t gHwInitSts;
	BC_DTS_STATS stats;
} bc_dil_glob = {0,0,{0,0}};*/

#ifdef _USE_SHMEM_
bc_dil_glob_s *bc_dil_glob_ptr=NULL;

BOOL glob_mode_valid=TRUE;
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
			DebugLog_Trace(LDIL_DBG,"DtsCreateShMem:shmem lareday exists :%d\n",errno);
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
	if((int)bc_dil_glob_ptr==-1) {
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
				DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:deleted shmem segment ...\n");
				return BC_STS_ERROR;
			
		} else{
			DebugLog_Trace(LDIL_DBG,"DtsDelDilShMem:unable to delete shmem segment ...\n");
		}

	}
	return BC_STS_SUCCESS;


}
#else
struct _bc_dil_glob_s{
	uint32_t gDilOpMode;
	uint32_t gHwInitSts;
	BC_DTS_STATS stats;
} bc_dil_glob = {0,0,{0,0}};
#endif


uint32_t DtsGetOPMode( void )
{
#ifdef _USE_SHMEM_
	return  bc_dil_glob_ptr->gDilOpMode;
#else
	return bc_dil_glob.gDilOpMode;
#endif


}

void DtsSetOPMode( uint32_t value )
{
#ifdef _USE_SHMEM_
	bc_dil_glob_ptr->gDilOpMode = value;
#else
	bc_dil_glob.gDilOpMode = value;
#endif
}

uint32_t DtsGetHwInitSts( void )
{
#ifdef _USE_SHMEM_
	return bc_dil_glob_ptr->gHwInitSts;
#else
	return bc_dil_glob.gHwInitSts;
#endif
}

void DtsSetHwInitSts( uint32_t value )
{
#ifdef _USE_SHMEM_
	bc_dil_glob_ptr->gHwInitSts = value;
#else
	bc_dil_glob.gHwInitSts = value;
#endif
}
void DtsRstStats( void ) 
{
#ifdef _USE_SHMEM_
	memset(&bc_dil_glob_ptr->stats, 0, sizeof(bc_dil_glob_ptr->stats));
#else
	memset(&bc_dil_glob.stats, 0, sizeof(bc_dil_glob.stats));
#endif
}

BC_DTS_STATS * DtsGetgStats ( void )
{
#ifdef _USE_SHMEM_
	return &bc_dil_glob_ptr->stats;
#else
	return &bc_dil_glob.stats;
#endif

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
	pthread_mutex_init(&Ctx->thLock, NULL);

}
static void DtsDelLock(DTS_LIB_CONTEXT	*Ctx)
{
	pthread_mutex_destroy(&Ctx->thLock);
	
}
static void DtsLock(DTS_LIB_CONTEXT	*Ctx)
{
	pthread_mutex_lock(&Ctx->thLock);
}
static void DtsUnLock(DTS_LIB_CONTEXT	*Ctx)
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
	BC_PIC_INFO_BLOCK	*srcPib = &decOut->PibInfo;
	BC_PIC_INFO_BLOCK	*dstPib = &pOut->PicInfo;
	uint16_t					sNum = 0;

	memcpy(dstPib, srcPib,sizeof(*dstPib));

	/* Calculate the appropriate source width */
	if(dstPib->width > 1280) 	
		Ctx->picWidth = 1920;			
	else if(dstPib->width > 720) 	
		Ctx->picWidth = 1280;			
	else 
		Ctx->picWidth = 720;

	Ctx->picHeight = dstPib->height;
	if (Ctx->picHeight == 1088) {
		Ctx->picHeight = 1080;
	}
	/* FIX_ME:: Add extensions part.. */

	/* Retrieve Timestamp */
	if(srcPib->flags & VDEC_FLAG_PICTURE_META_DATA_PRESENT){ 
		sNum = (uint16_t) ( ((srcPib->picture_meta_payload & 0xFF) << 8) |
						    ((srcPib->picture_meta_payload& 0xFF00) >> 8) );
		DtsFetchMdata(Ctx,sNum,pOut);
	}
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
		for(i=0; i<128; i++) {
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
	//unused BC_STATUS			sts = BC_STS_SUCCESS;
	uint8_t*			pPicInfoLine = NULL;
	uint32_t			PictureNumber = 0;
	uint32_t			PicInfoLineNum; 
	
	if (Ctx->DevId == BC_PCI_DEVID_FLEA)
	{
		PicInfoLineNum = *(ULONG *)(pOut->Ybuff);		
	}
	else if (Ctx->b422Mode == OUTPUT_MODE422_YUY2) 
	{
		PicInfoLineNum = ((ULONG)(*(pOut->Ybuff + 6)) & 0xff)
						| (((ULONG)(*(pOut->Ybuff + 4)) << 8)  & 0x0000ff00)
						| (((ULONG)(*(pOut->Ybuff + 2)) << 16) & 0x00ff0000)
						| (((ULONG)(*(pOut->Ybuff + 0)) << 24) & 0xff000000);		
	} 
	else if (Ctx->b422Mode == OUTPUT_MODE422_UYVY) 
	{
		PicInfoLineNum = ((ULONG)(*(pOut->Ybuff + 7)) & 0xff)
						| (((ULONG)(*(pOut->Ybuff + 5)) << 8)  & 0x0000ff00)
						| (((ULONG)(*(pOut->Ybuff + 3)) << 16) & 0x00ff0000)
						| (((ULONG)(*(pOut->Ybuff + 1)) << 24) & 0xff000000);
	} 
	else 
	{
		PicInfoLineNum = ((ULONG)(*(pOut->Ybuff + 3)) & 0xff)
						| (((ULONG)(*(pOut->Ybuff + 2)) << 8)  & 0x0000ff00)
						| (((ULONG)(*(pOut->Ybuff + 1)) << 16) & 0x00ff0000)
						| (((ULONG)(*(pOut->Ybuff + 0)) << 24) & 0xff000000);
	}
	
	if (PicInfoLineNum == BC_EOS_DETECTED)  // EOS
	{
		memcpy((uint32_t*)&pOut->PicInfo,(uint32_t*)(pOut->Ybuff + 4), sizeof(BC_PIC_INFO_BLOCK));
		if (pOut->PicInfo.flags & VDEC_FLAG_EOS)
		{
			Ctx->bEOS = true;
			Ctx->pOutData->RetSts = BC_STS_NO_DATA;
			return BC_STS_NO_DATA;
		}
	}
	/*
	-- To take care of 16 byte alignment the firmware might put extra 
	-- line so that the PIB starts with a line boundary. We will need to 
	-- have additional checks for the following condition to take care of 
	-- extra lines.
	*/

	if( ( (PicInfoLineNum != Ctx->picHeight) && (PicInfoLineNum != (Ctx->picHeight+1))) && 
		( (PicInfoLineNum != Ctx->picHeight/2) && (PicInfoLineNum != (Ctx->picHeight+1)/2)) )
	{	
		return BC_STS_IO_XFR_ERROR;
	}

	if (Ctx->b422Mode) {
		pPicInfoLine = pOut->Ybuff + PicInfoLineNum * Ctx->picWidth * 2;
	} else {
		pPicInfoLine = pOut->Ybuff + PicInfoLineNum * Ctx->picWidth;
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

	if(PictureNumber & 0x40000000)
	{
		PictureNumber &= ~0x40000000;
		pOut->PoutFlags |= BC_POUT_FLAGS_FLD_BOT;
	}
	if(PictureNumber & 0x80000000)
	{
		PictureNumber &= ~0x80000000;
		pOut->PoutFlags |= BC_POUT_FLAGS_ENCRYPTED;
	}
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

	switch(pOut->PicInfo.frame_rate)
	{
		case vdecRESOLUTION_480i0:
		case vdecRESOLUTION_1080i0:
		case vdecRESOLUTION_1080i29_97:
		case vdecRESOLUTION_1080i25:
		case vdecRESOLUTION_1080i:
		case vdecRESOLUTION_480i:
		case vdecRESOLUTION_NTSC:
		case vdecRESOLUTION_PAL1:
			Ctx->VidParams.Progressive = FALSE;
			break;
		default:
			Ctx->VidParams.Progressive = TRUE;
			break;
	}	
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
		Ctx->FlushIssued = FALSE;
		return TRUE;
	}

	if (Ctx->LastPicNum == pOut->PicInfo.picture_number && Ctx->LastSessNum == pOut->PicInfo.sess_num)
	{
		if (Ctx->VidParams.Progressive)
			nCheckFlag = PROGRESSIVE_FRAME_FLAG;
		else if (pOut->PoutFlags & BC_POUT_FLAGS_FLD_BOT)
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
		Ctx->eosCnt = 0;
	}

	
	if (Ctx->FlushIssued)
	{
		if (bRepeat == TRUE)
			Ctx->eosCnt ++;

		if (Ctx->eosCnt >= BC_EOS_PIC_COUNT) 
		{
			Ctx->bEOS = TRUE;
			pOut->PicInfo.flags |= VDEC_FLAG_LAST_PICTURE;
			Ctx->FlushIssued = FALSE;
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

	if(Ctx->RegCfg.DbgOptions & BC_BIT(6)){
	/* Decoder PIC_INFO_ON mode, PIB is NOT embedded in frame */
		if(pIo->u.DecOutData.Flags & COMP_FLAG_PIB_VALID){
			pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;
			DtsCopyAppPIB(Ctx, &pIo->u.DecOutData, pOut);
		}
		if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_ENC){
			pOut->PoutFlags |= BC_POUT_FLAGS_ENCRYPTED;
		}
		if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_BOT){
			pOut->PoutFlags |= BC_POUT_FLAGS_FLD_BOT;
		}
	}

	/* No change in Format Change method */
	if(pIo->u.DecOutData.Flags & COMP_FLAG_FMT_CHANGE){
		if(pIo->u.DecOutData.Flags & COMP_FLAG_PIB_VALID){
			pOut->PoutFlags |= BC_POUT_FLAGS_PIB_VALID;
			DtsCopyAppPIB(Ctx, &pIo->u.DecOutData, pOut);
		}else{
			DebugLog_Trace(LDIL_DBG,"Error: Can't hadle F/C w/o PIB_VALID \n");
			return;
		}
		if(0 == (Ctx->picWidth = DtsGetWidthfromResolution(Ctx, pIo->u.DecOutData.PibInfo.frame_rate)))
			Ctx->picWidth = pIo->u.DecOutData.PibInfo.width;
		Ctx->picHeight = pIo->u.DecOutData.PibInfo.height;
		pOut->PoutFlags |= BC_POUT_FLAGS_FMT_CHANGE;
		pOut->PicInfo.frame_rate = pIo->u.DecOutData.PibInfo.frame_rate;
		DebugLog_Trace(LDIL_DBG,"FormatCh:Height:%x Width:%x Res:%x\n",pOut->PicInfo.height,pOut->PicInfo.width,pOut->PicInfo.frame_rate);
		if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_VALID){
			DebugLog_Trace(LDIL_DBG,"Error: Data not expected with F/C \n");
			return;
		}
	}
	
	if(pIo->u.DecOutData.Flags & COMP_FLAG_DATA_VALID){
		
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
	
	DebugLog_Trace(LDIL_DBG,"Mdata Pool Created...\n");

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

	while(temp != DTS_MDATA_PEND_LINK(Ctx)){
		DebugLog_Trace(LDIL_DBG,"Clearing InMdata %p %x \n", temp->Spes.SeqNum, temp->IntTag);
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
	
	DebugLog_Trace(LDIL_DBG,"Deleted Mdata Pool...\n");
	
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

	if( !Ctx )
		return FALSE;
	
	if(Ctx->Sig != LIB_CTX_SIG)
		return FALSE;

	// == FIX ME ==
	// We need to take care of Async ioctl.
	// WILL need to take care of lb bytes returned.
	// Check the existing code.
	//
	if(BC_STS_SUCCESS != DtsDrvCmd(Ctx,dwIoControlCode,Async,(BC_IOCTL_DATA *)lpInBuffer,FALSE))
		return FALSE;

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
	rc = ioctl(Ctx->DevHandle, Code, pIo);
	if (rc < 0) {
		DebugLog_Trace(LDIL_DBG,"IOCTL Command Failed %d %d\n",rc,Code);
#if 0
		/* FIXME: jarod: should we add an ioctl data release check/call here? */
		if (locRel || Rel)
			DtsRelIoctlData(Ctx, pIo);
#endif
		return BC_STS_ERROR;
	}

	Sts = pIo->RetSts;
	if (locRel || Rel)
		DtsRelIoctlData(Ctx, pIo);

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
	int ret=0;
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

	ret = sem_init(&Ctx->CancelProcOut,0,0);
	if(ret == -1) {
		DebugLog_Trace(LDIL_DBG,"Unable to create event\n");
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

	if(&Ctx->CancelProcOut)
		sem_destroy(&Ctx->CancelProcOut);


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

	if(!(pIocData = DtsAllocIoctlData(Ctx)))
		return BC_STS_INSUFF_RES;

    DebugLog_Trace(LDIL_INFO,"ADD buffs\n");
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

	if(sem_post(&Ctx->CancelProcOut) == -1) {
		DebugLog_Trace(LDIL_DBG, "SetEvent for CancelProcOut Failed (Error)\n");
	}
	return sts;
}
//------------------------------------------------------------------------
// Name: DtsCancelFetchOutInt
// Description: Cancel Pending ProcOut Request..
//------------------------------------------------------------------------
BC_STATUS DtsCancelFetchOutInt(DTS_LIB_CONTEXT *Ctx)
{
	struct timespec ts;

	DebugLog_Trace(LDIL_DBG,"DtsCancelFetchOutInt: Called\n");
		
	if(!(DtsIsPend(Ctx))){
		DebugLog_Trace(LDIL_DBG,"DtsCancelFetchOutInt: No Pending Req\n");
		return BC_STS_SUCCESS;
	}

	Ctx->CancelWaiting = 1;

	ts.tv_sec=time(NULL)+(BC_PROC_OUTPUT_TIMEOUT/1000)+2;
        ts.tv_nsec=0;

	if(sem_timedwait(&Ctx->CancelProcOut,&ts)){
		if(errno == ETIMEDOUT){
			DebugLog_Trace(LDIL_DBG,"DtsCancelFetchOutInt: TimeOut\n");
			Ctx->CancelWaiting = 0;
			return BC_STS_TIMEOUT;
		}
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
			if(sts != BC_STS_SUCCESS)
				return sts;
		}
	}

	Ctx->bMapOutBufDone = true;
	return BC_STS_SUCCESS;
}
//------------------------------------------------------------------------
// Name: DtsInitInterface
// Description: Do application specific allocation and other initialization.
//------------------------------------------------------------------------
BC_STATUS DtsInitInterface(int hDevice,HANDLE *RetCtx, uint32_t mode)
{

	DTS_LIB_CONTEXT *Ctx = NULL;
	BC_STATUS	sts = BC_STS_SUCCESS;

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
	Ctx->InSampleCount = 0;
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
		return sts;
	}

	if(!(Ctx->CfgFlags & BC_ADDBUFF_MOVE)){
		sts = DtsMapYUVBuffs(Ctx);
		if(sts != BC_STS_SUCCESS){
			DebugLog_Trace(LDIL_DBG,"DtsMapYUVBuffs failed Sts:%d\n",sts);
			return sts;
		}
	}
	
	*RetCtx = (HANDLE)Ctx;

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
		DebugLog_Trace(LDIL_DBG,"DtsGetVersion: Ioctl failed: %d\n",sts);
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
// Name: DtsReleaseInterface
// Description: Do application specific Release and other initialization.
//------------------------------------------------------------------------
BC_STATUS DtsReleaseInterface(DTS_LIB_CONTEXT *Ctx)
{
	
	if(!Ctx)
		return BC_STS_INV_ARG;

	DtsReleaseMemPools(Ctx);
#ifdef _USE_SHMEM_
	DtsDelDilShMem();
#endif

	DebugLog_Trace(LDIL_DBG,"DtsDeviceClose: Invoked\n");

	if(close(Ctx->DevHandle)!=0) //Zero if success
	{
		DebugLog_Trace(LDIL_DBG,"DtsDeviceClose: Close Handle Failed with error %d\n",errno);

	}

	free(Ctx);

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
	DebugLog_Trace(LDIL_DATA,"DtsGetFirmwareFiles:Ctx->FwBinFile is %s\n", Ctx->FwBinFile);
	
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
		if((last) && (Ctx->MDLastFetchTag > (last->IntTag + MAX_DISOEDER_GAP)))
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

	if(!Ctx || !Ctx->MdataPoolPtr){
		return BC_STS_INV_ARG;
	}

	DtsLock(Ctx);
	/* Remove all Pending elements */
	temp = Ctx->MDPendHead;

	while(temp != DTS_MDATA_PEND_LINK(Ctx)){
		DebugLog_Trace(LDIL_DBG,"Clearing PendMdata %p %x \n", temp->Spes.SeqNum, temp->IntTag);
		DtsRemoveMdata(Ctx,temp,FALSE);
		temp = Ctx->MDPendHead;
	}

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
	while(temp != DTS_MDATA_PEND_LINK(Ctx)){
		if(temp->IntTag == InTag){
			pout->PicInfo.timeStamp = temp->appTimeStamp;
			//DebugLog_Trace(LDIL_DBG,"Found entry for %x %x tstamp: %x\n", 
			//	snum, temp->IntTag, pout->PicInfo.timeStamp);
			sts = BC_STS_SUCCESS;
			DtsRemoveMdata(Ctx, temp, FALSE);

			//Reserve the Last Fetch Tag
			Ctx->MDLastFetchTag = InTag;
			break;
		}
		temp = temp->flink;
	}
	DtsUnLock(Ctx);

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
		DebugLog_Trace(LDIL_DBG, "SeqNum is zero \n");
		return BC_STS_NO_DATA;
	}

	InTag = DtsMdataGetIntTag(Ctx, snum);
	temp = Ctx->MDPendHead;

	DtsLock(Ctx);
	while(temp != DTS_MDATA_PEND_LINK(Ctx)) {
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
	DTS_INPUT_MDATA		*temp=NULL;

	if( !mData || !Ctx)
		return BC_STS_INV_ARG;

	/* Alloc clears all fields */
	if( (temp = DtsAllocMdata(Ctx)) == NULL)
	{
		return BC_STS_BUSY;
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

	//DebugLog_Trace(LDIL_DBG,"Inserting entry for[%x] (%02x%02x) %x \n", 
	//				Ctx->InMdataTag, temp->Spes.SeqNum[1],temp->Spes.SeqNum[0], temp->IntTag);

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

	Ctx->InSampleCount ++;
	if (Ctx->InSampleCount > 65530)
		Ctx->InSampleCount = 1;

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
