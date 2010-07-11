/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_fwload_if.cpp
 *
 *  Description: Firmware diagnostics
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

#include "7411d.h"
#include "bc_defines.h"
#include "bcm_70012_regs.h"	/* Link Register defs */
#include "libcrystalhd_fwload_if.h"
#include "libcrystalhd_int_if.h"
#include "libcrystalhd_priv.h"

DRVIFLIB_INT_API BC_STATUS
DtsPushAuthFwToLink(HANDLE hDevice, char *FwBinFile)
{
	BC_STATUS		status=BC_STS_ERROR;
	uint32_t		byesDnld=0;
	char			*fwfile=NULL;
	DTS_LIB_CONTEXT *Ctx=NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->OpMode == DTS_DIAG_MODE){
		/* In command line case, we don't get a close
		 * between successive devinit commands.
		 */
		Ctx->FixFlags &= ~DTS_LOAD_FILE_PLAY_FW;

		if(FwBinFile){
			if(!strncmp(FwBinFile,"FILE_PLAY_BACK",14)){
				Ctx->FixFlags |=DTS_LOAD_FILE_PLAY_FW;
				FwBinFile=NULL;
			}
		}
	}

	/* Get the firmware file to download */
	if (!FwBinFile)
	{
		status = DtsGetFirmwareFiles(Ctx);
		if (status == BC_STS_SUCCESS) fwfile = Ctx->FwBinFile;
		else return status;

	} else {
		fwfile = FwBinFile;
	}

	//DebugLog_Trace(LDIL_DBG,"Firmware File is :%s\n",fwfile);

	/* Push the F/W bin file to the driver */
	status = fwbinPushToLINK(hDevice, fwfile, &byesDnld);

	if (status != BC_STS_SUCCESS) {
		DebugLog_Trace(LDIL_DBG,"DtsPushAuthFwToLink: Failed to download firmware\n");
		return status;
	}

	return BC_STS_SUCCESS;
}

DRVIFLIB_INT_API BC_STATUS
DtsPushFwToFlea(HANDLE hDevice, char *FwBinFile)
{
	BC_STATUS		status=BC_STS_ERROR;
	uint32_t		byesDnld=0;
	char			*fwfile=NULL;
	DTS_LIB_CONTEXT *Ctx=NULL;

	DTS_GET_CTX(hDevice,Ctx);

	if(Ctx->OpMode == DTS_DIAG_MODE){
		/* In command line case, we don't get a close
		* between successive devinit commands.
		*/
		Ctx->FixFlags &= ~DTS_LOAD_FILE_PLAY_FW;

		if(FwBinFile){
			if(!strncmp(FwBinFile,"FILE_PLAY_BACK",14)){
				Ctx->FixFlags |=DTS_LOAD_FILE_PLAY_FW;
				FwBinFile=NULL;
			}
		}
	}

	/* Get the firmware file to download */
	if (!FwBinFile)
	{
		status = DtsGetFirmwareFiles(Ctx);
		if (status == BC_STS_SUCCESS) fwfile = Ctx->FwBinFile;
		else return status;

	} else {
		fwfile = FwBinFile;
	}

	//DebugLog_Trace(LDIL_DBG,"Firmware File is :%s\n",fwfile);

	/* Push the F/W bin file to the driver */
	status = fwbinPushToFLEA(hDevice, fwfile, &byesDnld);

	if (status != BC_STS_SUCCESS) {
		DebugLog_Trace(LDIL_DBG,"DtsPushFwToFlea: Failed to download firmware\n");
		return status;
	}

	return BC_STS_SUCCESS;
}

DRVIFLIB_INT_API BC_STATUS
fwbinPushToLINK(HANDLE hDevice, char *FwBinFile, uint32_t *bytesDnld)
{
	BC_STATUS	status=BC_STS_ERROR;
	uint32_t	FileSz=0;
	char		*buff=NULL;
	FILE 		*fp=NULL;

	if( (!FwBinFile) || (!hDevice) || (!bytesDnld))
	{
		DebugLog_Trace(LDIL_DBG,"Invalid Arguments\n");
		return BC_STS_INV_ARG;
	}

	fp = fopen(FwBinFile,"rb");
	if(!fp)
	{
		DebugLog_Trace(LDIL_DBG,"Failed to Open FW file.  %s\n", FwBinFile);
		perror("LINK FW");
		return BC_STS_ERROR;
	}

	fseek(fp,0,SEEK_END);
	FileSz = ftell(fp);
	fseek(fp,0,SEEK_SET);

	buff = (char*)malloc(FileSz);
	if (!buff) {
		DebugLog_Trace(LDIL_DBG,"Failed to allocate memory\n");
		return BC_STS_INSUFF_RES;
	}

	*bytesDnld = fread(buff,1,FileSz,fp);
	if(0 == *bytesDnld)
	{
		DebugLog_Trace(LDIL_DBG,"Failed to Read The File\n");
		return BC_STS_IO_ERROR;
	}

	status = DtsPushFwBinToLink(hDevice, (uint32_t*)buff, *bytesDnld);
	if(buff) free(buff);
	if(fp) fclose(fp);
	return status;
}

DRVIFLIB_INT_API BC_STATUS
fwbinPushToFLEA(HANDLE hDevice, char *FwBinFile, uint32_t *bytesDnld)
{
	BC_STATUS	status=BC_STS_ERROR;
	uint32_t	FileSz=0;
	char		*buff=NULL;
	FILE 		*fp=NULL;

	if( (!FwBinFile) || (!hDevice) || (!bytesDnld))
	{
		DebugLog_Trace(LDIL_DBG,"Invalid Arguments\n");
		return BC_STS_INV_ARG;
	}

	fp = fopen(FwBinFile,"rb");
	if(!fp)
	{
		DebugLog_Trace(LDIL_DBG,"Failed to Open FW file.  %s\n", FwBinFile);
		perror("FLEA FW");
		return BC_STS_ERROR;
	}

	fseek(fp,0,SEEK_END);
	FileSz = ftell(fp);
	fseek(fp,0,SEEK_SET);

	buff = (char*)malloc(FileSz);
	if (!buff) {
		DebugLog_Trace(LDIL_DBG,"Failed to allocate memory\n");
		return BC_STS_INSUFF_RES;
	}

	*bytesDnld = fread(buff,1,FileSz,fp);
	if(0 == *bytesDnld)
	{
		DebugLog_Trace(LDIL_DBG,"Failed to Read The File\n");
		return BC_STS_IO_ERROR;
	}

	status = DtsPushFwBinToLink(hDevice, (uint32_t*)buff, *bytesDnld);
	if(buff) free(buff);
	if(fp) fclose(fp);
	return status;
}

BC_STATUS dec_write_fw_Sig(HANDLE hndl, uint32_t* Sig)
{
    unsigned int *ptr = Sig;
    unsigned int DciSigDataReg = (unsigned int)DCI_SIGNATURE_DATA_7;
    BC_STATUS sts = BC_STS_ERROR;

    for (int reg_cnt=0;reg_cnt<8;reg_cnt++)
    {
        sts = DtsFPGARegisterWr(hndl, DciSigDataReg, bswap_32_1(*ptr));

        if(sts != BC_STS_SUCCESS)
        {
            DebugLog_Trace(LDIL_DBG,"Error Wrinting Fw Sig data register\n");
            return sts;
        }
        DciSigDataReg-=4;
        ptr++;
    }
    return sts;
}




