#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVariantMap>
#include <QSize>
#include <QPoint>
#include <QList>
#include "stylemanager.h"
#include "gui/messageconsole/messageconsoledock.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SettingsFileManager;
class ApplicationController;
class ImageSessionViewer;
class PSFControlWidget;
class QSplitter;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(SettingsFileManager* guiSettings,
						StyleManager* styleManager, ApplicationController* applicationController,
						QWidget *parent = nullptr);
	~MainWindow();

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	// Style management
	void selectStyle(QAction* action);
	void updateStyleMenuChecks(StyleManager::StyleMode newStyle);

	// File operations
	void openImageData();
	void openGroundTruth();

	// ApplicationController response handlers
	void onInputFileLoaded(const QString& filePath);
	void onFileLoadError(const QString& filePath, const QString& error);

private:
	void setupMenuBar();
	void setupFileMenu();
	void setupViewMenu();
	void setupCentralWidget();
	void connectApplicationController();
	void connectImageSessionViewer();
	void connectPSFControlWidget();
	void loadSettings();
	void saveSettings();

	Ui::MainWindow *ui;
	SettingsFileManager* guiSettings;
	StyleManager* styleManager;
	ApplicationController* applicationController;
	MessageConsoleDock* messageConsoleDock;
	MessageConsoleWidget* consoleWidget() const;

	QSize windowSize;
	QPoint windowPosition;
	QString lastOpenDirInput;
	QString lastOpenDirGroundTruth;
	QString lastNameFilterInput;
	QString lastNameFilterGroundTruth;

	// Menus
	QMenu* fileMenu;
	QMenu* viewMenu;
	QMenu* styleMenu;

	// File actions
	QAction* openImageDataAction;
	QAction* openGroundTruthAction;

	// Style actions
	QList<QAction*> styleActions;

	// View actions
	QAction* toggleMessageConsoleAction;

	// Main widgets
	QSplitter* centralSplitter;
	ImageSessionViewer* sessionViewer;
	PSFControlWidget* psfControlWidget;

signals:

};

#endif // MAINWINDOW_H
