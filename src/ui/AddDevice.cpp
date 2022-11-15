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
#include <stdlib.h>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtGui/QFont>
#include <QtSvg/QSvgRenderer>
#include "AddDevice.h"

#include "plugin.h"
#include "plugin_properties.h"
#include "obs-frontend-api.h"

const char *DROIDCAM_OBS_ID = "droidcam_obs";

inline static void retainSize(QWidget *item) {
    QSizePolicy sp = item->sizePolicy(); sp.setRetainSizeWhenHidden(true); item->setSizePolicy(sp);
}

AddDevice::AddDevice(QWidget *parent) : QDialog(parent),
    loadingSvg(this),
    ui(new Ui_AddDeviceDC)
{
    char *file = obs_module_file("loading.svg");
    if (file) {
        QString path(file);
        loadingSvg.load(path);
        bfree(file);
    }
    loadingSvg.renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
    loadingSvg.renderer()->blockSignals(true);
    loadingSvg.setVisible(false);

    file = obs_module_file("smartphone.svg");
    if (file) {
        phoneIcon.addFile(file, QSize(96, 96));
        bfree(file);
    }

    ui->setupUi(this);
    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::WindowContextHelpButtonHint;
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    setWindowFlags(flags);
    retainSize(ui->refresh_button);
    retainSize(ui->addDevice_button);

    ui->addDevice_label->setStyleSheet("font-size: 14pt;");
    ui->addDevice_button->setVisible(false);
    ui->refresh_button->setVisible(false);
    ui->horizontalLayout1->addWidget(&loadingSvg);

    refresh_count = 0;
    enable_audio = false;
    ui->enableAudio_checkBox->connect(ui->enableAudio_checkBox,
        &QCheckBox::stateChanged, [=] (int state) {
            enable_audio = state == Qt::Checked;
        });

    ui->deviceList_widget->connect(ui->deviceList_widget,
        &QListWidget::itemSelectionChanged, [=]() {
            int active = ui->deviceList_widget->currentRow();
            ui->addDevice_button->setVisible(active >= 0);
        });

    ui->deviceList_widget->connect(ui->deviceList_widget,
        &QListWidget::itemActivated, [=](QListWidgetItem *item) {
            if (item) CreateNewSource(item);
        });

    ui->addDevice_button->connect(ui->addDevice_button,
        &QPushButton::clicked, [=]() {
            QListWidgetItem *item = ui->deviceList_widget->currentItem();
            if (item) CreateNewSource(item);
        });

    connect(ui->refresh_button, SIGNAL(clicked()), this, SLOT(ReloadList()));

    const char *name = "dummy_droidcam_source";
    dummy_droidcam_source = obs_get_source_by_name(name);
    if (!dummy_droidcam_source) {
        obs_data_t *settings = obs_data_create();
        obs_data_set_bool(settings, OPT_DUMMY_SOURCE, true);

        dummy_droidcam_source = obs_source_create_private("droidcam_obs", name, settings);
        dummy_properties = obs_source_properties((obs_source_t *) dummy_droidcam_source);
        obs_data_release(settings);
    }
}

AddDevice::~AddDevice() {
    if (thread) thread->wait();

    ClearList();
    obs_properties_destroy((obs_properties_t *)dummy_properties);
    obs_source_release((obs_source_t *) dummy_droidcam_source);
}

void AddDevice::ShowHideDialog(int show) {
    refresh_count = 0;

    if (show < 0) {
        show = !isVisible();
    }

    if (show) {
        if (!isVisible()) {
            setVisible(true);
            ReloadList();
        }
    } else {
        setVisible(false);
    }
}

void AddDevice::AddDeviceManual() {
    QDialog dialog(this, Qt::WindowTitleHint);
    QVBoxLayout form(&dialog);
    QLineEdit nameInput(&dialog);
    QLineEdit wifiInput(&dialog);

    QString label0 = QString(obs_module_text("Device"));
    form.addWidget(new QLabel(label0));
    form.addWidget(&nameInput);

    form.addWidget(new QLabel("WiFi IP"));
    form.addWidget(&wifiInput);

    QString enableAudio_label = QString(obs_module_text("EnableAudio"));
    QString enableAudio_hint = QString(obs_module_text("EnableAudioHint"));

    QCheckBox enableAudio_checkBox(enableAudio_label);
    enableAudio_checkBox.setCursor(Qt::WhatsThisCursor);
    enableAudio_checkBox.setToolTip(enableAudio_hint);
    enableAudio_checkBox.setWhatsThis(enableAudio_hint);

    form.addWidget(&enableAudio_checkBox);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                               Qt::Horizontal, &dialog);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
    form.addSpacing(16);
    form.addWidget(&buttonBox);

    dialog.setWindowTitle(QString(obs_module_text("AddADevice")));
    if (dialog.exec() != QDialog::Accepted) {
        refresh_count --;
        return;
    }

    const char *err = nullptr;
    if (nameInput.text().isEmpty())
        err = obs_module_text("NoName");

    else if (wifiInput.text().isEmpty())
        err = obs_module_text("NoWifiIP");

    if (err) {
        QString title = QString(obs_module_text("DroidCam"));
        QString msg = QString(err);
        QMessageBox mb(QMessageBox::Warning, title, msg,
             QMessageBox::StandardButtons(QMessageBox::Ok), this);
        mb.exec();
        AddDeviceManual();
        return;
    }

    QByteArray wifiIP = wifiInput.text().toUtf8();
    auto info = (DeviceInfo *) bzalloc(sizeof(DeviceInfo));
    info->id = opt_use_wifi;
    info->ip = wifiIP.constData();
    info->port = DEFAULT_PORT;
    info->type = DeviceType::WIFI;
    enable_audio = enableAudio_checkBox.checkState() == Qt::Checked;

    // AddListEntry(name, info); <- alternate flow
    QListWidgetItem item(phoneIcon, nameInput.text());
    item.setData(Qt::UserRole, QVariant::fromValue((void*)info));
    CreateNewSource(&item);

    bfree(info);
    enable_audio = ui->enableAudio_checkBox->checkState() == Qt::Checked;
}

