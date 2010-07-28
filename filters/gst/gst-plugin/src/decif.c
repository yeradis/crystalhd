/********************************************************************
 * Copyright(c) 2008 Broadcom Corporation.
 *
 *  Name: decif.cpp
 *
 *  Description: Device Interface API.
 *
 *  AU
 *
 *  HISTORY:
 *
 *******************************************************************
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <getopt.h>
#include <errno.h>
#include <glib.h>
#include <gst/gst.h>

#include "decif.h"

BC_STATUS decif_getcaps(BcmDecIF *decif, BC_HW_CAPS *hwCaps)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	if(decif != NULL)
		sts = DtsGetCapabilities(decif->hdev, hwCaps);
	else
		sts = DtsGetCapabilities(NULL, hwCaps);
	return sts;
}

BC_STATUS decif_open(BcmDecIF *decif)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	guint32 mode = DTS_PLAYBACK_MODE | DTS_LOAD_FILE_PLAY_FW | DTS_SKIP_TX_CHK_CPB |
		       DTS_DFLT_RESOLUTION(vdecRESOLUTION_720p29_97);
	sts = DtsDeviceOpen(&decif->hdev,mode);
	if (sts != BC_STS_SUCCESS)
		decif->hdev = NULL;

	return sts;
}

BC_STATUS decif_close(BcmDecIF *decif)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	if (decif->hdev) {
		sts = DtsDeviceClose(decif->hdev);
		decif->hdev = NULL;
	}

	return sts;
}

BC_STATUS decif_prepare_play(BcmDecIF *decif)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	uint32_t stream_type = BC_STREAM_TYPE_ES;

	sts = DtsOpenDecoder(decif->hdev, stream_type);
	return sts;

}

BC_STATUS decif_start_play(BcmDecIF *decif)
{
	BC_STATUS sts = DtsStartDecoder(decif->hdev);

	if (sts != BC_STS_SUCCESS)
		return sts;

	sts = DtsStartCapture(decif->hdev);

	return sts;
}

BC_STATUS decif_pause(BcmDecIF *decif, gboolean pause)
{
	BC_STATUS sts;

	if (pause)
		sts = DtsPauseDecoder(decif->hdev);
	else
		sts = DtsResumeDecoder(decif->hdev);

	return sts;
}

BC_STATUS decif_stop(BcmDecIF *decif)
{
	BC_STATUS sts = DtsStopDecoder(decif->hdev);

	if (sts != BC_STS_SUCCESS)
		return sts;

	sts = DtsCloseDecoder(decif->hdev);

	return sts;
}

BC_STATUS decif_flush_dec(BcmDecIF *decif, gint8 flush_type)
{
	return DtsFlushInput(decif->hdev, flush_type);
}

BC_STATUS decif_flush_rxbuf(BcmDecIF *decif, gboolean discard_only)
{
	return DtsFlushRxCapture(decif->hdev, discard_only);
}

BC_STATUS decif_setcolorspace(BcmDecIF *decif, BC_OUTPUT_FORMAT mode)
{
	return DtsSetColorSpace(decif->hdev,mode);
}

BC_STATUS decif_send_buffer(BcmDecIF *decif, guint8 *buffer, guint32 size,
			    GstClockTime time_stamp, guint8 flags)
{
	BC_STATUS sts = BC_STS_SUCCESS;
// 	guint8 odd_bytes = 0;
// 	guint8 *psend_buff = decif->aligned_buf;
//
// 	while(((uintptr_t)psend_buff) & 0x03)
// 		psend_buff++;
//
// 	if (((uintptr_t)buffer) % 4) odd_bytes = 4 - ((guint8)((uintptr_t)buffer % 4));
//
// 	if (odd_bytes) {
// 		memcpy(psend_buff, buffer, odd_bytes);
// 		sts = DtsProcInput(decif->hdev, psend_buff, odd_bytes, time_stamp, flags);
// 		time_stamp = 0;
// 		if (sts != BC_STS_SUCCESS)
// 			return sts;
// 		buffer += odd_bytes;
// 		size -= odd_bytes;
// 		if (!size)
// 			return BC_STS_SUCCESS;
// 	}

	sts = DtsProcInput(decif->hdev, buffer, size, time_stamp, flags);

	return sts;
}

BC_STATUS decif_get_drv_status(BcmDecIF *decif, gboolean *suspended, guint32 *rll, guint32 *picNumFlags)
{
	BC_DTS_STATUS drv_status;
	BC_STATUS sts = DtsGetDriverStatus(decif->hdev, &drv_status);
	if (sts == BC_STS_SUCCESS) {
		if (drv_status.PowerStateChange)
			*suspended = TRUE;
		else
			*suspended = FALSE;
		*rll = drv_status.ReadyListCount;
		*picNumFlags = drv_status.picNumFlags;
	}

	return sts;
}

BC_STATUS decif_get_eos(BcmDecIF *decif, gboolean *bEOS)
{
	return DtsIsEndOfStream(decif->hdev, (uint8_t *)bEOS);
}

BC_STATUS decif_decode_catchup(BcmDecIF *decif, gboolean catchup)
{
	BC_STATUS sts;

	return sts=BC_STS_SUCCESS; // Temporarily disable Catchup

	if (catchup)
		sts = DtsSetFFRate(decif->hdev, 2);
	else
		sts = DtsSetFFRate(decif->hdev, 1);

	return sts;
}

BC_STATUS decif_setinputformat(BcmDecIF *decif, BC_INPUT_FORMAT bcInputFormat)
{
	BC_STATUS	sts = BC_STS_SUCCESS;

	sts = DtsSetInputFormat(decif->hdev, &bcInputFormat);

	return sts;
}