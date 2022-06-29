#pragma once

#include <QtWidgets/QDialog>
#include "ui_AddDevice.h"

class AddDevice : public QDialog {
    Q_OBJECT

signals:
    // custom signals (events) go here

public slots:
    // Event handlers go here

public:
    std::unique_ptr<Ui_AddDeviceDC> ui;
    explicit AddDevice(QWidget *parent);
};
