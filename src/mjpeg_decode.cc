/*
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
#include "mjpeg_decode.h"

extern "C" {
    FILE __iob_func[3] = { *stdin,*stdout,*stderr };
}

MJpegDecoder::~MJpegDecoder(void) {
    if (frameBuf)
        bfree(frameBuf);

    if (tj)
        tjDestroy(tj);
}

bool MJpegDecoder::init(void) {
    if (tj) {
        elog("tj != NULL on init");
        return false;
    }

    tj = tjInitDecompress();
    if (!tj) {
        elog("error creating mjpeg decoder");
        return false;
    }

    ready = true;
    return true;
}

void MJpegDecoder::push_ready_packet(DataPacket* packet) {
    if (decodeQueue.items.size() > 1) {
        dlog("discard frame");
        recieveQueue.add_item(packet);
    } else {
        decodeQueue.add_item(packet);
    }
}

bool MJpegDecoder::decode_video(struct obs_source_frame2* obs_frame, DataPacket* data_packet,
        bool *got_output)
{
    *got_output = false;
    if (mSubsamp == 0) {
        int width, height, subsamp, colorspace;
        if (tjDecompressHeader3(tj,
            data_packet->data, data_packet->used,
            &width, &height, &subsamp, &colorspace) < 0)
        {
            elog("tjDecompressHeader3() failure: %d\n", tjGetErrorCode(tj));
            elog("%s\n", tjGetErrorStr2(tj));
            return false;
        }

        ilog("stream is %dx%d subsamp %d colorspace %d\n", width, height, subsamp, colorspace);
        if (subsamp != TJSAMP_420) {
            elog("error: unexpected video image stream subsampling: %d\n", subsamp);
            return false;
        }

        int ySize  = width * height;
        int uvSize = ySize / 4;

        size_t Yuv420Size = ySize * 3 / 2;
        frameBuf = (uint8_t*) brealloc(frameBuf, Yuv420Size);

        obs_frame->linesize[0] = width;
        obs_frame->linesize[1] = width>>1;
        obs_frame->linesize[2] = width>>1;
        obs_frame->linesize[3] = 0;

        obs_frame->data[0] = frameBuf;
        obs_frame->data[1] = obs_frame->data[0] + ySize;
        obs_frame->data[2] = obs_frame->data[1] + uvSize;
        obs_frame->data[3] = NULL;

        obs_frame->width = width;
        obs_frame->height = height;
        obs_frame->format = VIDEO_FORMAT_I420;
        mSubsamp = subsamp;
    }

    if (obs_frame->range != VIDEO_RANGE_FULL) {
        video_format_get_parameters(
            VIDEO_CS_DEFAULT, VIDEO_RANGE_FULL, obs_frame->color_matrix,
            obs_frame->color_range_min, obs_frame->color_range_max);
        obs_frame->range = VIDEO_RANGE_FULL;
    }

    if (tjDecompressToYUVPlanes(tj,
        data_packet->data, data_packet->used,
        obs_frame->data, obs_frame->width,
        (int*)obs_frame->linesize, obs_frame->height,
        TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE))
    {
        elog("tjDecompressToYUV2 failure: %d\n", tjGetErrorCode(tj));
        return false;
    }

    obs_frame->flip = false;
    *got_output = true;
    return true;
}
