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
#include "core/psf/psfsettings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SettingsFileManager;
class ApplicationController;
class ImageSessionViewer;
class PSFGenerationWidget;
class ProcessingControlWidget;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(SettingsFileManager* guiSettings,
						StyleManager* styleManager, ApplicationController* applicationController,
						QWidget *parent = nullptr);
	~MainWindow();

protected:
	void closeEvent(QCloseEvent *event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
	// Style management
	void selectStyle(QAction* action);
	void updateStyleMenuChecks(StyleManager::StyleMode newStyle);

	// File operations
	void openImageData();
	void openGroundTruth();
	void saveParameters();
	void loadParameters();
	void saveOutputData();

	// Settings
	void openSettings();

	// PSF operations
	void loadPSF();
	void savePSF();
	void setPSFSaveFolder();
	void setCustomPSFFolder();

	// Batch processing
	void deconvolveAll();

	// ApplicationController response handlers
	void onInputFileLoaded(const QString& filePath);
	void onFileLoadError(const QString& filePath, const QString& error);

private:
	void setupMenuBar();
	void setupFileMenu();
	void setupPSFMenu();
	void setupProcessingMenu();
	void setupViewMenu();
	void setupExtrasMenu();
	void setupCentralWidget();
	void connectApplicationController();
	void connectImageSessionViewer();
	void connectPSFGenerationWidget();
	void connectProcessingControlWidget();
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
	QMenu* psfMenu;
	QMenu* processingMenu;
	QMenu* viewMenu;
	QMenu* extrasMenu;
	QMenu* styleMenu;

	// File actions
	QAction* openImageDataAction;
	QAction* openGroundTruthAction;
	QAction* saveParametersAction;
	QAction* loadParametersAction;
	QAction* saveOutputAction;

	// PSF actions
	QAction* loadPSFAction;
	QAction* savePSFAction;
	QAction* autoSavePSFAction;
	QAction* setPSFSaveFolderAction;
	QAction* useCustomPSFFolderAction;
	QAction* setCustomPSFFolderAction;

	// Processing actions
	QAction* deconvolveAllAction;

	// Style actions
	QList<QAction*> styleActions;

	// View actions
	QAction* toggleMessageConsoleAction;

	// Settings state
	PSFSettings currentPSFSettings;

	// Main widgets
	ImageSessionViewer* sessionViewer;
	PSFGenerationWidget* psfGenerationWidget;
	ProcessingControlWidget* processingControlWidget;

signals:

};

#endif // MAINWINDOW_H
