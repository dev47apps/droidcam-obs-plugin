/*
Copyright (C) 2023 DEV47APPS, github.com/dev47apps

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
extern "C" {
#include <libavcodec/avcodec.h>
}

#if DROIDCAM_OVERRIDE
#define ENABLE_GUI 1
#endif

#if ENABLE_GUI
#include <QAction>
#include <QMainWindow>
#include <QMessageBox>
#include "obs-frontend-api.h"
QMainWindow *main_window = NULL;

#if DROIDCAM_OVERRIDE
#include "AddDevice.h"
AddDevice* addDevUI = NULL;
#endif

#endif /* ENABLE_GUI */

#include "plugin.h"
#include "source.h"
#include "plugin_properties.h"

#if DROIDCAM_OVERRIDE
static const char *droidcam_signals[] = {
    "void droidcam_connect(ptr source)",
    "void droidcam_disconnect(ptr source)",
    NULL,
};

void droidcam_signal(obs_source_t* source, const char* signal) {
    calldata_t cd;
    calldata_init(&cd);
    calldata_set_ptr(&cd, "source", source);
    signal_handler_signal(obs_get_signal_handler(), signal, &cd);
    calldata_free(&cd);
}
#endif


static const char *plugin_getname(void *data) {
    UNUSED_PARAMETER(data);
    #if DROIDCAM_OVERRIDE
    return "DroidCam";
    #else
    return obs_module_text("DroidCamOBS");
    #endif
}


char os_name_version[64];
struct obs_source_info droidcam_obs_info;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("droidcam-obs", "en-US")
MODULE_EXPORT const char *obs_module_description(void) {
    return "Android and iOS camera source";
}

bool obs_module_load(void) {
    memset(os_name_version, 0, sizeof(os_name_version));
    memset(&droidcam_obs_info, 0, sizeof(struct obs_source_info));

    if (AV_VERSION_MAJOR(avcodec_version()) > LIBAVCODEC_VERSION_MAJOR) {
        elog("droidcam_obs: libavcodec version %u is too high (<= %d required for this release).",
            AV_VERSION_MAJOR(avcodec_version()), LIBAVCODEC_VERSION_MAJOR);
        return false;
    }

    droidcam_obs_info.id           = "droidcam_obs";
    droidcam_obs_info.type         = OBS_SOURCE_TYPE_INPUT;
    droidcam_obs_info.output_flags = OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_AUDIO | OBS_SOURCE_ASYNC_VIDEO;
    droidcam_obs_info.get_name     = plugin_getname;
    droidcam_obs_info.create       = source_create;
    droidcam_obs_info.destroy      = source_destroy;
    droidcam_obs_info.show         = source_show;
    droidcam_obs_info.hide         = source_hide;
    droidcam_obs_info.update       = source_update;
    #if DROIDCAM_OVERRIDE
    droidcam_obs_info.icon_type    = OBS_ICON_TYPE_CAMERA;
    #else
    droidcam_obs_info.icon_type    = OBS_ICON_TYPE_CUSTOM;
    #endif
    droidcam_obs_info.get_defaults = source_defaults;
    droidcam_obs_info.get_properties = source_properties;
    obs_register_source(&droidcam_obs_info);

    #if DROIDCAM_OVERRIDE
    signal_handler_add_array(obs_get_signal_handler(), droidcam_signals);
    #endif

    #if ENABLE_GUI
    main_window = (QMainWindow *)obs_frontend_get_main_window();
    #endif

    #if DROIDCAM_OVERRIDE
    obs_frontend_push_ui_translation(obs_module_get_string);
    addDevUI = new AddDevice(main_window);
    obs_frontend_pop_ui_translation();

    QAction *tools_menu_action = (QAction*)obs_frontend_add_tools_menu_qaction("DroidCam");
    tools_menu_action->connect(tools_menu_action, &QAction::triggered, [] () {
        addDevUI->ShowHideDevicePicker(1);
    });
    #endif

    get_os_name_version(os_name_version, sizeof(os_name_version));
    return true;
}

void obs_module_unload(void) {
}
