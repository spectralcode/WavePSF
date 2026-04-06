#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/settingsfilemanager.h"
#include "controller/applicationcontroller.h"
#include "gui/imagesessionviewer/imagesessionviewer.h"
#include "gui/imagesessionviewer/datacrosssectionwidget.h"
#include "gui/psfcontrol/psfgenerationwidget.h"
#include "gui/psfcontrol/processingcontrolwidget.h"
#include "gui/psfcontrol/settingsdialog.h"
#include "utils/afdevicemanager.h"
#include "utils/logging.h"
#include "utils/supportedfilechecker.h"
#include "gui/messageconsole/messagerouter.h"
#include "gui/messageconsole/messageconsoledock.h"
#include "gui/aboutdialog.h"
#include "gui/shortcutsdialog.h"
#include "gui/recentfilesmenu.h"
#include "gui/viewertoolbar.h"
#include "controller/imagesession.h"


#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStandardPaths>
#include <QStatusBar>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QApplication>
#include <QMessageBox>
namespace {
	const QString SETTINGS_GROUP       = QStringLiteral("main_window");
	const QString KEY_WINDOW_SIZE      = QStringLiteral("window_size");
	const QString KEY_WINDOW_POSITION  = QStringLiteral("window_position");
	const QString KEY_WINDOW_MAXIMIZED = QStringLiteral("window_maximized");
	const QString KEY_LAST_OPEN_DIR_INPUT  = QStringLiteral("last_open_dir_input");
	const QString KEY_LAST_OPEN_DIR_FOLDER = QStringLiteral("last_open_dir_folder");
	const QString KEY_LAST_OPEN_DIR_GT    = QStringLiteral("last_open_dir_ground_truth");
	const QString KEY_LAST_OPEN_DIR_COEFF  = QStringLiteral("last_open_dir_coefficients");
	const QString KEY_LAST_OPEN_DIR_OUTPUT = QStringLiteral("last_open_dir_output");
	const QString KEY_LAST_FILTER_INPUT   = QStringLiteral("last_name_filter_input");
	const QString KEY_LAST_FILTER_GT      = QStringLiteral("last_name_filter_ground_truth");
	const QString KEY_DOCK_STATE          = QStringLiteral("dock_state_v1");
	const QString KEY_CONSOLE_VISIBLE     = QStringLiteral("message_console_visible");
	const QString KEY_PSF_GRID_VISIBLE    = QStringLiteral("psf_grid_visible");
	const QString KEY_CROSS_SECTION_VISIBLE = QStringLiteral("cross_section_visible");
	const bool DEF_WINDOW_MAXIMIZED = false;
	const bool DEF_CONSOLE_VISIBLE  = false;
	const bool DEF_PSF_GRID_VISIBLE = false;
	const bool DEF_CROSS_SECTION_VISIBLE = false;
}

MainWindow::MainWindow(SettingsFileManager* guiSettings,
					   StyleManager* styleManager, AFDeviceManager* afDeviceManager,
					   ApplicationController* applicationController, QWidget *parent)
	: QMainWindow(parent), ui(new Ui::MainWindow),
	  guiSettings(guiSettings), styleManager(styleManager), afDeviceManager(afDeviceManager),
	  applicationController(applicationController),
	  fileMenu(nullptr), recentInput(nullptr), recentGroundTruth(nullptr),
	  psfMenu(nullptr), processingMenu(nullptr),
	  viewMenu(nullptr), extrasMenu(nullptr), styleMenu(nullptr),
	  openImageDataAction(nullptr), openGroundTruthAction(nullptr),
	  saveParametersAction(nullptr), loadParametersAction(nullptr), saveOutputAction(nullptr),
	  deconvolveAllAction(nullptr),
	  toggleCrossSectionAction(nullptr),
	  viewerToolBar(nullptr),
	  sessionViewer(nullptr),
	  psfGenerationWidget(nullptr), processingControlWidget(nullptr),
	  settingsDialog(nullptr), shortcutsDialog(nullptr), aboutDialog(nullptr) {
	MessageRouter::instance()->install();
	this->ui->setupUi(this);
	this->setupMenuBar();
	this->setupCentralWidget();
	this->setupViewerToolBar();
	this->connectApplicationController();
	this->connectImageSessionViewer();
	this->connectPSFGenerationWidget();
	this->connectProcessingControlWidget();
	this->connectPSFGridWidget();
	this->loadSettings();

	// Apply loaded PSF settings to the controller (loadSettings only deserializes
	// into PSFGenerationWidget, this actually activates them in PSFModule)
	this->currentPSFSettings = this->psfGenerationWidget->getPSFSettings();
	this->applicationController->applyPSFSettings(this->currentPSFSettings);

	// Broadcast initial state after all connections are made
	this->applicationController->broadcastCurrentState();

	//style manager init
	connect(this->styleManager, &StyleManager::styleChanged, this, &MainWindow::updateStyleMenuChecks);
	this->updateStyleMenuChecks(this->styleManager->getStyleMode());

	qApp->installEventFilter(this);
}

MainWindow::~MainWindow() {
	delete this->ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
	this->saveSettings();
	event->accept();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		QWidget* focused = QApplication::focusWidget();
		bool isTextWidget = qobject_cast<QLineEdit*>(focused)
						 || qobject_cast<QAbstractSpinBox*>(focused)
						 || qobject_cast<QTextEdit*>(focused)
						 || qobject_cast<QPlainTextEdit*>(focused);

		if (!isTextWidget) {
			if (keyEvent->matches(QKeySequence::Copy)) {
				this->applicationController->copyCoefficients();
				return true;
			}
			if (keyEvent->matches(QKeySequence::Paste)) {
				this->applicationController->pasteCoefficients();
				return true;
			}
			if (keyEvent->matches(QKeySequence::Undo)) {
				this->applicationController->undoPasteCoefficients();
				return true;
			}
			if (keyEvent->key() == Qt::Key_Delete) {
				this->applicationController->resetPSFCoefficients();
				return true;
			}
		}
	}

	return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupMenuBar() {
	this->setupFileMenu();
	this->setupEditMenu();
	this->setupPSFMenu();
	this->setupProcessingMenu();
	this->setupViewMenu();
	this->setupExtrasMenu();
	this->setupHelpMenu();
}

