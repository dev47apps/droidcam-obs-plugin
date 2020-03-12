#ifndef __PLUGIN_PROPS_H__
#define __PLUGIN_PROPS_H__

#define OPT_CONNECT_IP        "wifi_ip"
#define OPT_CONNECT_PORT      "app_port"
#define OPT_RESOLUTION        "resolution"
#define OPT_CONNECT           "connect"
#define OPT_REFRESH           "refresh"
#define OPT_DEACTIVATE_WNS    "deactivate_wns"
#define OPT_SYNC_AV           "sync_av"
#define OPT_ENABLE_AUDIO      "enable_aduio"
#define OPT_DEVICE_LIST       "device_list"
#define OPT_DEVICE_ID_WIFI    "dev_id_wifi"

// FIXME get these strings out of the other module
#define TEXT_DEVICE         obs_module_text("Device")
#define TEXT_REFRESH        obs_module_text("Refresh")
#define TEXT_RESOLUTION     obs_module_text("Resolution")
#define TEXT_CONNECT        obs_module_text("Connect")
#define TEXT_DEACTIVATE     obs_module_text("Deactivate")
#define TEXT_DWNS           obs_module_text("DeactivateWhenNotShowing")
#define TEXT_USE_WIFI       obs_module_text("UseWiFi")
#define TEXT_ENABLE_AUDIO   obs_module_text("EnableAudio")
#define TEXT_SYNC_AV        obs_module_text("SyncAV")

#define PING_REQ "GET /ping"
#define AUDIO_REQ "GET /v1/audio"
#define VIDEO_REQ "GET /v1/video"

#endif