void AddDevice::AddListEntry(const char *name, void* data) {
    if (!isVisible())
        return;

    QListWidgetItem *item = new QListWidgetItem(phoneIcon, name, ui->deviceList_widget);
    item->setData(Qt::UserRole, QVariant::fromValue(data));
    QFont font = item->font();
    font.setPointSize(14);
    item->setFont(font);
}

void AddDevice::ClearList() {
    while (ui->deviceList_widget->count() > 0) {
        QListWidgetItem *item = ui->deviceList_widget->takeItem(0);
        void *data = item->data(Qt::UserRole).value<void *>();
        if (data) bfree(data);
        delete item;
    }
}

void AddDevice::ReloadList() {
    if (refresh_count > 2) {
        AddDeviceManual();
        return;
    }

    if (dummy_droidcam_source && dummy_properties) {
        if (thread && thread->isRunning())
            return;

        thread.reset(new ReloadThread(this));
        connect((ReloadThread*) thread.get(), &ReloadThread::AddListEntry, this, &AddDevice::AddListEntry);
        connect((ReloadThread*) thread.get(), &ReloadThread::finished, this, &AddDevice::ReloadFinish);

        loadingSvg.setVisible(true);
        loadingSvg.renderer()->blockSignals(false);
        ui->refresh_button->setVisible(false);
        ClearList();
        thread->start();
        refresh_count ++;
    }
    else {
        elog("AddDevice UI: Trying to reload device list without dummy source: '%p' '%p'",
            dummy_droidcam_source, dummy_properties);
    }

    return;
}

void ReloadThread::run() {
    auto ppts = (obs_properties_t *) parent->dummy_properties;
    obs_property_t *p = obs_properties_get(ppts, OPT_REFRESH);
    parent->dummy_source_priv_data = NULL;

    if (p && obs_property_button_clicked(p, parent->dummy_droidcam_source)
            && parent->dummy_source_priv_data /* set in the refresh handler */)
    {
        obs_property_t *list = obs_properties_get(ppts, OPT_DEVICE_LIST);
        for (size_t i = 0; i < obs_property_list_item_count(list); i++) {
            const char *name = obs_property_list_item_name(list, i);
            const char *value = obs_property_list_item_string(list, i);
            if (!(name && value))
                continue;

            if (strncmp(value, opt_use_wifi, strlen(opt_use_wifi)) == 0)
                continue;

            auto info = (DeviceInfo *) bzalloc(sizeof(DeviceInfo));
            info->id = value;
            info->port = DEFAULT_PORT;
            info->ip = "";
            resolve_device_type(info, parent->dummy_source_priv_data);
            if (info->type != DeviceType::NONE && parent->isVisible())
                emit AddListEntry(name, info);
        }
    }
}


void AddDevice::ReloadFinish() {
    if (isVisible()) {
        loadingSvg.renderer()->blockSignals(true);
        loadingSvg.setVisible(false);
        ui->refresh_button->setVisible(true);
    }
}

static bool removeSource(obs_source_t *source) {
    obs_source_t *scene = obs_frontend_get_current_scene();
    struct io {
        obs_source_t *source;
        obs_sceneitem_t *scene_item;
    } io { source, NULL };
    bool result = false;

    obs_scene_enum_items(obs_scene_from_source(scene),
        [](obs_scene_t *, obs_sceneitem_t *item, void* data) -> bool {
            auto io = (struct io*) data;
            if (obs_sceneitem_get_source(item) == io->source) {
                io->scene_item = item;
                return false;
            }
            return true;
        },
        &io);

    if (io.scene_item) {
        obs_sceneitem_remove(io.scene_item);
        result = true;
    }

    obs_source_release(scene);
    return result;
}

