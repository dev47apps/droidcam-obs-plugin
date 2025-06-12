// Copyright (C) 2021 DEV47APPS, github.com/dev47apps
#pragma once

#ifdef _WIN32
typedef idevice_error_t (*idevice_new_t)(idevice_t*, const char*);
typedef idevice_error_t (*idevice_free_t)(idevice_t);

typedef lockdownd_error_t (*lockdownd_client_new_t)(idevice_t, lockdownd_client_t*, const char*);
typedef lockdownd_error_t (*lockdownd_client_free_t)(lockdownd_client_t);
typedef lockdownd_error_t (*lockdownd_get_device_name_t)(lockdownd_client_t, char **);

typedef int (*usbmuxd_get_device_list_t)(usbmuxd_device_info_t **);
typedef int (*usbmuxd_device_list_free_t)(usbmuxd_device_info_t **);
typedef int (*usbmuxd_connect_t)(const uint32_t, const unsigned short);
typedef int (*usbmuxd_disconnect_t)(int);
typedef void (*libusbmuxd_set_debug_level_t) (int);
typedef const char* (*libusbmuxd_version_t) (void);
#endif