void MainWindow::setupFileMenu() {
	this->fileMenu = this->menuBar()->addMenu("&File");

	// Open Image Data action
	this->openImageDataAction = new QAction("Open &Image Data...", this);
	this->openImageDataAction->setShortcut(QKeySequence::Open);
	this->openImageDataAction->setStatusTip("Open hyperspectral or RGB image data");
	connect(this->openImageDataAction, &QAction::triggered, this, &MainWindow::openImageData);
	this->fileMenu->addAction(this->openImageDataAction);

	// Open Image Folder action
	this->openImageFolderAction = new QAction("Open Image &Folder...", this);
	this->openImageFolderAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
	this->openImageFolderAction->setStatusTip("Open folder of images as multi-frame stack");
	connect(this->openImageFolderAction, &QAction::triggered, this, &MainWindow::openImageFolder);
	this->fileMenu->addAction(this->openImageFolderAction);

	// Open Ground Truth action
	this->openGroundTruthAction = new QAction("Open &Ground Truth...", this);
	this->openGroundTruthAction->setShortcut(QKeySequence("Ctrl+G"));
	this->openGroundTruthAction->setStatusTip("Open ground truth image for quality metrics");
	connect(this->openGroundTruthAction, &QAction::triggered, this, &MainWindow::openGroundTruth);
	this->fileMenu->addAction(this->openGroundTruthAction);

	this->recentInput       = new RecentFilesMenu(tr("Recent Input Files"),       "recent_input_files",       this);
	this->recentGroundTruth = new RecentFilesMenu(tr("Recent Ground Truth Files"), "recent_ground_truth_files", this);
	this->fileMenu->addMenu(this->recentInput->menu());
	this->fileMenu->addMenu(this->recentGroundTruth->menu());

	this->fileMenu->addSeparator();

	// Save Parameters action
	this->saveParametersAction = new QAction("&Save Wavefront Coefficients...", this);
	this->saveParametersAction->setShortcut(QKeySequence::Save);
	this->saveParametersAction->setStatusTip("Save wavefront coefficients to CSV file");
	connect(this->saveParametersAction, &QAction::triggered, this, &MainWindow::saveParameters);
	this->fileMenu->addAction(this->saveParametersAction);

	// Load Parameters action
	this->loadParametersAction = new QAction("&Load Wavefront Coefficients...", this);
	this->loadParametersAction->setStatusTip("Load wavefront coefficients from CSV file");
	connect(this->loadParametersAction, &QAction::triggered, this, &MainWindow::loadParameters);
	this->fileMenu->addAction(this->loadParametersAction);

	this->fileMenu->addSeparator();

	// Save Output Data action
	this->saveOutputAction = new QAction("Save &Output Data...", this);
	this->saveOutputAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
	this->saveOutputAction->setStatusTip("Save deconvolved output data to file");
	connect(this->saveOutputAction, &QAction::triggered, this, &MainWindow::saveOutputData);
	this->fileMenu->addAction(this->saveOutputAction);

	this->fileMenu->addSeparator();

	// Exit action
	QAction* exitAction = new QAction("E&xit", this);
	exitAction->setShortcut(QKeySequence::Quit);
	exitAction->setStatusTip("Exit the application");
	connect(exitAction, &QAction::triggered, this, &QWidget::close);
	this->fileMenu->addAction(exitAction);
}

void MainWindow::setupEditMenu() {
	this->editMenu = this->menuBar()->addMenu("&Edit");

	QAction* resetAllCoeffsAction = new QAction("Reset All Coefficients", this);
	resetAllCoeffsAction->setStatusTip("Set all wavefront coefficients to zero for all frames and patches");
	connect(resetAllCoeffsAction, &QAction::triggered,
			this->applicationController, &ApplicationController::resetAllCoefficients);
	this->editMenu->addAction(resetAllCoeffsAction);

}

void MainWindow::setupPSFMenu() {
	this->psfMenu = this->menuBar()->addMenu("&PSF");

	this->savePSFAction = new QAction("&Save PSF...", this);
	this->savePSFAction->setStatusTip("Save the current PSF kernel as a 32-bit float TIFF");
	connect(this->savePSFAction, &QAction::triggered, this, &MainWindow::savePSF);
	this->psfMenu->addAction(this->savePSFAction);

	this->psfMenu->addSeparator();

	this->autoSavePSFAction = new QAction("&Auto Save PSF", this);
	this->autoSavePSFAction->setCheckable(true);
	this->autoSavePSFAction->setStatusTip("Automatically save PSF on each update");
	connect(this->autoSavePSFAction, &QAction::toggled,
			this->applicationController, &ApplicationController::setAutoSavePSF);
	this->psfMenu->addAction(this->autoSavePSFAction);

	this->setPSFSaveFolderAction = new QAction("Set PSF Save &Folder...", this);
	this->setPSFSaveFolderAction->setStatusTip("Set the folder for PSF auto-save output");
	connect(this->setPSFSaveFolderAction, &QAction::triggered, this, &MainWindow::setPSFSaveFolder);
	this->psfMenu->addAction(this->setPSFSaveFolderAction);
}

void MainWindow::setupProcessingMenu() {
	this->processingMenu = this->menuBar()->addMenu("P&rocessing");

	this->deconvolveAllAction = new QAction("Deconvolve &All Frames", this);
	this->deconvolveAllAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
	this->deconvolveAllAction->setStatusTip("Deconvolve all patches of every frame using stored coefficients or external PSFs");
	this->deconvolveAllAction->setEnabled(false);
	connect(this->deconvolveAllAction, &QAction::triggered, this, &MainWindow::deconvolveAll);
	this->processingMenu->addAction(this->deconvolveAllAction);
}

void MainWindow::deconvolveAll() {
	this->applicationController->requestBatchDeconvolution();
}

void MainWindow::savePSF() {
	const QString filePath = QFileDialog::getSaveFileName(
		this, "Save PSF", QString(),
		"TIFF Image (*.tif)");
	if (!filePath.isEmpty()) {
		this->applicationController->savePSFToFile(filePath);
		this->statusBar()->showMessage("PSF saved", 3000);
	}
}

