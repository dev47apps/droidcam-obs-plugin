// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#pragma once

#define DEVICES_LIMIT 8

struct Device {
    char serial[80];
    char model[80];
    char state[32];
    char address[64];
    Device(){}
    ~Device(){}
};

struct DeviceDiscovery {
    int iter;
    Device* deviceList[DEVICES_LIMIT];
    inline void ResetIter(void) {
        iter = 0;
    }

    inline void ClearDeviceList() {
        for (int i = 0; i < DEVICES_LIMIT; i++) {
            if(deviceList[i]) delete deviceList[i];
            deviceList[i] = NULL;
        }
    }

    DeviceDiscovery() {
        ClearDeviceList();
        ResetIter();
    };

    ~DeviceDiscovery() {
        ClearDeviceList();
    };
};


// MARK: WiFi MDNS
struct MDNS : DeviceDiscovery {
    int sock;
    int query_id;
    char buffer[2048];
    MDNS();
    ~MDNS();
    bool Query(const char* service_name);
    void FinishQuery(void);
    Device* NextDevice(void);
};



// MARK: Android USB
typedef struct Device AdbDevice;

struct AdbMgr : DeviceDiscovery {
    int disabled;
    AdbMgr();
    ~AdbMgr();
    bool Reload(void);

    AdbDevice* NextDevice(int *is_offline, int get_name = 0);
    bool AddForward(const char *serial, int local_port, int remote_port);
    void ClearForwards(const char *serial);
};



// MARK: Apple USB
#include "usbmuxd.h"
typedef usbmuxd_device_info_t iOSDevice;

struct USBMux : DeviceDiscovery {
    usbmuxd_device_info_t *deviceList;

#ifdef _WIN32
    libusbmuxd_set_debug_level_t usbmuxd_set_debug_level;
    usbmuxd_get_device_list_t    usbmuxd_get_device_list;
    usbmuxd_device_list_free_t   usbmuxd_device_list_free;
    usbmuxd_connect_t            usbmuxd_connect;
    usbmuxd_disconnect_t         usbmuxd_disconnect;

    HMODULE hModule;
#else
    void* hModule;
#endif
    int deviceCount;

    USBMux();
    ~USBMux();
    bool Reload(void);
    iOSDevice* NextDevice(void);
    int Connect(int device_id, int port);
};
