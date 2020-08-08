/*
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
#include <stdlib.h>
#include <util/threading.h>
#include <util/platform.h>

#include "plugin.h"
#include "plugin_properties.h"
#include "ffmpeg_decode.h"
#include "net.h"
#include "buffer_util.h"
#include "usb_util.h"

#define VERSION_TEXT "v0.3"
#define FPS 25
#define MILLI_SEC 1000
#define NANO_SEC  1000000000

#define PLUGIN_RUNNING() (os_event_try(plugin->stop_signal) == EAGAIN)

enum class Action {
    None,
};

enum class DeviceType {
    NONE,
    WIFI,
    ADB,
    IOS,
};

struct active_device_info {
    DeviceType type;
    int port;
    const char *id;
    const char *ip;
};

struct droidcam_obs_plugin {
    AdbMgr *adbMgr;
    USBMux *iosMgr;
    Decoder* video_decoder;
    Decoder* audio_decoder;
    obs_source_t *source;
    os_event_t *stop_signal;
    pthread_t audio_thread;
    pthread_t video_thread;
    pthread_t video_decode_thread;
    enum video_range_type range;
    bool is_showing;
    bool activated;
    bool deactivateWNS;
    bool enable_audio;
    bool use_hw;
    bool audio_running;
    bool video_running;
    struct active_device_info device_info;
    struct obs_source_audio obs_audio_frame;
    struct obs_source_frame2 obs_video_frame;
    uint64_t time_start;

    //Queue<Action> actionQueue;
};

/* TODO and squash
os_performance_token_t* perf_token;
if (s->perf_token) {
    os_end_high_performance(s->perf_token);
}
s->perf_token = os_request_high_performance("NDI Receiver Thread");
...
os_end_high_performance(s->perf_token);
s->perf_token = NULL;
*/
#define ADB_PORT_START 7173
#define ADB_PORT_LAST  7203
int adb_port = ADB_PORT_START;

#if 0
test_image(&plugin->obs_video_frame, 320);
plugin->obs_video_frame.timestamp = os_gettime_ns();
obs_source_output_video2(plugin->source, &plugin->obs_video_frame);

