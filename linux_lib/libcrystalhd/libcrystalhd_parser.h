/********************************************************************
 * Copyright(c) 2006-2009 Broadcom Corporation.
 *
 *  Name: libcrystalhd_parser.h
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

#ifndef CPARSE
#define CPARSE

#include <sys/types.h>

//VC1 prefix 000001
#define VC1_FRM_SUFFIX	0x0D
#define VC1_SEQ_SUFFIX	0x0F

//VC1 SM Profile prefix 000001
#define VC1_SM_FRM_SUFFIX	0xE0

//Check WMV SP/MP PES Payload for PTS Info
#define VC1_SM_MAGIC_WORD	0x5A5A5A5A
#define VC1_SM_PTS_INFO_START_CODE	0xBD

//MPEG2 prefix 000001
#define	MPEG2_FRM_SUFFIX 0x00
#define	MPEG2_SEQ_SUFFIX 0xB3

#define BRCM_START_CODE_SIZE	4

//Packetized PES
#define MAX_RE_PES_BOUND (LONG)0xFFF0

static const uint8_t b_pes_header[9]={0x00, 0x00, 0x01, 0xE0,
									  0x00, 0x00, 0x81, 0x00, 0x00};
typedef enum
{
  P_SLICE = 0,
  B_SLICE,
  I_SLICE,
  SP_SLICE,
  SI_SLICE
} SliceType;


typedef enum
{
	NALU_TYPE_SLICE =  1,
	NALU_TYPE_DPA,
	NALU_TYPE_DPB,
	NALU_TYPE_DPC,
	NALU_TYPE_IDR,
	NALU_TYPE_SEI,
	NALU_TYPE_SPS,
	NALU_TYPE_PPS,
	NALU_TYPE_AUD,
	NALU_TYPE_EOSEQ,
	NALU_TYPE_EOSTREAM,
	NALU_TYPE_FILL
}NALuType;

typedef struct
{
  int32_t StartcodePrefixLen;		//! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
  uint32_t Len;				//! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
  uint32_t MaxSize;			//! Nal Unit Buffer size
  int32_t NalUnitType;				//! NALU_TYPE_xxxx
  int32_t ForbiddenBit;				//! should be always FALSE
  uint8_t* pNalBuf;
} NALU_t;

typedef struct stSYMBINT
{
	uint8_t*	m_pInputBuffer;
	uint8_t*	m_pInputBufferEnd;
	uint8_t*	m_pCurrent;
	uint32_t	m_ulMask;
	uint32_t	m_ulOffset;
	uint32_t  m_nSize;
	uint32_t  m_nUsed;
	uint32_t	m_ulZero;
} SYMBINT;

typedef struct stPES_CONVERT_PARAMS
{
	bool		m_bIsFirstByteStreamNALU;
	SYMBINT		m_SymbInt;

	uint8_t*	m_pSpsPpsBuf;
	uint32_t	m_iSpsPpsLen;
	uint32_t	m_lStartCodeDataSize;

	uint8_t  	*pStartcodePendBuff;
	uint32_t 	lPendBufferSize;

	//Get Sequence Header Info (Sequence Layer Bitestream for Simple and Main Profile)
	bool		m_bRangered;
	bool		m_bFinterpFlag;
	bool		m_bMaxbFrames;

	bool 		m_bIsAdd_SCode_CodeIn;
	bool		m_bAddSpsPps;
	//PES header parameter
	bool		m_bPESPrivData;
	bool		m_bPESExtField;
	uint32_t	m_nPESExtLen;
	bool		m_bStuffing;
	uint32_t	m_nStuffingBytes;

	uint8_t		*m_pPESPrivData;
	uint8_t		*m_pPESExtField;
	//SoftRave (VC-1 S/M and Divx) EOS Timing Marker
	bool		m_bSoftRave;
}PES_CONVERT_PARAMS;

BC_STATUS DtsSetPESConverter( HANDLE hDevice);
BC_STATUS DtsInitPESConverter(HANDLE hDevice);
BC_STATUS DtsReleasePESConverter(HANDLE hDevice);
BC_STATUS DtsCheckProfile(HANDLE hDevice);

BC_STATUS DtsCheckKeyFrame(HANDLE hDevice, uint8_t *pBuffer);
BC_STATUS DtsSetSpsPps(HANDLE hDevice);
BOOL DtsCheckSpsPps(HANDLE hDevice, uint8_t *pBuffer, uint32_t ulSize);

BC_STATUS DtsSetVC1SH(HANDLE hDevice);

BC_STATUS DtsAddH264SCode(HANDLE hDevice, uint8_t **ppBuffer, uint32_t *pUlDataSize, uint64_t *timeStamp);
BC_STATUS DtsAddVC1SCode(HANDLE hDevice, uint8_t **ppBuffer, uint32_t *pUlDataSize, uint64_t *timeStamp);
BC_STATUS DtsAddStartCode(HANDLE hDevice, uint8_t **ppBuffer, uint32_t *pUlDataSize, uint64_t *timeStamp);

int32_t DtsFindBSStartCode (uint8_t *Buf, int ZerosInStartcode);
int32_t DtsGetNaluType(HANDLE hDevice, uint8_t* pInputBuf, uint32_t ulSize, NALU_t* pNalu, bool bSkipSyncMarker);
BC_STATUS DtsParseAVC(HANDLE hDevice, uint8_t* pInputBuf, uint32_t ulSize, uint32_t* Offset, bool bIDR, int32_t *pNalType);
BC_STATUS DtsFindIDR(HANDLE hDevice, uint8_t* pInputBuffer, uint32_t ulSizeInBytes, uint32_t* pOffset);
BC_STATUS DtsFindStartCode(HANDLE hDevice, uint8_t* pInputBuffer, uint32_t ulSizeInBytes, uint32_t* pOffset);
BOOL DtsFindPTSInfoCode(HANDLE hDevice, uint8_t* pInputBuffer, uint32_t ulSizeInBytes);

inline int32_t DtsSymbIntNextBit ( HANDLE hDevice );
BC_STATUS DtsSymbIntSiUe (HANDLE hDevice, uint32_t* pCode);
BC_STATUS DtsSymbIntSiBuffer (HANDLE hDevice, uint8_t* pInputBuffer, uint32_t ulSize);

void PTS2MakerBit5Bytes(uint8_t *pMakerBit, int64_t llPTS);
uint16_t WORD_SWAP(uint16_t x);
#endif
