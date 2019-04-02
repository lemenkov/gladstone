/* GladSToNe g729 Decoder
 * Copyright (C) <2009> Gibrovacco <gibrovacco@gmail.com>
 * Copyright (C) 2016,2019 Sebastian Dröge <sebastian@centricular.com>
 *
 * It is possible to redistribute this code using the LGPL license:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Alternatively, you can redistribute at your choice using the MIT license,
 * reported below:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


/**
 * SECTION:element-g729dec
 * @see_also: g729enc
 *
 * This element decodes a G729 stream to raw integer audio.
 *
 * <refsect2>
 * <title>Example pipelines</title>
 * TODO
 * documentation of g729dec.
 * </refsect2>
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstg729dec.h"
#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (g729dec_debug);
#define GST_CAT_DEFAULT g729dec_debug

static GstStaticPadTemplate g729_dec_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (
        "audio/x-raw,"
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "rate = (int) 8000,"
        "channels = (int) 1, "
        "layout = (string) interleaved")
    );

static GstStaticPadTemplate g729_dec_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/G729, "
        "rate = (int) 8000, "
        "channels = (int) 1")
    );

G_DEFINE_TYPE (GstG729Dec, gst_g729_dec, GST_TYPE_AUDIO_DECODER);

static gboolean gst_g729_dec_set_format (GstAudioDecoder *adec, GstCaps *caps);
static gboolean gst_g729_dec_stop (GstAudioDecoder *adec);
static GstFlowReturn gst_g729_dec_handle_frame (GstAudioDecoder *adec, GstBuffer *buf);

static void
gst_g729_dec_class_init (GstG729DecClass * klass)
{
  GstElementClass *gstelement_class;
  GstAudioDecoderClass *gstaudiodecoder_class;

  gstelement_class = (GstElementClass *) klass;
  gstaudiodecoder_class = (GstAudioDecoderClass *) klass;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&g729_dec_src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&g729_dec_sink_factory));
  gst_element_class_set_static_metadata (gstelement_class, "G729 audio decoder",
    "Codec/Decoder/Audio",
    "decode g729 streams to audio",
    "Gibro Vacco <gibrovacco@gmail.com>");

  gstaudiodecoder_class->stop = GST_DEBUG_FUNCPTR (gst_g729_dec_stop);
  gstaudiodecoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_g729_dec_handle_frame);
  gstaudiodecoder_class->set_format = GST_DEBUG_FUNCPTR (gst_g729_dec_set_format);
}

static void
gst_g729_dec_init (GstG729Dec * dec)
{
  gst_audio_decoder_set_drainable (GST_AUDIO_DECODER (dec), FALSE);
  dec->vad = 0;
  dec->dec = initBcg729DecoderChannel();
}

static gboolean
gst_g729_dec_stop (GstAudioDecoder *adec)
{
  GstG729Dec * dec = GST_G729_DEC (adec);

  dec->vad = 0;
  closeBcg729DecoderChannel(dec->dec);

  return TRUE;
}

static gboolean
gst_g729_dec_set_format (GstAudioDecoder *adec, GstCaps *caps)
{
  GstCaps *outcaps;
  GstAudioInfo info;
  gboolean ret;

  outcaps = gst_pad_get_pad_template_caps (GST_AUDIO_DECODER_SRC_PAD (adec));
  gst_audio_info_from_caps (&info, outcaps);
  ret = gst_audio_decoder_set_output_format (adec, &info);
  gst_caps_unref (outcaps);

  return ret;
}

static guint32
g729_decode(GstG729Dec * dec, const guint8 *g729_data, guint g729_len, gint16 *pcm_data)
{
  switch(g729_len){
    case G729_SID_BYTES:
      GST_DEBUG_OBJECT(dec, "SID frame");
      bcg729Decoder(dec->dec, g729_data, g729_len, 0 /* no erasure */, 1 /* SID */, 0 /* not RFC3389 */, (int16_t*)pcm_data);
      break;
    case G729_SILENCE_BYTES:
      GST_DEBUG_OBJECT(dec, "silence frame");
      bcg729Decoder(dec->dec, g729_data, g729_len, 1 /* erasure */, 0 /* no SID */, 1 /* RFC3389 */, (int16_t*)pcm_data);
      break;
    case G729_FRAME_BYTES:
      bcg729Decoder(dec->dec, g729_data, g729_len, 0 /* no erasure */, 0 /* no SID */, 0 /* not RFC3389 */, (int16_t*)pcm_data);
      break;
  }

  return RAW_FRAME_BYTES;
}

static GstFlowReturn
gst_g729_dec_handle_frame (GstAudioDecoder * adec, GstBuffer * buf)
{
  GstG729Dec * dec = GST_G729_DEC (adec);
  guint size;
  GstMapInfo imap, omap;
  GstBuffer *outbuf;
  guint i, num_frames;
  const guint8 *in_ptr;
  gint16 *out_ptr;

  size = gst_buffer_get_size (buf);

  /* G729 frames are either 10 or 2 bytes, and we allow multiple 10 bytes
   * frames at once followed by up to a single 2 byte frame.
   * Also allow 0 byte silence frames. */
  if (size % G729_FRAME_BYTES != 0 && size % G729_FRAME_BYTES != G729_SID_BYTES)
  {
    GST_ERROR_OBJECT (dec, "wrong buffer size: %d", size);
    return GST_FLOW_ERROR;
  }

  num_frames = size / G729_FRAME_BYTES;
  if (size % G729_FRAME_BYTES == G729_SID_BYTES)
    num_frames += 1;
  if (size == 0)
    num_frames = 1;

  outbuf = gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (dec), num_frames * RAW_FRAME_BYTES);
  if (!outbuf)
    return GST_FLOW_OK;

  gst_buffer_map (buf, &imap, GST_MAP_READ);
  gst_buffer_map (outbuf, &omap, GST_MAP_READ);

  in_ptr = imap.data;
  out_ptr = (gint16 *) omap.data;

  for (i = 0; i < num_frames; i++) {
    /* Consider every frame except for the last one as a normal frame. The
     * last frame can be either of the three frame types */
    g729_decode (dec, in_ptr, size >= G729_FRAME_BYTES ? G729_FRAME_BYTES : size, out_ptr);

    in_ptr += G729_FRAME_BYTES;
    size -= G729_FRAME_BYTES;
    out_ptr += RAW_FRAME_BYTES / 2;
  }

  gst_buffer_unmap (buf, &imap);
  gst_buffer_unmap (outbuf, &omap);

  return gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (dec), outbuf, 1);
}

