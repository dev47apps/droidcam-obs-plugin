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

#ifndef __FFMPEG_DECODE_H__
#define __FFMPEG_DECODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <obs.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#pragma warning(disable : 4204)
#endif

#include <libavcodec/avcodec.h>
#include <libavutil/log.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef __cplusplus
}
#endif

#include "decoder.h"

struct FFMpegDecoder : Decoder {
	AVCodecContext *decoder;
	AVCodec *codec;

	AVFrame *hw_frame;
	AVFrame *frame;
	bool hw;
	bool catchup;
	bool b_frame_check;

	FFMpegDecoder(void) {
		ready = false;
		failed = false;
		alloc_count = 0;

		decoder = NULL;
		codec = NULL;
		hw_frame = NULL;
		frame = NULL;
		hw = false;
		catchup = false;
		b_frame_check = false;
	}

	~FFMpegDecoder(void);

	int init(uint8_t* header, enum AVCodecID id, bool use_hw);
	bool decode_video(struct obs_source_frame2*, DataPacket*, bool *got_output);

	bool decode_audio(struct obs_source_audio*, DataPacket*, bool *got_output);

	DataPacket* pull_empty_packet(size_t size);
	void push_ready_packet(DataPacket*);
};
#endif
