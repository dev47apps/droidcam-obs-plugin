// Copyright (C) 2020 github.com/aramg
#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include <obs-module.h>

#define blog(log_level, format, ...) \
        blog(log_level, "[DroidCamOBS] " format, ##__VA_ARGS__)

#define dlog(format, ...) blog(LOG_INFO, format, ##__VA_ARGS__)
#define elog(format, ...) blog(LOG_WARNING, format, ##__VA_ARGS__)

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(-1)

#define FREE_OBJECT(obj, free_func) if(obj){dlog(" " #obj " %p\n", obj); free_func(obj); obj=NULL;}

void test_image(struct obs_source_frame2 *frame, int size);

#endif
