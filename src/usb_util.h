// Copyright (C) 2020 github.com/aramg
#ifndef __USB_UTIL_H__
#define __USB_UTIL_H__

#define DEVICES_LIMIT 8

struct AdbDevice {
    char serial[80];
    char model[30];
    char state[18];
    AdbDevice(){}
    ~AdbDevice(){}
};

struct AdbMgr {
    int iter;
    int disabled;
    AdbDevice* deviceList[DEVICES_LIMIT];
    AdbMgr();
    ~AdbMgr();
    bool Reload(void);

    inline void ResetIter(void){ iter = 0; }
    AdbDevice* NextDevice(int *is_offline);
};

bool adb_forward(const char *serial, int local_port, int remote_port);
void adb_forward_remove_all(const char *serial);


#include "usbmuxd.h"

class USBMux {
private:
    usbmuxd_device_info_t *deviceList;

#ifdef _WIN32
    libusbmuxd_set_debug_level_t usbmuxd_set_debug_level;
    usbmuxd_get_device_list_t    usbmuxd_get_device_list;
    usbmuxd_device_list_free_t   usbmuxd_device_list_free;
    usbmuxd_connect_t            usbmuxd_connect;
    usbmuxd_disconnect_t         usbmuxd_disconnect;

    HMODULE hModule;
#else
    int *hModule;
#endif
public:
    int iter;
    int deviceCount;

    USBMux();
    ~USBMux();
    int Reload(void);
    usbmuxd_device_info_t* NextDevice(void);
    inline void ResetIter(void){ iter = 0; }
    int Connect(int device_id, int port);
};



#endif
