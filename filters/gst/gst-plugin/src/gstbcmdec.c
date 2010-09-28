/********************************************************************
 * Copyright(c) 2008 Broadcom Corporation.
 *
 *  Name: gstbcmdec.c
 *
 *  Description: Broadcom 70012 decoder plugin
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
#include <sys/shm.h>
#include <pthread.h>
#include <sys/times.h>
#include <semaphore.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>
#include <glib.h>
#include <gst/base/gstadapter.h>
#include <gst/video/video.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>

#include "decif.h"
#include "parse.h"
#include "gstbcmdec.h"

GST_DEBUG_CATEGORY_STATIC (gst_bcmdec_debug);
#define GST_CAT_DEFAULT gst_bcmdec_debug

//#define YV12__

//#define FILE_DUMP__ 1

static GstFlowReturn bcmdec_send_buff_detect_error(GstBcmDec *bcmdec, GstBuffer *buf,
						   guint8* pbuffer, guint32 size,
						   guint32 offset, GstClockTime tCurrent,
						   guint8 flags)
{
	BC_STATUS sts, suspend_sts = BC_STS_SUCCESS;
	gboolean suspended = FALSE;
	guint32 rll=0;
	guint32 nextPicNumFlags = 0;

	sts = decif_send_buffer(&bcmdec->decif, pbuffer, size, tCurrent, flags);

	if (sts != BC_STS_SUCCESS) {
		GST_ERROR_OBJECT(bcmdec, "proc input failed sts = %d", sts);
		GST_ERROR_OBJECT(bcmdec, "Chain: timeStamp = %llu size = %d data = %p",
				 GST_BUFFER_TIMESTAMP(buf), GST_BUFFER_SIZE(buf),
				 GST_BUFFER_DATA (buf));
		if ((sts == BC_STS_IO_USER_ABORT) || (sts == BC_STS_ERROR)) {
			suspend_sts = decif_get_drv_status(&bcmdec->decif,&suspended, &rll, &nextPicNumFlags);
			if (suspend_sts == BC_STS_SUCCESS) {
				if (suspended) {
					GST_DEBUG_OBJECT(bcmdec, "suspend status recv");
					if (!bcmdec->suspend_mode)  {
						bcmdec_suspend_callback(bcmdec);
						bcmdec->suspend_mode = TRUE;
						GST_DEBUG_OBJECT(bcmdec, "suspend done", sts);
					}
					if (bcmdec_resume_callback(bcmdec) == BC_STS_SUCCESS) {
						GST_DEBUG_OBJECT(bcmdec, "resume done", sts);
						bcmdec->suspend_mode = FALSE;
						sts = decif_send_buffer(&bcmdec->decif, pbuffer, size, tCurrent, flags);
						GST_ERROR_OBJECT(bcmdec, "proc input..2 sts = %d", sts);
					} else {
						GST_DEBUG_OBJECT(bcmdec, "resume failed", sts);
					}
				}
				else if (sts == BC_STS_ERROR) {
					GST_DEBUG_OBJECT(bcmdec, "device is not suspended");
					//gst_buffer_unref (buf);
					return GST_FLOW_ERROR;
				}
			} else {
				GST_DEBUG_OBJECT(bcmdec, "decif_get_drv_status -- failed %d", sts);
			}
		}
	}

	return GST_FLOW_OK;
}

/* bcmdec signals and args */
enum {
	/* FILL ME */
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_SILENT
};


GLB_INST_STS *g_inst_sts = NULL;

/*
 * the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory_bcm70015 = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("video/mpeg, " "mpegversion = (int) {2, 4}," "systemstream =(boolean) false; "
						"video/x-h264;" "video/x-vc1;" "video/x-wmv, " "wmvversion = (int) {3};"
						"video/x-msmpeg, " "msmpegversion = (int) {43};"
						"video/x-divx, " "divxversion = (int) {3, 4, 5};" "video/x-xvid;"));

static GstStaticPadTemplate sink_factory_bcm70012 = GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("video/mpeg, " "mpegversion = (int) {2}," "systemstream =(boolean) false; "
						"video/x-h264;" "video/x-vc1;" "video/x-wmv, " "wmvversion = (int) {3};"));

#ifdef YV12__
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("video/x-raw-yuv, " "format = (fourcc) { YV12 }, " "width = (int) [ 1, MAX ], "
				"height = (int) [ 1, MAX ], " "framerate = (fraction) [ 0/1, 2147483647/1 ]"));
#define BUF_MULT (12 / 8)
#define BUF_MODE MODE420
#else
static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS,
		GST_STATIC_CAPS("video/x-raw-yuv, " "format = (fourcc) { YUY2 } , " "framerate = (fraction) [0,MAX], "
				"width = (int) [1,MAX], " "height = (int) [1,MAX]; " "video/x-raw-yuv, "
				"format = (fourcc) { UYVY } , " "framerate = (fraction) [0,MAX], " "width = (int) [1,MAX], "
				"height = (int) [1,MAX]; "));
#define BUF_MULT (16 / 8)
#define BUF_MODE MODE422_YUY2
#endif

GST_BOILERPLATE(GstBcmDec, gst_bcmdec, GstElement, GST_TYPE_ELEMENT);

/* GObject vmethod implementations */

static void gst_bcmdec_base_init(gpointer gclass)
{
	static GstElementDetails element_details;
	BC_HW_CAPS hwCaps;

	element_details.klass = (gchar *)"Codec/Decoder/Video";
	element_details.longname = (gchar *)"Generic Video Decoder";
	element_details.description = (gchar *)"Decodes various Video Formats using CrystalHD Decoders";
	element_details.author = (gchar *)"BRCM";

	GstElementClass *element_class = GST_ELEMENT_CLASS(gclass);

	hwCaps.DecCaps = 0;
	decif_getcaps(NULL, &hwCaps);

	gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&src_factory));
	if(hwCaps.DecCaps & BC_DEC_FLAGS_M4P2)
		gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&sink_factory_bcm70015));
	else
		gst_element_class_add_pad_template(element_class, gst_static_pad_template_get (&sink_factory_bcm70012));
	gst_element_class_set_details(element_class, &element_details);
}

/* initialize the bcmdec's class */
static void gst_bcmdec_class_init(GstBcmDecClass *klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *)klass;
	gstelement_class = (GstElementClass *)klass;

	gstelement_class->change_state = gst_bcmdec_change_state;

	gobject_class->set_property = gst_bcmdec_set_property;
	gobject_class->get_property = gst_bcmdec_get_property;
	gobject_class->finalize     = gst_bcmdec_finalize;

	g_object_class_install_property(gobject_class, PROP_SILENT,
					g_param_spec_boolean("silent", "Silent",
							     "Produce verbose output ?",
							     FALSE, (GParamFlags)G_PARAM_READWRITE));
}

/*
 * initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_bcmdec_init(GstBcmDec *bcmdec, GstBcmDecClass *gclass)
{
	pid_t pid;
	BC_STATUS sts = BC_STS_SUCCESS;
	int shmid = 0;
	BC_HW_CAPS hwCaps;

	GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_init");

	bcmdec_reset(bcmdec);

	hwCaps.DecCaps = 0;
	sts = decif_getcaps(&bcmdec->decif, &hwCaps);
	if(hwCaps.DecCaps & BC_DEC_FLAGS_M4P2) {
		bcmdec->sinkpad = gst_pad_new_from_static_template(&sink_factory_bcm70015, "sink");
	}
	else
		bcmdec->sinkpad = gst_pad_new_from_static_template(&sink_factory_bcm70012, "sink");

	gst_pad_set_event_function(bcmdec->sinkpad, GST_DEBUG_FUNCPTR(gst_bcmdec_sink_event));

	gst_pad_set_setcaps_function(bcmdec->sinkpad, GST_DEBUG_FUNCPTR(gst_bcmdec_sink_set_caps));
	gst_pad_set_getcaps_function(bcmdec->sinkpad, GST_DEBUG_FUNCPTR(gst_bcmdec_getcaps));
	gst_pad_set_chain_function(bcmdec->sinkpad, GST_DEBUG_FUNCPTR(gst_bcmdec_chain));

	bcmdec->srcpad = gst_pad_new_from_static_template (&src_factory, "src");

	gst_pad_set_getcaps_function(bcmdec->srcpad, GST_DEBUG_FUNCPTR(gst_bcmdec_getcaps));

	gst_pad_set_event_function(bcmdec->srcpad, GST_DEBUG_FUNCPTR(gst_bcmdec_src_event));

	gst_pad_use_fixed_caps(bcmdec->srcpad);
	bcmdec_negotiate_format(bcmdec);

	gst_element_add_pad(GST_ELEMENT(bcmdec), bcmdec->sinkpad);
	gst_element_add_pad(GST_ELEMENT(bcmdec), bcmdec->srcpad);
	bcmdec->silent = FALSE;
	pid = getpid();
	GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_init _-- PID = %x",pid);

	sts = bcmdec_create_shmem(bcmdec, &shmid);

	GST_DEBUG_OBJECT(bcmdec, "bcmdec_create_shmem _-- Sts = %x",sts);
}

/* plugin close function*/
static void gst_bcmdec_finalize(GObject *object)
{
	GstBcmDec *bcmdec = GST_BCMDEC(object);

	bcmdec_del_shmem(bcmdec);
	/*gst_bcmdec_cleanup(bcmdec);*/
	GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_finalize");
	G_OBJECT_CLASS(parent_class)->finalize(object);
}

