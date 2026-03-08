#include "gui/mainwindow.h"
#include "utils/settingsfilemanager.h"
#include "utils/afdevicemanager.h"
#include "gui/stylemanager.h"
#include "controller/applicationcontroller.h"
#include "gui/messageconsole/messagerouter.h"

#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QIcon>


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MessageRouter::instance()->install();

	//application properties
	a.setApplicationName(APP_NAME);
	a.setApplicationVersion(APP_VERSION);
	a.setWindowIcon(QIcon(QStringLiteral(":/icons/wavepsf.ico")));

	//create settings managers
	QString settingsDir = QCoreApplication::applicationDirPath(); //file paths next to executable
	//QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); //file paths in user's app data directory
	SettingsFileManager guiSettings(QDir(settingsDir).filePath("wavepsf.ini"));

	//create AF device manager (enumerates backends, restores saved device from INI)
	AFDeviceManager afDeviceManager(&guiSettings);

	//create style manager
	StyleManager styleManager(&a, &guiSettings);

	//create application controller
	ApplicationController applicationController(&afDeviceManager, &a);

	//create and show main window
	MainWindow w(&guiSettings, &styleManager, &afDeviceManager, &applicationController);
	w.setWindowTitle(a.applicationName());
	w.show();

	return a.exec();
}