void MainWindow::setPSFSaveFolder() {
	const QString dir = QFileDialog::getExistingDirectory(
		this, "Select PSF Save Folder");
	if (!dir.isEmpty()) {
		this->applicationController->setPSFSaveFolder(dir);
		this->statusBar()->showMessage("PSF save folder: " + dir, 3000);
	}
}

void MainWindow::setupViewMenu() {
	this->viewMenu = this->menuBar()->addMenu("&View");

	this->styleMenu = this->viewMenu->addMenu("&Style");

	QActionGroup* styleActionGroup = new QActionGroup(this);
	styleActionGroup->setExclusive(true);

	//auto-generate menu items from the same macro that defines the styles in stylemanager.h
	#define STYLE_ITEM(enumName, displayName, path, qtStyle) \
		{ \
			QAction* action = new QAction(displayName, this); \
			action->setCheckable(true); \
			action->setData(static_cast<int>(StyleManager::enumName)); \
			styleActionGroup->addAction(action); \
			this->styleMenu->addAction(action); \
			this->styleActions.append(action); \
		}
	STYLE_LIST
	#undef STYLE_ITEM

	// Connect the GROUP signal
	connect(styleActionGroup, &QActionGroup::triggered, this, &MainWindow::selectStyle);


	// Message console toggle
	this->toggleMessageConsoleAction = new QAction("Message Console", this);
	this->toggleMessageConsoleAction->setShortcut(QKeySequence(Qt::Key_F12));
	this->toggleMessageConsoleAction->setCheckable(true);
	this->viewMenu->addSeparator();
	this->viewMenu->addAction(this->toggleMessageConsoleAction);

	// Create message console dock
	this->messageConsoleDock = new MessageConsoleDock(this);
	this->addDockWidget(Qt::BottomDockWidgetArea, this->messageConsoleDock);
	this->messageConsoleDock->hide();

	connect(this->toggleMessageConsoleAction, &QAction::toggled, this->messageConsoleDock, &QDockWidget::setVisible);
	connect(this->messageConsoleDock, &QDockWidget::visibilityChanged, this->toggleMessageConsoleAction, &QAction::setChecked);

	// PSF Grid dock
	this->togglePSFGridAction = new QAction("PSF Grid", this);
	this->togglePSFGridAction->setCheckable(true);
	this->viewMenu->addAction(this->togglePSFGridAction);

	this->psfGridDock = new PSFGridDock(this);
	this->addDockWidget(Qt::RightDockWidgetArea, this->psfGridDock);
	this->psfGridDock->hide();

	connect(this->togglePSFGridAction, &QAction::toggled,
	        this->psfGridDock, &QDockWidget::setVisible);
	connect(this->psfGridDock, &QDockWidget::visibilityChanged,
	        this->togglePSFGridAction, &QAction::setChecked);

	// Cross-Section Viewer dock
	this->toggleCrossSectionAction = new QAction("Cross-Section Viewer", this);
	this->toggleCrossSectionAction->setCheckable(true);
	this->viewMenu->addAction(this->toggleCrossSectionAction);
}

void MainWindow::setupExtrasMenu() {
	this->extrasMenu = this->menuBar()->addMenu("E&xtras");

	QAction* settingsAction = new QAction("&Settings...", this);
	settingsAction->setStatusTip("Configure application settings");
	connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);
	this->extrasMenu->addAction(settingsAction);
}

void MainWindow::setupHelpMenu() {
	this->helpMenu = this->menuBar()->addMenu("&Help");

	this->shortcutsAction = this->helpMenu->addAction(tr("&Keyboard Shortcuts"));
	this->shortcutsAction->setShortcut(QKeySequence(Qt::Key_F1));
	this->shortcutsAction->setStatusTip(tr("Show all keyboard shortcuts"));
	connect(this->shortcutsAction, &QAction::triggered, this, [this]() {
		if (!this->shortcutsDialog) {
			this->shortcutsDialog = new ShortcutsDialog(this);
		}
		this->shortcutsDialog->show();
		this->shortcutsDialog->raise();
		this->shortcutsDialog->activateWindow();
	});

	this->helpMenu->addSeparator();

	this->aboutAction = this->helpMenu->addAction("&About WavePSF");
	this->aboutAction->setStatusTip("About this application");
	connect(this->aboutAction, &QAction::triggered, this, [this]() {
		if (!this->aboutDialog) {
			this->aboutDialog = new AboutDialog(this);
		}
		this->aboutDialog->show();
		this->aboutDialog->raise();
		this->aboutDialog->activateWindow();
	});
}

void MainWindow::openSettings() {
	if (this->settingsDialog) {
		this->settingsDialog->raise();
		this->settingsDialog->activateWindow();
		return;
	}

	this->settingsDialog = new SettingsDialog(
		this->currentPSFSettings,
		this->afDeviceManager->getAvailableBackends(),
		this->afDeviceManager->getActiveBackendId(),
		this->afDeviceManager->getActiveDeviceId(),
		this);
	this->settingsDialog->setAttribute(Qt::WA_DeleteOnClose);

	connect(this->settingsDialog, &SettingsDialog::settingsApplied,
			this->applicationController, &ApplicationController::applyPSFSettings);
	connect(this->settingsDialog, &SettingsDialog::deviceSettingsApplied,
			this->afDeviceManager, &AFDeviceManager::setDevice);
	connect(this->settingsDialog, &QDialog::accepted, this, [this]() {
		this->applicationController->applyPSFSettings(this->settingsDialog->getSettings());
		this->afDeviceManager->setDevice(
			this->settingsDialog->getSelectedBackend(),
			this->settingsDialog->getSelectedDeviceId());
	});
	connect(this->settingsDialog, &QDialog::destroyed, this, [this]() {
		this->settingsDialog = nullptr;
	});

	// Keep dialog in sync when generator mode changes externally
	connect(this->applicationController, &ApplicationController::psfModeChanged,
		this->settingsDialog, &SettingsDialog::updateGeneratorType);

	this->settingsDialog->show();
}