static void gst_bcmdec_set_property(GObject *object, guint prop_id,
				    const GValue *value, GParamSpec *pspec)
{
	GstBcmDec *bcmdec = GST_BCMDEC(object);

	switch (prop_id) {
	case PROP_SILENT:
		bcmdec->silent = g_value_get_boolean (value);
		GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_set_property PROP_SILENT");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}

	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_set_property");
}

static void gst_bcmdec_get_property(GObject *object, guint prop_id,
				    GValue *value, GParamSpec *pspec)
{
	GstBcmDec *bcmdec = GST_BCMDEC(object);

	switch (prop_id) {
	case PROP_SILENT:
		g_value_set_boolean (value, bcmdec->silent);
		GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_get_property PROP_SILENT");
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_get_property");
}

/* GstElement vmethod implementations */
static gboolean gst_bcmdec_sink_event(GstPad* pad, GstEvent* event)
{
	GstBcmDec *bcmdec;
	BC_STATUS sts = BC_STS_SUCCESS;
	bcmdec = GST_BCMDEC(gst_pad_get_parent(pad));

	gboolean result = TRUE;

	switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_NEWSEGMENT:
		GstFormat newsegment_format;
		gint64 newsegment_start;

		gst_event_parse_new_segment(event, NULL, NULL, &newsegment_format,
					    &newsegment_start, NULL, NULL);

		bcmdec->base_clock_time = newsegment_start;
		bcmdec->cur_stream_time = 0;

		if (!bcmdec->silent)
			GST_DEBUG_OBJECT(bcmdec, "new segment");

		bcmdec->codec_params.inside_buffer = TRUE;
		bcmdec->codec_params.consumed_offset = 0;
		bcmdec->codec_params.strtcode_offset = 0;
		bcmdec->codec_params.nal_sz = 0;
		bcmdec->insert_pps = TRUE;
		bcmdec->base_time = 0;

		bcmdec->spes_frame_cnt = 0;
		bcmdec->catchup_on = FALSE;
		bcmdec->last_output_spes_time = 0;
		bcmdec->last_spes_time = 0;

		result = gst_pad_push_event(bcmdec->srcpad, event);
		break;

	case GST_EVENT_FLUSH_START:
		GST_DEBUG_OBJECT(bcmdec, "Flush Start");

#if 0
		pthread_mutex_lock(&bcmdec->fn_lock);
		if (!g_inst_sts->waiting) /*in case of playback process waiting*/
			bcmdec_process_flush_start(bcmdec);
		pthread_mutex_unlock(&bcmdec->fn_lock);
#endif
		bcmdec_process_flush_start(bcmdec);
		result = gst_pad_push_event(bcmdec->srcpad, event);
		break;

	case GST_EVENT_FLUSH_STOP:
		if (!bcmdec->silent)
			GST_DEBUG_OBJECT(bcmdec, "Flush Stop");
		//if (!g_inst_sts->waiting)
		//	bcmdec_process_flush_stop(bcmdec);
		bcmdec_process_flush_stop(bcmdec);
		result = gst_pad_push_event(bcmdec->srcpad, event);
		break;

	case GST_EVENT_EOS:
		if (!bcmdec->silent)
			GST_DEBUG_OBJECT(bcmdec, "EOS on sink pad");
		sts = decif_flush_dec(&bcmdec->decif, 0);
		GST_DEBUG_OBJECT(bcmdec, "dec_flush ret = %d", sts);
		bcmdec->ev_eos = event;
		gst_event_ref(bcmdec->ev_eos);
		break;

	default:
		result = gst_pad_push_event(bcmdec->srcpad, event);
		GST_DEBUG_OBJECT(bcmdec, "unhandled event on sink pad");
		break;
	}

	gst_object_unref(bcmdec);
	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_sink_event %u", GST_EVENT_TYPE(event));
	return result;
}

static GstCaps *gst_bcmdec_getcaps (GstPad * pad)
{
	GstBcmDec *bcmdec;
  	GstCaps *caps;
	bcmdec = GST_BCMDEC(gst_pad_get_parent(pad));

	caps = gst_caps_copy (gst_pad_get_pad_template_caps (pad));

	return caps;
}

/* this function handles the link with other elements */
static gboolean gst_bcmdec_sink_set_caps(GstPad *pad, GstCaps *caps)
{
	GstBcmDec *bcmdec;
	bcmdec = GST_BCMDEC(gst_pad_get_parent(pad));
	GstStructure *structure;
	GstCaps *intersection;
	const gchar *mime;
	guint num = 0;
	guint den = 0;
	const GValue *g_value;
	int version = 0;
	GstBuffer *buffer;
	guint8 *data;
	guint size;
	guint index;

	GST_DEBUG_OBJECT (pad, "setcaps called");

	intersection = gst_caps_intersect(gst_pad_get_pad_template_caps(pad), caps);
	GST_DEBUG_OBJECT(bcmdec, "Intersection return %", GST_PTR_FORMAT, intersection);
	if (gst_caps_is_empty(intersection)) {
		GST_ERROR_OBJECT(bcmdec, "setscaps:caps empty");
		gst_object_unref(bcmdec);
		return FALSE;
	}
	gst_caps_unref(intersection);
	structure = gst_caps_get_structure(caps, 0);
	mime = gst_structure_get_name(structure);
	if (!strcmp("video/x-h264", mime)) {
		bcmdec->input_format = BC_MSUBTYPE_AVC1; // GStreamer uses the bit-stream format so have to add start codes
		// We might override this later down below if the codec_data indicates otherwise
		// So don't print codec type yet GST_DEBUG_OBJECT(bcmdec, "InFmt H.264");
	} else if (!strcmp("video/mpeg", mime)) {
		gst_structure_get_int(structure, "mpegversion", &version);
		if (version == 2) {
			bcmdec->input_format = BC_MSUBTYPE_MPEG2VIDEO;
			GST_DEBUG_OBJECT(bcmdec, "InFmt MPEG2");
		} else if (version == 4) {
			bcmdec->input_format = BC_MSUBTYPE_DIVX;
			GST_DEBUG_OBJECT(bcmdec, "InFmt MPEG4");
		} else {
			gst_object_unref(bcmdec);
			return FALSE;
		}
	} else if (!strcmp("video/x-vc1", mime)) {
		bcmdec->input_format = BC_MSUBTYPE_VC1;
		GST_DEBUG_OBJECT(bcmdec, "InFmt VC1");
	} else if (!strcmp("video/x-divx", mime)) {
		gst_structure_get_int(structure, "divxversion", &version);
		if(version == 3) {
			bcmdec->input_format = BC_MSUBTYPE_DIVX311;
			GST_DEBUG_OBJECT(bcmdec, "InFmt DIVX3");
		} else {
			bcmdec->input_format = BC_MSUBTYPE_DIVX;
			GST_DEBUG_OBJECT(bcmdec, "InFmt DIVX%d", version);
		}
	} else if (!strcmp("video/x-xvid", mime)) {
		bcmdec->input_format = BC_MSUBTYPE_DIVX;
		GST_DEBUG_OBJECT(bcmdec, "InFmt XVID");
	} else if (!strcmp("video/x-msmpeg", mime)) {
		bcmdec->input_format = BC_MSUBTYPE_DIVX311;
		GST_DEBUG_OBJECT(bcmdec, "InFmt MPMPEGv43");
	} else if (!strcmp("video/x-wmv", mime)) {
		gst_structure_get_int(structure, "wmvversion", &version);
		if(version == 3) {
			bcmdec->input_format = BC_MSUBTYPE_WMV3;
			GST_DEBUG_OBJECT(bcmdec, "InFmt WMV9");
		} else {
			gst_object_unref(bcmdec);
			return FALSE;
		}
	} else {
		GST_DEBUG_OBJECT(bcmdec, "unknown mime %s", mime);
		gst_object_unref(bcmdec);
		return FALSE;
	}

	g_value = gst_structure_get_value(structure, "framerate");
	if (g_value != NULL) {
		num = gst_value_get_fraction_numerator(g_value);
		den = gst_value_get_fraction_denominator(g_value);

		bcmdec->input_framerate = (double)num / den;
		GST_LOG_OBJECT(bcmdec, "demux frame rate = %f ", bcmdec->input_framerate);
	} else {
		GST_DEBUG_OBJECT(bcmdec, "no demux framerate_value");
	}

	g_value = gst_structure_get_value(structure, "pixel-aspect-ratio");
	if (g_value) {
		bcmdec->input_par_x = gst_value_get_fraction_numerator(g_value);
		bcmdec->input_par_y = gst_value_get_fraction_denominator(g_value);
		GST_DEBUG_OBJECT(bcmdec, "sink caps have pixel-aspect-ratio of %d:%d",
				 bcmdec->input_par_x, bcmdec->input_par_y);
		if (bcmdec->input_par_x > 5 * bcmdec->input_par_y) {
			bcmdec->input_par_x = 1;
			bcmdec->input_par_y = 1;
			GST_DEBUG_OBJECT(bcmdec, "demux par reset");
		}

	} else {
		GST_DEBUG_OBJECT (bcmdec, "no par from demux");
	}

	gst_structure_get_int(structure, "width", &bcmdec->frame_width);
	gst_structure_get_int(structure, "height", &bcmdec->frame_height);

	// Check Codec Data for various codecs
	// Determine if this is bitstream video (AVC1 or no start codes) or Byte stream video (H264)
	// Determine if this is VC-1 AP or SP/MP for VC-1
	if ((g_value = gst_structure_get_value (structure, "codec_data"))) {
		if (G_VALUE_TYPE(g_value) == GST_TYPE_BUFFER) {
			if (!strcmp("video/x-h264", mime)) {
				GST_DEBUG_OBJECT (bcmdec, "Don't have start codes'");
				bcmdec->input_format = BC_MSUBTYPE_AVC1;
				GST_DEBUG_OBJECT(bcmdec, "InFmt H.264 (AVC1)");

				buffer = gst_value_get_buffer(g_value);
				data = GST_BUFFER_DATA(buffer);
				size = GST_BUFFER_SIZE(buffer);

				GST_DEBUG_OBJECT(bcmdec, "codec_data size = %d", size);

				/* parse the avcC data */
				if (size < 7) {
					GST_ERROR_OBJECT(bcmdec, "avcC size %u < 7", size);
					goto avcc_error;
				}
				/* parse the version, this must be 1 */
				if (data[0] != 1)
					goto wrong_version;

				if (bcmdec->codec_params.sps_pps_buf == NULL)
					bcmdec->codec_params.sps_pps_buf = (guint8 *)malloc(size * 2);
				if (bcmdec_insert_sps_pps(bcmdec, buffer) != BC_STS_SUCCESS) {
					bcmdec->codec_params.pps_size = 0;
				}
			} else if (!strcmp("video/x-wmv", mime)) {
				buffer = gst_value_get_buffer(g_value);
				data = GST_BUFFER_DATA(buffer);
				size = GST_BUFFER_SIZE(buffer);

				GST_DEBUG_OBJECT(bcmdec, "codec_data size = %d", size);
				if (size == 4) {
					// Simple or Main Profile
					bcmdec->input_format = BC_MSUBTYPE_WMV3;
					GST_DEBUG_OBJECT(bcmdec, "InFmt VC-1 (SP/MP)");
					if (bcmdec->codec_params.sps_pps_buf == NULL)
						bcmdec->codec_params.sps_pps_buf = (guint8 *)malloc(4);
					memcpy(bcmdec->codec_params.sps_pps_buf, data, 4);
					bcmdec->codec_params.pps_size = 4;
				} else {
					bcmdec->input_format = BC_MSUBTYPE_VC1;
					GST_DEBUG_OBJECT(bcmdec, "InFmt VC-1 (AP)");
					for (index = 0; index < size; index++) {
						data += index;
						if (((size - index) >= 4) && (*data == 0x00) && (*(data + 1) == 0x00) &&
							(*(data + 2) == 0x01) && (*(data + 3) == 0x0f)) {
							GST_DEBUG_OBJECT(bcmdec, "VC1 Sequence Header Found for Adv Profile");

							if ((size - index + 1) > MAX_ADV_PROF_SEQ_HDR_SZ)
								bcmdec->codec_params.pps_size = MAX_ADV_PROF_SEQ_HDR_SZ;
							else
								bcmdec->codec_params.pps_size = size - index + 1;
							memcpy(bcmdec->codec_params.sps_pps_buf, data, bcmdec->codec_params.pps_size);
							break;
						}
					}
				}
			}
		}
	} else {
		if (!strcmp("video/x-h264", mime)) {
			GST_DEBUG_OBJECT (bcmdec, "Have start codes'");
			bcmdec->input_format = BC_MSUBTYPE_H264;
			GST_DEBUG_OBJECT(bcmdec, "InFmt H.264 (H264)");;
			bcmdec->codec_params.nal_size_bytes = 4; // 4 sync bytes used
		} else {
			// No Codec data. So try with FourCC for VC1/WMV9
			if (!strcmp("video/x-wmv", mime)) {
				guint32 fourcc;
				if (gst_structure_get_fourcc (structure, "format", &fourcc)) {
					if ((fourcc == GST_MAKE_FOURCC ('W', 'V', 'C', '1')) ||
						(fourcc == GST_MAKE_FOURCC ('W', 'M', 'V', 'A'))) {
						bcmdec->input_format = BC_MSUBTYPE_VC1;
						GST_DEBUG_OBJECT(bcmdec, "InFmt VC-1 (AP)");
					} else {
						GST_DEBUG_OBJECT(bcmdec, "no codec_data. Don't know how to handle");
						gst_object_unref(bcmdec);
						return FALSE;
					}
				}
			} else {
				GST_DEBUG_OBJECT(bcmdec, "no codec_data. Don't know how to handle");
				gst_object_unref(bcmdec);
				return FALSE;
			}
		}
	}

    if (bcmdec->play_pending) {
		bcmdec->play_pending = FALSE;
		bcmdec_process_play(bcmdec);
	}

	gst_object_unref(bcmdec);

	return TRUE;

	/* ERRORS */
avcc_error:
	{
		gst_object_unref(bcmdec);
		return FALSE;
	}

wrong_version:
	{
		GST_ERROR_OBJECT(bcmdec, "wrong avcC version");
		gst_object_unref(bcmdec);
		return FALSE;
	}
}

void bcmdec_msleep(gint msec)
{
	gint cnt = msec;

	while (cnt) {
		usleep(1000);
		cnt--;
	}
}

/*
 * chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_bcmdec_chain(GstPad *pad, GstBuffer *buf)
{
	GstBcmDec *bcmdec;
//	BC_STATUS sts = BC_STS_SUCCESS;
	guint32 buf_sz = 0;
	guint32 offset = 0;
	GstClockTime tCurrent = 0;
	guint8 *pbuffer;
	guint32 size = 0;
//	guint32 vc1_buff_sz = 0;


#ifdef FILE_DUMP__
	guint32 bytes_written =0;
#endif
	bcmdec = GST_BCMDEC (GST_OBJECT_PARENT (pad));

#ifdef FILE_DUMP__
	if (bcmdec->fhnd == NULL)
		bcmdec->fhnd = fopen("dump2.264", "a+");
#endif

	if (bcmdec->flushing) {
		GST_DEBUG_OBJECT(bcmdec, "input while flushing");
		gst_buffer_unref(buf);
		return GST_FLOW_OK;
	}

	if (GST_CLOCK_TIME_NONE != GST_BUFFER_TIMESTAMP(buf)) {
		if (bcmdec->base_time == 0) {
			bcmdec->base_time = GST_BUFFER_TIMESTAMP(buf);
			GST_DEBUG_OBJECT(bcmdec, "base time is set to %llu", bcmdec->base_time / 1000000);
		}
		tCurrent = GST_BUFFER_TIMESTAMP(buf);
	}
	buf_sz = GST_BUFFER_SIZE(buf);

	if (bcmdec->play_pending) {
		bcmdec->play_pending = FALSE;
		bcmdec_process_play(bcmdec);
	} else if (!bcmdec->streaming) {
		GST_DEBUG_OBJECT(bcmdec, "input while streaming is false");
		gst_buffer_unref(buf);
		return GST_FLOW_WRONG_STATE;
	}

	pbuffer = GST_BUFFER_DATA (buf);
	size = GST_BUFFER_SIZE(buf);

	if (GST_FLOW_OK != bcmdec_send_buff_detect_error(bcmdec, buf, pbuffer, size, offset, tCurrent, bcmdec->proc_in_flags)) {
		gst_buffer_unref(buf);
		return GST_FLOW_ERROR;
	}

#ifdef FILE_DUMP__
	bytes_written = fwrite(GST_BUFFER_DATA(buf), sizeof(unsigned char), GST_BUFFER_SIZE(buf), bcmdec->fhnd);
#endif

	gst_buffer_unref(buf);
	return GST_FLOW_OK;
}

static gboolean gst_bcmdec_src_event(GstPad *pad, GstEvent *event)
{
	gboolean result;
	GstBcmDec *bcmdec;

	bcmdec = GST_BCMDEC(GST_OBJECT_PARENT(pad));

	result = gst_pad_push_event(bcmdec->sinkpad, event);

	return result;
}

static gboolean bcmdec_negotiate_format(GstBcmDec *bcmdec)
{
	GstCaps *caps;
	guint32 fourcc;
	gboolean result;
	guint num = (guint)(bcmdec->output_params.framerate * 1000);
	guint den = 1000;
	GstStructure *s1;
	const GValue *framerate_value;
	GstVideoFormat vidFmt;

#ifdef YV12__
	fourcc = GST_STR_FOURCC("YV12");
	vidFmt = GST_VIDEO_FORMAT_YV12;
#else
	fourcc = GST_STR_FOURCC("YUY2");
	vidFmt = GST_VIDEO_FORMAT_YUY2;
#endif
	GST_DEBUG_OBJECT(bcmdec, "framerate = %f", bcmdec->output_params.framerate);

	if(bcmdec->interlace) {
		caps = gst_video_format_new_caps_interlaced(vidFmt, bcmdec->output_params.width,
													bcmdec->output_params.height, num, den,
													bcmdec->output_params.aspectratio_x,
													bcmdec->output_params.aspectratio_y,
													TRUE);
	} else {
		caps = gst_video_format_new_caps(vidFmt, bcmdec->output_params.width,
													bcmdec->output_params.height, num, den,
													bcmdec->output_params.aspectratio_x,
													bcmdec->output_params.aspectratio_y);
	}

	result = gst_pad_set_caps(bcmdec->srcpad, caps);
	GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_negotiate_format %d", result);

	if (bcmdec->output_params.clr_space == MODE422_YUY2) {
		bcmdec->output_params.y_size = bcmdec->output_params.width * bcmdec->output_params.height * BUF_MULT;
		if (bcmdec->interlace) {
			GST_DEBUG_OBJECT(bcmdec, "bcmdec_negotiate_format Interlaced");
			bcmdec->output_params.y_size /= 2;
		}
		bcmdec->output_params.uv_size = 0;
		GST_DEBUG_OBJECT(bcmdec, "YUY2 set on caps");
	} else if (bcmdec->output_params.clr_space == MODE420) {
		bcmdec->output_params.y_size = bcmdec->output_params.width * bcmdec->output_params.height;
		bcmdec->output_params.uv_size = bcmdec->output_params.width * bcmdec->output_params.height / 2;
#ifdef YV12__
		if (bcmdec->interlace) {
			GST_DEBUG_OBJECT(bcmdec, "bcmdec_negotiate_format Interlaced");
			bcmdec->output_params.y_size = bcmdec->output_params.width * bcmdec->output_params.height / 2;
			bcmdec->output_params.uv_size = bcmdec->output_params.y_size / 2;
		}
#endif
		GST_DEBUG_OBJECT(bcmdec, "420 set on caps");
	}

	s1 = gst_caps_get_structure(caps, 0);

	framerate_value = gst_structure_get_value(s1, "framerate");
	if (framerate_value != NULL) {
		num = gst_value_get_fraction_numerator(framerate_value);
		den = gst_value_get_fraction_denominator(framerate_value);
		GST_DEBUG_OBJECT(bcmdec, "framerate = %f rate_num %d rate_den %d", bcmdec->output_params.framerate, num, den);
	} else {
		GST_DEBUG_OBJECT(bcmdec, "failed to get framerate_value");
	}

	framerate_value = gst_structure_get_value (s1, "pixel-aspect-ratio");
	if (framerate_value) {
		num = gst_value_get_fraction_numerator(framerate_value);
		den = gst_value_get_fraction_denominator(framerate_value);
		GST_DEBUG_OBJECT(bcmdec, "pixel-aspect-ratio_x = %d y %d ", num, den);
	} else {
		GST_DEBUG_OBJECT(bcmdec, "failed to get par");
	}

	gst_caps_unref(caps);

	return result;
}

static gboolean bcmdec_process_play(GstBcmDec *bcmdec)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	BC_INPUT_FORMAT bcInputFormat;

	GST_DEBUG_OBJECT(bcmdec, "Starting Process Play");

	bcInputFormat.OptFlags = 0; // NAREN - FIXME - Should we enable BD mode and max frame rate mode for LINK?
	bcInputFormat.FGTEnable = FALSE;
	bcInputFormat.MetaDataEnable = FALSE;
	bcInputFormat.Progressive =  !(bcmdec->interlace);
	bcInputFormat.mSubtype= bcmdec->input_format;

	//Use Demux Image Size for VC-1 Simple/Main
	if(bcInputFormat.mSubtype == BC_MSUBTYPE_WMV3)
	{
		//VC-1 Simple/Main
		bcInputFormat.width = bcmdec->frame_width;
		bcInputFormat.height = bcmdec->frame_height;
	}
	else
	{
		bcInputFormat.width = bcmdec->output_params.width;
		bcInputFormat.height = bcmdec->output_params.height;
	}

	bcInputFormat.startCodeSz = bcmdec->codec_params.nal_size_bytes;
	bcInputFormat.pMetaData = bcmdec->codec_params.sps_pps_buf;
	bcInputFormat.metaDataSz = bcmdec->codec_params.pps_size;
	bcInputFormat.OptFlags = 0x80000000 | vdecFrameRate23_97;

	// ENABLE the Following lines if HW Scaling is desired
	bcInputFormat.bEnableScaling = false;
//	bcInputFormat.ScalingParams.sWidth = 800;

	sts = decif_setinputformat(&bcmdec->decif, bcInputFormat);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "set input format success");
	} else {
		GST_ERROR_OBJECT(bcmdec, "set input format failed");
		bcmdec->streaming = FALSE;
		return FALSE;
	}

	sts = decif_prepare_play(&bcmdec->decif);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "prepare play success");
	} else {
		GST_ERROR_OBJECT(bcmdec, "prepare play failed");
		bcmdec->streaming = FALSE;
		return FALSE;
	}

	GST_DEBUG_OBJECT(bcmdec, "Setting color space %d", BUF_MODE);

	decif_setcolorspace(&bcmdec->decif, BUF_MODE);

	sts = decif_start_play(&bcmdec->decif);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "start play success");
		bcmdec->streaming = TRUE;
	} else {
		GST_ERROR_OBJECT(bcmdec, "start play failed");
		bcmdec->streaming = FALSE;
		return FALSE;
	}

	if (sem_post(&bcmdec->play_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "sem_post failed");

	if (sem_post(&bcmdec->push_start_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "push_start post failed");

	return TRUE;
}

static GstStateChangeReturn gst_bcmdec_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn result = GST_STATE_CHANGE_SUCCESS;
	GstBcmDec *bcmdec = GST_BCMDEC(element);
	BC_STATUS sts = BC_STS_SUCCESS;
	GstClockTime clock_time;
	GstClockTime base_clock_time;
	int ret = 0;

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_DEBUG_OBJECT(bcmdec, "State change from NULL_TO_READY");
		if (bcmdec_mul_inst_cor(bcmdec)) {
			sts = decif_open(&bcmdec->decif);
			if (sts == BC_STS_SUCCESS) {
				GST_DEBUG_OBJECT(bcmdec, "dev open success");
				parse_init(&bcmdec->parse);
			} else {
				GST_ERROR_OBJECT(bcmdec, "dev open failed %d",sts);
				GST_ERROR_OBJECT(bcmdec, "dev open failed...ret GST_STATE_CHANGE_FAILURE");
				return GST_STATE_CHANGE_FAILURE;
			}
		} else {
			GST_ERROR_OBJECT(bcmdec, "dev open failed...ret GST_STATE_CHANGE_FAILURE");
			return GST_STATE_CHANGE_FAILURE;
		}

		break;

	case GST_STATE_CHANGE_READY_TO_PAUSED:
		if (!bcmdec_start_recv_thread(bcmdec)) {
			GST_ERROR_OBJECT(bcmdec, "GST_STATE_CHANGE_NULL_TO_READY -failed");
			return GST_STATE_CHANGE_FAILURE;
		}
		if (!bcmdec_start_push_thread(bcmdec)) {
			GST_ERROR_OBJECT(bcmdec, "GST_STATE_CHANGE_READY_TO_THREAD -failed");
			return GST_STATE_CHANGE_FAILURE;
		}
		if (!bcmdec_start_get_rbuf_thread(bcmdec)) {
			GST_ERROR_OBJECT(bcmdec, "GST_STATE_CHANGE_THREAD_TO_RBUF -failed");
			return GST_STATE_CHANGE_FAILURE;
		}

		bcmdec->play_pending = TRUE;
		GST_DEBUG_OBJECT(bcmdec, "GST_STATE_CHANGE_READY_TO_PAUSED");
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_DEBUG_OBJECT(bcmdec, "GST_STATE_CHANGE_PAUSED_TO_PLAYING");
		bcmdec->gst_clock = gst_element_get_clock(element);
		if (bcmdec->gst_clock) {
			//printf("clock available %p\n",bcmdec->gst_clock);
			base_clock_time = gst_element_get_base_time(element);
			//printf("base clock time %lld\n",base_clock_time);
			clock_time = gst_clock_get_time(bcmdec->gst_clock);
			//printf(" clock time %lld\n",clock_time);
		}
		break;

	case GST_STATE_CHANGE_PAUSED_TO_READY:
		GST_DEBUG_OBJECT(bcmdec, "GST_STATE_CHANGE_PAUSED_TO_READY");
		bcmdec->streaming = FALSE;
		GST_DEBUG_OBJECT(bcmdec, "Flushing\n");
		sts = decif_flush_dec(&bcmdec->decif, 2);
		if (sts != BC_STS_SUCCESS)
			GST_ERROR_OBJECT(bcmdec, "Dec flush failed %d",sts);
		if (!bcmdec->play_pending) {
			GST_DEBUG_OBJECT(bcmdec, "Stopping\n");
			sts = decif_stop(&bcmdec->decif);
			if (sts == BC_STS_SUCCESS) {
				if (!bcmdec->silent)
					GST_DEBUG_OBJECT(bcmdec, "stop play success");
				g_inst_sts->cur_decode = UNKNOWN;
				g_inst_sts->rendered_frames = 0;
				GST_DEBUG_OBJECT(bcmdec, "cur_dec set to UNKNOWN");

			} else {
				GST_ERROR_OBJECT(bcmdec, "stop play failed %d", sts);
			}
		}
		GST_DEBUG_OBJECT(bcmdec, "Stopping threads\n");
		if (bcmdec->get_rbuf_thread) {
			GST_DEBUG_OBJECT(bcmdec, "rbuf stop event");
			if (sem_post(&bcmdec->rbuf_stop_event) == -1)
				GST_ERROR_OBJECT(bcmdec, "sem_post failed");
			GST_DEBUG_OBJECT(bcmdec, "waiting for get_rbuf_thread exit");
			ret = pthread_join(bcmdec->get_rbuf_thread, NULL);
			GST_DEBUG_OBJECT(bcmdec, "get_rbuf_thread exit - %d errno = %d", ret, errno);
			bcmdec->get_rbuf_thread = 0;
		}

		if (bcmdec->recv_thread) {
			GST_DEBUG_OBJECT(bcmdec, "quit event");
			if (sem_post(&bcmdec->quit_event) == -1)
				GST_ERROR_OBJECT(bcmdec, "sem_post failed");
			GST_DEBUG_OBJECT(bcmdec, "waiting for rec_thread exit");
			ret = pthread_join(bcmdec->recv_thread, NULL);
			GST_DEBUG_OBJECT(bcmdec, "thread exit - %d errno = %d", ret, errno);
			bcmdec->recv_thread = 0;
		}

		if (bcmdec->push_thread) {
			GST_DEBUG_OBJECT(bcmdec, "waiting for push_thread exit");
			ret = pthread_join(bcmdec->push_thread, NULL);
			GST_DEBUG_OBJECT(bcmdec, "push_thread exit - %d errno = %d", ret, errno);
			bcmdec->push_thread = 0;
		}

		break;

	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_DEBUG_OBJECT(bcmdec, "GST_STATE_CHANGE_PLAYING_TO_PAUSED");
		break;

	default:
		GST_DEBUG_OBJECT(bcmdec, "default %d", transition);
		break;
	}
	result = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
	if (result == GST_STATE_CHANGE_FAILURE) {
		GST_ERROR_OBJECT(bcmdec, "parent class state change failed");
		return result;
	}

	if(transition == GST_STATE_CHANGE_READY_TO_NULL) {
		GST_DEBUG_OBJECT(bcmdec, "GST_STATE_CHANGE_READY_TO_NULL");
		sts = gst_bcmdec_cleanup(bcmdec);
		if (sts == BC_STS_SUCCESS)
			GST_DEBUG_OBJECT(bcmdec, "dev close success");
		else
			GST_ERROR_OBJECT(bcmdec, "dev close failed %d", sts);
	}

	return result;
}


GstClockTime gst_get_current_timex (void)
{
	GTimeVal tv;

	g_get_current_time(&tv);
	return GST_TIMEVAL_TO_TIME(tv);
}

clock_t bcm_get_tick_count()
{
	tms tm;
	return times(&tm);
}

static gboolean bcmdec_get_buffer(GstBcmDec *bcmdec, GstBuffer **obuf)
{
	GstFlowReturn ret;
	GST_DEBUG_OBJECT(bcmdec, "gst_pad_alloc_buffer_and_set_caps ");

	ret = gst_pad_alloc_buffer_and_set_caps(bcmdec->srcpad,
						GST_BUFFER_OFFSET_NONE,
						bcmdec->output_params.width * bcmdec->output_params.height * BUF_MULT,
						GST_PAD_CAPS (bcmdec->srcpad), obuf);
	if (ret != GST_FLOW_OK) {
		GST_ERROR_OBJECT(bcmdec, "gst_pad_alloc_buffer_and_set_caps failed %d ",ret);
		return FALSE;
	}

	if (((uintptr_t)GST_BUFFER_DATA(*obuf)) % 4)
		GST_DEBUG_OBJECT(bcmdec, "buf is not aligned");

	return TRUE;
}


static void bcmdec_init_procout(GstBcmDec *bcmdec,BC_DTS_PROC_OUT* pout, guint8* buf)
{
	// GSTREAMER only supports Interleaved mode for Interlaced content
	//if (bcmdec->format_reset)
	{
		memset(pout,0,sizeof(BC_DTS_PROC_OUT));
		pout->PicInfo.width = bcmdec->output_params.width ;
		pout->PicInfo.height = bcmdec->output_params.height;
		pout->YbuffSz = bcmdec->output_params.y_size / 4;
		pout->UVbuffSz = bcmdec->output_params.uv_size / 4;

		pout->PoutFlags = BC_POUT_FLAGS_SIZE ;
#ifdef YV12__
		pout->PoutFlags |= BC_POUT_FLAGS_YV12;
#endif
		if (bcmdec->interlace)
			pout->PoutFlags |= BC_POUT_FLAGS_INTERLACED;

		if ((bcmdec->output_params.stride) || (bcmdec->interlace)) {
			pout->PoutFlags |= BC_POUT_FLAGS_STRIDE ;
			if (bcmdec->interlace)
				pout->StrideSz = bcmdec->output_params.width + 2 * bcmdec->output_params.stride;
			else
				pout->StrideSz = bcmdec->output_params.stride;
		}
//		bcmdec->format_reset = FALSE;
	}
	pout->PoutFlags = pout->PoutFlags & 0xff;
	pout->Ybuff = (uint8_t*)buf;

	if (pout->UVbuffSz) {
		if (bcmdec->interlace) {
			pout->UVbuff = buf + bcmdec->output_params.y_size * 2;
			if (bcmdec->sec_field) {
				pout->Ybuff += bcmdec->output_params.width;
				pout->UVbuff += bcmdec->output_params.width / 2;
			}
		} else {
			pout->UVbuff = buf + bcmdec->output_params.y_size;
		}
	} else {
		pout->UVbuff = NULL;
		if (bcmdec->interlace) {
			if (bcmdec->sec_field)
				pout->Ybuff +=  bcmdec->output_params.width * 2;
		}
	}

	return;
}

static void bcmdec_set_framerate(GstBcmDec * bcmdec,guint32 nFrameRate)
{
	gdouble framerate;

//	bcmdec->interlace = FALSE;
	framerate = (gdouble)nFrameRate / 1000;

	if((framerate) && (bcmdec->output_params.framerate != framerate))
	{
		bcmdec->output_params.framerate = framerate;
		bcmdec->frame_time = (GstClockTime)(UNITS / bcmdec->output_params.framerate);

		//if (bcmdec->interlace)
		//	bcmdec->output_params.framerate /= 2;

		GST_DEBUG_OBJECT(bcmdec, "framerate = %x", framerate);
	}
}

static void bcmdec_set_aspect_ratio(GstBcmDec *bcmdec, BC_PIC_INFO_BLOCK *pic_info)
{
	switch (pic_info->aspect_ratio) {
	case vdecAspectRatioSquare:
		bcmdec->output_params.aspectratio_x = 1;
		bcmdec->output_params.aspectratio_y = 1;
		break;
	case vdecAspectRatio12_11:
		bcmdec->output_params.aspectratio_x = 12;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio10_11:
		bcmdec->output_params.aspectratio_x = 10;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio16_11:
		bcmdec->output_params.aspectratio_x = 16;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio40_33:
		bcmdec->output_params.aspectratio_x = 40;
		bcmdec->output_params.aspectratio_y = 33;
		break;
	case vdecAspectRatio24_11:
		bcmdec->output_params.aspectratio_x = 24;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio20_11:
		bcmdec->output_params.aspectratio_x = 20;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio32_11:
		bcmdec->output_params.aspectratio_x = 32;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio80_33:
		bcmdec->output_params.aspectratio_x = 80;
		bcmdec->output_params.aspectratio_y = 33;
		break;
	case vdecAspectRatio18_11:
		bcmdec->output_params.aspectratio_x = 18;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio15_11:
		bcmdec->output_params.aspectratio_x = 15;
		bcmdec->output_params.aspectratio_y = 11;
		break;
	case vdecAspectRatio64_33:
		bcmdec->output_params.aspectratio_x = 64;
		bcmdec->output_params.aspectratio_y = 33;
		break;
	case vdecAspectRatio160_99:
		bcmdec->output_params.aspectratio_x = 160;
		bcmdec->output_params.aspectratio_y = 99;
		break;
	case vdecAspectRatio4_3:
		bcmdec->output_params.aspectratio_x = 4;
		bcmdec->output_params.aspectratio_y = 3;
		break;
	case vdecAspectRatio16_9:
		bcmdec->output_params.aspectratio_x = 16;
		bcmdec->output_params.aspectratio_y = 9;
		break;
	case vdecAspectRatio221_1:
		bcmdec->output_params.aspectratio_x = 221;
		bcmdec->output_params.aspectratio_y = 1;
		break;
	case vdecAspectRatioOther:
		bcmdec->output_params.aspectratio_x = pic_info->custom_aspect_ratio_width_height & 0x0000ffff;
		bcmdec->output_params.aspectratio_y = pic_info->custom_aspect_ratio_width_height >> 16;
		break;
	case vdecAspectRatioUnknown:
	default:
		bcmdec->output_params.aspectratio_x = 0;
		bcmdec->output_params.aspectratio_y = 0;
		break;
	}

	// Use Demux Aspect ratio first before falling back to HW ratio
	if(bcmdec->input_par_x != 0) {
		bcmdec->output_params.aspectratio_x = bcmdec->input_par_x;
		bcmdec->output_params.aspectratio_y = bcmdec->input_par_y;
	} else if (bcmdec->output_params.aspectratio_x == 0) {
		bcmdec->output_params.aspectratio_x = 1;
		bcmdec->output_params.aspectratio_y = 1;
	}

	GST_DEBUG_OBJECT(bcmdec, "dec_par x = %d", bcmdec->output_params.aspectratio_x);
	GST_DEBUG_OBJECT(bcmdec, "dec_par y = %d", bcmdec->output_params.aspectratio_y);
}

static gboolean bcmdec_format_change(GstBcmDec *bcmdec, BC_PIC_INFO_BLOCK *pic_info)
{
	GST_DEBUG_OBJECT(bcmdec, "Got format Change to %dx%d", pic_info->width, pic_info->height);
	gboolean result = FALSE;

	if (pic_info->height == 1088)
		pic_info->height = 1080;

	bcmdec->output_params.width  = pic_info->width;
	bcmdec->output_params.height = pic_info->height;

	bcmdec_set_aspect_ratio(bcmdec,pic_info);

	// Interlaced
	if((pic_info->flags & VDEC_FLAG_INTERLACED_SRC) == VDEC_FLAG_INTERLACED_SRC)
		bcmdec->interlace = true;
	else
		bcmdec->interlace = false;

	if( (bcmdec->input_format == BC_MSUBTYPE_AVC1) || (bcmdec->input_format == BC_MSUBTYPE_H264)) {
		if (!bcmdec->interlace &&
			(pic_info->pulldown == vdecFrame_X1) &&
			(pic_info->flags & VDEC_FLAG_FIELDPAIR) &&
			(pic_info->flags & VDEC_FLAG_INTERLACED_SRC))
				bcmdec->interlace = true;
	}

	result = bcmdec_negotiate_format(bcmdec);
	if (!bcmdec->silent) {
		if (result)
			GST_DEBUG_OBJECT(bcmdec, "negotiate_format success");
		else
			GST_ERROR_OBJECT(bcmdec, "negotiate_format failed");
	}
	//bcmdec->format_reset = TRUE;
	return result;
}

static int bcmdec_wait_for_event(GstBcmDec *bcmdec)
{
	int ret = 0, i = 0;
	sem_t *event_list[] = { &bcmdec->play_event, &bcmdec->quit_event };

	GST_DEBUG_OBJECT(bcmdec, "Waiting for event\n");

	while (1) {
		for (i = 0; i < 2; i++) {

			ret = sem_trywait(event_list[i]);
			if (ret == 0) {
				GST_DEBUG_OBJECT(bcmdec, "event wait over in Rx thread ret = %d",i);
				return i;
			} else if (errno == EINTR) {
				break;
			}
			if (bcmdec->streaming)
				break;
		}
		usleep(10);
	}
}

static void bcmdec_flush_gstbuf_queue(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *gst_queue_element;
	int sval;

	do {
		gst_queue_element = bcmdec_rem_buf(bcmdec);
		if (gst_queue_element) {
			if (gst_queue_element->gstbuf) {
				gst_buffer_unref (gst_queue_element->gstbuf);
				bcmdec_put_que_mem_buf(bcmdec,gst_queue_element);
			}
		} else {
			GST_DEBUG_OBJECT(bcmdec, "no gst_queue_element");
		}
	} while (gst_queue_element && gst_queue_element->gstbuf);

	// Re-initialize the buf_event semaphone since we have just flushed the entire queued
	sem_destroy(&bcmdec->buf_event);
	sem_init(&bcmdec->buf_event, 0, 0);
	sem_getvalue(&bcmdec->buf_event, &sval);
	GST_DEBUG_OBJECT(bcmdec, "sem value after flush is %d", sval);
}

static void * bcmdec_process_push(void *ctx)
{
	GstBcmDec *bcmdec = (GstBcmDec *)ctx;
	GSTBUF_LIST *gst_queue_element = NULL;
	gboolean result = FALSE, done = FALSE;
	struct timespec ts;
	gint ret;

	ts.tv_sec = time(NULL);
	ts.tv_nsec = 30 * 1000000;

	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "process push starting ");

	while (1) {

		if (!bcmdec->recv_thread && !bcmdec->streaming) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "process push exiting..");
			break;
		}

		ret = sem_timedwait(&bcmdec->push_start_event, &ts);
		if (ret < 0) {
			if (errno == ETIMEDOUT)
				continue;
			else if (errno == EINTR)
				break;
		} else {
			GST_DEBUG_OBJECT(bcmdec, "push_start wait over");
			done = FALSE;
		}

		ts.tv_sec = time(NULL) + 1;
		ts.tv_nsec = 0;

		while (bcmdec->streaming && !done) {
			ret = sem_timedwait(&bcmdec->buf_event, &ts);
			if (ret < 0) {
				switch (errno) {
				case ETIMEDOUT:
					if (bcmdec->streaming) {
						continue;
					} else {
						done = TRUE;
						GST_DEBUG_OBJECT(bcmdec, "TOB ");
						break;
					}
				case EINTR:
					GST_DEBUG_OBJECT(bcmdec, "Sig interrupt ");
					done = TRUE;
					break;

				default:
					GST_ERROR_OBJECT(bcmdec, "sem wait failed %d ", errno);
					done = TRUE;
					break;
				}
			}
			if (ret == 0) {
				GST_DEBUG_OBJECT(bcmdec, "Starting to PUSH ");
				gst_queue_element = bcmdec_rem_buf(bcmdec);
				if (gst_queue_element) {
					if (gst_queue_element->gstbuf) {
						GST_DEBUG_OBJECT(bcmdec, "Trying to PUSH ");
						result = gst_pad_push(bcmdec->srcpad, gst_queue_element->gstbuf);
						if (result != GST_FLOW_OK) {
							GST_DEBUG_OBJECT(bcmdec, "exiting, failed to push sts = %d", result);
							gst_buffer_unref(gst_queue_element->gstbuf);
							done = TRUE;
						} else {
							GST_DEBUG_OBJECT(bcmdec, "PUSHED, Qcnt:%d, Rcnt:%d", bcmdec->gst_que_cnt, bcmdec->gst_padbuf_que_cnt);
							if ((g_inst_sts->rendered_frames++ > THUMBNAIL_FRAMES) && (g_inst_sts->cur_decode != PLAYBACK)) {
								g_inst_sts->cur_decode = PLAYBACK;
								GST_DEBUG_OBJECT(bcmdec, "cur_dec set to PLAYBACK");
							}
						}
					} else {
						/*exit */ /* NULL value of gstbuf indicates EOS */
						gst_pad_push_event(bcmdec->srcpad, bcmdec->ev_eos);
						gst_event_unref(bcmdec->ev_eos);
						done = TRUE;
						bcmdec->streaming = FALSE;
						GST_DEBUG_OBJECT(bcmdec, "eos sent, cnt:%d", bcmdec->gst_que_cnt);
					}
					bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
				} else
					GST_DEBUG_OBJECT(bcmdec, "NO BUFFER FOUND");
			}
			if (bcmdec->flushing && bcmdec->push_exit) {
				GST_DEBUG_OBJECT(bcmdec, "push -flush exit");
				break;
			}
		}
		if (bcmdec->flushing) {
			GST_DEBUG_OBJECT(bcmdec, "flushing gstbuf queue");
			bcmdec_flush_gstbuf_queue(bcmdec);
			if (sem_post(&bcmdec->push_stop_event) == -1)
				GST_ERROR_OBJECT(bcmdec, "push_stop sem_post failed");
			g_inst_sts->rendered_frames = 0;
		}
	}
	bcmdec_flush_gstbuf_queue(bcmdec);
	GST_DEBUG_OBJECT(bcmdec, "process push exiting.. ");
	pthread_exit((void*)&result);
}

