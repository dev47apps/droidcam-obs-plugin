// Copyright (C) 2020 github.com/aramg
#include <plugin.h>

void test_image(struct obs_source_frame2 *frame, int size) {
    int y, x;
    uint8_t *pixels = malloc(size * size * 4);
    if (!pixels) return;

    frame->data[0] = pixels;
    frame->linesize[0] = size * 4;
    frame->format = VIDEO_FORMAT_BGRX;
    frame->width = size;
    frame->height = size;

    uint8_t *p = pixels;
    for (y = 0; y < size; y++) {
        for (x = 0; x < size/4; x++) {
            *p++ = 0; *p++ = 0; *p++ = 0xFF; p++;
        }
        for (x = 0; x < size/4; x++) {
            *p++ = 0; *p++ = 0xFF; *p++ = 0; p++;
        }
        for (x = 0; x < size/4; x++) {
            *p++ = 0xFF; *p++ = 0; *p++ = 0; p++;
        }
        for (x = 0; x < size/4; x++) {
            *p++ = 0x80; *p++ = 0x80; *p++ = 0x80; p++;
        }
    }
}
