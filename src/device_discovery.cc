/*
Copyright (C) 2022 DEV47APPS, github.com/dev47apps

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <dlfcn.h>
#include <assert.h>
#elif defined(TEST)
#include <cassert>
#endif

#include "net.h"
#include "command.h"
#include "device_discovery.h"
#include "plugin_properties.h"

bool process_check_success(process_t proc, const char *name) {
    if (proc == PROCESS_NONE) {
        return false;
    }
    bool rc = true;
    exit_code_t exit_code;
    if (!cmd_simple_wait(proc, &exit_code)) {
        if (exit_code != NO_EXIT_CODE) {
            elog("\"%s\" exit value %" PRIexitcode, name, exit_code);
        } else {
            elog("\"%s\" exited unexpectedly with %d", name, (int) exit_code);
        }
        rc = false;
    }

    CloseProcessHandle(proc);
    return rc;
}

// serialize argv to string "[arg1], [arg2], [arg3]"
bool
argv_to_string(const char *const *argv, char *buf, size_t bufsize) {
    bool truncated = false;
    size_t idx = 0;
    while (*argv && !truncated) {
        const char *arg = *argv;
        size_t len = strlen(arg);

        if (idx + len + 1 >= bufsize) {
            truncated = true;
            len = bufsize - idx - 2;
        }
        memcpy(&buf[idx], arg, len);
        idx += len;
        buf[idx++] = ' ';
        argv++;
    }
    if (idx > 0) idx--; // overwrite the last space
    buf[idx] = '\0';

    return truncated;
}

void
process_print_error(enum process_result err, const char *const argv[]) {
    char buf[256];
    switch (err) {
        case PROCESS_ERROR_GENERIC:
            argv_to_string(argv, buf, sizeof(buf));
            elog("failed to exec: %s", buf);
            break;
        case PROCESS_ERROR_MISSING_BINARY:
            argv_to_string(argv, buf, sizeof(buf));
            elog("command not found: %s", buf);
            break;
        case PROCESS_SUCCESS:
            break;
    }
}

void *reload_thread(void *data) {
    ((DeviceDiscovery*) data) -> Clear();
    ((DeviceDiscovery*) data) -> DoReload();
    return 0;
}

void DeviceDiscovery::Reload(void) {
    join();

    assert(rthr == 0);
    if (pthread_create(&pthr, NULL, reload_thread, this) != 0) {
        elog("Error creating reload thread");
        return;
    }
    rthr = 1;
}

void DeviceDiscovery::Clear(void) {
    for (int i = 0; i < DEVICES_LIMIT; i++) {
        if(deviceList[i]) delete deviceList[i];
        deviceList[i] = NULL;
    }
}

Device* DeviceDiscovery::NextDevice(void) {
    if (iter < DEVICES_LIMIT && deviceList[iter]) {
        Device* dev = deviceList[iter++];
        return dev;
    }

    return 0;
}

Device* DeviceDiscovery::GetDevice(const char* serial, size_t length) {
    for (int i = 0; i < DEVICES_LIMIT; i++) {
        if (deviceList[i] == NULL)
            break;

        if (strncmp(deviceList[i]->serial, serial, length) == 0) {
            iter = i;
            return deviceList[i];
        }
    }
    return NULL;
}

Device* DeviceDiscovery::AddDevice(const char* serial, size_t length) {
    if (GetDevice(serial, length)) {
        elog("warn: duplicate device");
        return NULL;
    }

    for (int i = 0; i < DEVICES_LIMIT; i++) {
        if (deviceList[i] == NULL) {
            deviceList[i] = new Device();
            memcpy(deviceList[i]->serial, serial, length);
            return deviceList[i];
        }
    }

    elog("warn: device list full");
    return NULL;
}

// adb commands
static const char *adb_exe = NULL;

process_t
adb_execute(const char *serial, const char *const adb_cmd[], size_t len, char *output, size_t out_size) {
    const char *cmd[32];
    int i = 0;
    process_t process;
    if (len > sizeof(cmd)-6) {
        elog("max 32 command args allowed");
        return PROCESS_NONE;
    }

    if (!adb_exe) {
        elog("adb exe not available");
        return PROCESS_NONE;
    }

    #ifdef __linux__
    if (FileExists("/.flatpak-info")) {
        cmd[i++] = "flatpak-spawn";
        cmd[i++] = "--host";
    }
    #endif

    cmd[i++] = adb_exe;
    if (serial) {
        cmd[i++] = "-s";
        cmd[i++] = serial;
    }

    memcpy(&cmd[i], adb_cmd, len * sizeof(const char *));
    cmd[len + i] = NULL;

    enum process_result r = cmd_execute(cmd[0], cmd, &process, output, out_size);
    process_print_error(r, cmd);
    return r == PROCESS_SUCCESS ? process : PROCESS_NONE;
}

AdbMgr::AdbMgr() {
    process_t proc;
    const char *version[] = {"version"};
    char buf[256];

    #ifdef TEST
    adb_exe_local = NULL;
    #elif defined(_WIN32)
    adb_exe_local = obs_module_file("adb/adb.exe");
    #else // __APPLE__, __linux__
    adb_exe_local = obs_module_file("adb");
    #endif

    const char *ADB_VARIANTS[] = {
        #ifdef TEST
        "build/adbz.exe",
        #else
        "adb",

        #ifdef __APPLE__
        "/opt/homebrew/bin/adb",
        #endif

        #ifndef _WIN32
        "/usr/local/bin/adb",
        "/usr/bin/adb",
        "/bin/adb",
        #endif

        adb_exe_local
        #endif // TEST
    };

    disabled = 1;

    #ifdef _WIN32
    if (SetEnvironmentVariableA("ADB_MDNS", "0") == 0) elog("warn: setenv failed");
    SetEnvironmentVariableA("ADB_MDNS_AUTO_CONNECT", "0");
    #else
    if (setenv("ADB_MDNS", "0", 1/* overwrite */) != 0) elog("warn: setenv failed");
    setenv("ADB_MDNS_AUTO_CONNECT", "0", 1);
    #endif

    for (size_t i = 0; i < ARRAY_LEN(ADB_VARIANTS); i++) {
        adb_exe = ADB_VARIANTS[i];
        if (!adb_exe)
            continue;

        ilog("trying '%s'", adb_exe);
        if (strncmp(adb_exe, "adb", 3) == 0 || FileExists(adb_exe)) {
            proc = adb_execute(NULL, version, ARRAY_LEN(version), buf, sizeof(buf));
            if (process_check_success(proc, "adb version")) {
                dlog("> %.*s", 52, buf);
                disabled = 0;
                break;
            }
        }
    }

    if (disabled) {
        elog("adb not found");
        ilog("PATH=%s", getenv("PATH"));
        return;
    }

    const char *ss[] = {"start-server"};
    proc = adb_execute(NULL, ss, ARRAY_LEN(ss), NULL, 0);
    process_check_success(proc, "adb start-server");
    ilog("adb found");
}