static void * bcmdec_process_output(void *ctx)
{
	BC_DTS_PROC_OUT pout;
	BC_STATUS sts = BC_STS_SUCCESS;
	GstBcmDec *bcmdec = (GstBcmDec *)ctx;
	GstBuffer *gstbuf = NULL;
	gboolean rx_flush = FALSE, bEOS = FALSE;
	const guint first_picture = 3;
	guint32 pic_number = 0;
	GstClockTime clock_time = 0;
	gboolean first_frame_after_seek = FALSE;
	GstClockTime cur_stream_time_diff = 0;
	int wait_cnt = 0;
	guint32 nextPicNumFlags = 0;

	gboolean is_paused = FALSE;

	GSTBUF_LIST *gst_queue_element = NULL;

	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "Rx thread started");

	while (1) {
		if (1 == bcmdec_wait_for_event(bcmdec)) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "quit event set, exit");
			break;
		}

		GST_DEBUG_OBJECT(bcmdec, "wait over streaming = %d", bcmdec->streaming);
		while (bcmdec->streaming && !bcmdec->last_picture_set) {
			GST_DEBUG_OBJECT(bcmdec, "Getting Status\n");
			// NAREN FIXME - This is HARDCODED right now till we get HW PAUSE and RESUME working from the driver
			uint32_t rll;
			gboolean tmp;
			decif_get_drv_status(&(bcmdec->decif), &tmp, &rll, &nextPicNumFlags);
			if(rll >= 12 && !is_paused) {
				GST_DEBUG_OBJECT(bcmdec, "HW PAUSE with RLL %u", rll);
				decif_pause(&(bcmdec->decif), TRUE);
				is_paused = TRUE;
			}
			else if (rll < 12 && is_paused) {
				GST_DEBUG_OBJECT(bcmdec, "HW RESUME with RLL %u", rll);
				decif_pause(&(bcmdec->decif), false);
				is_paused = FALSE;
			}

			if(rll == 0) {
				usleep(3 * 1000);
				continue;
			}

			guint8* data_ptr;
			if (gstbuf == NULL) {
				if (!bcmdec->rbuf_thread_running) {
					if (!bcmdec_get_buffer(bcmdec, &gstbuf)) {
						usleep(30 * 1000);
						continue;
					}
					GST_DEBUG_OBJECT(bcmdec, "got default buffer, going to proc output");
				} else {
					if (gst_queue_element) {
						if (gst_queue_element->gstbuf)
							gst_buffer_unref(gst_queue_element->gstbuf);
						bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
						gst_queue_element = NULL;
					}

					gst_queue_element = bcmdec_rem_padbuf(bcmdec);
					if (!gst_queue_element) {
						GST_DEBUG_OBJECT(bcmdec, "rbuf queue empty");
						usleep(10 * 1000);
						continue;
					}

					gstbuf = gst_queue_element->gstbuf;
					if (!gstbuf) {
						bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
						gst_queue_element = NULL;
						usleep(10 * 1000);
						continue;
					}
					GST_DEBUG_OBJECT(bcmdec, "got rbuf, going to proc output");
				}
			}
			else
				GST_DEBUG_OBJECT(bcmdec, "re-using rbuf, going to proc output");

			data_ptr = GST_BUFFER_DATA(gstbuf);

			bcmdec_init_procout(bcmdec, &pout, data_ptr);
			rx_flush = TRUE;
			pout.PicInfo.picture_number = 0;
			// For interlaced content, if I am holding a buffer but the next buffer is not from the same picture
			// i.e. the second field, then assume that this is a case of one field per picture and deliver this field
			// Don't deliver the one with picture number of 0
			if(bcmdec->sec_field) {
				if(((nextPicNumFlags & 0x0FFFFFFF) - first_picture) != pic_number) {
					if(pic_number == 0)
						gst_buffer_unref(gstbuf);
					else if (gst_queue_element) {
						GST_BUFFER_FLAG_SET(gstbuf, GST_VIDEO_BUFFER_ONEFIELD);
						gst_queue_element->gstbuf = gstbuf;
						bcmdec_ins_buf(bcmdec, gst_queue_element);
						bcmdec->prev_pic = pic_number;
						gst_queue_element = NULL;
					} else {
						GST_DEBUG_OBJECT(bcmdec, "SOMETHING BAD HAPPENED\n");
						gst_buffer_unref(gstbuf);
					}
					gstbuf = NULL;
					bcmdec->sec_field = FALSE;;
					continue;
				}
			}
			sts = DtsProcOutput(bcmdec->decif.hdev, PROC_TIMEOUT, &pout);
			GST_DEBUG_OBJECT(bcmdec, "procoutput status %d", sts);
			switch (sts) {
			case BC_STS_FMT_CHANGE:
				if (!bcmdec->silent)
					GST_DEBUG_OBJECT(bcmdec, "procout ret FMT");
				if ((pout.PoutFlags & BC_POUT_FLAGS_PIB_VALID) &&
				    (pout.PoutFlags & BC_POUT_FLAGS_FMT_CHANGE)) {
					if (bcmdec_format_change(bcmdec, &pout.PicInfo)) {
						GST_DEBUG_OBJECT(bcmdec, "format change success");
						//bcmdec->frame_time = (GstClockTime)(UNITS / bcmdec->output_params.framerate);
						bcmdec->last_spes_time  = 0;
						bcmdec->prev_clock_time = 0;
						cur_stream_time_diff    = 0;
						first_frame_after_seek  = TRUE;
					} else {
						GST_DEBUG_OBJECT(bcmdec, "format change failed");
					}
				}
				gst_buffer_unref(gstbuf);
				gstbuf = NULL;
				bcmdec->sec_field = FALSE;

				if (sem_post(&bcmdec->rbuf_start_event) == -1)
					GST_ERROR_OBJECT(bcmdec, "rbuf sem_post failed");

				//should modify to wait event
				wait_cnt = 0;
				while (!bcmdec->rbuf_thread_running && (wait_cnt < 5000)) {
					usleep(1000);
					wait_cnt++;
				}
				GST_DEBUG_OBJECT(bcmdec, "format change wait for rbuf thread start wait_cnt:%d", wait_cnt);

				break;
			case BC_STS_SUCCESS:
				if (!(pout.PoutFlags & BC_POUT_FLAGS_PIB_VALID)) {
					if (!bcmdec->silent)
						GST_DEBUG_OBJECT(bcmdec, "procout ret PIB miss %d", pout.PicInfo.picture_number - 3);
					continue;
				}

				bcmdec_set_framerate(bcmdec, pout.PicInfo.frame_rate);
				pic_number = pout.PicInfo.picture_number - first_picture;

				if (!bcmdec->silent)
					GST_DEBUG_OBJECT(bcmdec, "pic_number from HW is %u", pout.PicInfo.picture_number);

				if (bcmdec->flushing) {
					GST_DEBUG_OBJECT(bcmdec, "flushing discard, pic = %d", pic_number);
					continue;
				}

				if (bcmdec->prev_pic + 1 < pic_number) {
					if (!bcmdec->silent)
						GST_DEBUG_OBJECT(bcmdec, "LOST PICTURE pic_no = %d, prev = %d", pic_number, bcmdec->prev_pic);
				}

/*				if ((bcmdec->prev_pic == pic_number) && (bcmdec->ses_nbr  == pout.PicInfo.sess_num) && !bcmdec->interlace) {
					if (!bcmdec->silent)
						GST_DEBUG_OBJECT(bcmdec, "rp");

					if (!(pout.PicInfo.flags &  VDEC_FLAG_LAST_PICTURE))
						continue;
				}*/

				if (!bcmdec->interlace || bcmdec->sec_field) {
					GST_DEBUG_OBJECT(bcmdec, "Progressive or Second Field");
					GST_BUFFER_OFFSET(gstbuf) = 0;
					GST_BUFFER_TIMESTAMP(gstbuf) = bcmdec_get_time_stamp(bcmdec, pic_number, pout.PicInfo.timeStamp);
					GST_BUFFER_DURATION(gstbuf)  = bcmdec->frame_time;
					if (bcmdec->gst_clock) {
						clock_time = gst_clock_get_time(bcmdec->gst_clock);
						if (first_frame_after_seek) {
							bcmdec->prev_clock_time = clock_time;
							first_frame_after_seek = FALSE;
							cur_stream_time_diff = 0;
						}
						if (bcmdec->prev_clock_time > clock_time)
							bcmdec->prev_clock_time = 0;
						cur_stream_time_diff += clock_time - bcmdec->prev_clock_time;
						bcmdec->cur_stream_time = cur_stream_time_diff + bcmdec->base_clock_time;
						bcmdec->prev_clock_time = clock_time;
						if ((bcmdec->last_spes_time < bcmdec->cur_stream_time) &&
						    (!bcmdec->catchup_on) && (pout.PicInfo.timeStamp)) {
							bcmdec->catchup_on = TRUE;
							decif_decode_catchup(&bcmdec->decif, TRUE);
						} else if (bcmdec->catchup_on) {
							decif_decode_catchup(&bcmdec->decif, FALSE);
							bcmdec->catchup_on = FALSE;
						}
					}
				}

				GST_BUFFER_SIZE(gstbuf) = bcmdec->output_params.width * bcmdec->output_params.height * BUF_MULT;

				if (!bcmdec->interlace || bcmdec->sec_field) {
					if (gst_queue_element) {
						// If interlaced, set the GST_VIDEO_BUFFER_TFF flags
						if(bcmdec->sec_field)
							GST_BUFFER_FLAG_SET(gstbuf, GST_VIDEO_BUFFER_TFF);
						gst_queue_element->gstbuf = gstbuf;
						bcmdec_ins_buf(bcmdec, gst_queue_element);
						bcmdec->prev_pic = pic_number;
					} else {
						GST_ERROR_OBJECT(bcmdec, "This CANNOT HAPPEN");//pending error recovery
					}
					gstbuf = NULL;
					bcmdec->sec_field = FALSE;
					gst_queue_element = NULL;
				} else {
					GST_DEBUG_OBJECT(bcmdec, "Wait for second field");
					bcmdec->sec_field = TRUE;
				}
				break;

			case BC_STS_TIMEOUT:
				GST_DEBUG_OBJECT(bcmdec, "procout timeout QCnt:%d, RCnt:%d, Paused:%d",
						bcmdec->gst_que_cnt, bcmdec->gst_padbuf_que_cnt, bcmdec->paused);
				break;
			case BC_STS_IO_XFR_ERROR:
				GST_DEBUG_OBJECT(bcmdec, "procout xfer error");
				break;
			case BC_STS_IO_USER_ABORT:
			case BC_STS_IO_ERROR:
				bcmdec->streaming = FALSE;
				GST_DEBUG_OBJECT(bcmdec, "ABORT sts = %d", sts);
				if (gstbuf) {
					gst_buffer_unref(gstbuf);
					gstbuf = NULL;
				}
				break;
			case BC_STS_NO_DATA:
				GST_DEBUG_OBJECT(bcmdec, "procout no data");
				// Check for EOS
				decif_get_eos(&bcmdec->decif, &bEOS);
				if (bEOS) {
					if (gstbuf) {
						gst_buffer_unref(gstbuf);
						gstbuf = NULL;
					}
					if (gst_queue_element) {
						gst_queue_element->gstbuf = NULL;
						bcmdec_ins_buf(bcmdec, gst_queue_element);
						gst_queue_element = NULL;
					} else {
						GST_DEBUG_OBJECT(bcmdec, "queue element failed");
					}
					GST_DEBUG_OBJECT(bcmdec, "last picture set ");
					bcmdec->last_picture_set = TRUE;
				}
				break;
			default:
				GST_DEBUG_OBJECT(bcmdec, "unhandled status from Procout sts %d",sts);
				if (gstbuf) {
					gst_buffer_unref(gstbuf);
					gstbuf = NULL;
				}
				break;
			}
		}
		if (gstbuf) {
			gst_buffer_unref(gstbuf);
			gstbuf = NULL;
		}
		if (gst_queue_element) {
			bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
			gst_queue_element = NULL;
		}
		if (rx_flush) {
			if (!bcmdec->flushing) {
// 				GST_DEBUG_OBJECT(bcmdec, "DtsFlushRxCapture called");
// 				sts = decif_flush_rxbuf(&bcmdec->decif, FALSE);
// 				if (sts != BC_STS_SUCCESS)
// 					GST_DEBUG_OBJECT(bcmdec, "DtsFlushRxCapture failed");
			}
			rx_flush = FALSE;
			if (bcmdec->flushing) {
				if (sem_post(&bcmdec->recv_stop_event) == -1)
					GST_ERROR_OBJECT(bcmdec, "recv_stop sem_post failed");
			}
			GST_DEBUG_OBJECT(bcmdec, "DtsFlushRxCapture Done");
		}
	}
	GST_DEBUG_OBJECT(bcmdec, "Rx thread exiting ..");
	pthread_exit((void*)&sts);
}