void MainWindow::setupCentralWidget()
{
	this->sessionViewer = new ImageSessionViewer(this);

	// Insert PSF Generation widget into the left sidebar
	this->psfGenerationWidget = new PSFGenerationWidget();
	this->sessionViewer->addSidebarWidget(this->psfGenerationWidget);

	// Bottom panel below viewers: 3-tab control widget (Deconvolution, Optimization, Interpolation)
	this->processingControlWidget = new ProcessingControlWidget();
	this->sessionViewer->addBottomPanel(this->processingControlWidget);

	this->setCentralWidget(this->sessionViewer);
}

void MainWindow::setupViewerToolBar()
{
	this->viewerToolBar = new ViewerToolBar(this->styleManager, this);
	this->addToolBar(Qt::RightToolBarArea, this->viewerToolBar);

	connect(this->viewerToolBar, &ViewerToolBar::rotateRequested,      this->sessionViewer, &ImageSessionViewer::rotateViewers90);
	connect(this->viewerToolBar, &ViewerToolBar::flipHRequested,       this->sessionViewer, &ImageSessionViewer::flipViewersH);
	connect(this->viewerToolBar, &ViewerToolBar::flipVRequested,       this->sessionViewer, &ImageSessionViewer::flipViewersV);
	connect(this->viewerToolBar, &ViewerToolBar::syncViewsToggled,     this->sessionViewer, &ImageSessionViewer::setViewSyncEnabled);
	connect(this->viewerToolBar, &ViewerToolBar::showPatchGridToggled, this->sessionViewer, &ImageSessionViewer::setPatchGridVisible);
	connect(this->viewerToolBar, &ViewerToolBar::showAxisToggled,     this->sessionViewer, &ImageSessionViewer::setAxisOverlayVisible);

	this->viewMenu->insertAction(this->toggleMessageConsoleAction, this->viewerToolBar->toggleViewAction());
}

void MainWindow::connectApplicationController() {
	if (this->applicationController != nullptr) {
		// Connect file loading responses
		connect(this->applicationController, &ApplicationController::inputFileLoaded,
				this, &MainWindow::onInputFileLoaded);
		connect(this->applicationController, &ApplicationController::inputFileLoaded,
				this->recentInput, &RecentFilesMenu::addFile);
		connect(this->applicationController, &ApplicationController::groundTruthFileLoaded,
				this->recentGroundTruth, &RecentFilesMenu::addFile);
		connect(this->recentInput, &RecentFilesMenu::fileRequested,
				this->applicationController, &ApplicationController::requestOpenInputFile);
		connect(this->recentGroundTruth, &RecentFilesMenu::fileRequested,
				this->applicationController, &ApplicationController::requestOpenGroundTruthFile);
		connect(this->applicationController, &ApplicationController::fileLoadError,
				this, &MainWindow::onFileLoadError);

		// Keep local copy of PSF settings in sync for the settings dialog
		connect(this->applicationController, &ApplicationController::psfSettingsUpdated,
				this, [this](const PSFSettings& s) { this->currentPSFSettings = s; });

		// Enable batch deconvolution when parameters are loaded
		connect(this->applicationController, &ApplicationController::parametersLoaded,
				this, [this]() { this->deconvolveAllAction->setEnabled(true); });

		// Disable batch deconvolution action when session is closed
		connect(this->applicationController, &ApplicationController::sessionClosed,
				this, [this]() {
					this->deconvolveAllAction->setEnabled(false);
				});
	}
}

void MainWindow::connectImageSessionViewer() {
	if (this->applicationController != nullptr && this->sessionViewer != nullptr) {
		// ImageSessionViewer requests → ApplicationController actions
		connect(this->sessionViewer, &ImageSessionViewer::frameChangeRequested,
				this->applicationController, &ApplicationController::setCurrentFrame);
		connect(this->sessionViewer, &ImageSessionViewer::patchChangeRequested,
				this->applicationController, &ApplicationController::setCurrentPatch);
		connect(this->sessionViewer, &ImageSessionViewer::inputFileDropRequested,
				this->applicationController, &ApplicationController::requestOpenInputFile);
		connect(this->sessionViewer, &ImageSessionViewer::patchGridConfigurationRequested,
				this->applicationController, &ApplicationController::configurePatchGrid);

		// ApplicationController state changes → ImageSessionViewer updates
		connect(this->applicationController, &ApplicationController::imageSessionChanged,
				this->sessionViewer, &ImageSessionViewer::updateImageSession);
		connect(this->applicationController, &ApplicationController::frameChanged,
				this->sessionViewer, &ImageSessionViewer::setCurrentFrame);
		connect(this->applicationController, &ApplicationController::patchChanged,
				this->sessionViewer, &ImageSessionViewer::highlightPatch);
		connect(this->applicationController, &ApplicationController::patchGridConfigured,
				this->sessionViewer, &ImageSessionViewer::configurePatchGrid);

		// Coefficient operations from viewer context menus
		connect(this->sessionViewer, &ImageSessionViewer::copyCoefficientsRequested,
				this->applicationController, &ApplicationController::copyCoefficients);
		connect(this->sessionViewer, &ImageSessionViewer::pasteCoefficientsRequested,
				this->applicationController, &ApplicationController::pasteCoefficients);
		connect(this->sessionViewer, &ImageSessionViewer::resetCoefficientsRequested,
				this->applicationController, &ApplicationController::resetPSFCoefficients);
		connect(this->sessionViewer, &ImageSessionViewer::navigatePatch,
				this->applicationController, &ApplicationController::navigatePatch);

		LOG_DEBUG() << "ImageSessionViewer signal connections established";
	}

	// Cross-Section Viewer connections (inline panel in ImageSessionViewer)
	if (this->sessionViewer != nullptr) {
		auto* csWidget = this->sessionViewer->getCrossSectionWidget();
		connect(this->toggleCrossSectionAction, &QAction::toggled,
		        this->sessionViewer, &ImageSessionViewer::setCrossSectionVisible);
		connect(this->sessionViewer, &ImageSessionViewer::crossSectionVisibilityChanged,
		        this->toggleCrossSectionAction, &QAction::setChecked);

		connect(this->applicationController, &ApplicationController::imageSessionChanged,
		        csWidget, [this, csWidget](ImageSession* session) {
			if (session && session->hasInputData()) {
				csWidget->setInputData(session->getInputData());
			} else {
				csWidget->setInputData(nullptr);
			}
			if (session && session->hasOutputData()) {
				csWidget->setOutputData(session->getOutputData());
			} else {
				csWidget->setOutputData(nullptr);
			}

			// Keep cross-section slices updated when output is modified in-place
			if (session) {
				QObject::connect(session, &ImageSession::outputPatchUpdated,
					csWidget, &DataCrossSectionWidget::refreshPanels,
					Qt::UniqueConnection);
				QObject::connect(session, &ImageSession::outputDataChanged,
					csWidget, &DataCrossSectionWidget::refreshPanels,
					Qt::UniqueConnection);
			}
		});

		connect(this->applicationController, &ApplicationController::frameChanged,
		        csWidget, &DataCrossSectionWidget::setCurrentFrame);
		connect(csWidget, &DataCrossSectionWidget::frameChangeRequested,
		        this->applicationController, &ApplicationController::setCurrentFrame);
	}
}

