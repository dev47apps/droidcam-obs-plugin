/*
Copyright (C) github.com/aramg

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

#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>
#include <obs-module.h>

struct droidcam_obs_plugin {
    obs_source_t *source;
    os_event_t *stop_signal;
    pthread_t video_thread;
    bool initialized;
};

// https://obsproject.com/docs/reference-sources.html#c.obs_source_output_video
#define SIZE 128
static void *video_thread(void *data) {
    struct droidcam_obs_plugin *plugin = data;
    uint32_t y, x;
    uint8_t *p;
    uint8_t pixels[SIZE * SIZE * 4];
    uint64_t cur_time = os_gettime_ns();

    struct obs_source_frame frame = {
        .data = {[0] = pixels},
        .linesize = {[0] = SIZE * 4},
        .width = SIZE,
        .height = SIZE,
        .format = VIDEO_FORMAT_BGRX,
    };

    while (os_event_try(plugin->stop_signal) == EAGAIN) {
        p = &pixels[0];
        for (y = 0; y < SIZE; y++) {
            for (x = 0; x < SIZE/4; x++) {
                *p++ = 0; *p++ = 0; *p++ = 0xFF; p++;
            }
            for (x = 0; x < SIZE/4; x++) {
                *p++ = 0; *p++ = 0xFF; *p++ = 0; p++;
            }
            for (x = 0; x < SIZE/4; x++) {
                *p++ = 0xFF; *p++ = 0; *p++ = 0; p++;
            }
            for (x = 0; x < SIZE/4; x++) {
                *p++ = 0x80; *p++ = 0x80; *p++ = 0x80; p++;
            }
        }

        frame.timestamp = cur_time;
        obs_source_output_video(plugin->source, &frame);
        os_sleepto_ns(cur_time += 250000000);
    }

    return NULL;
}

static void plugin_destroy(void *data) {
    struct droidcam_obs_plugin *plugin = data;

    if (plugin) {
        if (plugin->initialized) {
            os_event_signal(plugin->stop_signal);
            pthread_join(plugin->video_thread, NULL);
        }

        os_event_destroy(plugin->stop_signal);
        bfree(plugin);
    }
}

static void *plugin_create(obs_data_t *settings, obs_source_t *source) {
    struct droidcam_obs_plugin *plugin = bzalloc(sizeof(struct droidcam_obs_plugin));
    plugin->source = source;

    if (os_event_init(&plugin->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    if (pthread_create(&plugin->video_thread, NULL, video_thread, plugin) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    plugin->initialized = true;

    UNUSED_PARAMETER(settings);
    return plugin;
}

static const char *plugin_getname(void *unused) {
    UNUSED_PARAMETER(unused);
    return obs_module_text("PluginName");
}

struct obs_source_info droidcam_obs_info = {
    .id = "droidcam_obs",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_ASYNC_VIDEO,//| OBS_SOURCE_AUDIO,
    .get_name = plugin_getname,
    .create = plugin_create,
    .destroy = plugin_destroy,
    .icon_type = OBS_ICON_TYPE_CAMERA,
/*
.update = vlcs_update,
.get_defaults = vlcs_defaults,
.get_properties = vlcs_properties,
.activate = vlcs_activate,
.deactivate = vlcs_deactivate,
*/
};
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("droidcam-obs", "en-US")
MODULE_EXPORT const char *obs_module_description(void) {
    return "Android camera source";
}

bool obs_module_load(void) {
    obs_register_source(&droidcam_obs_info);
    return true;
}

/*
void obs_module_unload(void) {
    if (libvlc) libvlc_release_(libvlc);
#ifdef __APPLE__
    if (libvlc_core_module) os_dlclose(libvlc_core_module);
#endif
    if (libvlc_module) os_dlclose(libvlc_module);
}
*/

