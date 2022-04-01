// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#pragma once
#include <util/threading.h>

#define DEVICES_LIMIT 8

struct Device {
    char serial[80];
    char model[80];
    char state[32];
    char address[64];
    Device(){
        memset(state, 0, sizeof(state));
        memset(model, 0, sizeof(model));
        memset(serial, 0, sizeof(serial));
        memset(address, 0, sizeof(address));
    }
    ~Device(){}
};

class DeviceDiscovery {
protected:
    int iter;
    Device* deviceList[DEVICES_LIMIT];
    virtual void DoReload(void) = 0;

private:
    int rthr;
    pthread_t pthr;
    friend void *reload_thread(void *data);

    inline void join(void) {
        if (rthr) {
            pthread_join(pthr, NULL);
            rthr = 0;
        }
    }

public:
    inline int Iter(void) { return iter; }
    void ResetIter(void) {
        join();
        iter = 0;
    }

    DeviceDiscovery() {
        for (int i = 0; i < DEVICES_LIMIT; i++) {
            deviceList[i] = NULL;
        }
        iter = 0;
        rthr = 0;
    };

    virtual ~DeviceDiscovery() {
        join();
        Clear();
    };

    void Reload(void);
    void Clear(void);
    Device* NextDevice(void);
    Device* AddDevice(const char* serial, size_t length);
    Device* GetDevice(const char* serial, size_t length = sizeof(Device::serial));
};


// MARK: WiFi MDNS
struct MDNS : DeviceDiscovery {
    MDNS(){}
    ~MDNS(){}
    void DoReload();
};



// MARK: Android USB
struct AdbMgr : DeviceDiscovery {
    int disabled;
    AdbMgr();
    ~AdbMgr();
    void DoReload();

    bool AddForward(Device* dev, int local_port, int remote_port);
    void ClearForwards(Device* dev);
    void GetModel(Device* dev);
    bool DeviceOffline(Device *dev) {
        return memcmp(dev->state, "device", 6) != 0;
    }
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
    void DoReload();
    iOSDevice* NextDevice(void);
    int Connect(int device_id, int port);
};