void MainWindow::connectPSFGenerationWidget() {
	if (this->applicationController == nullptr) return;

	auto* genWidget = this->psfGenerationWidget;

	// Coefficient editing → ApplicationController
	connect(genWidget, &PSFGenerationWidget::coefficientChanged,
			this->applicationController, &ApplicationController::setPSFCoefficient);
	connect(genWidget, &PSFGenerationWidget::resetRequested,
			this->applicationController, &ApplicationController::resetPSFCoefficients);

	// ApplicationController → PSF Generation Widget
	connect(this->applicationController, &ApplicationController::psfWavefrontUpdated,
			genWidget, &PSFGenerationWidget::updateWavefront);
	connect(this->applicationController, &ApplicationController::psfUpdated,
			genWidget, &PSFGenerationWidget::updatePSF);
	connect(this->applicationController, &ApplicationController::coefficientsLoaded,
			genWidget, &PSFGenerationWidget::setCoefficients);
	connect(this->applicationController, &ApplicationController::psfSettingsUpdated,
			genWidget, &PSFGenerationWidget::setPSFSettings);

	// Parameter descriptors → PSF Generation Widget
	connect(this->applicationController, &ApplicationController::psfParameterDescriptorsChanged,
			genWidget, &PSFGenerationWidget::setParameterDescriptors);

	// Generator switching (single API)
	connect(genWidget, &PSFGenerationWidget::psfModeChangeRequested,
			this->applicationController, &ApplicationController::switchGenerator);
	connect(this->applicationController, &ApplicationController::psfModeChanged,
			genWidget, &PSFGenerationWidget::setPSFMode);

	// Inline settings → ApplicationController
	connect(genWidget, &PSFGenerationWidget::inlineSettingsChanged,
			this->applicationController, &ApplicationController::applyInlineSettings);

	// File PSF source → ApplicationController
	connect(genWidget, &PSFGenerationWidget::filePSFSourceSelected,
			this->applicationController, &ApplicationController::setFilePSFSource);
	connect(this->applicationController, &ApplicationController::filePSFInfoUpdated,
			genWidget, &PSFGenerationWidget::setFilePSFInfo);

	// Frame changes → PSF preview (for "Sync to Data Frame" feature)
	connect(this->applicationController, &ApplicationController::frameChanged,
			genWidget, &PSFGenerationWidget::setCurrentFrame);

	LOG_DEBUG() << "PSFGenerationWidget signal connections established";
}