static gboolean bcmdec_start_push_thread(GstBcmDec *bcmdec)
{
	gboolean result = TRUE;
	pthread_attr_t thread_attr;
	gint ret = 0;

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&bcmdec->push_thread, &thread_attr, bcmdec_process_push, bcmdec);
	pthread_attr_destroy(&thread_attr);

	if (!bcmdec->push_thread) {
		GST_ERROR_OBJECT(bcmdec, "Failed to create PushThread");
		result = FALSE;
	} else {
		GST_DEBUG_OBJECT(bcmdec, "Success to create PushThread");
	}

	ret = sem_init(&bcmdec->buf_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "play event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->push_start_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "play event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->push_stop_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "push_stop event init failed");
		result = FALSE;
	}

	return result;
}

static gboolean bcmdec_start_recv_thread(GstBcmDec *bcmdec)
{
	gboolean result = TRUE;
	gint ret = 0;
	pthread_attr_t thread_attr;

	if (!bcmdec_alloc_mem_buf_que_pool(bcmdec))
		GST_ERROR_OBJECT(bcmdec, "pool alloc failed/n");

	ret = sem_init(&bcmdec->play_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "play event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->quit_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "play event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->recv_stop_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "recv_stop event init failed");
		result = FALSE;
	}

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&bcmdec->recv_thread, &thread_attr, bcmdec_process_output, bcmdec);
	pthread_attr_destroy(&thread_attr);

	if (!bcmdec->recv_thread) {
		GST_ERROR_OBJECT(bcmdec, "Failed to create RxThread");
		result = FALSE;
	} else {
		GST_DEBUG_OBJECT(bcmdec, "Success to create RxThread");
	}

	return result;
}

