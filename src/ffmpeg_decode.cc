/*
	Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>
	Copyright (C) 2021 DEV47APPS, github.com/dev47apps

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "plugin.h"
#include "ffmpeg_decode.h"
#include <libavutil/channel_layout.h>

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
#error LIBAVCODEC VERSION 58,9,100 REQUIRED
#endif

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
#define DECODER_CHANNELS		decoder->channels
#else
#define DECODER_CHANNELS		decoder->ch_layout.nb_channels
#endif

// https://trac.ffmpeg.org/wiki/HWAccelIntro
// https://ffmpeg.org/doxygen/3.4/hw__decode_8c_source.html

enum AVHWDeviceType hw_device_list[] = {
	#ifdef _WIN32
	AV_HWDEVICE_TYPE_D3D11VA,
	AV_HWDEVICE_TYPE_CUDA,
	#endif

	#ifdef __APPLE__
	AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
	#endif

	#ifdef __linux__
	AV_HWDEVICE_TYPE_CUDA,
	AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_VDPAU,
	#endif

	AV_HWDEVICE_TYPE_NONE,
};

static AVPixelFormat has_hw_type(const AVCodec *c, enum AVHWDeviceType type)
{
	for (int i = 0;; i++) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(c, i);
		if (!config) {
			break;
		}

		if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) && config->device_type == type) {
			dlog("hw_pix_fmt=%d", config->pix_fmt);
			return config->pix_fmt;
		}
	}

	return AV_PIX_FMT_NONE;
}

static void init_hw_decoder(FFMpegDecoder *d)
{
	enum AVHWDeviceType *hw_device_iter = hw_device_list;
	AVBufferRef *hw_ctx = NULL;

	while (*hw_device_iter != AV_HWDEVICE_TYPE_NONE) {
		dlog("trying hw device %d", *hw_device_iter);
		d->hw_pix_fmt = has_hw_type(d->codec, *hw_device_iter);
		if (d->hw_pix_fmt != AV_PIX_FMT_NONE) {
			if (av_hwdevice_ctx_create(&hw_ctx, *hw_device_iter, NULL, NULL, 0) == 0)
				break;

			d->hw_pix_fmt = AV_PIX_FMT_NONE;
		}

		hw_device_iter++;
	}

	if (hw_ctx) {
		d->decoder->hw_device_ctx = av_buffer_ref(hw_ctx);
		d->hw_ctx = hw_ctx;
		d->hw = true;
	}
	ilog("use hw: %d", d->hw);
}

int FFMpegDecoder::init(uint8_t* header, enum AVCodecID id, bool use_hw)
{
	int ret;

	codec = avcodec_find_decoder(id);
	if (!codec)
		return -1;

	decoder = avcodec_alloc_context3(codec);
	decoder->opaque = this;

	if (id == AV_CODEC_ID_AAC) {
		// https://wiki.multimedia.cx/index.php/MPEG-4_Audio
		static int aac_frequencies[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000};
		if (!header) {
			elog("missing AAC header required to init decoder");
			return -1;
		}

		int sr_idx = ((header[0] << 1) | (header[1] >> 7)) & 0x1F;
		if (sr_idx < 0 || sr_idx >= (int)ARRAY_LEN(aac_frequencies)) {
			elog("failed to parse AAC header, sr_idx=%d [0x%2x 0x%2x]", sr_idx, header[0], header[1]);
			return -1;
		}

		decoder->sample_rate = aac_frequencies[sr_idx];
		decoder->profile = AV_PROFILE_AAC_LOW;

		const int channels = (header[1] >> 3) & 0xF;
		#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100)
		decoder->channels = channels;
		switch (decoder->channels) {
			case 1:
				decoder->channel_layout = AV_CH_LAYOUT_MONO;
				break;
			case 2:
				decoder->channel_layout = AV_CH_LAYOUT_STEREO;
				break;
			default:
				decoder->channel_layout = 0; // unknown
		}
		#else
		av_channel_layout_default(&decoder->ch_layout, channels);
		#endif

		ilog("audio: sample_rate=%d channels=%d", decoder->sample_rate, DECODER_CHANNELS);
	}

	if (use_hw) {
		init_hw_decoder(this);
	}

	ret = avcodec_open2(decoder, codec, NULL);
	if (ret < 0) {
		return ret;
	}

	// if (codec->capabilities & CODEC_CAP_TRUNC)
	// 	decoder->flags |= CODEC_FLAG_TRUNC;
	decoder->flags |= AV_CODEC_FLAG_LOW_DELAY;
	decoder->flags2 |= AV_CODEC_FLAG2_FAST;
	// decoder->flags2 |= AV_CODEC_FLAG2_CHUNKS;
	decoder->thread_type = FF_THREAD_SLICE;

	frame = av_frame_alloc();
	if (!frame)
		return -1;

	if (hw) {
		frame_hw = av_frame_alloc();
		if (!frame_hw) hw = false;
	}

	packet = av_packet_alloc();
	if (!packet)
		return -1;

	ready = true;
	return 0;
}

FFMpegDecoder::~FFMpegDecoder(void)
{
	if (frame_hw)
		av_frame_free(&frame_hw);

	if (frame)
		av_frame_free(&frame);

	if (hw_ctx)
		av_buffer_unref(&hw_ctx);

	if (packet)
		av_packet_free(&packet);

	if (decoder)
		avcodec_free_context(&decoder);
}

// TODO:
// add AV_PIX_FMT_YUVJ420P to obs-ffmpeg-formats.h
// add convert_color_space to obs-ffmpeg-formats.h
// remove these duplicates (also in win-dshow)
// Clean obs-ffmpeg-compat.h duplicate (plugins/obs-ffmpeg & libobs)

static inline enum video_format convert_pixel_format(int f)
{
	switch (f) {
	case AV_PIX_FMT_YUV420P:
	case AV_PIX_FMT_YUVJ420P:
		return VIDEO_FORMAT_I420;
	case AV_PIX_FMT_NV12:
		return VIDEO_FORMAT_NV12;
	case AV_PIX_FMT_YUYV422:
		return VIDEO_FORMAT_YUY2;
	case AV_PIX_FMT_UYVY422:
		return VIDEO_FORMAT_UYVY;
	case AV_PIX_FMT_YUV422P:
	case AV_PIX_FMT_YUVJ422P:
		return VIDEO_FORMAT_I422;
	case AV_PIX_FMT_RGBA:
		return VIDEO_FORMAT_RGBA;
	case AV_PIX_FMT_BGRA:
		return VIDEO_FORMAT_BGRA;
	case AV_PIX_FMT_BGR0:
		return VIDEO_FORMAT_BGRX;
	}

	return VIDEO_FORMAT_NONE;
}

static enum video_colorspace
convert_color_space(enum AVColorSpace s, enum AVColorTransferCharacteristic trc,
		    enum AVColorPrimaries color_primaries)
{
	switch (s) {
	case AVCOL_SPC_BT709:
		return (trc == AVCOL_TRC_IEC61966_2_1) ? VIDEO_CS_SRGB : VIDEO_CS_709;
	case AVCOL_SPC_FCC:
	case AVCOL_SPC_BT470BG:
	case AVCOL_SPC_SMPTE170M:
	case AVCOL_SPC_SMPTE240M:
		return VIDEO_CS_601;
#if LIBOBS_API_MAJOR_VER < 28
	default:
		return VIDEO_CS_DEFAULT;

#else
	case AVCOL_SPC_BT2020_NCL:
		return (trc == AVCOL_TRC_ARIB_STD_B67) ? VIDEO_CS_2100_HLG
						       : VIDEO_CS_2100_PQ;
	default:
		return (color_primaries == AVCOL_PRI_BT2020)
			       ? ((trc == AVCOL_TRC_ARIB_STD_B67)
					  ? VIDEO_CS_2100_HLG
					  : VIDEO_CS_2100_PQ)
			       : VIDEO_CS_DEFAULT;
#endif
	}
}

static inline enum audio_format convert_sample_format(int f)
{
	switch (f) {
	case AV_SAMPLE_FMT_S16:
		return AUDIO_FORMAT_16BIT;
	case AV_SAMPLE_FMT_S32:
		return AUDIO_FORMAT_32BIT;
	case AV_SAMPLE_FMT_U8:
		return AUDIO_FORMAT_U8BIT;
	case AV_SAMPLE_FMT_FLT:
		return AUDIO_FORMAT_FLOAT;
	case AV_SAMPLE_FMT_U8P:
		return AUDIO_FORMAT_U8BIT_PLANAR;
	case AV_SAMPLE_FMT_S16P:
		return AUDIO_FORMAT_16BIT_PLANAR;
	case AV_SAMPLE_FMT_S32P:
		return AUDIO_FORMAT_32BIT_PLANAR;
	case AV_SAMPLE_FMT_FLTP:
		return AUDIO_FORMAT_FLOAT_PLANAR;
	default:;
	}

	return AUDIO_FORMAT_UNKNOWN;
}

static inline enum speaker_layout convert_speaker_layout(int channels)
{
	switch (channels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	default:
		return SPEAKERS_UNKNOWN;
	}
}

DataPacket* FFMpegDecoder::pull_empty_packet(size_t size)
{
	size_t new_size = size + AV_INPUT_BUFFER_PADDING_SIZE;
	DataPacket* packet = Decoder::pull_empty_packet(new_size);
	memset(packet->data, 0, new_size);
	return packet;
}

void FFMpegDecoder::push_ready_packet(DataPacket* packet)
{
	if (catchup) {
		if (decodeQueue.items.size() > 0){
			recieveQueue.add_item(packet);
			return;
		}

		// Discard P/B frames and continue on anything higher
		if (codec->id == AV_CODEC_ID_H264) {
			int nalType = packet->data[2] == 1 ? (packet->data[3] & 0x1f) : (packet->data[4] & 0x1f);
			if (nalType < 5) {
				dlog("discard non-keyframe");
				recieveQueue.add_item(packet);
				return;
			}
		}

		ilog("decoder catchup: decodeQueue: %ld recieveQueue: %ld", decodeQueue.items.size(), recieveQueue.items.size());
		catchup = false;
	}

	decodeQueue.add_item(packet);

	if (codec->id == AV_CODEC_ID_H264 && decodeQueue.items.size() > 25) {
		catchup = true;
	}
	// ((uint64_t)plugin->obs_audio_frame.frames * MILLI_SEC / (uint64_t)plugin->obs_audio_frame.samples_per_sec)
	// At 44100HZ, 1 AAC Frame = 23ms
	else if (codec->id == AV_CODEC_ID_AAC && decodeQueue.items.size() > (1000/23)) {
		catchup = true;
	}
}

bool FFMpegDecoder::decode_video(struct obs_source_frame2* obs_frame, DataPacket* data_packet,
		bool *got_output)
{
	int ret;
	AVFrame *out_frame;
	*got_output = false;

	packet->data = data_packet->data;
	packet->size = data_packet->used;
	packet->pts = (data_packet->pts == NO_PTS) ? AV_NOPTS_VALUE : data_packet->pts;

	if (decoder->has_b_frames && !b_frame_check) {
		elog("WARNING Stream has b-frames!");
		b_frame_check = true;
	}

	ret = avcodec_send_packet(decoder, packet);
	if (ret == 0) {
		out_frame = hw ? frame_hw : frame;
		ret = avcodec_receive_frame(decoder, out_frame);
		if (ret == 0) goto GOT_FRAME;
	}

	return ret == AVERROR(EAGAIN);

GOT_FRAME:
	if (hw) {
		if (frame_hw->format == hw_pix_fmt) {
			if (av_hwframe_transfer_data(frame, frame_hw, 0) != 0)
				return false;
			out_frame = frame;
		}
	}

	for (int i = 0; i < MAX_AV_PLANES; i++) {
		obs_frame->data[i]     = out_frame->data[i];
		obs_frame->linesize[i] = out_frame->linesize[i];
	}

	if (obs_frame->format == VIDEO_FORMAT_NONE) {
		obs_frame->format = convert_pixel_format(out_frame->format);
		if (obs_frame->format == VIDEO_FORMAT_NONE)
			return false;

		#if LIBOBS_API_MAJOR_VER >= 28
		switch (out_frame->color_trc) {
		case AVCOL_TRC_BT709:
		case AVCOL_TRC_GAMMA22:
		case AVCOL_TRC_GAMMA28:
		case AVCOL_TRC_SMPTE170M:
		case AVCOL_TRC_SMPTE240M:
		case AVCOL_TRC_IEC61966_2_1:
			obs_frame->trc = VIDEO_TRC_SRGB;
			break;
		case AVCOL_TRC_SMPTE2084:
			obs_frame->trc = VIDEO_TRC_PQ;
			break;
		case AVCOL_TRC_ARIB_STD_B67:
			obs_frame->trc = VIDEO_TRC_HLG;
			break;
		default:
			obs_frame->trc = VIDEO_TRC_DEFAULT;
		}
		#endif
	}

	enum video_range_type range =
		(out_frame->color_range == AVCOL_RANGE_JPEG)
		? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;

	if (range != obs_frame->range) {
		const enum video_colorspace cs = convert_color_space(
			out_frame->colorspace, out_frame->color_trc,
			out_frame->color_primaries);

		#if LIBOBS_API_MAJOR_VER < 28
		video_format_get_parameters(
			cs, range, obs_frame->color_matrix,
		#else
		video_format_get_parameters_for_format(
			cs, range, obs_frame->format, obs_frame->color_matrix,
		#endif
			obs_frame->color_range_min, obs_frame->color_range_max);

		obs_frame->range = range;
	}

	obs_frame->width = out_frame->width;
	obs_frame->height = out_frame->height;
	obs_frame->flip = false;

	*got_output = true;
	return true;
}

bool FFMpegDecoder::decode_audio(struct obs_source_audio* obs_frame, DataPacket* data_packet, bool *got_output)
{
	int ret;
	*got_output = false;

	packet->data = data_packet->data;
	packet->size = data_packet->used;
	packet->pts = (data_packet->pts == NO_PTS) ? AV_NOPTS_VALUE : data_packet->pts;

	ret = avcodec_send_packet(decoder, packet);
	if (ret == 0) {
		ret = avcodec_receive_frame(decoder, frame);
		if (ret == 0) goto GOT_FRAME;
	}

	return ret == AVERROR(EAGAIN);

GOT_FRAME:
	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		obs_frame->data[i] = frame->data[i];

	obs_frame->samples_per_sec = frame->sample_rate;
	obs_frame->frames = frame->nb_samples;

	if (obs_frame->format == AUDIO_FORMAT_UNKNOWN) {
		obs_frame->format = convert_sample_format(frame->format);
		obs_frame->speakers = convert_speaker_layout(DECODER_CHANNELS);
	}

	*got_output = true;
	return true;
}