void MainWindow::connectProcessingControlWidget() {
	if (this->applicationController == nullptr) return;

	auto* ctrl = this->processingControlWidget;

	// Parameter descriptors → ProcessingControlWidget (for optimization widget)
	connect(this->applicationController, &ApplicationController::psfParameterDescriptorsChanged,
			ctrl, &ProcessingControlWidget::setParameterDescriptors);
	connect(this->applicationController, &ApplicationController::frameChanged,
			ctrl, &ProcessingControlWidget::setCurrentFrame);

	// --- Deconvolution signals ---
	connect(ctrl, &ProcessingControlWidget::deconvAlgorithmChanged,
			this->applicationController, &ApplicationController::setDeconvolutionAlgorithm);
	connect(ctrl, &ProcessingControlWidget::deconvIterationsChanged,
			this->applicationController, &ApplicationController::setDeconvolutionIterations);
	connect(ctrl, &ProcessingControlWidget::deconvRelaxationFactorChanged,
			this->applicationController, &ApplicationController::setDeconvolutionRelaxationFactor);
	connect(ctrl, &ProcessingControlWidget::deconvRegularizationFactorChanged,
			this->applicationController, &ApplicationController::setDeconvolutionRegularizationFactor);
	connect(ctrl, &ProcessingControlWidget::deconvNoiseToSignalFactorChanged,
			this->applicationController, &ApplicationController::setDeconvolutionNoiseToSignalFactor);
	connect(ctrl, &ProcessingControlWidget::deconvPaddingModeChanged,
			this->applicationController, &ApplicationController::setVolumePaddingMode);
	connect(ctrl, &ProcessingControlWidget::deconvAccelerationModeChanged,
			this->applicationController, &ApplicationController::setAccelerationMode);
	connect(ctrl, &ProcessingControlWidget::deconvRegularizer3DChanged,
			this->applicationController, &ApplicationController::setRegularizer3D);
	connect(ctrl, &ProcessingControlWidget::deconvRegularizationWeightChanged,
			this->applicationController, &ApplicationController::setRegularizationWeight);
	connect(ctrl, &ProcessingControlWidget::deconvLiveModeChanged,
			this->applicationController, &ApplicationController::setDeconvolutionLiveMode);
	connect(ctrl, &ProcessingControlWidget::deconvolutionRequested,
			this->applicationController, &ApplicationController::requestDeconvolution);

	// Deconvolution completed → refresh output viewer
	connect(this->applicationController, &ApplicationController::deconvolutionCompleted,
			this->sessionViewer, &ImageSessionViewer::refreshOutputViewer);

	// --- Optimization signals ---
	connect(ctrl, &ProcessingControlWidget::optimizationRequested,
			this->applicationController, &ApplicationController::startOptimization);
	connect(ctrl, &ProcessingControlWidget::optimizationCancelRequested,
			this->applicationController, &ApplicationController::cancelOptimization);
	connect(ctrl, &ProcessingControlWidget::optimizationLivePreviewChanged,
			this->applicationController, &ApplicationController::updateOptimizationLivePreview);
	connect(ctrl, &ProcessingControlWidget::optimizationAlgorithmParametersChanged,
			this->applicationController, &ApplicationController::updateOptimizationAlgorithmParameters);

	connect(this->applicationController, &ApplicationController::optimizationStarted,
			ctrl, &ProcessingControlWidget::onOptimizationStarted);
	connect(this->applicationController, &ApplicationController::optimizationProgressUpdated,
			ctrl, &ProcessingControlWidget::updateOptimizationProgress);
	connect(this->applicationController, &ApplicationController::optimizationFinished,
			ctrl, &ProcessingControlWidget::onOptimizationFinished);

	// Ground truth availability → ProcessingControlWidget
	connect(this->applicationController, &ApplicationController::groundTruthFileLoaded,
			this, [this](const QString&) {
				this->processingControlWidget->setGroundTruthAvailable(true);
			});

	// Multi-patch highlighting from optimization widget
	connect(ctrl, &ProcessingControlWidget::optimizationPatchSelectionChanged,
			this->sessionViewer, &ImageSessionViewer::highlightPatches);

	// --- Interpolation signals ---
	connect(ctrl, &ProcessingControlWidget::interpolateInXRequested,
			this->applicationController, &ApplicationController::interpolateCoefficientsInX);
	connect(ctrl, &ProcessingControlWidget::interpolateInYRequested,
			this->applicationController, &ApplicationController::interpolateCoefficientsInY);
	connect(ctrl, &ProcessingControlWidget::interpolateInZRequested,
			this->applicationController, &ApplicationController::interpolateCoefficientsInZ);
	connect(ctrl, &ProcessingControlWidget::interpolateAllInZRequested,
			this->applicationController, &ApplicationController::interpolateAllCoefficientsInZ);
	connect(ctrl, &ProcessingControlWidget::interpolationPolynomialOrderChanged,
			this->applicationController, &ApplicationController::setInterpolationPolynomialOrder);

	connect(this->applicationController, &ApplicationController::interpolationCompleted,
			ctrl, &ProcessingControlWidget::updateInterpolationResult);

	// --- Patch grid signals ---
	connect(ctrl, &ProcessingControlWidget::patchGridConfigurationRequested,
			this->applicationController, &ApplicationController::configurePatchGrid);
	connect(this->applicationController, &ApplicationController::patchGridConfigured,
			ctrl, &ProcessingControlWidget::setPatchGridConfiguration);
	connect(this->applicationController, &ApplicationController::patchChanged,
			ctrl, &ProcessingControlWidget::updateCurrentPatch);

	LOG_DEBUG() << "ProcessingControlWidget signal connections established";
}

void MainWindow::connectPSFGridWidget() {
	if (this->applicationController == nullptr || this->psfGridDock == nullptr) return;

	PSFGridWidget* gridWidget = qobject_cast<PSFGridWidget*>(this->psfGridDock->widget());
	if (gridWidget == nullptr) return;

	// Generate request → ApplicationController
	connect(gridWidget, &PSFGridWidget::generateRequested,
	        this->applicationController, &ApplicationController::generatePSFGrid);
	connect(this->applicationController, &ApplicationController::psfGridGenerated,
	        gridWidget, &PSFGridWidget::displayPSFGrid);

	// Bidirectional patch selection
	connect(gridWidget, &PSFGridWidget::patchClicked,
	        this->applicationController, &ApplicationController::setCurrentPatch);
	connect(this->applicationController, &ApplicationController::patchChanged,
	        gridWidget, &PSFGridWidget::setCurrentPatch);

	// Patch grid dimensions and frame tracking
	connect(this->applicationController, &ApplicationController::patchGridConfigured,
	        gridWidget, &PSFGridWidget::setPatchGridDimensions);
	connect(this->applicationController, &ApplicationController::frameChanged,
	        gridWidget, &PSFGridWidget::setCurrentFrame);

	// Live PSF grid view update 
	connect(this->applicationController, &ApplicationController::psfUpdatedForPatch,
	        gridWidget, &PSFGridWidget::updateSinglePSF);

	// Auto-sync orientation with viewers (when sync is on)
	connect(this->sessionViewer, &ImageSessionViewer::viewerTransformChanged,
	        gridWidget, &PSFGridWidget::applyViewTransform);

	// Sync toggle — reorder so PSFGridWidget knows sync state before
	// ImageSessionViewer broadcasts the current transform
	disconnect(this->viewerToolBar, &ViewerToolBar::syncViewsToggled,
	           this->sessionViewer, &ImageSessionViewer::setViewSyncEnabled);
	connect(this->viewerToolBar, &ViewerToolBar::syncViewsToggled,
	        gridWidget, &PSFGridWidget::setSyncActive);
	connect(this->viewerToolBar, &ViewerToolBar::syncViewsToggled,
	        this->sessionViewer, &ImageSessionViewer::setViewSyncEnabled);

	LOG_DEBUG() << "PSFGridWidget signal connections established";
}

void MainWindow::openImageData() {
	QString selectedFilter = this->lastNameFilterInput;
	const QString initialDir = this->lastOpenDirInput.isEmpty()	? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : this->lastOpenDirInput;
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"Open Image Data",
		initialDir,
		SupportedFileChecker::getFileDialogFilters(),
		&selectedFilter
	);

	if (!filePath.isEmpty()) {
		this->lastOpenDirInput = QFileInfo(filePath).absolutePath();
		this->lastNameFilterInput = selectedFilter;

		this->applicationController->requestOpenInputFile(filePath);
	}
}

