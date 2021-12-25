/*
Copyright (C) 2021 DEV47APPS, github.com/dev47apps

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
#ifdef __linux__
#include <dlfcn.h>
#endif

#include "net.h"
#include "command.h"
#include "usb_util.h"

bool process_check_success(process_t proc, const char *name) {
    if (proc == PROCESS_NONE) {
        elog("Could not execute \"%s\"", name);
        return false;
    }
    exit_code_t exit_code;
    if (!cmd_simple_wait(proc, &exit_code)) {
        if (exit_code != NO_EXIT_CODE) {
            elog("\"%s\" exit value %" PRIexitcode, name, exit_code);
        } else {
            elog("\"%s\" exited unexpectedly with %d", name, (int) exit_code);
        }
        return false;
    }
    return true;
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

// adb commands
static const char *adb_exe =
#ifdef _WIN32
    PLUGIN_DATA_DIR "\\adb\\adb.exe";
#else
    #ifdef __APPLE__
    "/usr/local/bin/adb";
    #else
    "adb";
    #endif
#endif

process_t
adb_execute(const char *serial, const char *const adb_cmd[], size_t len, char *output, size_t out_size) {
    const char *cmd[32];
    int i;
    process_t process;
    if (len > sizeof(cmd)-4) {
        elog("max 32 command args allowed");
        return PROCESS_NONE;
    }
    cmd[0] = adb_exe;
    if (serial) {
        cmd[1] = "-s";
        cmd[2] = serial;
        i = 3;
    } else {
        i = 1;
    }

    memcpy(&cmd[i], adb_cmd, len * sizeof(const char *));
    cmd[len + i] = NULL;

    enum process_result r = cmd_execute(cmd[0], cmd, &process, output, out_size);
    if (r != PROCESS_SUCCESS) {
        process_print_error(r, cmd);
        return PROCESS_NONE;
    }
    return process;
}

AdbMgr::AdbMgr() {
    int i = 0;
    for (; i < DEVICES_LIMIT; i++) deviceList[i] = NULL;

#if defined(_WIN32) || defined(__APPLE__)
   if (!FileExists(adb_exe)) {

#else
    if (system("adb version > /dev/null 2>&1")) {
        ilog("PATH=%s", getenv("PATH"));

#endif
        elog("%s not found", adb_exe);
        disabled = 1;
        return;
    }
    disabled = 0;

    const char *ss[] = {"start-server"};
    process_t proc = adb_execute(NULL, ss, ARRAY_LEN(ss), NULL, 0);
    process_check_success(proc, "adb start-server");
}

AdbMgr::~AdbMgr() {
    int i = 0;
#if 0
    const char *ss[] = {"kill-server"};
    adb_execute(NULL, ss, ARRAY_LEN(ss), NULL, 0);
#endif
    for (; i < DEVICES_LIMIT; i++) if(deviceList[i]) delete deviceList[i];
}

bool AdbMgr::Reload(void) {
    char buf[1024];
    AdbDevice dev;
    process_t proc;
    if (disabled) // adb.exe was not found
        return false;
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
        return false;
    }

    size_t i = 0, len;
    char *n, *sep;
    char *p = strtok_r(buf, "\n", &n);
    do {
        dlog(": %s", p);
        if (p[0] == ' ' || p[0] == 0) {
            continue;
        }
        if (strstr(p, "* daemon") != NULL) {
            continue;
        }
        if (strstr(p, "List of") != NULL) {
            continue;
        }

        memset(dev.serial, 0, sizeof(dev.serial));
        memset(dev.state, 0, sizeof(dev.state));

        // eg. 00a3a5185d8ac3b1  device
        sep = strchr(p, ' ');
        if (!sep) {
            sep = strchr(p, '\t');
            if (!sep) break;
        }
        len = sep - p;
        if (len <= 0) continue;
        if (len > (sizeof(dev.serial)-1)) len = sizeof(dev.serial)-1;
        p[len] = 0;
        memcpy(dev.serial, p, len);

        p = sep + 1;
        while (*p != '\r' && *p != '\0' && (*p == ' ' || *p == '\t')) { p++; }
        sep = strchr(p, '\0');
        if (!sep) break;
        len = sep - p;
        if (len <= 0) continue;
        if (len > (sizeof(dev.state)-1)) len = sizeof(dev.state)-1;
        memcpy(dev.state, p, len);

        if (deviceList[i] == NULL)
            deviceList[i] = new AdbDevice();

        memcpy(deviceList[i]->serial, dev.serial, sizeof(dev.serial));
        memcpy(deviceList[i]->state, dev.state, sizeof(dev.state));
        memset(deviceList[i]->model, 0, sizeof(dev.model));
        if (++i == DEVICES_LIMIT) break;
    } while ((p = strtok_r(NULL, "\n", &n)) != NULL);

    for (; i < DEVICES_LIMIT; i++) if(deviceList[i]){ delete deviceList[i]; deviceList[i]=0; }
    return true;
}

static void GetModel(AdbDevice *dev) {
    char buf[1024] = {0};
    process_t proc;
    const char *ro[] = {"shell", "getprop", "ro.product.model"};
    proc = adb_execute(dev->serial, ro, ARRAY_LEN(ro), buf, sizeof(buf));
    if (process_check_success(proc, "adb get model")) {
        char *p = buf;
        char *end = buf + sizeof(dev->model) - 2;
        while (p < end && (isalnum(*p) || *p == ' ' || *p == '-' || *p == '_')) p++;
        snprintf(dev->model, sizeof(dev->model), "%.*s (%.*s)", (int) (p - buf), buf, (int) sizeof(dev->serial)/2, dev->serial);
        dlog("model: %s", dev->model);
    }
}

AdbDevice* AdbMgr::NextDevice(int *is_offline, int get_name) {
    #define STATE_DEVICE "device"
    const char* device = STATE_DEVICE;

    if (iter >= DEVICES_LIMIT) iter = 0;

    if (deviceList[iter]) {
        ilog("device %s is %s", deviceList[iter]->serial, deviceList[iter]->state);
        if (memcmp(device, deviceList[iter]->state, sizeof(STATE_DEVICE)-1) == 0) {
            *is_offline = 0;
        } else {
            *is_offline = 1;
        }

        AdbDevice* dev = deviceList[iter];
        iter++;
        if (get_name && *is_offline == 0) GetModel(dev);
        return dev;
    }
    return 0;
}

bool
AdbMgr::AddForward(const char *serial, int local_port, int remote_port) {
    char local[32];
    char remote[32];

    if (disabled) // adb.exe was not found
        return false;

    snprintf(local, 32, "tcp:%d", local_port);
    snprintf(remote, 32, "tcp:%d", remote_port);

    const char *const cmd[] = {"forward", local, remote};
    process_t proc = adb_execute(serial, cmd, ARRAY_LEN(cmd), NULL, 0);
    return process_check_success(proc, "adb fwd");
}

void AdbMgr::ClearForwards(const char *serial) {
    if (disabled) // adb.exe was not found
        return;

    const char *const cmd[] = {"forward", "--remove-all"};
    process_t proc = adb_execute(serial, cmd, ARRAY_LEN(cmd), NULL, 0);
    process_check_success(proc, "adb fwd clear");
    return;
}

// MARK: USBMUX

USBMux::USBMux() {
    const char *errmsg = "Error loading usbmuxd dll, iOS USB support n/a";
    deviceList = NULL;
    hModule = NULL;

#ifdef _WIN32
    const char *usbmuxd_dll = PLUGIN_DATA_DIR PATH_SEPARATOR "usbmuxd.dll";
    if (!FileExists(usbmuxd_dll)) {
        elog("iOS USB support not available");
        return;
    }

    SetDllDirectory(PLUGIN_DATA_DIR);
    hModule = LoadLibrary(usbmuxd_dll);
    if (!hModule) {
        elog("%s", errmsg);
        return;
    }

    usbmuxd_set_debug_level  = (libusbmuxd_set_debug_level_t) GetProcAddress(hModule, "libusbmuxd_set_debug_level");
    usbmuxd_get_device_list  = (usbmuxd_get_device_list_t   ) GetProcAddress(hModule, "usbmuxd_get_device_list");
    usbmuxd_device_list_free = (usbmuxd_device_list_free_t  ) GetProcAddress(hModule, "usbmuxd_device_list_free");
    usbmuxd_connect          = (usbmuxd_connect_t           ) GetProcAddress(hModule, "usbmuxd_connect");
    usbmuxd_disconnect       = (usbmuxd_disconnect_t        ) GetProcAddress(hModule, "usbmuxd_disconnect");
    #ifdef DEBUG
    usbmuxd_set_debug_level(9);
    #endif
#endif

#ifdef __linux__
    hModule = dlopen("libusbmuxd.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!hModule) {
        elog("%s", errmsg);
        return;
    }
    #ifdef DEBUG
    libusbmuxd_set_debug_level(9);
    #endif
#endif

#ifdef __APPLE__
    (void) errmsg;
    return;
#endif
}

USBMux::~USBMux() {
#ifdef __APPLE__
    return;

#else // Not __APPLE__

    if (deviceList) usbmuxd_device_list_free(&deviceList);

#ifdef _WIN32
    if (hModule) FreeLibrary(hModule);
#endif

#ifdef __linux__
    if (hModule) dlclose(hModule);
#endif

#endif // __APPLE__
}

int USBMux::Reload(void) {
#ifdef __APPLE__
    deviceCount = 0;
    return 0;

#else // _WIN32 || _Linux
    if (!hModule) {
        deviceCount = 0;
        return 0;
    }

    if (deviceList) usbmuxd_device_list_free(&deviceList);
    deviceCount = usbmuxd_get_device_list(&deviceList);
    ilog("USBMux: Reload: %d devices", deviceCount);
    if (deviceCount < 0) {
        elog("Could not get iOS device list, usbmuxd not running?");
        deviceCount = 0;
        return 0;
    }
    return 1;
#endif // __APPLE__
}

usbmuxd_device_info_t* USBMux::NextDevice(void) {
    usbmuxd_device_info_t* device;
    if (iter < deviceCount) {
        device = &deviceList[iter++];
        if (device && device->handle)
            return device;
    }
    return 0;
}

int USBMux::Connect(int device, int port) {
#ifdef __APPLE__
    return INVALID_SOCKET;
#else

    int rc;
    dlog("USBMUX Connect: dev=%d/%d, port=%d", device, deviceCount, port);

    if (!hModule) {
        elog("USBMUX dll not loaded");
        goto out;
    }

    if (device < deviceCount) {
        rc = usbmuxd_connect(deviceList[device].handle, (short) port);
        if (rc <= 0) {
            elog("usbmuxd_connect failed: %d", rc);
            goto out;
        }
        return rc;
    }
out:
    return INVALID_SOCKET;
#endif // __APPLE__
}
