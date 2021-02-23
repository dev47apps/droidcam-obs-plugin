// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#ifndef __MJPEG_DECODE_H__
#define __MJPEG_DECODE_H__

extern "C" {
#include <obs.h>
#include "turbojpeg.h"
}

#include "decoder.h"

struct MJpegDecoder : Decoder {
    tjhandle tj;
    uint8_t *frameBuf;
    int mSubsamp;

    MJpegDecoder(void) {
        ready = false;
        failed = false;
        alloc_count = 0;

        tj = NULL;
        frameBuf = NULL;
        mSubsamp = 0;
    }

    ~MJpegDecoder(void);
    bool init(void);
    bool decode_video(struct obs_source_frame2*, DataPacket*, bool *got_output);
    bool decode_audio(struct obs_source_audio* a, DataPacket* d, bool *got_output) {
        (void) a; (void) d;
        *got_output = false;
        return false;
    }

    void push_ready_packet(DataPacket*);
};

#endif
