// Copyright (C) 2022 DEV47APPS, github.com/dev47apps
#pragma once

#include <QThread>
#include <QDialog>
#include <QIcon>
#include <QSvgWidget>

#include "obs.hpp"

#include "uic_AddDevice.h"

typedef struct active_device_info DeviceInfo;

class AddDevice : public QDialog {
    Q_OBJECT

signals:
    // custom signals (events) go here

public slots:
    // Event handlers go here
    void AddListEntry(const char *name, void* data);
    void AddDeviceManually();
    void ReloadFinish();
    void ReloadList();
    void ClearList();

public:
    QSvgWidget loadingSvg;
    QIcon phoneIcon;
    void *dummy_droidcam_source;
    void *dummy_properties;
    void *dummy_source_priv_data;
    bool enable_audio;
    int refresh_count;
    std::unique_ptr<Ui_AddDeviceDC> ui;
    std::unique_ptr<QThread> thread;
    std::vector<OBSSignal> signal_handlers;
    ~AddDevice();
    explicit AddDevice(QWidget *parent);
    void ShowHideDevicePicker(int show);
    void AddNewDevice(QListWidgetItem *item);

private:
    static bool VideoReset(obs_scene_t*, obs_sceneitem_t*, void* data);
    void AddSourceInternal(DeviceInfo* device_info, const char* device_name);

};

class ReloadThread : public QThread {
    Q_OBJECT
    virtual void run() override;

signals:
    void AddListEntry(const char *name, void* data);

public:
    AddDevice *parent;
    ReloadThread(AddDevice *parent_) : parent(parent_) {}
};