static GstClockTime bcmdec_get_time_stamp(GstBcmDec *bcmdec, guint32 pic_no, GstClockTime spes_time)
{
	GstClockTime time_stamp = 0;
	GstClockTime frame_time = (GstClockTime)(UNITS / bcmdec->output_params.framerate);

	if (bcmdec->enable_spes) {
		if (spes_time) {
			time_stamp = spes_time ;
			if (bcmdec->spes_frame_cnt && bcmdec->last_output_spes_time) {
				bcmdec->frame_time = (time_stamp - bcmdec->last_output_spes_time) / bcmdec->spes_frame_cnt;
				bcmdec->spes_frame_cnt = 0;
			}
			if (bcmdec->frame_time > 0)
				frame_time = bcmdec->frame_time;
			bcmdec->spes_frame_cnt++;
			bcmdec->last_output_spes_time = bcmdec->last_spes_time = time_stamp;
		} else {
			if (bcmdec->frame_time > 0)
				frame_time = bcmdec->frame_time;
			bcmdec->last_spes_time += frame_time;
			time_stamp = bcmdec->last_spes_time;
			bcmdec->spes_frame_cnt++;
		}
	} else {
		time_stamp = (GstClockTime)(bcmdec->base_time + frame_time * pic_no);
	}

	if (!bcmdec->enable_spes) {
		if (bcmdec->interlace) {
			if (bcmdec->prev_pic == pic_no)
				bcmdec->rpt_pic_cnt++;
			time_stamp += bcmdec->rpt_pic_cnt * frame_time;
		}
	}

	return time_stamp;
}