AdbMgr::~AdbMgr() {

#ifndef TEST
    if (adb_exe_local)
        bfree(adb_exe_local);

#endif

#if 0
    const char *ss[] = {"kill-server"};
    adb_execute(NULL, ss, ARRAY_LEN(ss), NULL, 0);
#endif
}

void AdbMgr::DoReload(void) {
    process_t proc;
    char buf[1024];

    if (disabled) // adb.exe was not found
        return;
#if 0
    const char *ro[] = {"reconnect", "offline"};
    proc = adb_execute(NULL, ro, ARRAY_LEN(ro), NULL, 0);
    if (!process_check_success(proc, "adb r.o.")) {
        elog("adb r.o. failed");
    }
#endif

    const char *dd[] = {"devices"};
    proc = adb_execute(NULL, dd, ARRAY_LEN(dd), buf, sizeof(buf));
    if (!process_check_success(proc, "adb devices")) {
        return;
    }

    size_t len;
    char *n, *sep;
    char *p = strtok_r(buf, "\n", &n);
    do {
        dlog("adb> %s", p);
        if (p[0] == 0) {
            break;
        }
        if (p[0] == '\r' || p[0] == '\n' || p[0] == ' ') {
            continue;
        }
        if (strstr(p, "* daemon") != NULL) {
            continue;
        }
        if (strstr(p, "List of") != NULL) {
            continue;
        }

        // eg. 00a3a5185d8ac3b1  device

        // Serial
        sep = strchr(p, ' ');
        if (!sep) {
            sep = strchr(p, '\t');
            if (!sep) break;
        }
        len = sep - p;
        if (len <= 0) continue;
        if (len > (sizeof(Device::serial)-1)) len = sizeof(Device::serial)-1;
        p[len] = 0;

        Device *dev = AddDevice(p, len);
        if (!dev) {
            break;
        }

        // whitespace
        p = sep + 1;
        while ((*p == ' ' || *p == '\t') && *p != '\r' && *p != '\n' && *p != '\0') { p++; }

        // state (offline | bootloader | device)
        sep = p;
        while (isalpha(*sep)){ sep++; }
        len = sep - p;
        if (len <= 0) continue;
        if (len > (sizeof(Device::state)-1)) len = sizeof(Device::state)-1;
        memset(dev->state, 0, sizeof(Device::state));
        memcpy(dev->state, p, len);

    } while ((p = strtok_r(NULL, "\n", &n)) != NULL);
    return;
}

