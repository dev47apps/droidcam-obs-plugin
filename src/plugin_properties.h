#ifndef __PLUGIN_PROPS_H__
#define __PLUGIN_PROPS_H__

#define OPT_CONNECT_IP        "wifi_ip"
#define OPT_CONNECT_PORT      "app_port"
#define OPT_RESOLUTION        "resolution"
#define OPT_CONNECT           "connect"
#define OPT_REFRESH           "refresh"
#define OPT_DEACTIVATE_WNS    "deactivate_wns"
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

#endif