static void bcmdec_process_flush_stop(GstBcmDec *bcmdec)
{
	bcmdec->ses_change  = TRUE;
	bcmdec->base_time   = 0;
	bcmdec->flushing    = FALSE;
	bcmdec->streaming   = TRUE;
	bcmdec->rpt_pic_cnt = 0;

	GST_DEBUG_OBJECT(bcmdec, "flush stop started");

	if (sem_post(&bcmdec->play_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "sem_post failed");

	bcmdec->push_exit = FALSE;

	if (sem_post(&bcmdec->push_start_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "push_start post failed");

	GST_DEBUG_OBJECT(bcmdec, "flush stop complete");

}

static void bcmdec_process_flush_start(GstBcmDec *bcmdec)
{
	gint ret = 1;
	BC_STATUS sts = BC_STS_SUCCESS;
	struct timespec ts;

	ts.tv_sec=time(NULL) + 5;
	ts.tv_nsec = 0;

	bcmdec->flushing  = TRUE;
	bcmdec->streaming = FALSE;

	ret = sem_timedwait(&bcmdec->recv_stop_event, &ts);
	if (ret < 0) {
		switch (errno) {
		case ETIMEDOUT:
			GST_DEBUG_OBJECT(bcmdec, "recv_stop_event sig timed out ");
			break;
		case EINTR:
			GST_DEBUG_OBJECT(bcmdec, "Sig interrupt ");
			break;
		default:
			GST_ERROR_OBJECT(bcmdec, "sem wait failed %d ",errno);
			break;
		}
	}

	bcmdec->push_exit = TRUE;

	ret = sem_timedwait(&bcmdec->push_stop_event, &ts);
	if (ret < 0) {
		switch (errno) {
		case ETIMEDOUT:
			GST_DEBUG_OBJECT(bcmdec, "push_stop_event sig timed out ");
			break;
		case EINTR:
			GST_DEBUG_OBJECT(bcmdec, "Sig interrupt ");
			break;
		default:
			GST_ERROR_OBJECT(bcmdec, "sem wait failed %d ",errno);
			break;
		}
	}
	sts = decif_flush_dec(&bcmdec->decif, 2);
	if (sts != BC_STS_SUCCESS)
		GST_ERROR_OBJECT(bcmdec, "flush_dec failed sts %d", sts);
}

static BC_STATUS gst_bcmdec_cleanup(GstBcmDec *bcmdec)
{
	BC_STATUS sts = BC_STS_SUCCESS;

	GST_DEBUG_OBJECT(bcmdec, "gst_bcmdec_cleanup - enter");
	bcmdec->streaming = FALSE;

	bcmdec_release_mem_buf_que_pool(bcmdec);
//	bcmdec_release_mem_rbuf_que_pool(bcmdec);

	if (bcmdec->decif.hdev)
		sts = decif_close(&bcmdec->decif);

	sem_destroy(&bcmdec->quit_event);
	sem_destroy(&bcmdec->play_event);
	sem_destroy(&bcmdec->push_start_event);
	sem_destroy(&bcmdec->buf_event);
	sem_destroy(&bcmdec->rbuf_start_event);
	sem_destroy(&bcmdec->rbuf_stop_event);
	sem_destroy(&bcmdec->rbuf_ins_event);
	sem_destroy(&bcmdec->push_stop_event);
	sem_destroy(&bcmdec->recv_stop_event);

	pthread_mutex_destroy(&bcmdec->gst_buf_que_lock);
	pthread_mutex_destroy(&bcmdec->gst_padbuf_que_lock);
	//pthread_mutex_destroy(&bcmdec->fn_lock);
	if (bcmdec->codec_params.sps_pps_buf) {
		free(bcmdec->codec_params.sps_pps_buf);
		bcmdec->codec_params.sps_pps_buf = NULL;
	}

	if (bcmdec->dest_buf) {
		free(bcmdec->dest_buf);
		bcmdec->dest_buf = NULL;
	}

// 	if (bcmdec->vc1_dest_buffer) {
// 		free(bcmdec->vc1_dest_buffer);
// 		bcmdec->vc1_dest_buffer = NULL;
// 	}

	if (bcmdec->gst_clock) {
		gst_object_unref(bcmdec->gst_clock);
		bcmdec->gst_clock = NULL;
	}

	if (sem_post(&g_inst_sts->inst_ctrl_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "inst_ctrl_event post failed");
	else
		GST_DEBUG_OBJECT(bcmdec, "inst_ctrl_event posted");

	return sts;
}

static void bcmdec_reset(GstBcmDec * bcmdec)
{
	bcmdec->dec_ready = FALSE;
	bcmdec->streaming = FALSE;
	bcmdec->format_reset = TRUE;
	bcmdec->interlace = FALSE;

	bcmdec->output_params.width = 720;
	bcmdec->output_params.height = 480;
	bcmdec->output_params.framerate = 29;
	bcmdec->output_params.aspectratio_x = 16;
	bcmdec->output_params.aspectratio_y = 9;
	bcmdec->output_params.clr_space = BUF_MODE;
	if (bcmdec->output_params.clr_space == MODE420) { /* MODE420 */
		bcmdec->output_params.y_size = 720 * 480;
		bcmdec->output_params.uv_size = 720 * 480 / 2;
	} else { /* MODE422_YUV */
		bcmdec->output_params.y_size = 720 * 480 * 2;
		bcmdec->output_params.uv_size = 0;
	}

	bcmdec->output_params.stride = 0;

	bcmdec->base_time = 0;
	bcmdec->fhnd = NULL;

	bcmdec->play_pending = FALSE;

	bcmdec->gst_buf_que_hd = NULL;
	bcmdec->gst_buf_que_tl = NULL;
	bcmdec->gst_que_cnt = 0;
	bcmdec->last_picture_set = FALSE;
	bcmdec->gst_buf_que_sz = GST_BUF_LIST_POOL_SZ;
	bcmdec->gst_padbuf_que_sz = GST_RENDERER_BUF_POOL_SZ;
	bcmdec->rbuf_thread_running = FALSE;

	bcmdec->insert_start_code = FALSE;
	bcmdec->codec_params.sps_pps_buf = NULL;

	bcmdec->input_framerate = 0;
	bcmdec->input_par_x = 0;
	bcmdec->input_par_y = 0;
	bcmdec->prev_pic = -1;

	bcmdec->codec_params.inside_buffer = TRUE;
	bcmdec->codec_params.consumed_offset = 0;
	bcmdec->codec_params.strtcode_offset = 0;
	bcmdec->codec_params.nal_sz = 0;
	bcmdec->codec_params.pps_size = 0;
	bcmdec->codec_params.nal_size_bytes = 4;

	bcmdec->paused = FALSE;

	bcmdec->flushing = FALSE;
	bcmdec->ses_nbr = 0;
	bcmdec->insert_pps = TRUE;
	bcmdec->ses_change = FALSE;

	bcmdec->push_exit = FALSE;

	bcmdec->suspend_mode = FALSE;
	bcmdec->gst_clock = NULL;
	bcmdec->rpt_pic_cnt = 0;

	//bcmdec->enable_spes = FALSE;
	bcmdec->enable_spes = TRUE;
	bcmdec->dest_buf = NULL;
	bcmdec->catchup_on = FALSE;
	bcmdec->last_output_spes_time = 0;

	pthread_mutex_init(&bcmdec->gst_buf_que_lock, NULL);
	pthread_mutex_init(&bcmdec->gst_padbuf_que_lock, NULL);
	//pthread_mutex_init(&bcmdec->fn_lock,NULL);
}

static void bcmdec_put_que_mem_buf(GstBcmDec *bcmdec, GSTBUF_LIST *gst_queue_element)
{
	pthread_mutex_lock(&bcmdec->gst_buf_que_lock);

	gst_queue_element->next = bcmdec->gst_mem_buf_que_hd;
	bcmdec->gst_mem_buf_que_hd = gst_queue_element;

	bcmdec->gst_que_cnt++;
	GST_DEBUG_OBJECT(bcmdec, "mem pool inc is %u", bcmdec->gst_que_cnt);

	pthread_mutex_unlock(&bcmdec->gst_buf_que_lock);
}

static GSTBUF_LIST * bcmdec_get_que_mem_buf(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *gst_queue_element = NULL;

	pthread_mutex_lock(&bcmdec->gst_buf_que_lock);

	gst_queue_element = bcmdec->gst_mem_buf_que_hd;
	if (gst_queue_element) {
		bcmdec->gst_mem_buf_que_hd = bcmdec->gst_mem_buf_que_hd->next;
		bcmdec->gst_que_cnt--;

		GST_DEBUG_OBJECT(bcmdec, "mem pool dec is %u", bcmdec->gst_que_cnt);
	}

	pthread_mutex_unlock(&bcmdec->gst_buf_que_lock);

	return gst_queue_element;
}

static gboolean bcmdec_alloc_mem_buf_que_pool(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *gst_queue_element = NULL;
	guint i = 0;

	bcmdec->gst_mem_buf_que_hd = NULL;
	while (i++<bcmdec->gst_buf_que_sz) {
		if (!(gst_queue_element = (GSTBUF_LIST *)malloc(sizeof(GSTBUF_LIST)))) {
			GST_ERROR_OBJECT(bcmdec, "mempool malloc failed ");
			return FALSE;
		}
		memset(gst_queue_element, 0, sizeof(GSTBUF_LIST));
		bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
	}
	return TRUE;
}

static gboolean bcmdec_release_mem_buf_que_pool(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *gst_queue_element;
	guint i = 0;

	do {
		gst_queue_element = bcmdec_get_que_mem_buf(bcmdec);
		if (gst_queue_element) {
			free(gst_queue_element);
			i++;
		}
	} while (gst_queue_element);

	bcmdec->gst_mem_buf_que_hd = NULL;
	if (!bcmdec->silent)
		GST_DEBUG_OBJECT(bcmdec, "mem_buf_que_pool released...  %d", i);

	return TRUE;
}

static void bcmdec_ins_buf(GstBcmDec *bcmdec,GSTBUF_LIST	*gst_queue_element)
{
	pthread_mutex_lock(&bcmdec->gst_buf_que_lock);

	if (!bcmdec->gst_buf_que_hd) {
		bcmdec->gst_buf_que_hd = bcmdec->gst_buf_que_tl = gst_queue_element;
	} else {
		bcmdec->gst_buf_que_tl->next = gst_queue_element;
		bcmdec->gst_buf_que_tl = gst_queue_element;
		gst_queue_element->next = NULL;
	}

	if (sem_post(&bcmdec->buf_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "buf sem_post failed");
	else
		GST_DEBUG_OBJECT(bcmdec, "buffer inserted and buf_event signalled");

	pthread_mutex_unlock(&bcmdec->gst_buf_que_lock);
}

static GSTBUF_LIST * bcmdec_rem_buf(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *temp;

	pthread_mutex_lock(&bcmdec->gst_buf_que_lock);

	if (bcmdec->gst_buf_que_hd == bcmdec->gst_buf_que_tl) {
		temp = bcmdec->gst_buf_que_hd;
		bcmdec->gst_buf_que_hd = bcmdec->gst_buf_que_tl = NULL;
	} else {
		temp = bcmdec->gst_buf_que_hd;
		bcmdec->gst_buf_que_hd = temp->next;
	}

	pthread_mutex_unlock(&bcmdec->gst_buf_que_lock);

	return temp;
}

static BC_STATUS bcmdec_insert_sps_pps(GstBcmDec *bcmdec, GstBuffer* gstbuf)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	guint8 *data = GST_BUFFER_DATA(gstbuf);
	guint32 data_size = GST_BUFFER_SIZE(gstbuf);
	gint profile;
	guint nal_size;
	guint num_sps, num_pps, i;

	bcmdec->codec_params.pps_size = 0;

	profile = (data[1] << 16) | (data[2] << 8) | data[3];
	GST_DEBUG_OBJECT(bcmdec, "profile %06x",profile);

	bcmdec->codec_params.nal_size_bytes = (data[4] & 0x03) + 1;

	GST_DEBUG_OBJECT(bcmdec, "nal size %d",bcmdec->codec_params.nal_size_bytes);

	num_sps = data[5] & 0x1f;
	GST_DEBUG_OBJECT(bcmdec, "num sps %d",num_sps);

	data += 6;
	data_size -= 6;

	for (i = 0; i < num_sps; i++) {

		if (data_size < 2) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "too small 2");
			return BC_STS_ERROR;
		}

		nal_size = (data[0] << 8) | data[1];
		data += 2;
		data_size -= 2;

		if (data_size < nal_size) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "too small 3");
			return BC_STS_ERROR;
		}

		bcmdec->codec_params.sps_pps_buf[0] = 0;
		bcmdec->codec_params.sps_pps_buf[1] = 0;
		bcmdec->codec_params.sps_pps_buf[2] = 0;
		bcmdec->codec_params.sps_pps_buf[3] = 1;

		bcmdec->codec_params.pps_size += 4;

		memcpy(bcmdec->codec_params.sps_pps_buf + bcmdec->codec_params.pps_size, data, nal_size);
		bcmdec->codec_params.pps_size += nal_size;

		data += nal_size;
		data_size -= nal_size;
	}

	if (data_size < 1) {
		if (!bcmdec->silent)
			GST_DEBUG_OBJECT(bcmdec, "too small 4");
		return BC_STS_ERROR;
	}

	num_pps = data[0];
	data += 1;
	data_size -= 1;

	for (i = 0; i < num_pps; i++) {

		if (data_size < 2) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "too small 5");
			return BC_STS_ERROR;
		}

		nal_size = (data[0] << 8) | data[1];
		data += 2;
		data_size -= 2;

		if (data_size < nal_size) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "too small 6");
			return BC_STS_ERROR;
		}

		bcmdec->codec_params.sps_pps_buf[bcmdec->codec_params.pps_size+0] = 0;
		bcmdec->codec_params.sps_pps_buf[bcmdec->codec_params.pps_size+1] = 0;
		bcmdec->codec_params.sps_pps_buf[bcmdec->codec_params.pps_size+2] = 0;
		bcmdec->codec_params.sps_pps_buf[bcmdec->codec_params.pps_size+3] = 1;

		bcmdec->codec_params.pps_size += 4;

		memcpy(bcmdec->codec_params.sps_pps_buf + bcmdec->codec_params.pps_size, data, nal_size);
		bcmdec->codec_params.pps_size += nal_size;

		data += nal_size;
		data_size -= nal_size;
	}

	GST_DEBUG_OBJECT(bcmdec, "data size at end = %d ",data_size);

	return sts;
}