static void test_image(struct obs_source_frame2 *frame, size_t size) {
    size_t y, x;
    uint8_t *pixels = (uint8_t *)malloc(size * size * 4);
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
#endif

static socket_t connect(struct droidcam_obs_plugin *plugin) {
    AdbDevice* dev;
    usbmuxd_device_info_t* iosdevice;
    AdbMgr* adbMgr = plugin->adbMgr;
    USBMux* iosMgr = plugin->iosMgr;
    struct active_device_info *device_info = &plugin->device_info;
    dlog("connect device: id=%s type=%d", device_info->id, device_info->type);

    if (device_info->type == DeviceType::WIFI) {
        return net_connect(device_info->ip, device_info->port);
    }

    if (device_info->type == DeviceType::ADB) {
        int is_offline, port;
        adbMgr->Reload();
        adbMgr->ResetIter();
        while ((dev = adbMgr->NextDevice(&is_offline)) != NULL) {
            dlog("checking against serial:%s state:%s\n", dev->serial, dev->state);
            if (strncmp(device_info->id, dev->serial, sizeof(dev->serial)) == 0) {
                if (is_offline) {
                    elog("device is offline...");
                    goto out;
                }

                if (adb_port > ADB_PORT_LAST) {
                    elog("warning: excessive adb port usage!");
                    adb_port = ADB_PORT_START;
                    adbMgr->ClearForwards(NULL);
                }

                port = adb_port++;
                dlog("ADB: mapping %d -> %d\n", port, device_info->port);
                if (!adbMgr->AddForward(dev->serial, port, device_info->port)) {
                    elog("adb_forward failed");
                    goto out;
                }

                socket_t rc = net_connect(ADB_LOCALHOST_IP, port);
                if (rc > 0) return rc;

                elog("adb connect failed");
                adbMgr->ClearForwards(dev->serial);
                goto out;
            }
        }

        goto out;
    }

    if (device_info->type == DeviceType::IOS) {
        iosMgr->Reload();
        iosMgr->ResetIter();
        while ((iosdevice = iosMgr->NextDevice()) != NULL) {
            dlog("checking against serial:%s\n", iosdevice->udid);
            if (strncmp(device_info->id, iosdevice->udid, sizeof(iosdevice->udid)) == 0) {
                return iosMgr->Connect(iosMgr->iter - 1, device_info->port);
            }
        }

        goto out;
    }

out:
    return INVALID_SOCKET;
}

#define MAXCONFIG 1024
#define MAXPACKET 1024 * 1024
static DataPacket*
read_frame(Decoder *decoder, socket_t sock, int *has_config)
{
    uint8_t header[HEADER_SIZE];
    uint8_t config[MAXCONFIG];
    size_t r;
    size_t len, config_len = 0;
    uint64_t pts;

AGAIN:
    r = net_recv_all(sock, header, HEADER_SIZE);
    if (r != HEADER_SIZE) {
        elog("read header recv returned %ld", r);
        return NULL;
    }

    pts = buffer_read64be(header);
    len = buffer_read32be(&header[8]);
    // dlog("read_frame: header: pts=%llu len=%ld", pts, len);

    if (pts == NO_PTS) {
        if (config_len != 0) {
             elog("double config ???");
             return NULL;
        }

        if ((int)len == -1) {
            elog("stop/error from app side");
            return NULL;
        }

        if (len == 0 || len > MAXCONFIG) {
            elog("config packet too large at %ld!", len);
            return NULL;
        }

        r = net_recv_all(sock, config, len);
        if (r != len) {
            elog("read config recv returned %ld", r);
            return NULL;
        }

        config_len = len;
        *has_config = 1;
        goto AGAIN;
    }

    if (len == 0 || len > MAXPACKET) {
        elog("data packet too large at %ld!", len);
        return NULL;
    }

    DataPacket* data_packet = decoder->pull_empty_packet(config_len + len);
    uint8_t *p = data_packet->data;
    if (config_len) {
        memcpy(p, config, config_len);
        p += config_len;
    }

    r = net_recv_all(sock, p, len);
    if (r != len) {
        elog("read_frame: read %ld bytes wanted %ld", r, len);
        decoder->push_empty_packet(data_packet);
        return NULL;
    }

    data_packet->pts = pts;
    data_packet->used = config_len + len;
    return data_packet;
}

static void *video_decode_thread(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);

    Decoder *decoder = NULL;
    DataPacket* data_packet = NULL;
    bool got_output;

    ilog("video_decode_thread start");

    while (PLUGIN_RUNNING()) {
        if ((decoder = plugin->video_decoder) == NULL || (data_packet = decoder->pull_ready_packet()) == NULL) {
            os_sleep_ms(2);
            continue;
        }

        if (decoder->failed)
            goto LOOP;

        if (!decoder->decode_video(&plugin->obs_video_frame, data_packet, &got_output)) {
            elog("error decoding video");
            decoder->failed = true;
            goto LOOP;
        }

        if (got_output) {
            plugin->obs_video_frame.timestamp = data_packet->pts * 100;
            //if (flip) plugin->obs_video_frame.flip = !plugin->obs_video_frame.flip;
    #if 0
            dlog("output video: %dx%d %lu",
                plugin->obs_video_frame.width,
                plugin->obs_video_frame.height,
                plugin->obs_video_frame.timestamp);
    #endif
            obs_source_output_video2(plugin->source, &plugin->obs_video_frame);
        }

LOOP:
        decoder->push_empty_packet(data_packet);
    }

    ilog("video_decode_thread end");
    return NULL;
}

static bool
recv_video_frame(struct droidcam_obs_plugin *plugin, socket_t sock) {
    Decoder *decoder = plugin->video_decoder;

    if (!decoder) {
        ilog("create video decoder");
        decoder = new FFMpegDecoder();
        plugin->video_decoder = decoder;
    }

    int has_config = 0;
    DataPacket* data_packet = read_frame(decoder, sock, &has_config);
    if (!data_packet)
        return false;

    // All paths must do something with data_packet
    if (has_config) {
        if (decoder->ready) {
            ilog("unexpected video config change while decoder is init'd");
            decoder->failed = true;
            goto FAILED;
        }

        bool use_hw = plugin->use_hw;
        if (((FFMpegDecoder*)decoder)->init(NULL, AV_CODEC_ID_H264, use_hw) < 0) {
            elog("could not initialize AVC decoder");
            decoder->failed = true;
            goto FAILED;
        }

        plugin->obs_video_frame.format = VIDEO_FORMAT_NONE;
    }

    // This should not happen, rather than causing a connection reset... idle
    if (decoder->failed) {
    FAILED:
        dlog("discarding frame.. decoder failed");
        decoder->push_empty_packet(data_packet);
        return true;
    }

    decoder->push_ready_packet(data_packet);
    return true;
}

