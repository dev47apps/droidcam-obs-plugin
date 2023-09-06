// Copyright (C) 2023 DEV47APPS, github.com/dev47apps
#pragma once

void source_show(void *data);
void source_hide(void *data);

void source_show_main(void *data);
void source_hide_main(void *data);

void *source_create(obs_data_t *settings, obs_source_t *source);
void source_destroy(void *data);

void source_update(void *data, obs_data_t *settings);
void source_defaults(obs_data_t *settings);
obs_properties_t *source_properties(void *data);

void resolve_device_type(struct active_device_info*, void* data);

enum class DeviceType {
    NONE,
    WIFI,
    ADB,
    IOS,
    MDNS,
};

struct active_device_info {
    DeviceType type;
    int port;
    const char *id;
    const char *ip;
};

enum VideoFormat {
    FORMAT_AVC,
    FORMAT_MJPG,
};

struct Tally_t {
    bool on_program = false;
    bool on_preview = false;
};

enum class CommsTask {
    NONE,
    TALLY,
};
