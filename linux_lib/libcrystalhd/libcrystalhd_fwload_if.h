/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_fwload_if.h
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

#ifndef _libcrystalhd_FWLOAD_IF_
#define _libcrystalhd_FWLOAD_IF_

#ifndef __APPLE__
#include "bc_dts_glob_lnx.h"
#else
#include "bc_dts_glob_osx.h"
#endif
#include "libcrystalhd_if.h"

#define BC_FWIMG_ST_ADDR					0x00000000
#define MAX_BIN_FILE_SZ						0x300000

/* BOOTLOADER IMPLEMENTATION */
DRVIFLIB_INT_API BC_STATUS 
fwbinPushToLINK(HANDLE hDevice, char *FwBinFile, uint32_t *bytesDnld);

DRVIFLIB_INT_API BC_STATUS
DtsPushAuthFwToLink(HANDLE hDevice, char *FwBinFile);

DRVIFLIB_INT_API BC_STATUS
fwbinPushToFLEA(HANDLE hDevice, char *FwBinFile, uint32_t *bytesDnld);

DRVIFLIB_INT_API BC_STATUS
DtsPushFwToFlea(HANDLE hDevice, char *FwBinFile);

DRVIFLIB_INT_API BC_STATUS dec_write_fw_Sig(HANDLE hndl,uint32_t* Sig);

#endif
