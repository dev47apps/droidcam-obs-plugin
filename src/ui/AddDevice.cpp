#include <stdlib.h>
#include <QtWidgets/QMessageBox>
#include <QtGui/QFont>
#include <QtSvg/QSvgRenderer>
#include "AddDevice.h"

#include "plugin.h"
#include "plugin_properties.h"
#include "obs-frontend-api.h"

#define DROIDCAM_OBS_ID "droidcam_obs"

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
    ClearList();
    obs_properties_destroy((obs_properties_t *)dummy_properties);
    obs_source_release((obs_source_t *) dummy_droidcam_source);
}

void AddDevice::ShowHideDialog(int show) {
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

void AddDevice::AddListEntry(const char *name, void* data) {
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
    if (dummy_droidcam_source && dummy_properties) {
        ReloadThread* thread = new ReloadThread();
        thread->parent = this;
        connect(thread, &ReloadThread::AddListEntry, this, &AddDevice::AddListEntry);
        connect(thread, &ReloadThread::ReloadFinish, this, &AddDevice::ReloadFinish);
        connect(thread, &ReloadThread::finished, thread, &QObject::deleteLater);

        loadingSvg.setVisible(true);
        loadingSvg.renderer()->blockSignals(false);
        ui->refresh_button->setVisible(false);
        ClearList();
        thread->start();
    }
    else {
        elog("AddDevice UI: Trying to reload device list without dummy source: '%p' '%p'",
            dummy_droidcam_source, dummy_properties);
    }

    return;
}

void ReloadThread::ReloadList() {
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

            if (strncmp(value, opt_use_wifi, sizeof(OPT_DEVICE_ID_WIFI)-1) == 0)
                continue;


            auto info = (DeviceInfo *) bzalloc(sizeof(DeviceInfo));
            info->id = value;
            info->ip = "";
            info->port = 4747;
            info->type = get_device_type(info->id, parent->dummy_source_priv_data);
            if (info->type != DeviceType::NONE)
                emit AddListEntry(name, info);
        }
    }

    // FIXME testing
    auto info = (DeviceInfo *) bzalloc(sizeof(DeviceInfo));
    info->type = DeviceType::WIFI;
    info->port = 4747;
    info->id = "192.168.0.69";
    info->ip = "192.168.0.69";
    emit AddListEntry("Test device [USBx]", info);
}


void AddDevice::ReloadFinish() {
    loadingSvg.renderer()->blockSignals(true);
    loadingSvg.setVisible(false);
    ui->refresh_button->setVisible(true);
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
                ilog("existing source name %s matched", name);
                goto found;
            }

            const char *id;
            obs_data_t *settings = obs_source_get_settings(source);
            if (settings) {
                id = obs_data_get_string(settings, OPT_ACTIVE_DEV_ID);
                obs_data_release(settings);
            } else {
                id = NULL;
            }

            DeviceInfo* device_info = (DeviceInfo*) item->data(Qt::UserRole).value<void*>();
            if (id && device_info && device_info->id && strcmp(id, device_info->id) == 0) {
                ilog("existing source id %s matched", id);
                found:
                // replace the data to indicate a duplicate was found
                item->setData(Qt::UserRole, QVariant::fromValue((void*)source));
                return false;
            }
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
    obs_data_set_string(settings, OPT_CONNECT_IP, device_info->ip);

    obs_get_video_info(&ovi);
    snprintf(resolution, sizeof(resolution), "%dx%d", ovi.base_width, ovi.base_height);
    obs_data_set_int(settings, OPT_RESOLUTION, getResolutionIndex(resolution));

    obs_data_set_bool(settings, OPT_ENABLE_AUDIO, enable_audio);
    // TODO obs_data_set_bool(settings, OPT_IS_ACTIVATED, true);

    obs_source_t *source = obs_source_create(DROIDCAM_OBS_ID, device_name, settings, nullptr);
    if (source) {
        obs_source_t *scene = obs_frontend_get_current_scene();
        obs_scene_add(obs_scene_from_source(scene), source);
        obs_source_release(source);
        obs_source_release(scene);
    }

    obs_data_release(settings);
    ShowHideDialog(0);
}
