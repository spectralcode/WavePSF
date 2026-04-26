#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QPointer>
#include <QVariantMap>
#include <QSize>
#include <QPoint>
#include <QList>
#include "stylemanager.h"
#include "gui/messageconsole/messageconsoledock.h"
#include "gui/psfgridview/psfgriddock.h"
#include "gui/viewertoolbar.h"
#include "core/processing/deconvolutiontypes.h"
#include "core/psf/psfsettings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class SettingsFileManager;
class ApplicationController;
class AFDeviceManager;
class ImageSessionViewer;
class PSFGenerationWidget;
class ProcessingControlWidget;
class SettingsDialog;
class AboutDialog;
class ShortcutsDialog;
class RecentFilesMenu;
class QProgressDialog;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	explicit MainWindow(SettingsFileManager* guiSettings,
						StyleManager* styleManager, AFDeviceManager* afDeviceManager,
						ApplicationController* applicationController,
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
	void openImageFolder();
	void openGroundTruth();
	void saveParameters();
	void loadParameters();
	void saveOutputData();


	// Settings
	void openSettings();

	// PSF operations
	void savePSF();
	void setPSFSaveFolder();

	// Batch processing
	void deconvolveAll();

	// ApplicationController response handlers
	void onInputFileLoaded(const QString& filePath);
	void onFileLoadError(const QString& filePath, const QString& error);
	void handleDeconvolutionRunStarted();
	void handleDeconvolutionRunProgressUpdated(const DeconvolutionProgress& progress);
	void handleDeconvolutionRunFinished(const DeconvolutionRunResult& result);

private:
	void setupMenuBar();
	void setupFileMenu();
	void setupEditMenu();
	void setupPSFMenu();
	void setupProcessingMenu();
	void setupViewMenu();
	void setupExtrasMenu();
	void setupHelpMenu();
	void setupCentralWidget();
	void setupViewerToolBar();
	void connectApplicationController();
	void connectImageSessionViewer();
	void connectPSFGenerationWidget();
	void connectProcessingControlWidget();
	void connectPSFGridWidget();
	void loadSettings();
	void saveSettings();
	void applyDeconvolutionProgress(const DeconvolutionProgress& progress);
	void closeDeconvolutionProgressDialog();

	Ui::MainWindow *ui;
	SettingsFileManager* guiSettings;
	StyleManager* styleManager;
	AFDeviceManager* afDeviceManager;
	ApplicationController* applicationController;
	MessageConsoleDock* messageConsoleDock;
	MessageConsoleWidget* consoleWidget() const;

	QSize windowSize;
	QPoint windowPosition;
	QString lastOpenDirInput;
	QString lastOpenDirFolder;
	QString lastOpenDirGroundTruth;
	QString lastNameFilterInput;
	QString lastNameFilterGroundTruth;
	QString lastOpenDirCoefficients;
	QString lastOpenDirOutput;


	// Menus
	QMenu* fileMenu;
	RecentFilesMenu* recentInput;
	RecentFilesMenu* recentGroundTruth;
	QMenu* editMenu;
	QMenu* psfMenu;
	QMenu* processingMenu;
	QMenu* viewMenu;
	QMenu* extrasMenu;
	QMenu* helpMenu;
	QMenu* styleMenu;

	// File actions
	QAction* openImageDataAction;
	QAction* openImageFolderAction;
	QAction* openGroundTruthAction;
	QAction* saveParametersAction;
	QAction* loadParametersAction;
	QAction* saveOutputAction;

	// PSF actions
	QAction* savePSFAction;
	QAction* autoSavePSFAction;
	QAction* setPSFSaveFolderAction;

	// Processing actions
	QAction* deconvolveAllAction;
	bool restoreDeconvolveAllAction;
	QPointer<QProgressDialog> deconvolutionProgressDialog;
	bool deconvolutionInProgress;
	bool updatingDeconvolutionProgress;
	bool pendingDeconvolutionProgressValid;
	DeconvolutionProgress pendingDeconvolutionProgress;

	// Style actions
	QList<QAction*> styleActions;

	// View actions
	QAction* toggleMessageConsoleAction;
	QAction* togglePSFGridAction;
	QAction* toggleCrossSectionAction;
	PSFGridDock* psfGridDock;

	// Viewer toolbar
	ViewerToolBar* viewerToolBar;

	// Help actions
	QAction* shortcutsAction;
	QAction* aboutAction;

	// Settings state
	PSFSettings currentPSFSettings;

	// Main widgets
	ImageSessionViewer* sessionViewer;
	PSFGenerationWidget* psfGenerationWidget;
	ProcessingControlWidget* processingControlWidget;
	SettingsDialog* settingsDialog;
	ShortcutsDialog* shortcutsDialog;
	AboutDialog* aboutDialog;

signals:

};

#endif // MAINWINDOW_H
