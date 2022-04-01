#pragma once
#define OPT_VERSION           "version"
#define OPT_CONNECT_IP        "wifi_ip"
#define OPT_CONNECT_PORT      "app_port"
#define OPT_RESOLUTION        "resolution"
#define OPT_VIDEO_FORMAT      "video_format"
#define OPT_CONNECT           "connect"
#define OPT_REFRESH           "refresh"
#define OPT_DEACTIVATE_WNS    "deactivate_wns"
#define OPT_SYNC_AV           "sync_av"
#define OPT_USE_HW_ACCEL      "allow_hw_accel"
#define OPT_IS_ACTIVATED      "activated"
#define OPT_ENABLE_AUDIO      "enable_aduio"
#define OPT_DEVICE_LIST       "device_list"
#define OPT_DEVICE_ID_WIFI    "dev_id_wifi"
#define OPT_ACTIVE_DEV_ID     "cur_dev_id"
#define OPT_ACTIVE_DEV_TYPE   "cur_dev_type"

#define TEXT_DEVICE         obs_module_text("Device")
#define TEXT_REFRESH        obs_module_text("Refresh")
#define TEXT_RESOLUTION     obs_module_text("Resolution")
#define TEXT_VIDEO_FORMAT   obs_module_text("VideoFormat")
#define TEXT_CONNECT        obs_module_text("Activate")
#define TEXT_DEACTIVATE     obs_module_text("Deactivate")
#define TEXT_DWNS           obs_module_text("DeactivateWhenNotShowing")
#define TEXT_USE_WIFI       obs_module_text("UseWiFi")
#define TEXT_ENABLE_AUDIO   obs_module_text("EnableAudio")
#define TEXT_SYNC_AV        obs_module_text("SyncAV")
#define TEXT_USE_HW_ACCEL   obs_module_text("AllowHWAccel")

#define PING_REQ "GET /ping"
// TODO test against current live apps (ios + android)
#define AUDIO_REQ "GET /v1/audio.2"
#define VIDEO_REQ "GET /v4/video/%s/%s/port/%d/client/%s/nonce/%d/"

#define ADB_LOCALHOST_IP "127.0.0.1"

#define DROIDCAM_SERVICE_NAME "_droidcamobs._tcp.local."
