// Copyright (C) 2023 DEV47APPS, github.com/dev47apps
#include <util/platform.h>
#include <util/windows/win-version.h>
#include "plugin.h"

void get_os_name_version(char *out, size_t out_size) {
    uint32_t version = get_win_ver_int();
    if (version > 0) {
        snprintf(out, out_size, "win%d.%d", ((version>>8)&0xFF), (version&0xFF));
    }
}
