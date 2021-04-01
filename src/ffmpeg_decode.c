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
#include <obs-ffmpeg-compat.h>
#include <obs-avc.h>

#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
#error LIBAVCODEC VERSION 58,9,100 REQUIRED
#endif

// https://trac.ffmpeg.org/wiki/HWAccelIntro
// https://ffmpeg.org/doxygen/3.4/hw__decode_8c_source.html

enum AVHWDeviceType hw_device_list[] = {
	#ifdef _WIN32
	AV_HWDEVICE_TYPE_D3D11VA,
	#endif

	#ifdef __APPLE__
	#endif

	#ifdef __linux__
	AV_HWDEVICE_TYPE_VAAPI, AV_HWDEVICE_TYPE_VDPAU,
	#endif

	AV_HWDEVICE_TYPE_NONE,
};

static AVPixelFormat has_hw_type(AVCodec *c, enum AVHWDeviceType type)
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
		ilog("trying hw device %d", *hw_device_iter);
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
		decoder->profile = FF_PROFILE_AAC_LOW;
		decoder->channel_layout = AV_CH_LAYOUT_MONO;
		// also hard coded in decode_audio_frame
		decoder->channels = 1;
		ilog("audio sample_rate=%d", decoder->sample_rate);
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
	decoder->flags2 |= AV_CODEC_FLAG2_CHUNKS;
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

	av_init_packet(packet);

	ready = true;
	return 0;
}

FFMpegDecoder::~FFMpegDecoder(void)
{
	if (frame_hw) {
		av_frame_unref(frame_hw);
		av_free(frame_hw);
	}

	if (frame) {
		av_frame_unref(frame);
		av_free(frame);
	}

	if (hw_ctx)
		av_buffer_unref(&hw_ctx);

	if (packet)
		av_packet_free(&packet);

	if (decoder)
		avcodec_free_context(&decoder);
}

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
/*
static inline enum speaker_layout convert_speaker_layout(uint8_t channels)
{
	switch (channels) {
	case 0:
		return SPEAKERS_UNKNOWN;
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
*/
DataPacket* FFMpegDecoder::pull_empty_packet(size_t size)
{
	size_t new_size = size + INPUT_BUFFER_PADDING_SIZE;
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

	enum video_range_type range =
		(out_frame->color_range == AVCOL_RANGE_JPEG)
		? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;

	if (range != obs_frame->range) {
		video_format_get_parameters(
			VIDEO_CS_DEFAULT, range, obs_frame->color_matrix,
			obs_frame->color_range_min, obs_frame->color_range_max);
		obs_frame->range = range;
	}

	obs_frame->width = out_frame->width;
	obs_frame->height = out_frame->height;
	obs_frame->flip = false;

	if (obs_frame->format == VIDEO_FORMAT_NONE) {
		obs_frame->format = convert_pixel_format(out_frame->format);
		if (obs_frame->format == VIDEO_FORMAT_NONE)
			return false;
	}

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
	obs_frame->speakers = SPEAKERS_MONO;

	if (obs_frame->format == AUDIO_FORMAT_UNKNOWN) {
		obs_frame->format = convert_sample_format(frame->format);
		if (obs_frame->format == AUDIO_FORMAT_UNKNOWN)
			return false;
	}

	*got_output = true;
	return true;
}
