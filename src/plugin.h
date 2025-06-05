// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#pragma once
#include <obs-module.h>

#define PLUGIN_VERSION_STR "235"

#define xlog(log_level, format, ...) \
        blog(log_level, "[DroidCamOBS] " format, ##__VA_ARGS__)

#ifdef DEBUG
#define dlog(format, ...) xlog(LOG_INFO, format, ##__VA_ARGS__)
#else
#define dlog(format, ...) /* */
#endif
#define ilog(format, ...) xlog(LOG_INFO, format, ##__VA_ARGS__)
#define elog(format, ...) xlog(LOG_WARNING, format, ##__VA_ARGS__)

#define HEADER_SIZE 12
#define NO_PTS UINT64_C(~0)
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))

#if DROIDCAM_OVERRIDE
void droidcam_signal(obs_source_t* source, const char* signal);
#else
#define droidcam_signal(source, signal) /* */
#endif

void get_os_name_version(char *, size_t);
