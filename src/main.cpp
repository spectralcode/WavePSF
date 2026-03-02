#include "gui/mainwindow.h"
#include "utils/settingsfilemanager.h"
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
	a.setApplicationName("WavePSF");
	a.setApplicationVersion("0.1");
	a.setWindowIcon(QIcon(QStringLiteral(":/icons/wavepsf.png")));
	
	//create settings managers
	QString settingsDir = QCoreApplication::applicationDirPath(); //file paths next to executable
	//QString settingsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); //file paths in user's app data directory
	SettingsFileManager guiSettings(QDir(settingsDir).filePath("wavepsf.ini"));

	//todo: hier weitermachen: remove paraemterSettings, only use guiSettings from now on (maybe rename it?) and rename gui.ini to wavepsf.ini

	//create style manager
	StyleManager styleManager(&a, &guiSettings);

	//create application controller
	ApplicationController applicationController(&a);
	
	//create and show main window
	MainWindow w(&guiSettings, &styleManager, &applicationController);
	w.setWindowTitle("WavePSF");
	w.show();
	
	return a.exec();
}