void AdbMgr::GetModel(Device *dev) {
    char buf[1024] = {0};
    process_t proc;
    const char *ro[] = {"shell", "getprop", "ro.product.model"};
    proc = adb_execute(dev->serial, ro, ARRAY_LEN(ro), buf, sizeof(buf));
    if (process_check_success(proc, "adb get model")) {
        char *p = buf;
        char *end = buf + sizeof(Device::model) - strlen(suffix) - 6 - 8;
        while (p < end && (isalnum(*p) || *p == ' ' || *p == '-' || *p == '_')) p++;
        snprintf(dev->model, sizeof(Device::model), "%.*s [%s] (%.*s)",
            (int) (p - buf), buf, suffix, (int) sizeof(Device::serial)/2, dev->serial);
        dlog("model: %s", dev->model);
    }
}

bool AdbMgr::AddForward(Device *dev, int local_port, int remote_port) {
    char local[32];
    char remote[32];

    if (disabled) // adb.exe was not found
        return false;

    snprintf(local, 32, "tcp:%d", local_port);
    snprintf(remote, 32, "tcp:%d", remote_port);

    const char *serial = dev->serial;
    const char *const cmd[] = {"forward", local, remote};
    process_t proc = adb_execute(serial, cmd, ARRAY_LEN(cmd), NULL, 0);
    return process_check_success(proc, "adb fwd");
}

// the 'adb forward --remove' command is device agnostic
void AdbMgr::ClearForwards(int port_start, int port_last) {
    if (disabled) // adb.exe was not found
        return;

    char local[32] = {0};
    const char *const cmd[] = {"forward", "--remove", local};

    for (int port = port_start; port <= port_last; port++) {
        snprintf(local, 32, "tcp:%d", port);
        process_t proc = adb_execute(NULL, cmd, ARRAY_LEN(cmd), NULL, 0);
        process_check_success(proc, "adb fwd remove");
    }
    return;
}

// MARK: USBMUX

