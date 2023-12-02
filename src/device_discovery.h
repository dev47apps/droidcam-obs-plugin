// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#pragma once
#include <util/threading.h>

#define DEVICES_LIMIT 8

struct Device {
    char serial[80];
    char model[80];
    char state[32];
    char address[64];
    int handle;
    Device(){
        handle = 0;
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
    const char* suffix = "";
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

struct Proxy {
    DeviceDiscovery* discovery_mgr;
    volatile Device *proxy_device;
    volatile socket_t proxy_sock;

    int port_local;
    int port_remote;
    int thread_active;

    pthread_t pthr;
    friend void *proxy_run(void *data);

    Proxy(DeviceDiscovery*);
    ~Proxy();
    int Start(Device*, int remote_port);
};

// MARK: WiFi MDNS
struct MDNS : DeviceDiscovery {
    int networkPrefix = 0;
    const char* suffix = "WIFI";
    void DoReload();
};



// MARK: Android USB
struct AdbMgr : DeviceDiscovery {
    const char* suffix = "USB";
    char *adb_exe_local;
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
#ifndef __APPLE__
#include <usbmuxd.h>
#include <libimobiledevice/lockdown.h>
#endif

struct USBMux : DeviceDiscovery {
    const char* suffix = "USB";

#ifdef _WIN32
    char *usbmuxd_dll;
    char *idevice_dll;

    idevice_new_t  idevice_new;
    idevice_free_t  idevice_free;

    lockdownd_client_new_t       lockdownd_client_new;
    lockdownd_client_free_t      lockdownd_client_free;
    lockdownd_get_device_name_t  lockdownd_get_device_name;

    libusbmuxd_set_debug_level_t usbmuxd_set_debug_level;
    usbmuxd_get_device_list_t    usbmuxd_get_device_list;
    usbmuxd_device_list_free_t   usbmuxd_device_list_free;
    usbmuxd_connect_t            usbmuxd_connect;
    usbmuxd_disconnect_t         usbmuxd_disconnect;

    HMODULE hModuleIDevice;
    HMODULE hModuleUsbmux;
#else
    void* hModuleIDevice;
    void* hModuleUsbmux;
#endif

#ifdef __APPLE__
    MDNS* mdns;
    void* usbmuxd_device_list;
#else
    usbmuxd_device_info_t* usbmuxd_device_list;
#endif
    Proxy iproxy;

    USBMux();
    ~USBMux();
    void DoReload();
    void GetModel(Device* dev);
    socket_t Connect(Device* dev, int port, int* iproxy_port);
};
