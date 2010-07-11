/********************************************************************
 * Copyright(c) 2006 Broadcom Corporation, all rights reserved.
 * Contains proprietary and confidential information.
 *
 * This source file is the property of Broadcom Corporation, and
 * may not be copied or distributed in any isomorphic form without
 * the prior written consent of Broadcom Corporation.
 *
 *
 *  Name: bc_fwcmds.h
 *
 *  Description: FW commands.
 *
 *  AU
 *
 *  HISTORY:
 *
 *******************************************************************/
#ifndef _BCM_FWCMDS_H_
#define _BCM_FWCMDS_H_

#include "libcrystalhd_priv.h"

DRVIFLIB_INT_API BC_STATUS
DtsFWInitialize(
    HANDLE		hDevice,
	uint32_t	resrv1
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWOpenChannel(
    HANDLE		hDevice,
    uint32_t	StreamType,
	uint32_t	reserved
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWActivateDecoder(
    HANDLE  hDevice
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetSingleField(
    HANDLE	hDevice,
	bool	bSingleField
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWHwSelfTest(
    HANDLE		hDevice,
    uint32_t	testID
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWVersion(
    HANDLE  hDevice,
	uint32_t	*Stream,
	uint32_t	*DecCore,
	uint32_t	*HwNumber
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWFifoStatus(
    HANDLE  hDevice,
	uint32_t	*CpbSize,
	uint32_t	*CpbFullness
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWCloseChannel(
    HANDLE  hDevice,
	uint32_t	ChannelID
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWSetVideoInput(
    HANDLE  hDevice
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWSetVideoPID(
    HANDLE  hDevice,
	uint32_t	pid
    );

DRVIFLIB_INT_API BC_STATUS
DtsFWFlushDecoder(
    HANDLE  hDevice,
	uint32_t	rsrv
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWStartVideo(
    HANDLE  hDevice,
	uint32_t	videoAlg,
	uint32_t	FGTEnable,
	uint32_t	MetaDataEnable,
	uint32_t	Progressive,
	uint32_t	OptFlags
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWStopVideo(
    HANDLE  hDevice,
	uint32_t	ChannelId,
	bool		ForceStop
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWDecFlushChannel(
    HANDLE  hDevice,
	uint32_t	Operation
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWPauseVideo(
    HANDLE  hDevice,
	uint32_t	Operation
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetTrickPlay(
	HANDLE hDevice,
	uint32_t	trickMode,
	uint8_t		direction
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetHostTrickMode(
	HANDLE hDevice,
	uint32_t	enable
	);
DRVIFLIB_INT_API BC_STATUS
DtsFWSetFFRate(
	HANDLE hDevice,
	uint32_t	Rate
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetSlowMotionRate(
	HANDLE hDevice,
	uint32_t	Rate
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetSkipPictureMode(
	HANDLE hDevice,
	uint32_t	SkipMode
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWFrameAdvance(
    HANDLE  hDevice
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetContentKeys(
	HANDLE hDevice,
	uint8_t		*buffer,
	uint32_t	dwLength,
	uint32_t	flags
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWSetSessionKeys(
	HANDLE hDevice,
	uint8_t		*buffer,
	uint32_t	Length,
	uint32_t	flags
	);

BC_STATUS
DtsFormatChangeAck(
	HANDLE hDevice,
	uint32_t	flags
	);

DRVIFLIB_INT_API BC_STATUS
DtsFWDrop(
	HANDLE hDevice,
	uint32_t	Pictures
	);

#endif //_BCM_FWCMDS_H