static BC_STATUS bcmdec_suspend_callback(GstBcmDec *bcmdec)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	bcmdec_flush_gstbuf_queue(bcmdec);

	bcmdec->base_time = 0;
	if (bcmdec->decif.hdev)
		sts = decif_close(&bcmdec->decif);
	bcmdec->codec_params.inside_buffer = TRUE;
	bcmdec->codec_params.consumed_offset = 0;
	bcmdec->codec_params.strtcode_offset = 0;
	bcmdec->codec_params.nal_sz = 0;
	bcmdec->insert_pps = TRUE;

	return sts;
}

static BC_STATUS bcmdec_resume_callback(GstBcmDec *bcmdec)
{
	BC_STATUS sts = BC_STS_SUCCESS;
	BC_INPUT_FORMAT bcInputFormat;

	sts = decif_open(&bcmdec->decif);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "dev open success");
	} else {
		GST_ERROR_OBJECT(bcmdec, "dev open failed %d", sts);
		return sts;
	}

	bcInputFormat.OptFlags = 0; // NAREN - FIXME - Should we enable BD mode and max frame rate mode for LINK?
	bcInputFormat.FGTEnable = FALSE;
	bcInputFormat.MetaDataEnable = FALSE;
	bcInputFormat.Progressive =  !(bcmdec->interlace);
	bcInputFormat.mSubtype= bcmdec->input_format;

	//Use Demux Image Size for VC-1 Simple/Main
	if(bcInputFormat.mSubtype == BC_MSUBTYPE_WMV3)
	{
		//VC-1 Simple/Main
		bcInputFormat.width = bcmdec->frame_width;
		bcInputFormat.height = bcmdec->frame_height;
	}
	else
	{
		bcInputFormat.width = bcmdec->output_params.width;
		bcInputFormat.height = bcmdec->output_params.height;
	}

	bcInputFormat.startCodeSz = bcmdec->codec_params.nal_size_bytes;
	bcInputFormat.pMetaData = bcmdec->codec_params.sps_pps_buf;
	bcInputFormat.metaDataSz = bcmdec->codec_params.pps_size;
	bcInputFormat.OptFlags = 0x80000000 | vdecFrameRate23_97;

	sts = decif_setinputformat(&bcmdec->decif, bcInputFormat);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "set input format success");
	} else {
		GST_ERROR_OBJECT(bcmdec, "set input format failed");
		bcmdec->streaming = FALSE;
		return sts;
	}

	sts = decif_prepare_play(&bcmdec->decif);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "prepare play success");
	} else {
		GST_ERROR_OBJECT(bcmdec, "prepare play failed %d", sts);
		bcmdec->streaming = FALSE;
		return sts;
	}

	decif_setcolorspace(&bcmdec->decif, BUF_MODE);

	sts = decif_start_play(&bcmdec->decif);
	if (sts == BC_STS_SUCCESS) {
		GST_DEBUG_OBJECT(bcmdec, "start play success");
		bcmdec->streaming = TRUE;
	} else {
		GST_ERROR_OBJECT(bcmdec, "start play failed %d", sts);
		bcmdec->streaming = FALSE;
		return sts;
	}

	if (sem_post(&bcmdec->play_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "sem_post failed");

	if (sem_post(&bcmdec->push_start_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "push_start post failed");

	return sts;
}

static gboolean bcmdec_mul_inst_cor(GstBcmDec *bcmdec)
{
	struct timespec ts;
	gint ret = 0;
	int i = 0;

	if ((intptr_t)g_inst_sts == -1) {
		GST_ERROR_OBJECT(bcmdec, "mul_inst_cor :shmem ptr invalid");
		return FALSE;
	}

	if (g_inst_sts->cur_decode == PLAYBACK) {
		GST_DEBUG_OBJECT(bcmdec, "mul_inst_cor : ret false %d", g_inst_sts->cur_decode);
		return FALSE;
	}

	for (i = 0; i < 15; i++) {
		ts.tv_sec = time(NULL) + 3;
		ts.tv_nsec = 0;
		ret = sem_timedwait(&g_inst_sts->inst_ctrl_event, &ts);
		if (ret < 0) {
			if (errno == ETIMEDOUT) {
				if (g_inst_sts->cur_decode == PLAYBACK) {
					GST_DEBUG_OBJECT(bcmdec, "mul_inst_cor :playback is set , exit");
					return FALSE;
				} else {
					GST_DEBUG_OBJECT(bcmdec, "mul_inst_cor :wait for thumb nail decode finish");
					continue;
				}
			} else if (errno == EINTR) {
				return FALSE;
			}
		} else {
			GST_DEBUG_OBJECT(bcmdec, "mul_inst_cor :ctrl_event is given");
			return TRUE;
		}
	}
	GST_DEBUG_OBJECT(bcmdec, "mul_inst_cor : ret false cur_dec = %d wait = %d", g_inst_sts->cur_decode, g_inst_sts->waiting);

	return FALSE;
}

static BC_STATUS bcmdec_create_shmem(GstBcmDec *bcmdec, int *shmem_id)
{
	int shmid = -1;
	key_t shmkey = BCM_GST_SHMEM_KEY;
	shmid_ds buf;

	if (shmem_id == NULL) {
		GST_ERROR_OBJECT(bcmdec, "Invalid argument ...");
		return BC_STS_INSUFF_RES;
	}

	*shmem_id = shmid;

	//First Try to create it.
	shmid = shmget(shmkey, 1024, 0644 | IPC_CREAT | IPC_EXCL);
	if (shmid == -1) {
		if (errno == EEXIST) {
			GST_DEBUG_OBJECT(bcmdec, "bcmdec_create_shmem:shmem already exists :%d", errno);
			shmid = shmget(shmkey, 1024, 0644);
			if (shmid == -1) {
				GST_ERROR_OBJECT(bcmdec, "bcmdec_create_shmem:unable to get shmid :%d", errno);
				return BC_STS_INSUFF_RES;
			}

			//we got the shmid, see if any process is alreday attached to it
			if (shmctl(shmid,IPC_STAT,&buf) == -1) {
				GST_ERROR_OBJECT(bcmdec, "bcmdec_create_shmem:shmctl failed ...");
				return BC_STS_ERROR;
			}

			if (buf.shm_nattch == 0) {
				sem_destroy(&g_inst_sts->inst_ctrl_event);
				//No process is currently attached to the shmem seg. go ahead and delete it as its contents are stale.
				if (shmctl(shmid, IPC_RMID, NULL) != -1)
						GST_DEBUG_OBJECT(bcmdec, "bcmdec_create_shmem:deleted shmem segment and creating a new one ...");
				//create a new shmem
				shmid = shmget(shmkey, 1024, 0644 | IPC_CREAT | IPC_EXCL);
				if (shmid == -1) {
					GST_ERROR_OBJECT(bcmdec, "bcmdec_create_shmem:unable to get shmid :%d", errno);
					return BC_STS_INSUFF_RES;
				}
				//attach to it
				bcmdec_get_shmem(bcmdec, shmid, TRUE);

			} else {
				//attach to it
				bcmdec_get_shmem(bcmdec, shmid, FALSE);
			}

		} else {
			GST_ERROR_OBJECT(bcmdec, "shmcreate failed with err %d",errno);
			return BC_STS_ERROR;
		}
	} else {
		//we created just attach to it
		bcmdec_get_shmem(bcmdec, shmid, TRUE);
	}

	*shmem_id = shmid;

	return BC_STS_SUCCESS;
}

static BC_STATUS bcmdec_get_shmem(GstBcmDec *bcmdec, int shmid, gboolean newmem)
{
	gint ret = 0;
	g_inst_sts = (GLB_INST_STS *)shmat(shmid, (void *)0, 0);
	if ((intptr_t)g_inst_sts == -1) {
		GST_ERROR_OBJECT(bcmdec, "Unable to open shared memory ...errno = %d", errno);
		return BC_STS_ERROR;
	}

	if (newmem) {
		ret = sem_init(&g_inst_sts->inst_ctrl_event, 5, 1);
		if (ret != 0) {
			GST_ERROR_OBJECT(bcmdec, "inst_ctrl_event failed");
			return BC_STS_ERROR;
		}
	}

	return BC_STS_SUCCESS;
}

static BC_STATUS bcmdec_del_shmem(GstBcmDec *bcmdec)
{
	int shmid = 0;
	shmid_ds buf;

	//First dettach the shared mem segment
	if (shmdt(g_inst_sts) == -1)
		GST_ERROR_OBJECT(bcmdec, "Unable to detach shared memory ...");

	//delete the shared mem segment if there are no other attachments
	shmid = shmget((key_t)BCM_GST_SHMEM_KEY, 0, 0);
	if (shmid == -1) {
			GST_ERROR_OBJECT(bcmdec, "bcmdec_del_shmem:Unable get shmid ...");
			return BC_STS_ERROR;
	}

	if (shmctl(shmid, IPC_STAT, &buf) == -1) {
		GST_ERROR_OBJECT(bcmdec, "bcmdec_del_shmem:shmctl failed ...");
		return BC_STS_ERROR;
	}

	if (buf.shm_nattch == 0) {
		sem_destroy(&g_inst_sts->inst_ctrl_event);
		//No process is currently attached to the shmem seg. go ahead and delete it
		if (shmctl(shmid, IPC_RMID, NULL) != -1) {
				GST_ERROR_OBJECT(bcmdec, "bcmdec_del_shmem:deleted shmem segment ...");
				return BC_STS_ERROR;
		} else {
			GST_ERROR_OBJECT(bcmdec, "bcmdec_del_shmem:unable to delete shmem segment ...");
		}
	}

	return BC_STS_SUCCESS;
}

// For renderer buffer
static void bcmdec_flush_gstrbuf_queue(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *gst_queue_element = NULL;

	while (1) {
		gst_queue_element = bcmdec_rem_padbuf(bcmdec);
		if (gst_queue_element) {
			if (gst_queue_element->gstbuf) {
				gst_buffer_unref (gst_queue_element->gstbuf);
				bcmdec_put_que_mem_buf(bcmdec, gst_queue_element);
			} else {
				break;
			}
		}
		else {
			GST_DEBUG_OBJECT(bcmdec, "no gst_queue_element");
			break;
		}
	}
}

static void * bcmdec_process_get_rbuf(void *ctx)
{
	GstBcmDec *bcmdec = (GstBcmDec *)ctx;
	GstFlowReturn ret = GST_FLOW_ERROR;
	GSTBUF_LIST *gst_queue_element = NULL;
	gboolean result = FALSE, done = FALSE;
	GstBuffer *gstbuf = NULL;
	guint bufSz = 0;
	gboolean get_buf_start = FALSE;
	int revent = -1;

	while (1) {
		revent = sem_trywait(&bcmdec->rbuf_start_event);
		if (revent == 0) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "got start get buf event ");
			get_buf_start = TRUE;
			bcmdec->rbuf_thread_running = TRUE;
		}

		revent = sem_trywait(&bcmdec->rbuf_stop_event);
		if (revent == 0) {
			if (!bcmdec->silent)
				GST_DEBUG_OBJECT(bcmdec, "quit event set, exit");
			break;
		}

		if (!bcmdec->streaming || !get_buf_start) {
			GST_DEBUG_OBJECT(bcmdec, "SLEEPING in get bufs");
			usleep(100 * 1000);
		}

		while (bcmdec->streaming && get_buf_start)
		{
			//GST_DEBUG_OBJECT(bcmdec, "process get rbuf start....");
			gstbuf = NULL;

			if (!bcmdec->recv_thread && !bcmdec->streaming) {
				if (!bcmdec->silent)
					GST_DEBUG_OBJECT(bcmdec, "process get rbuf prepare exiting..");
				done = TRUE;
				break;
			}

			// If we have enough buffers from the renderer then don't get any more
			if(bcmdec->gst_padbuf_que_cnt >= GST_RENDERER_BUF_POOL_SZ) {
				usleep(100 * 1000);
				GST_DEBUG_OBJECT(bcmdec, "SLEEPING because we have enough buffers");
				continue;
			}

			if (gst_queue_element == NULL)
				gst_queue_element = bcmdec_get_que_mem_buf(bcmdec);

			if (!gst_queue_element) {
				if (!bcmdec->silent)
					GST_DEBUG_OBJECT(bcmdec, "mbuf full == TRUE %u", bcmdec->gst_buf_que_sz);

				usleep(1000 * 1000); // Sleep for a second since we have 350 buffers queued up
				continue;
			}

			bufSz = bcmdec->output_params.width * bcmdec->output_params.height * BUF_MULT;

			//GST_DEBUG_OBJECT(bcmdec, "process get rbuf gst_pad_alloc_buffer_and_set_caps ....");
			ret = gst_pad_alloc_buffer_and_set_caps(bcmdec->srcpad, GST_BUFFER_OFFSET_NONE,
								bufSz, GST_PAD_CAPS(bcmdec->srcpad), &gstbuf);
			if (ret != GST_FLOW_OK) {
				if (!bcmdec->silent)
					GST_ERROR_OBJECT(bcmdec, "gst_pad_alloc_buffer_and_set_caps failed %d ",ret);
				usleep(30 * 1000);
				continue;
			}

			GST_DEBUG_OBJECT(bcmdec, "Got GST Buf RCnt:%d", bcmdec->gst_padbuf_que_cnt);

			gst_queue_element->gstbuf = gstbuf;
			bcmdec_ins_padbuf(bcmdec, gst_queue_element);
			gst_queue_element = NULL;

			usleep(5 * 1000);
		}

		if (done) {
			GST_DEBUG_OBJECT(bcmdec, "process get rbuf done ");
			break;
		}
	}
	bcmdec_flush_gstrbuf_queue(bcmdec);
	GST_DEBUG_OBJECT(bcmdec, "process get rbuf exiting.. ");
	pthread_exit((void *)&result);
}