void AddDevice::CreateNewSource(QListWidgetItem *item) {
    char resolution[16];
    obs_video_info ovi;
    void *data = item->data(Qt::UserRole).value<void*>();

    QByteArray text = item->text().toUtf8();
    const char* device_name = text.constData();
    ilog("want to add: %s", device_name);

    obs_enum_sources([](void *data, obs_source_t *source) -> bool {
        const char *sid = obs_source_get_id(source);
        const char *name = obs_source_get_name(source);
        if (data && sid && name && strcmp(sid, DROIDCAM_OBS_ID) == 0) {
            auto item = (QListWidgetItem*) data;
            if (item->text() == QString(name)) {
                ilog("existing source '%s' matched", name);
                found:
                // replace the data to indicate a duplicate was found
                item->setData(Qt::UserRole, QVariant::fromValue((void*)source));
                return false;
            }

            DeviceInfo* device_info = (DeviceInfo*) item->data(Qt::UserRole).value<void*>();
            obs_data_t *settings = obs_source_get_settings(source);
            if (!settings || !device_info)
                return true;

            const char *id = obs_data_get_string(settings, OPT_ACTIVE_DEV_ID);
            const char *ip = obs_data_get_string(settings, OPT_ACTIVE_DEV_IP);
            const DeviceType type = (DeviceType) obs_data_get_int(settings, OPT_ACTIVE_DEV_TYPE);
            obs_data_release(settings);

            // Cross-check Wifi and MDNS devices by target IP
            if ((device_info->type == DeviceType::WIFI || device_info->type == DeviceType::MDNS)
                && (type == DeviceType::WIFI || type == DeviceType::MDNS))
            {
                dlog("%s ip=%s against ip %s", name, ip, device_info->ip);
                if (ip && strncmp(ip, device_info->ip, 64) == 0)
                    goto found;
            }

            dlog("%s id=%s against id %s", name, id, device_info->id);
            if (type != DeviceType::WIFI && id && device_info->id && strncmp(id, device_info->id, 256) == 0)
                goto found;
        }

        return true;
    }, item);

    void* data_out = item->data(Qt::UserRole).value<void*>();

    if (data_out && data_out != data) {
        // restore the original data
        item->setData(Qt::UserRole, QVariant::fromValue(data));

        QString title = QString(obs_module_text("DroidCam"));
        QString msg = QString(obs_module_text("AlreadyExist")).arg(device_name);
        QMessageBox mb(QMessageBox::Information, title, msg,
            QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No), this);
        mb.setDefaultButton(QMessageBox::No);

        if (mb.exec() != QMessageBox::Yes)
            return;

        if (!removeSource((obs_source_t*)data_out)) {
            elog("AddDevice UI: Error removing source for replacement!");
            QString msg = QString(obs_module_text("ErrorRemove"));
            QMessageBox mb(QMessageBox::Information, title, msg,
                QMessageBox::StandardButtons(QMessageBox::Ok), this);
            mb.exec();
            return;
        }
    }

    if (!data) {
        elog("AddDevice UI: Trying to create source without device_info!");
        return;
    }

    DeviceInfo* device_info = (DeviceInfo *) data;
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, OPT_ACTIVE_DEV_TYPE, (int) device_info->type);
    obs_data_set_string(settings, OPT_ACTIVE_DEV_ID, device_info->id);
    obs_data_set_string(settings, OPT_ACTIVE_DEV_IP, device_info->ip);
    obs_data_set_string(settings, OPT_WIFI_IP      , device_info->ip);

    obs_get_video_info(&ovi);
    snprintf(resolution, sizeof(resolution), "%dx%d", ovi.base_width, ovi.base_height);
    obs_data_set_int(settings, OPT_RESOLUTION, getResolutionIndex(resolution));

    obs_data_set_bool(settings, OPT_ENABLE_AUDIO, enable_audio);
    obs_data_set_bool(settings, OPT_IS_ACTIVATED, true);
    obs_data_set_bool(settings, OPT_DEACTIVATE_WNS, true);

    obs_source_t *source = obs_source_create(DROIDCAM_OBS_ID, device_name, settings, nullptr);
    if (source) {
        obs_source_t *scene = obs_frontend_get_current_scene();
        obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(scene), source);

        // Apply Fit to screen transform
        obs_transform_info txi;
        vec2_set(&txi.pos, 0.0f, 0.0f);
        vec2_set(&txi.scale, 1.0f, 1.0f);
        txi.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
        txi.rot = 0.0f;

        vec2_set(&txi.bounds, float(ovi.base_width), float(ovi.base_height));
        txi.bounds_type = OBS_BOUNDS_SCALE_INNER;
        txi.bounds_alignment = OBS_ALIGN_CENTER;
        obs_sceneitem_set_info(item, &txi);

        // Add a noise suppression filter
        if (enable_audio) {
            obs_source_t* filter = obs_source_create("noise_suppress_filter",
                "Noise suppression", nullptr, nullptr);
            obs_source_filter_add(source, filter);
            obs_source_release(filter);
        }

        obs_source_release(source);
        obs_source_release(scene);
    }

    obs_data_release(settings);
    ShowHideDialog(0);
}
