/*
	Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>
	Copyright (C) 2020 github.com/aramg

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

#if LIBAVCODEC_VERSION_INT > AV_VERSION_INT(58, 4, 100)
#define USE_NEW_HARDWARE_CODEC_METHOD
#endif

#ifdef USE_NEW_HARDWARE_CODEC_METHOD
enum AVHWDeviceType hw_priority[] = {
	AV_HWDEVICE_TYPE_NONE,
};

static bool has_hw_type(AVCodec *c, enum AVHWDeviceType type)
{
	for (int i = 0;; i++) {
		const AVCodecHWConfig *config = avcodec_get_hw_config(c, i);
		if (!config) {
			break;
		}

		if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type)
			return true;
	}

	return false;
}

static void init_hw_decoder(FFMpegDecoder *d)
{
	enum AVHWDeviceType *priority = hw_priority;
	AVBufferRef *hw_ctx = NULL;

	ilog("init hw decoder");
	while (*priority != AV_HWDEVICE_TYPE_NONE) {
		if (has_hw_type(d->codec, *priority)) {
			int ret = av_hwdevice_ctx_create(&hw_ctx, *priority, NULL, NULL, 0);
			if (ret == 0)
				break;
		}
		priority++;
	}

	if (hw_ctx) {
		d->decoder->hw_device_ctx = av_buffer_ref(hw_ctx);
		d->hw = true;
	}
}
#endif

int FFMpegDecoder::init(uint8_t* header, enum AVCodecID id, bool use_hw)
{
	int ret;
	// av_log_set_level(AV_LOG_DEBUG);
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
	error// avcodec_register_all();
#endif

	codec = avcodec_find_decoder(id);
	if (!codec)
		return -1;

	decoder = avcodec_alloc_context3(codec);

	if (id == AV_CODEC_ID_AAC) {
		// https://wiki.multimedia.cx/index.php/MPEG-4_Audio
		static int aac_frequencies[] = {96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000};
		if (!header) {
			blog(LOG_ERROR, "missing AAC header required to init decoder");
			return -1;
		}

		int sr_idx = ((header[0] << 1) | (header[1] >> 7)) & 0x1F;
		ilog("sr_idx=%d [0x%2x 0x%2x]", sr_idx, header[0], header[1]);
		if (sr_idx < 0 || sr_idx >= (int)(sizeof(aac_frequencies) / sizeof(int))) {
			blog(LOG_ERROR, "failed to parse AAC header, sr_idx=%d [0x%2x 0x%2x]", sr_idx, header[0], header[1]);
			return -1;
		}
		decoder->sample_rate = aac_frequencies[sr_idx];
		decoder->profile = FF_PROFILE_AAC_LOW;
		decoder->channel_layout = AV_CH_LAYOUT_MONO;
		// also hard coded in decode_audio_frame
		decoder->channels = 1;
		ilog("audio sample_rate=%d", decoder->sample_rate);
	}

#ifdef USE_NEW_HARDWARE_CODEC_METHOD
	if (use_hw) init_hw_decoder(this);
#else
	(void)use_hw;
#endif

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
	ready = true;
	return 0;
}

FFMpegDecoder::~FFMpegDecoder(void)
{
	if (hw_frame)
		av_free(hw_frame);

	if (frame)
		av_free(frame);

	if (decoder) {
		avcodec_close(decoder);
		av_free(decoder);
	}
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
	memset(packet->data + size, 0, INPUT_BUFFER_PADDING_SIZE);
	return packet;
}

void FFMpegDecoder::push_ready_packet(DataPacket* packet)
{
	if (catchup) {
		if (decodeQueue.items.size() > 0){
			recieveQueue.add_item(packet);
			return;
		}
		if (codec->id == AV_CODEC_ID_H264
			&& !obs_avc_keyframe(packet->data, packet->used))
		{
			dlog("discard non key-frame");
			recieveQueue.add_item(packet);
			return;
		}

		ilog("decoder catchup: decodeQueue: %ld recieveQueue: %ld", decodeQueue.items.size(), recieveQueue.items.size());
		catchup = false;
	}

	decodeQueue.add_item(packet);
	if (codec->id == AV_CODEC_ID_H264 && decodeQueue.items.size() > 24) {
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
	AVPacket packet = {0};
	int got_frame = false;
	AVFrame *out_frame;
	int ret;
	*got_output = false;

	av_init_packet(&packet);
	packet.data = data_packet->data;
	packet.size = data_packet->used;
	packet.pts = (data_packet->pts == NO_PTS) ? AV_NOPTS_VALUE : data_packet->pts;

	if (// codec->id == AV_CODEC_ID_H264 &&
		obs_avc_keyframe(data_packet->data, data_packet->used))	{
		packet.flags |= AV_PKT_FLAG_KEY;
	}

	if (decoder->has_b_frames && !b_frame_check) {
	    elog("WARNING Stream has b-frames!");
	    b_frame_check = true;
	}

	if (!frame) {
		frame = av_frame_alloc();
		if (!frame)
			return false;

		if (hw && !hw_frame) {
			hw_frame = av_frame_alloc();
			if (!hw_frame)
				return false;
		}
	}

	out_frame = hw ? hw_frame : frame;

	ret = avcodec_send_packet(decoder, &packet);
	if (ret == 0) {
		ret = avcodec_receive_frame(decoder, out_frame);
	}

	got_frame = (ret == 0);

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		ret = 0;

	if (ret < 0)
		return false;
	else if (!got_frame)
		return true;

#ifdef USE_NEW_HARDWARE_CODEC_METHOD
	if (got_frame && hw) {
		ret = av_hwframe_transfer_data(frame, out_frame, 0);
		if (ret < 0) {
			return false;
		}
	}
#endif

	for (int i = 0; i < MAX_AV_PLANES; i++) {
		obs_frame->data[i]     = frame->data[i];
		obs_frame->linesize[i] = frame->linesize[i];
	}

	enum video_range_type range =
		(frame->color_range == AVCOL_RANGE_JPEG)
		? VIDEO_RANGE_FULL : VIDEO_RANGE_PARTIAL;

	if (range != obs_frame->range) {
		const bool success = video_format_get_parameters(
			VIDEO_CS_601, range, obs_frame->color_matrix,
			obs_frame->color_range_min, obs_frame->color_range_max);
		if (!success) {
			blog(LOG_ERROR,
				"Failed to get video format "
				"parameters for video format %u",
				VIDEO_CS_601);
			return false;
		}

		obs_frame->range = range;
	}

	obs_frame->width = frame->width;
	obs_frame->height = frame->height;
	obs_frame->flip = false;

	if (obs_frame->format == VIDEO_FORMAT_NONE) {
		obs_frame->format = convert_pixel_format(frame->format);
		if (obs_frame->format == VIDEO_FORMAT_NONE)
			return false;
	}

	*got_output = true;
	return true;
}

bool FFMpegDecoder::decode_audio(struct obs_source_audio* obs_frame, DataPacket* data_packet, bool *got_output)
{
	AVPacket packet = {0};
	int got_frame = false;
	int ret = 0;
	*got_output = false;

	av_init_packet(&packet);
	packet.data = data_packet->data;
	packet.size = data_packet->used;
	packet.pts = (data_packet->pts == NO_PTS) ? AV_NOPTS_VALUE : data_packet->pts;

	if (!frame) {
		frame = av_frame_alloc();
		if (!frame)
			return false;
	}

	ret = avcodec_send_packet(decoder, &packet);
	if (ret == 0)
		ret = avcodec_receive_frame(decoder, frame);

	got_frame = (ret == 0);

	if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
		ret = 0;

	if (ret < 0) {
		return false;
	}
	else if (!got_frame)
		return true;

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