static void *video_thread(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    socket_t sock = INVALID_SOCKET;
    const char *video_req = VIDEO_REQ;

    ilog("video_thread start");
    while (PLUGIN_RUNNING()) {
        if (plugin->activated && plugin->is_showing) {
            if (plugin->video_running) {
                if (recv_video_frame(plugin, sock)) {
                    continue;
                }

                plugin->video_running = false;
                ilog("closing failed video socket %d", sock);
                net_close(sock);
                sock = INVALID_SOCKET;
                goto SLOW_LOOP;
            }

            if ((sock = connect(plugin)) == INVALID_SOCKET)
                goto SLOW_LOOP;

            if (net_send_all(sock, video_req, sizeof(VIDEO_REQ)-1) <= 0) {
                elog("send(/video) failed");
                net_close(sock);
                sock = INVALID_SOCKET;

SLOW_LOOP:
                os_sleep_ms(MILLI_SEC * 2);
                goto LOOP;
            }

            plugin->video_running = true;
            ilog("starting video via socket %d", sock);
            continue;
        }

        // else: not activated
        if (plugin->video_running) {
            plugin->video_running = false;
        }

LOOP:
        if (sock != INVALID_SOCKET) {
            ilog("closing active video socket %d", sock);
            net_close(sock);
            sock = INVALID_SOCKET;
        }

        if (plugin->video_decoder) {
            while (plugin->video_decoder->recieveQueue.items.size() < plugin->video_decoder->alloc_count
            && PLUGIN_RUNNING()){
                ilog("waiting for decode thread: %ld/%d",
                    plugin->video_decoder->recieveQueue.items.size(),
                    plugin->video_decoder->alloc_count);
                os_sleep_ms(MILLI_SEC);
            }

            ilog("release video_decoder");
            delete plugin->video_decoder;
            plugin->video_decoder = NULL;
        }

        obs_source_output_video2(plugin->source, NULL);
        os_sleep_ms(MILLI_SEC / FPS);
    }

    ilog("video_thread end");
    plugin->video_running = false;
    if (sock != INVALID_SOCKET) net_close(sock);
    return NULL;
}

static bool
do_audio_frame(struct droidcam_obs_plugin *plugin, socket_t sock) {
    FFMpegDecoder *decoder = (FFMpegDecoder*)plugin->audio_decoder;
    if (!decoder) {
        ilog("create audio decoder");
        decoder = new FFMpegDecoder();
        plugin->audio_decoder = decoder;
    }

    int has_config = 0;
    bool got_output;
    DataPacket* data_packet = read_frame(decoder, sock, &has_config);
    if (!data_packet)
        return false;

    if (has_config) {
        if (decoder->ready) {
            ilog("unexpected audio config change while decoder is init'd");
            decoder->failed = true;
            goto FAILED;
        }

        if (decoder->init(data_packet->data, AV_CODEC_ID_AAC, false) < 0) {
            elog("could not initialize AAC decoder");
            decoder->failed = true;
            goto FAILED;
        }

        plugin->obs_audio_frame.format = AUDIO_FORMAT_UNKNOWN;
        decoder->push_empty_packet(data_packet);
        return true;
    }

    if (decoder->failed) {
    FAILED:
        dlog("discarding audio frame.. decoder failed");
        decoder->push_empty_packet(data_packet);
        return true;
    }

    // decoder->push_ready_packet(data_packet);
    if (!decoder->decode_audio(&plugin->obs_audio_frame, data_packet, &got_output)) {
        elog("error decoding audio");
        decoder->failed = true;
        goto FAILED;
    }

    if (got_output) {
        plugin->obs_audio_frame.timestamp = data_packet->pts * 100;
#if 0
        dlog("output audio: %d frames: %d HZ, Fmt %d, Chan %d,  pts %lu",
            plugin->obs_audio_frame.frames,
            plugin->obs_audio_frame.samples_per_sec,
            plugin->obs_audio_frame.format,
            plugin->obs_audio_frame.speakers,
            plugin->obs_audio_frame.timestamp);
#endif
        obs_source_output_audio(plugin->source, &plugin->obs_audio_frame);
    }

    decoder->push_empty_packet(data_packet);
    return true;
}