void MainWindow::openImageFolder() {
	const QString initialDir = this->lastOpenDirFolder.isEmpty()
		? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
		: this->lastOpenDirFolder;

	const QString folderPath = QFileDialog::getExistingDirectory(
		this,
		"Open Image Folder",
		initialDir
	);

	if (!folderPath.isEmpty()) {
		this->lastOpenDirFolder = folderPath;
		this->applicationController->requestOpenInputFolder(folderPath);
	}
}

void MainWindow::openGroundTruth() {
	if (!this->applicationController->hasInputData()) {
		//NOTIFY_NOTICE("Please open image data first before loading ground truth.");
		LOG_ERROR() << "Please open image data first before loading ground truth.";
		return;
	}

	// Use last stored directory (fallback: Documents) and last-used name filter
	QString selectedFilter = this->lastNameFilterGroundTruth;
	const QString initialDir = this->lastOpenDirGroundTruth.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) : this->lastOpenDirGroundTruth;

	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"Open Ground Truth",
		initialDir,
		SupportedFileChecker::getFileDialogFilters(),
		&selectedFilter
	);

	if (!filePath.isEmpty()) {
		this->lastOpenDirGroundTruth = QFileInfo(filePath).absolutePath();
		this->lastNameFilterGroundTruth = selectedFilter;

		this->applicationController->requestOpenGroundTruthFile(filePath);
	}
}

void MainWindow::saveParameters() {
	const QString initialDir = this->lastOpenDirCoefficients.isEmpty()
		? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
		: this->lastOpenDirCoefficients;
	const QString filePath = QFileDialog::getSaveFileName(
		this,
		tr("Save Wavefront Coefficients"),
		initialDir,
		"CSV Files (*.csv)"
	);
	if (!filePath.isEmpty()) {
		this->lastOpenDirCoefficients = QFileInfo(filePath).absolutePath();
		QMessageBox busyMsg(QMessageBox::NoIcon, tr("Saving"), tr("Saving wavefront coefficients..."), QMessageBox::NoButton, this);
		busyMsg.setStandardButtons(QMessageBox::NoButton);
		busyMsg.show();
		QApplication::processEvents();

		this->applicationController->saveParametersToFile(filePath);
		this->statusBar()->showMessage(tr("Wavefront coefficients saved"), 3000);
	}
}

void MainWindow::loadParameters() {
	const QString initialDir = this->lastOpenDirCoefficients.isEmpty()
		? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
		: this->lastOpenDirCoefficients;
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		tr("Load Wavefront Coefficients"),
		initialDir,
		"CSV Files (*.csv)"
	);
	if (!filePath.isEmpty()) {
		this->lastOpenDirCoefficients = QFileInfo(filePath).absolutePath();
		QMessageBox busyMsg(QMessageBox::NoIcon, tr("Loading"), tr("Loading wavefront coefficients..."), QMessageBox::NoButton, this);
		busyMsg.setStandardButtons(QMessageBox::NoButton);
		busyMsg.show();
		QApplication::processEvents();

		this->applicationController->loadParametersFromFile(filePath);
		this->statusBar()->showMessage(tr("Wavefront coefficients loaded"), 3000);
	}
}

void MainWindow::saveOutputData() {
	if (!this->applicationController->hasOutputData()) {
		this->statusBar()->showMessage("No output data to save", 3000);
		return;
	}
	const QString initialDir = this->lastOpenDirOutput.isEmpty()
		? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
		: this->lastOpenDirOutput;
	const QString filePath = QFileDialog::getSaveFileName(
		this,
		"Save Output Data",
		initialDir,
		"ENVI Files (*.img);;TIFF Image (*.tif);;PNG Image (current frame only) (*.png)"
	);
	if (!filePath.isEmpty()) {
		this->lastOpenDirOutput = QFileInfo(filePath).absolutePath();
		this->applicationController->saveOutputToFile(filePath);
		this->statusBar()->showMessage("Output data saved", 3000);
	}
}

void MainWindow::onInputFileLoaded(const QString& filePath) {
	QFileInfo fileInfo(filePath);
	QString message = QString("Input data loaded: %1 (%2x%3x%4)")
		.arg(fileInfo.baseName())
		.arg(this->applicationController->getInputWidth())
		.arg(this->applicationController->getInputHeight())
		.arg(this->applicationController->getInputFrames());

	this->statusBar()->showMessage(message, 5000);

	LOG_INFO() << "Input file loaded successfully:" << filePath;
}

void MainWindow::onFileLoadError(const QString& filePath, const QString& error) {
	QFileInfo fileInfo(filePath);
	QString message = QString("Failed to load: %1").arg(fileInfo.fileName());

	this->statusBar()->showMessage(message, 5000);

	QMessageBox::warning(this, "File Load Error",
		QString("Failed to load file:\n%1\n\nError: %2").arg(filePath, error));

	LOG_WARNING() << "File load error:" << filePath << "Error:" << error;
}

void MainWindow::selectStyle(QAction* action) {
	// Get the stored enum value from the action's data - completely generic!
	StyleManager::StyleMode mode = static_cast<StyleManager::StyleMode>(action->data().toInt());
	this->styleManager->setStyleMode(mode);
}

void MainWindow::updateStyleMenuChecks(StyleManager::StyleMode newStyle) {
	for (QAction* action : qAsConst(this->styleActions)) {
		StyleManager::StyleMode actionMode = static_cast<StyleManager::StyleMode>(action->data().toInt());
		action->setChecked(actionMode == newStyle);
	}
}

