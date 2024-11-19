// Copyright (C) 2023 DEV47APPS, github.com/dev47apps
#include <util/platform.h>
#include <util/windows/win-version.h>
#include "plugin.h"

void get_os_name_version(char *out, size_t out_size) {
    struct win_version_info win_version = {0};
    get_win_ver(&win_version);

    if (win_version.major != 0) {
        snprintf(out, out_size, "win%d.%d.%d", win_version.major, win_version.minor, win_version.build);
    }
}