static void *audio_thread(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    socket_t sock = INVALID_SOCKET;
    const char *audio_req = AUDIO_REQ;

    ilog("audio_thread start");
    while (PLUGIN_RUNNING()) {
        if (plugin->activated && plugin->is_showing && plugin->enable_audio) {
            if (plugin->audio_running) {
                if (do_audio_frame(plugin, sock)) {
                    continue;
                }

                plugin->audio_running = false;
                ilog("closing failed audio socket %d", sock);
                net_close(sock);
                sock = INVALID_SOCKET;
                goto SLOW_LOOP;
            }

            // connect audio only after video works
            if (!plugin->video_running)
                goto LOOP;

            // no rush..
            os_sleep_ms(MILLI_SEC);

            if ((sock = connect(plugin)) == INVALID_SOCKET)
                goto SLOW_LOOP;

            if (net_send_all(sock, audio_req, sizeof(AUDIO_REQ)-1) <= 0) {
                elog("send(/audio) failed");
                net_close(sock);
                sock = INVALID_SOCKET;

SLOW_LOOP:
                os_sleep_ms(MILLI_SEC * 2);
                goto LOOP;
            }

            plugin->audio_running = true;
            ilog("starting audio via socket %d", sock);
            continue;
        }

        // else: not activated
        if (plugin->audio_running) {
            plugin->audio_running = false;
        }

LOOP:
        if (sock != INVALID_SOCKET) {
            ilog("closing active audio socket %d", sock);
            net_close(sock);
            sock = INVALID_SOCKET;
        }

        if (plugin->audio_decoder) {
            ilog("release audio_decoder");
            delete plugin->audio_decoder;
            plugin->audio_decoder = NULL;
        }

        if (plugin->enable_audio) obs_source_output_audio(plugin->source, NULL);
        os_sleep_ms(MILLI_SEC / FPS);
    }

    ilog("audio_thread end");
    plugin->audio_running = false;
    if (sock != INVALID_SOCKET) net_close(sock);
    return NULL;
}

static void plugin_destroy(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);

    if (plugin) {
        if (plugin->time_start != 0) {
            ilog("stopping");
            os_event_signal(plugin->stop_signal);
            pthread_join(plugin->video_thread, NULL);
            pthread_join(plugin->audio_thread, NULL);

            pthread_join(plugin->video_decode_thread, NULL);
        }

        ilog("cleanup");
        os_event_destroy(plugin->stop_signal);

        if (plugin->video_decoder) delete plugin->video_decoder;
        if (plugin->audio_decoder) delete plugin->audio_decoder;
        delete plugin->adbMgr;
        delete plugin->iosMgr;
        delete plugin;
    }
}

