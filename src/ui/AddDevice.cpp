#include "AddDevice.h"

AddDevice::AddDevice(QWidget *parent)
	: QDialog(parent), ui(new Ui_AddDeviceDC)
{
	ui->setupUi(this);

	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	setWindowFlags(flags);
}
