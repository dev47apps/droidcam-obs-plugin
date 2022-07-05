#include <stdlib.h>
#include <QtWidgets/QMessageBox>
#include <QtGui/QFont>
#include <QtSvg/QSvgRenderer>
#include "AddDevice.h"

#include "plugin.h"
#include "plugin_properties.h"
#include "obs-frontend-api.h"

// FIXME: res
#define DROIDCAM_OBS_ID "droidcam_obs"
#define LOADING_SVG "C:\\Users\\admin\\Downloads\\loading.svg"
#define PHONE_ICON  "C:\\Users\\admin\\Downloads\\smartphone.svg"

inline static void retainSize(QWidget *item) {
    QSizePolicy sp = item->sizePolicy(); sp.setRetainSizeWhenHidden(true); item->setSizePolicy(sp);
}

AddDevice::AddDevice(QWidget *parent) : QDialog(parent),
    loadingSvg(LOADING_SVG, this),
    ui(new Ui_AddDeviceDC)
{
    loadingSvg.renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
    loadingSvg.renderer()->blockSignals(true);
    loadingSvg.setVisible(false);

    ui->setupUi(this);
    Qt::WindowFlags flags = windowFlags();
    flags &= ~Qt::WindowContextHelpButtonHint;
    flags |= Qt::MSWindowsFixedSizeDialogHint;
    setWindowFlags(flags);
    retainSize(ui->refresh_button);
    retainSize(ui->addDevice_button);

    phoneIcon.addFile(PHONE_ICON, QSize(96, 96));
    ui->addDevice_label->setStyleSheet("font-size: 14pt;");
    ui->addDevice_button->setVisible(false);
    ui->refresh_button->setVisible(false);
    ui->horizontalLayout1->addWidget(&loadingSvg);

    enable_audio = false;
    ui->enableAudio_checkBox->connect(ui->enableAudio_checkBox,
        &QCheckBox::stateChanged, [=] (int state) {
            enable_audio = state == Qt::Checked;
            dlog("enable_audio=%d", enable_audio);
        });

    ui->deviceList_widget->connect(ui->deviceList_widget,
        &QListWidget::itemSelectionChanged, [=]() {
            int active = ui->deviceList_widget->currentRow();
            dlog("active: %d", active);
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

    const char *name = "dummy_discovery_source";
    dummy_droidcam_source = obs_get_source_by_name(name);
    if (!dummy_droidcam_source) {
        obs_data_t *settings = obs_data_create();
        obs_data_set_bool(settings, OPT_DUMMY_SOURCE, true);

        dummy_droidcam_source = obs_source_create_private("droidcam_obs", name, settings);
        dummy_properties = obs_source_properties((obs_source_t *) dummy_droidcam_source);
        obs_data_release(settings);
    }
    dummy_source_priv_data = NULL;
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

void AddDevice::ClearList() {
    while (ui->deviceList_widget->count() > 0) {
        QListWidgetItem *item = ui->deviceList_widget->takeItem(0);
        void *data = item->data(Qt::UserRole).value<void *>();
        if (data) bfree(data);
        delete item;
    }
}

void AddDevice::ReloadList() {
    if (dummy_droidcam_source && dummy_properties && dummy_source_priv_data) {
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
        elog("AddDevice UI: Trying to reload device list without dummy source: '%p' '%p' '%p'",
            dummy_droidcam_source, dummy_properties, dummy_source_priv_data);
    }

    return;
}

void ReloadThread::ReloadList() {
    auto ppts = (obs_properties_t *) parent->dummy_properties;
    obs_property_t *p = obs_properties_get(ppts, OPT_REFRESH);

    if (p && obs_property_button_clicked(p, parent->dummy_droidcam_source))
    {
        obs_property_t *list = obs_properties_get(ppts, OPT_DEVICE_LIST);
        for (size_t i = 0; i < obs_property_list_item_count(list); i++) {
            const char *name = obs_property_list_item_name(list, i);
            const char *value = obs_property_list_item_string(list, i);
            if (!name || !value)
                continue;

            if (strncmp(value, opt_use_wifi, sizeof(OPT_DEVICE_ID_WIFI)-1) == 0)
                continue;


            auto info = (ActiveDeviceInfo *) bzalloc(sizeof(ActiveDeviceInfo));
            info->id = value;
            info->ip = "";
            info->port = 4747;
            info->type = get_device_type(info->id, parent->dummy_source_priv_data);
            if (info->type != DeviceType::NONE)
                emit AddListEntry(name, info);
        }
    }

    // FIXME testing
    auto info = (ActiveDeviceInfo *) bzalloc(sizeof(ActiveDeviceInfo));
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

void AddDevice::AddListEntry(const char *name, void* data) {
    QListWidgetItem *item = new QListWidgetItem(phoneIcon, name, ui->deviceList_widget);
    item->setData(Qt::UserRole, QVariant::fromValue(data));
    QFont font = item->font();
    font.setPointSize(14);
    item->setFont(font);
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
        const char *id = obs_source_get_id(source);
        const char *name = obs_source_get_name(source);
        if (data && id && name && strcmp(id, DROIDCAM_OBS_ID) == 0) {
            ilog("checking existing source %s", name);
            auto item = (QListWidgetItem*) data;
            if (item->text() == QString(name)) {
                ilog("duplicate found");
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

    ActiveDeviceInfo* device_info = (ActiveDeviceInfo *) data;
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