USBMux::USBMux() : iproxy(this) {
    hModuleUsbmux = NULL;
    hModuleIDevice = NULL;
#ifdef _WIN32
    idevice_dll = NULL;
    usbmuxd_dll = NULL;
#endif
    usbmuxd_device_list = NULL;

#ifdef TEST

#elif defined(_WIN32)
    const char *USBMUXD_VARIANTS[] = {
        "libusbmuxd-2.0.dll",
        "usbmuxd.dll",
    };
    const char *IMOBILEDEV_VARIANTS[] = {
        "libimobiledevice-1.0.dll",
        "imobiledevice.dll",
    };

    SetDllDirectory(obs_get_module_data_path((obs_current_module())));

    for (size_t i = 0; i < ARRAY_LEN(IMOBILEDEV_VARIANTS); i++) {
        FreeAll();
        dlog("trying %s", IMOBILEDEV_VARIANTS[i]);

        usbmuxd_dll = obs_module_file(USBMUXD_VARIANTS[i]);
        idevice_dll = obs_module_file(IMOBILEDEV_VARIANTS[i]);

        if (!idevice_dll || !usbmuxd_dll)
            continue;

        hModuleIDevice = LoadLibrary(idevice_dll);
        if (!hModuleIDevice) {
            ilog("LoadLibrary failed for %s, errno=%d", IMOBILEDEV_VARIANTS[i], GetLastError());
            continue;
        }

        hModuleUsbmux = LoadLibrary(usbmuxd_dll);
        if (!hModuleUsbmux) {
            elog("LoadLibrary failed for %s, errno=%d", USBMUXD_VARIANTS[i], GetLastError());
            continue;
        }

        break;
    }

    if (!(hModuleUsbmux && hModuleIDevice)) {
        elog("iOS USB support not available");
        return;
    }

    idevice_new  = (idevice_new_t ) GetProcAddress(hModuleIDevice, "idevice_new");
    idevice_free = (idevice_free_t) GetProcAddress(hModuleIDevice, "idevice_free");

    lockdownd_client_new      = (lockdownd_client_new_t     ) GetProcAddress(hModuleIDevice, "lockdownd_client_new");
    lockdownd_client_free     = (lockdownd_client_free_t    ) GetProcAddress(hModuleIDevice, "lockdownd_client_free");
    lockdownd_get_device_name = (lockdownd_get_device_name_t) GetProcAddress(hModuleIDevice, "lockdownd_get_device_name");

    usbmuxd_set_debug_level  = (libusbmuxd_set_debug_level_t) GetProcAddress(hModuleUsbmux, "libusbmuxd_set_debug_level");
    usbmuxd_get_device_list  = (usbmuxd_get_device_list_t   ) GetProcAddress(hModuleUsbmux, "usbmuxd_get_device_list");
    usbmuxd_device_list_free = (usbmuxd_device_list_free_t  ) GetProcAddress(hModuleUsbmux, "usbmuxd_device_list_free");
    usbmuxd_connect          = (usbmuxd_connect_t           ) GetProcAddress(hModuleUsbmux, "usbmuxd_connect");
    usbmuxd_disconnect       = (usbmuxd_disconnect_t        ) GetProcAddress(hModuleUsbmux, "usbmuxd_disconnect");

    #ifdef DEBUG
    usbmuxd_set_debug_level(9);
    #endif
    libusbmuxd_version_t usbmuxd_version = (libusbmuxd_version_t)GetProcAddress(hModuleUsbmux, "libusbmuxd_version");
    if (usbmuxd_version)
    ilog("usbmuxd version %.*s loaded", 8, usbmuxd_version());

#elif defined(__linux__)

    #ifdef ALLOW_STATIC
    hModuleUsbmux = (void*)1;
    #else
    // Check for usbmuxd presence
    const char *USBMUXD_VARIANTS[] = {
        "libusbmuxd.so",
        "libusbmuxd.so.4",
        "libusbmuxd.so.6",
        "libusbmuxd-2.0.so",
        "libusbmuxd-2.0.so.6",
    };

    for (size_t i = 0; i < ARRAY_LEN(USBMUXD_VARIANTS); i++) {
        dlog("trying %s", USBMUXD_VARIANTS[i]);
        if ((hModuleUsbmux = dlopen(USBMUXD_VARIANTS[i], RTLD_LAZY)) != NULL)
            break;
    }

    if (!hModuleUsbmux) {
        elog("usbmuxd not found, iOS USB support not available");
        return;
    }
    #endif

    #ifdef DEBUG
    libusbmuxd_set_debug_level(9);
    #endif
#endif

#ifdef __APPLE__

    /*
     * The "correct" approach is to use usbmuxd just like Windows and Linux.
     * However, the macOS + iOS tether interface provides a very easy and an
     * ultra-fast usb connection option.
     * Using usbmuxd is less desirable as (a) it adds an extra hop and extra bloat,
     * (b) universal libraries would need to be packaged and distributed.
     * On the other hand, leveraging mdns to locate and connect iOS devices is a bit
     * of a hack, but it works (arguably better) with no additional dependencies.
     */
    mdns = new MDNS();
    mdns->suffix = "USB";
    mdns->networkPrefix = 0xfea9; // 169.254/16
    return;
#endif
}

USBMux::~USBMux() {
#ifdef __APPLE__
    delete mdns;

#else // _WIN32 || _Linux

    if (usbmuxd_device_list) {
        usbmuxd_device_list_free(&usbmuxd_device_list);
    }

#ifdef TEST
    //

#elif defined(_WIN32)
    FreeAll();

#elif defined(__linux__)

    #ifdef ALLOW_STATIC
    hModuleUsbmux = 0;
    #else
    if (hModuleUsbmux)
        dlclose(hModuleUsbmux);

    #endif

#endif

#endif // __APPLE__
}

#if defined(_WIN32)
void USBMux::FreeAll() {
    if (hModuleIDevice) {
        FreeLibrary(hModuleIDevice);
        hModuleIDevice = NULL;
    }

    if (hModuleUsbmux) {
        FreeLibrary(hModuleUsbmux);
        hModuleUsbmux = NULL;
    }

    if (idevice_dll) {
        bfree(idevice_dll);
        idevice_dll = NULL;
    }

    if (usbmuxd_dll) {
        bfree(usbmuxd_dll);
        usbmuxd_dll = NULL;
    }
}
#endif