static void *plugin_create(obs_data_t *settings, obs_source_t *source) {
    ilog("create(source=%p) " VERSION_TEXT, source);
    obs_source_set_async_unbuffered(source, true);

    droidcam_obs_plugin *plugin = new droidcam_obs_plugin();
    plugin->source = source;
    plugin->audio_running = false;
    plugin->video_running = false;
    plugin->adbMgr = new AdbMgr();
    plugin->iosMgr = new USBMux();
    plugin->use_hw = obs_data_get_bool(settings, OPT_USE_HW_ACCEL);
    plugin->enable_audio  = obs_data_get_bool(settings, OPT_ENABLE_AUDIO);
    plugin->deactivateWNS = obs_data_get_bool(settings, OPT_DEACTIVATE_WNS);
    plugin->activated = obs_data_get_bool(settings, OPT_IS_ACTIVATED);
    ilog("activated=%d, deactivateWNS=%d, is_showing=%d, enable_audio=%d",
        plugin->activated, plugin->deactivateWNS, plugin->is_showing, plugin->enable_audio);

    if (plugin->activated) {
        plugin->device_info.id = obs_data_get_string(settings, OPT_ACTIVE_DEV_ID);
        plugin->device_info.ip = obs_data_get_string(settings, OPT_CONNECT_IP);
        plugin->device_info.port = (int) obs_data_get_int(settings, OPT_CONNECT_PORT);
        plugin->device_info.type = (DeviceType) obs_data_get_int(settings, OPT_ACTIVE_DEV_TYPE);
        ilog("device_info.id=%s device_info.ip=%s device_info.port=%d device_info.type=%d",
            plugin->device_info.id, plugin->device_info.ip, plugin->device_info.port, plugin->device_info.type);
        if (plugin->device_info.type == DeviceType::NONE
            || plugin->device_info.port <= 0 || plugin->device_info.port > 65535
            || !plugin->device_info.id || plugin->device_info.id[0] == 0)
            plugin->activated = false;

        if (plugin->device_info.type == DeviceType::WIFI && (!plugin->device_info.ip || plugin->device_info.ip[0] == 0))
            plugin->activated = false;
    }

    if (os_event_init(&plugin->stop_signal, OS_EVENT_TYPE_MANUAL) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    if (pthread_create(&plugin->video_thread, NULL, video_thread, plugin) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    if (pthread_create(&plugin->video_decode_thread, NULL, video_decode_thread, plugin) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    if (pthread_create(&plugin->audio_thread, NULL, audio_thread, plugin) != 0) {
        plugin_destroy(plugin);
        return NULL;
    }

    plugin->time_start = os_gettime_ns() / 100;
    return plugin;
}

static void plugin_show(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    plugin->is_showing = true;
    dlog("plugin_show: is_showing=%d", plugin->is_showing);
}

static void plugin_hide(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    if (plugin->deactivateWNS && plugin->activated)
        plugin->is_showing = false;

    dlog("plugin_hide: is_showing=%d", plugin->is_showing);
}

static inline void toggle_ppts(obs_properties_t *ppts, bool enable) {
    obs_property_set_enabled(obs_properties_get(ppts, OPT_REFRESH)     , enable);
    obs_property_set_enabled(obs_properties_get(ppts, OPT_DEVICE_LIST) , enable);
    obs_property_set_enabled(obs_properties_get(ppts, OPT_CONNECT_IP)  , enable);
    obs_property_set_enabled(obs_properties_get(ppts, OPT_CONNECT_PORT), enable);
    obs_property_set_enabled(obs_properties_get(ppts, OPT_ENABLE_AUDIO), enable);
    obs_property_set_enabled(obs_properties_get(ppts, OPT_USE_HW_ACCEL), enable);
}

static bool connect_clicked(obs_properties_t *ppts, obs_property_t *p, void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);

    int is_offline;
    AdbDevice* dev;
    usbmuxd_device_info_t* iosdevice;
    AdbMgr* adbMgr = plugin->adbMgr;
    USBMux* iosMgr = plugin->iosMgr;
    struct active_device_info *device_info = &plugin->device_info;

    obs_data_t *settings = obs_source_get_settings(plugin->source);
    obs_property_t *cp = obs_properties_get(ppts, OPT_CONNECT);
    obs_property_set_enabled(cp, false);

    bool activated = obs_data_get_bool(settings, OPT_IS_ACTIVATED);
    if (activated) {
        plugin->activated = false;
        toggle_ppts(ppts, true);
        obs_data_set_bool(settings, OPT_IS_ACTIVATED, false);
        obs_property_set_description(cp, TEXT_CONNECT);
        goto out;
    }

    device_info->type = DeviceType::NONE;
    device_info->id = obs_data_get_string(settings, OPT_DEVICE_LIST);
    if (!device_info->id || device_info->id[0] == 0){
        elog("target device id is empty");
        goto out;
    }

    if (memcmp(device_info->id, OPT_DEVICE_ID_WIFI, sizeof(OPT_DEVICE_ID_WIFI)-1) == 0) {
        device_info->type = DeviceType::WIFI;
        device_info->ip = obs_data_get_string(settings, OPT_CONNECT_IP);
        if (!device_info->ip || device_info->ip[0] == 0) {
            elog("target IP is empty");
            goto out;
        }
    }

    device_info->port = (int) obs_data_get_int(settings, OPT_CONNECT_PORT);
    if (device_info->port <= 0 || device_info->port > 65535) {
        elog("invalid port: %d", device_info->port);
        goto out;
    }

    if (device_info->type != DeviceType::NONE)
        goto skip_usb_check;

    adbMgr->ResetIter();
    while ((dev = adbMgr->NextDevice(&is_offline)) != NULL) {
        dlog("ADB: serial:%s state:%s\n", dev->serial, dev->state);
        if (strncmp(device_info->id, dev->serial, sizeof(dev->serial)) == 0) {
            if (is_offline) {
                elog("adb device is offline");
                goto out;
            }

            device_info->type = DeviceType::ADB;
            goto skip_usb_check;
        }
    }

    iosMgr->ResetIter();
    while ((iosdevice = iosMgr->NextDevice()) != NULL) {
        dlog("IOS: serial:%s\n", iosdevice->udid);
        if (strncmp(device_info->id, iosdevice->udid, sizeof(iosdevice->udid)) == 0) {
            device_info->type = DeviceType::IOS;
            goto skip_usb_check;
        }
    }

    if (device_info->type == DeviceType::NONE) {
        elog("unable to determine devce type, this should not happen");
        goto out;
    }

skip_usb_check:
    obs_property_set_description(cp, TEXT_DEACTIVATE);
    toggle_ppts(ppts, false);
    obs_data_set_string(settings, OPT_ACTIVE_DEV_ID, device_info->id);
    obs_data_set_int(settings, OPT_ACTIVE_DEV_TYPE, (long long) device_info->type);
    obs_data_set_bool(settings, OPT_IS_ACTIVATED, true);
    plugin->activated = true;
    ilog("activated: id=%s type=%d ip=%s port=%d", device_info->id, device_info->type, device_info->ip, device_info->port);

out:
    obs_property_set_enabled(cp, true);
    if (settings) obs_data_release(settings);
    return true;
}

static bool refresh_clicked(obs_properties_t *ppts, obs_property_t *p, void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    int is_offline;
    AdbDevice* dev;
    usbmuxd_device_info_t* iosdevice;
    AdbMgr *adbMgr = plugin->adbMgr;
    USBMux* iosMgr = plugin->iosMgr;
    obs_property_t *cp = obs_properties_get(ppts, OPT_CONNECT);
    obs_property_set_enabled(cp, false);

    p = obs_properties_get(ppts, OPT_DEVICE_LIST);
    obs_property_list_clear(p);

    if (!adbMgr || !iosMgr){
        ilog("adbMgr=%p, iosMgr=%p in refresh_clicked", adbMgr, iosMgr);
        goto out;
    }

    adbMgr->ClearForwards(NULL);
    adbMgr->Reload();
    adbMgr->ResetIter();
    while ((dev = adbMgr->NextDevice(&is_offline)) != NULL) {
        char *label = dev->model[0] != 0 ? dev->model : dev->serial;
        size_t idx = obs_property_list_add_string(p, label, dev->serial);
        if (is_offline)
            obs_property_list_item_disable(p, idx, true);
    }

    iosMgr->Reload();
    iosMgr->ResetIter();
    // TODO reload ios devices

out:
    obs_property_list_add_string(p, TEXT_USE_WIFI, OPT_DEVICE_ID_WIFI);
    obs_property_set_enabled(cp, true);
    return true;
}

static void plugin_update(void *data, obs_data_t *settings) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    plugin->deactivateWNS = obs_data_get_bool(settings, OPT_DEACTIVATE_WNS);
    plugin->enable_audio  = obs_data_get_bool(settings, OPT_ENABLE_AUDIO);
    plugin->use_hw = obs_data_get_bool(settings, OPT_USE_HW_ACCEL);
    bool sync_av = obs_data_get_bool(settings, OPT_SYNC_AV);
    bool activated = obs_data_get_bool(settings, OPT_IS_ACTIVATED);

    ilog("plugin_udpate: activated=%d (actual=%d) audio=%d sync_av=%d",
        plugin->activated,
        activated,
        plugin->enable_audio,
        sync_av);
    obs_source_set_async_decoupled(plugin->source, !sync_av);

    // handle [Cancel] case
    if (activated != plugin->activated) {
        plugin->activated = activated;
    }
}

static obs_properties_t *plugin_properties(void *data) {
    droidcam_obs_plugin *plugin = reinterpret_cast<droidcam_obs_plugin *>(data);
    obs_data_t *settings = obs_source_get_settings(plugin->source);
    obs_properties_t *ppts = obs_properties_create();
    obs_property_t *cp;
    AdbDevice* dev;
    usbmuxd_device_info_t* iosdevice;
    int is_offline;
    bool activated = obs_data_get_bool(settings, OPT_IS_ACTIVATED);
    ilog("plugin_properties: activated=%d (actual=%d)", plugin->activated, activated);

    obs_properties_add_list(ppts, OPT_DEVICE_LIST, TEXT_DEVICE, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    cp = obs_properties_get(ppts, OPT_DEVICE_LIST);
    if (plugin->adbMgr) {
        plugin->adbMgr->ResetIter();
        while ((dev = plugin->adbMgr->NextDevice(&is_offline)) != NULL) {
            char *label = dev->model[0] != 0 ? dev->model : dev->serial;
            size_t idx = obs_property_list_add_string(cp, label, dev->serial);
            if (is_offline) obs_property_list_item_disable(cp, idx, true);
        }
    }

    if (plugin->iosMgr) {
        plugin->iosMgr->ResetIter();
        // TODO reload ios devices
    }
    obs_property_list_add_string(cp, TEXT_USE_WIFI, OPT_DEVICE_ID_WIFI);
    obs_properties_add_button(ppts, OPT_REFRESH, TEXT_REFRESH, refresh_clicked);

    obs_properties_add_text(ppts, OPT_CONNECT_IP, "WiFi IP", OBS_TEXT_DEFAULT);
    obs_properties_add_int(ppts, OPT_CONNECT_PORT, "DroidCam Port", 1, 65535, 1);

    cp = obs_properties_add_button(ppts, OPT_CONNECT, TEXT_CONNECT, connect_clicked);
    obs_properties_add_bool(ppts, OPT_USE_HW_ACCEL, TEXT_USE_HW_ACCEL);
    obs_properties_add_bool(ppts, OPT_DEACTIVATE_WNS, TEXT_DWNS);
    obs_properties_add_bool(ppts, OPT_ENABLE_AUDIO, TEXT_ENABLE_AUDIO);
    obs_properties_add_bool(ppts, OPT_SYNC_AV, TEXT_SYNC_AV);

    if (activated) {
        toggle_ppts(ppts, false);
        obs_property_set_description(cp, TEXT_DEACTIVATE);
    }
    return ppts;
}

static void plugin_defaults(obs_data_t *settings) {
    dlog("plugin_defaults");
    obs_data_set_default_bool(settings, OPT_IS_ACTIVATED, false);
    obs_data_set_default_bool(settings, OPT_SYNC_AV, false);
    obs_data_set_default_bool(settings, OPT_USE_HW_ACCEL,
#ifdef __linux__
    false
#else
    true
#endif
    );
    obs_data_set_default_bool(settings, OPT_ENABLE_AUDIO, false);
    obs_data_set_default_bool(settings, OPT_DEACTIVATE_WNS, false);
    obs_data_set_default_int(settings, OPT_CONNECT_PORT, 1212);
}

static const char *plugin_getname(void *x) {
    UNUSED_PARAMETER(x);
    return obs_module_text("PluginName");
}

struct obs_source_info droidcam_obs_info;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("droidcam-obs", "en-US")
MODULE_EXPORT const char *obs_module_description(void) {
    return "Android and iOS camera source";
}

bool obs_module_load(void) {
    memset(&droidcam_obs_info, 0, sizeof(struct obs_source_info));

    droidcam_obs_info.id           = "droidcam_obs";
    droidcam_obs_info.type         = OBS_SOURCE_TYPE_INPUT;
    droidcam_obs_info.output_flags = OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_AUDIO | OBS_SOURCE_ASYNC_VIDEO;
    droidcam_obs_info.get_name     = plugin_getname;
    droidcam_obs_info.create       = plugin_create;
    droidcam_obs_info.destroy      = plugin_destroy;
    droidcam_obs_info.show         = plugin_show;
    droidcam_obs_info.hide         = plugin_hide;
    droidcam_obs_info.update       = plugin_update;
    //droidcam_obs_info.icon_type    = OBS_ICON_TYPE_CAMERA;
    droidcam_obs_info.get_defaults = plugin_defaults;
    droidcam_obs_info.get_properties = plugin_properties;
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