static gboolean bcmdec_start_get_rbuf_thread(GstBcmDec *bcmdec)
{
	gboolean result = TRUE;
	gint ret = 0;
	pthread_attr_t thread_attr;

// 	if (!bcmdec_alloc_mem_rbuf_que_pool(bcmdec))
// 		GST_ERROR_OBJECT(bcmdec, "rend pool alloc failed/n");

	bcmdec->gst_padbuf_que_hd = bcmdec->gst_padbuf_que_tl = NULL;

	ret = sem_init(&bcmdec->rbuf_ins_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "get rbuf ins event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->rbuf_start_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "get rbuf start event init failed");
		result = FALSE;
	}

	ret = sem_init(&bcmdec->rbuf_stop_event, 0, 0);
	if (ret != 0) {
		GST_ERROR_OBJECT(bcmdec, "get rbuf stop event init failed");
		result = FALSE;
	}

	pthread_attr_init(&thread_attr);
	pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	pthread_create(&bcmdec->get_rbuf_thread, &thread_attr,
		       bcmdec_process_get_rbuf, bcmdec);
	pthread_attr_destroy(&thread_attr);

	if (!bcmdec->get_rbuf_thread) {
		GST_ERROR_OBJECT(bcmdec, "Failed to create Renderer buffer Thread");
		result = FALSE;
	} else {
		GST_DEBUG_OBJECT(bcmdec, "Success to create Renderer buffer Thread");
	}

	return result;
}

// static void bcmdec_put_que_mem_padbuf(GstBcmDec *bcmdec, GSTBUF_LIST *gst_queue_element)
// {
// 	pthread_mutex_lock(&bcmdec->gst_padbuf_que_lock);
//
// 	gst_queue_element->next = bcmdec->gst_mem_padbuf_que_hd;
// 	bcmdec->gst_mem_padbuf_que_hd = gst_queue_element;
//
// 	pthread_mutex_unlock(&bcmdec->gst_padbuf_que_lock);
// }
//
// static GSTBUF_LIST * bcmdec_get_que_mem_padbuf(GstBcmDec *bcmdec)
// {
// 	GSTBUF_LIST *gst_queue_element;
//
// 	pthread_mutex_lock(&bcmdec->gst_padbuf_que_lock);
//
// 	gst_queue_element = bcmdec->gst_mem_padbuf_que_hd;
// 	if (gst_queue_element !=NULL)
// 		bcmdec->gst_mem_padbuf_que_hd = bcmdec->gst_mem_padbuf_que_hd->next;
//
// 	pthread_mutex_unlock(&bcmdec->gst_padbuf_que_lock);
//
// 	return gst_queue_element;
// }
//
// static gboolean bcmdec_alloc_mem_rbuf_que_pool(GstBcmDec *bcmdec)
// {
// 	GSTBUF_LIST *gst_queue_element;
// 	guint i;
//
// 	bcmdec->gst_mem_padbuf_que_hd = NULL;
// 	for (i = 1; i < bcmdec->gst_padbuf_que_sz; i++) {
// 		gst_queue_element = (GSTBUF_LIST *)malloc(sizeof(GSTBUF_LIST));
// 		if (!gst_queue_element) {
// 			GST_ERROR_OBJECT(bcmdec, "mem_rbuf_que_pool malloc failed ");
// 			return FALSE;
// 		}
// 		memset(gst_queue_element, 0, sizeof(GSTBUF_LIST));
// 		bcmdec_put_que_mem_padbuf(bcmdec, gst_queue_element);
// 	}
//
// 	return TRUE;
// }
//
// static gboolean bcmdec_release_mem_rbuf_que_pool(GstBcmDec *bcmdec)
// {
// 	GSTBUF_LIST *gst_queue_element;
// 	guint i = 0;
//
// 	do {
// 		gst_queue_element = bcmdec_get_que_mem_padbuf(bcmdec);
// 		if (gst_queue_element) {
// 			free(gst_queue_element);
// 			i++;
// 		}
// 	} while (gst_queue_element);
//
// 	bcmdec->gst_mem_padbuf_que_hd = NULL;
// 	if (!bcmdec->silent)
// 		GST_DEBUG_OBJECT(bcmdec, "rend_rbuf_que_pool released... %d", i);
//
// 	return TRUE;
// }

static void bcmdec_ins_padbuf(GstBcmDec *bcmdec, GSTBUF_LIST *gst_queue_element)
{
	pthread_mutex_lock(&bcmdec->gst_padbuf_que_lock);

	if (!bcmdec->gst_padbuf_que_hd) {
		bcmdec->gst_padbuf_que_hd = bcmdec->gst_padbuf_que_tl = gst_queue_element;
	} else {
		bcmdec->gst_padbuf_que_tl->next = gst_queue_element;
		bcmdec->gst_padbuf_que_tl = gst_queue_element;
		gst_queue_element->next = NULL;
	}

	bcmdec->gst_padbuf_que_cnt++;
	GST_DEBUG_OBJECT(bcmdec, "Inc rbuf:%d", bcmdec->gst_padbuf_que_cnt);

	if (sem_post(&bcmdec->rbuf_ins_event) == -1)
		GST_ERROR_OBJECT(bcmdec, "rbuf sem_post failed");

	pthread_mutex_unlock(&bcmdec->gst_padbuf_que_lock);
}

static GSTBUF_LIST *bcmdec_rem_padbuf(GstBcmDec *bcmdec)
{
	GSTBUF_LIST *temp;

	pthread_mutex_lock(&bcmdec->gst_padbuf_que_lock);

	if (bcmdec->gst_padbuf_que_hd == bcmdec->gst_padbuf_que_tl) {
		temp = bcmdec->gst_padbuf_que_hd;
		bcmdec->gst_padbuf_que_hd = bcmdec->gst_padbuf_que_tl = NULL;
	} else {
		temp = bcmdec->gst_padbuf_que_hd;
		bcmdec->gst_padbuf_que_hd = temp->next;
	}

	if (temp)
		bcmdec->gst_padbuf_que_cnt--;

	GST_DEBUG_OBJECT(bcmdec, "Dec rbuf:%d", bcmdec->gst_padbuf_que_cnt);

	pthread_mutex_unlock(&bcmdec->gst_padbuf_que_lock);

	return temp;
}

// End of renderer buffer

/*
 * entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean plugin_init(GstPlugin *bcmdec)
{
	//printf("BcmDec_init");

	/*
	 * debug category for fltering log messages
	 *
	 * exchange the string 'Template bcmdec' with your description
	 */
	GST_DEBUG_CATEGORY_INIT(gst_bcmdec_debug, "bcmdec", 0, "Broadcom video decoder");

	return gst_element_register(bcmdec, "bcmdec", GST_BCMDEC_RANK, GST_TYPE_BCMDEC);
}

/* gstreamer looks for this structure to register bcmdec */
GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR,
		  "bcmdec", "Video decoder", plugin_init, VERSION,
		  "LGPL", "bcmdec", "http://broadcom.com/")