void USBMux::GetModel(Device* dev) {
#ifdef __APPLE__
    return;

#else // _WIN32 || _Linux
    if (!hModuleUsbmux)
        return;

    idevice_t device = NULL;
    char *udid = dev->serial;
    if (idevice_new(&device, udid) != IDEVICE_E_SUCCESS) {
        elog("Unable to get idevice_t for %s", udid);
        return;
    }

    lockdownd_client_t lockdown = NULL;
    lockdownd_error_t lerr = lockdownd_client_new(device, &lockdown, "droidcam-obs-plugin");
    if (lerr != LOCKDOWN_E_SUCCESS) {
        idevice_free(device);
        elog("Could not connect lockdown, error code %d\n", lerr);
        return;
    }

    char* name = NULL;
    lerr = lockdownd_get_device_name(lockdown, &name);
    if (name) {
        // XXX: skip the serial with iPhones
        #if 0
        int max = (int)(sizeof(Device::model) - strlen(suffix) - 6 - 8);
        snprintf(dev->model, sizeof(Device::model), "%.*s [%s] (%.*s)",
            max, name, suffix, (int) sizeof(Device::serial)/2, dev->serial);
        #else
        int max = (int)(sizeof(Device::model) - strlen(suffix) - 4);
        snprintf(dev->model, sizeof(Device::model), "%.*s [%s]",
            max, name, suffix);
        #endif
        free(name);
    }
    else {
        elog("Could not get device name, lockdown error %d\n", lerr);
    }
    lockdownd_client_free(lockdown);
    idevice_free(device);
#endif // __APPLE__
}

void USBMux::DoReload(void) {
#ifdef __APPLE__
    reload_thread(mdns);

    int i = 0;
    Device *idev;
    mdns->ResetIter();
    while ((idev = mdns->NextDevice()) != NULL) {
        // Edit the serial to avoid clashes with the same device
        // being available over the default (wifi) interface
        const char* text = "_usb";
        const size_t max = sizeof(Device::serial) - strlen(text) - 2;
        idev->serial[max] = 0;
        strcat(idev->serial, text);

        // Add device to the local (USBMux) list
        Device *dev = AddDevice(idev->serial, sizeof(Device::serial));
        if (!dev) {
            break;
        }

        memcpy(dev->model, idev->model, sizeof(Device::model));
        memcpy(dev->address, idev->address, sizeof(Device::address));
    }

    ilog("Apple USB: found %d devices", i);
    return;

#else // _WIN32 || _Linux
    if (!hModuleUsbmux)
        return;

    if (usbmuxd_device_list)
        usbmuxd_device_list_free(&usbmuxd_device_list);

    int deviceCount = usbmuxd_get_device_list(&usbmuxd_device_list);

    if (deviceCount < 0) {
        const char* hint =
        #ifdef TEST
            "";
        #elif defined(_WIN32)
            "AMDS missing?";
        #elif defined(__linux__)
            "Is usbmuxd running?";
        #endif

        elog("Could not get iOS device list. %s. error=%d",
            hint, deviceCount);
        return;
    }

    ilog("USBMux: found %d devices", deviceCount);
    for (int i = 0; i < deviceCount; i++) {
        usbmuxd_device_info_t *idev = &usbmuxd_device_list[i];
        if (idev == NULL || idev->handle == 0) {
            continue;
        }

        assert(sizeof(usbmuxd_device_info_t::udid) < sizeof(Device::serial));
        Device *dev = AddDevice(idev->udid, sizeof(usbmuxd_device_info_t::udid));
        if (!dev) {
            break;
        }

        dev->handle = (int) idev->handle;
    }

#endif // __APPLE__
}

socket_t USBMux::Connect(Device* dev, int port, int* iproxy_port) {
    dlog("USBMUX Connect: handle=%d, port=%d", dev->handle, port);

#ifdef __APPLE__
    return net_connect(dev->address, port);

#else
    if (!hModuleUsbmux)
        return INVALID_SOCKET;

    int rc = usbmuxd_connect((uint32_t) dev->handle, (short) port);
    if (rc <= 0) {
        elog("usbmuxd_connect failed: %d", rc);
        return INVALID_SOCKET;
    }

    set_nonblock(rc, 0);
    set_recv_timeout(rc, 5);

    *iproxy_port = iproxy.Start(dev, port);

    return rc;

#endif // __APPLE__
}