void MainWindow::loadSettings() {
	QVariantMap settings = this->guiSettings->getStoredSettings(SETTINGS_GROUP);

	this->windowSize         = settings.value(KEY_WINDOW_SIZE,     QSize(1200, 800)).toSize();
	this->windowPosition     = settings.value(KEY_WINDOW_POSITION,  QPoint(100, 100)).toPoint();
	this->lastOpenDirInput   = settings.value(KEY_LAST_OPEN_DIR_INPUT,  QString()).toString();
	this->lastOpenDirFolder  = settings.value(KEY_LAST_OPEN_DIR_FOLDER, QString()).toString();
	this->lastOpenDirGroundTruth    = settings.value(KEY_LAST_OPEN_DIR_GT,   QString()).toString();
	this->lastNameFilterInput       = settings.value(KEY_LAST_FILTER_INPUT,  QString()).toString();
	this->lastNameFilterGroundTruth = settings.value(KEY_LAST_FILTER_GT,     QString()).toString();
	this->lastOpenDirCoefficients  = settings.value(KEY_LAST_OPEN_DIR_COEFF, QString()).toString();
	this->lastOpenDirOutput        = settings.value(KEY_LAST_OPEN_DIR_OUTPUT, QString()).toString();
	this->recentInput->setSettings(this->guiSettings->getStoredSettings(this->recentInput->getName()));
	this->recentGroundTruth->setSettings(this->guiSettings->getStoredSettings(this->recentGroundTruth->getName()));

	if (settings.contains(KEY_DOCK_STATE)) {
		this->restoreState(settings.value(KEY_DOCK_STATE).toByteArray());
	}

	const bool showConsole = settings.value(KEY_CONSOLE_VISIBLE, DEF_CONSOLE_VISIBLE).toBool();
	if (this->messageConsoleDock) this->messageConsoleDock->setVisible(showConsole);
	if (this->toggleMessageConsoleAction) this->toggleMessageConsoleAction->setChecked(showConsole);

	const bool showPSFGrid = settings.value(KEY_PSF_GRID_VISIBLE, DEF_PSF_GRID_VISIBLE).toBool();
	if (this->psfGridDock) this->psfGridDock->setVisible(showPSFGrid);
	if (this->togglePSFGridAction) this->togglePSFGridAction->setChecked(showPSFGrid);

	const bool showCrossSection = settings.value(KEY_CROSS_SECTION_VISIBLE, DEF_CROSS_SECTION_VISIBLE).toBool();
	if (this->sessionViewer) this->sessionViewer->setCrossSectionVisible(showCrossSection);
	if (this->toggleCrossSectionAction) this->toggleCrossSectionAction->setChecked(showCrossSection);

	this->resize(this->windowSize);
	this->move(this->windowPosition);
	if (settings.value(KEY_WINDOW_MAXIMIZED, DEF_WINDOW_MAXIMIZED).toBool()) {
		this->showMaximized();
	}

	// Load widget settings
	this->consoleWidget()->setSettings(this->guiSettings->getStoredSettings(this->consoleWidget()->getName()));
	this->sessionViewer->setSettings(this->guiSettings->getStoredSettings(this->sessionViewer->getName()));
	this->viewerToolBar->setSettings(this->guiSettings->getStoredSettings(this->viewerToolBar->getName()));
	this->psfGenerationWidget->setSettings(this->guiSettings->getStoredSettings(this->psfGenerationWidget->getName()));
	this->processingControlWidget->setSettings(this->guiSettings->getStoredSettings(this->processingControlWidget->getName()));

	PSFGridWidget* gridWidget = qobject_cast<PSFGridWidget*>(this->psfGridDock->widget());
	if (gridWidget) {
		gridWidget->setSettings(this->guiSettings->getStoredSettings(gridWidget->getName()));
	}
}

void MainWindow::saveSettings() {
	// Save normal (non-maximized) geometry so restore works correctly
	if (!this->isMaximized()) {
		this->windowSize = this->size();
		this->windowPosition = this->pos();
	}

	QVariantMap settings;
	settings[KEY_WINDOW_SIZE]      = this->windowSize;
	settings[KEY_WINDOW_POSITION]  = this->windowPosition;
	settings[KEY_WINDOW_MAXIMIZED] = this->isMaximized();
	settings[KEY_LAST_OPEN_DIR_INPUT]  = this->lastOpenDirInput;
	settings[KEY_LAST_OPEN_DIR_FOLDER] = this->lastOpenDirFolder;
	settings[KEY_LAST_OPEN_DIR_GT]    = this->lastOpenDirGroundTruth;
	settings[KEY_LAST_FILTER_INPUT]   = this->lastNameFilterInput;
	settings[KEY_LAST_FILTER_GT]      = this->lastNameFilterGroundTruth;
	settings[KEY_LAST_OPEN_DIR_COEFF]  = this->lastOpenDirCoefficients;
	settings[KEY_LAST_OPEN_DIR_OUTPUT] = this->lastOpenDirOutput;
	settings[KEY_DOCK_STATE]       = this->saveState();
	settings[KEY_CONSOLE_VISIBLE]  = (this->messageConsoleDock && this->messageConsoleDock->isVisible());
	settings[KEY_PSF_GRID_VISIBLE] = (this->psfGridDock && this->psfGridDock->isVisible());
	settings[KEY_CROSS_SECTION_VISIBLE] = (this->sessionViewer && this->sessionViewer->getCrossSectionWidget() && this->sessionViewer->getCrossSectionWidget()->isVisible());
	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);

	this->guiSettings->storeSettings(this->consoleWidget()->getName(), this->consoleWidget()->getSettings());
	this->guiSettings->storeSettings(this->sessionViewer->getName(), this->sessionViewer->getSettings());
	this->guiSettings->storeSettings(this->viewerToolBar->getName(), this->viewerToolBar->getSettings());
	this->guiSettings->storeSettings(this->psfGenerationWidget->getName(), this->psfGenerationWidget->getSettings());
	this->guiSettings->storeSettings(this->processingControlWidget->getName(), this->processingControlWidget->getSettings());
	this->guiSettings->storeSettings(this->recentInput->getName(),       this->recentInput->getSettings());
	this->guiSettings->storeSettings(this->recentGroundTruth->getName(), this->recentGroundTruth->getSettings());

	PSFGridWidget* gridWidget = qobject_cast<PSFGridWidget*>(this->psfGridDock->widget());
	if (gridWidget) {
		this->guiSettings->storeSettings(gridWidget->getName(), gridWidget->getSettings());
	}
}

MessageConsoleWidget *MainWindow::consoleWidget() const
{
	if (!this->messageConsoleDock) return nullptr;
	return qobject_cast<MessageConsoleWidget*>(this->messageConsoleDock->widget());
}
