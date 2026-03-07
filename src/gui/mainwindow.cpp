#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/settingsfilemanager.h"
#include "controller/applicationcontroller.h"
#include "gui/imagesessionviewer/imagesessionviewer.h"
#include "gui/psfcontrol/psfgenerationwidget.h"
#include "gui/psfcontrol/processingcontrolwidget.h"
#include "gui/psfcontrol/psfsettingsdialog.h"
#include "utils/logging.h"
#include "utils/supportedfilechecker.h"
#include "gui/messageconsole/messagerouter.h"
#include "gui/messageconsole/messageconsoledock.h"
#include "gui/aboutdialog.h"
#include "gui/shortcutsdialog.h"
#include "gui/recentfilesmenu.h"


#include <QFileDialog>
#include <QMessageBox>
#include <QImage>
#include <QPixmap>
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

namespace {
	const QString SETTINGS_GROUP       = QStringLiteral("main_window");
	const QString KEY_WINDOW_SIZE      = QStringLiteral("window_size");
	const QString KEY_WINDOW_POSITION  = QStringLiteral("window_position");
	const QString KEY_WINDOW_MAXIMIZED = QStringLiteral("window_maximized");
	const QString KEY_LAST_OPEN_DIR_INPUT = QStringLiteral("last_open_dir_input");
	const QString KEY_LAST_OPEN_DIR_GT    = QStringLiteral("last_open_dir_ground_truth");
	const QString KEY_LAST_FILTER_INPUT   = QStringLiteral("last_name_filter_input");
	const QString KEY_LAST_FILTER_GT      = QStringLiteral("last_name_filter_ground_truth");
	const QString KEY_DOCK_STATE          = QStringLiteral("dock_state_v1");
	const QString KEY_CONSOLE_VISIBLE     = QStringLiteral("message_console_visible");
	const bool DEF_WINDOW_MAXIMIZED = false;
	const bool DEF_CONSOLE_VISIBLE  = false;
}

MainWindow::MainWindow(SettingsFileManager* guiSettings,
					   StyleManager* styleManager, ApplicationController* applicationController, QWidget *parent)
	: QMainWindow(parent), ui(new Ui::MainWindow),
	  guiSettings(guiSettings), styleManager(styleManager), applicationController(applicationController),
	  fileMenu(nullptr), recentInput(nullptr), recentGroundTruth(nullptr),
	  psfMenu(nullptr), processingMenu(nullptr),
	  viewMenu(nullptr), extrasMenu(nullptr), styleMenu(nullptr),
	  openImageDataAction(nullptr), openGroundTruthAction(nullptr),
	  saveParametersAction(nullptr), loadParametersAction(nullptr), saveOutputAction(nullptr),
	  deconvolveAllAction(nullptr),
	  sessionViewer(nullptr),
	  psfGenerationWidget(nullptr), processingControlWidget(nullptr),
	  settingsDialog(nullptr), shortcutsDialog(nullptr), aboutDialog(nullptr) {
	MessageRouter::instance()->install();
	this->ui->setupUi(this);
	this->setupMenuBar();
	this->setupCentralWidget();
	this->connectApplicationController();
	this->connectImageSessionViewer();
	this->connectPSFGenerationWidget();
	this->connectProcessingControlWidget();
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

void MainWindow::setupPSFMenu() {
	this->psfMenu = this->menuBar()->addMenu("&PSF");

	this->loadPSFAction = new QAction("&Load PSF from File...", this);
	this->loadPSFAction->setStatusTip("Load a PSF kernel from an image file");
	connect(this->loadPSFAction, &QAction::triggered, this, &MainWindow::loadPSF);
	this->psfMenu->addAction(this->loadPSFAction);

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

	this->psfMenu->addSeparator();

	this->useCustomPSFFolderAction = new QAction("&Use Custom PSF Folder", this);
	this->useCustomPSFFolderAction->setCheckable(true);
	this->useCustomPSFFolderAction->setStatusTip("Load PSFs from folder on patch/frame switch instead of computing");
	connect(this->useCustomPSFFolderAction, &QAction::toggled,
			this->applicationController, &ApplicationController::setUseCustomPSFFolder);
	this->psfMenu->addAction(this->useCustomPSFFolderAction);

	this->setCustomPSFFolderAction = new QAction("Set &Custom PSF Folder...", this);
	this->setCustomPSFFolderAction->setStatusTip("Set the folder containing per-patch PSF files");
	connect(this->setCustomPSFFolderAction, &QAction::triggered, this, &MainWindow::setCustomPSFFolder);
	this->psfMenu->addAction(this->setCustomPSFFolderAction);
}

void MainWindow::setupProcessingMenu() {
	this->processingMenu = this->menuBar()->addMenu("P&rocessing");

	this->deconvolveAllAction = new QAction("Deconvolve &All Frames", this);
	this->deconvolveAllAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
	this->deconvolveAllAction->setStatusTip("Deconvolve all patches of every frame using stored coefficients");
	this->deconvolveAllAction->setEnabled(false);
	connect(this->deconvolveAllAction, &QAction::triggered, this, &MainWindow::deconvolveAll);
	this->processingMenu->addAction(this->deconvolveAllAction);
}

void MainWindow::deconvolveAll() {
	this->applicationController->requestBatchDeconvolution();
}

void MainWindow::loadPSF() {
	const QString filePath = QFileDialog::getOpenFileName(
		this, "Load PSF", QString(),
		"Images (*.tif *.tiff *.png *.jpg *.jpeg *.bmp);;All Files (*)");
	if (!filePath.isEmpty()) {
		this->applicationController->loadPSFFromFile(filePath);
		this->statusBar()->showMessage("PSF loaded from file", 3000);
	}
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

void MainWindow::setCustomPSFFolder() {
	const QString dir = QFileDialog::getExistingDirectory(
		this, "Select Custom PSF Folder");
	if (!dir.isEmpty()) {
		this->applicationController->setCustomPSFFolder(dir);
		this->statusBar()->showMessage("Custom PSF folder: " + dir, 3000);
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

	this->settingsDialog = new PSFSettingsDialog(
		this->currentPSFSettings,
		this->sessionViewer->getAutoRangeEnabled(),
		this->sessionViewer->getDisplayRangeMin(),
		this->sessionViewer->getDisplayRangeMax(),
		this->sessionViewer->isViewSyncEnabled(),
		this);
	this->settingsDialog->setAttribute(Qt::WA_DeleteOnClose);

	connect(this->settingsDialog, &PSFSettingsDialog::settingsApplied,
			this->applicationController, &ApplicationController::applyPSFSettings);
	connect(this->settingsDialog, &PSFSettingsDialog::displaySettingsApplied,
			this->sessionViewer, &ImageSessionViewer::setDisplaySettings);
	connect(this->settingsDialog, &PSFSettingsDialog::viewSyncChanged,
			this->sessionViewer, &ImageSessionViewer::setViewSyncEnabled);
	connect(this->settingsDialog, &QDialog::accepted, this, [this]() {
		this->applicationController->applyPSFSettings(this->settingsDialog->getSettings());
		this->sessionViewer->setDisplaySettings(
			this->settingsDialog->getAutoRange(),
			this->settingsDialog->getDisplayMin(),
			this->settingsDialog->getDisplayMax());
	});
	connect(this->settingsDialog, &QDialog::destroyed, this, [this]() {
		this->settingsDialog = nullptr;
	});

	// Keep dialog in sync when generator type changes externally
	connect(this->applicationController, &ApplicationController::generatorTypeChanged,
		this->settingsDialog, &PSFSettingsDialog::updateGeneratorType);

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

		// Enable batch deconvolution action when parameters are loaded
		connect(this->applicationController, &ApplicationController::parametersLoaded,
				this, [this]() { this->deconvolveAllAction->setEnabled(true); });

		// Disable batch deconvolution action when session is closed
		connect(this->applicationController, &ApplicationController::sessionClosed,
				this, [this]() { this->deconvolveAllAction->setEnabled(false); });
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

	// Generator type switching
	connect(genWidget, &PSFGenerationWidget::generatorTypeChangeRequested,
			this->applicationController, &ApplicationController::setGeneratorType);
	connect(this->applicationController, &ApplicationController::generatorTypeChanged,
			genWidget, &PSFGenerationWidget::setGeneratorType);

	LOG_DEBUG() << "PSFGenerationWidget signal connections established";
}

void MainWindow::connectProcessingControlWidget() {
	if (this->applicationController == nullptr) return;

	auto* ctrl = this->processingControlWidget;

	// Parameter descriptors → ProcessingControlWidget (for optimization widget)
	connect(this->applicationController, &ApplicationController::psfParameterDescriptorsChanged,
			ctrl, &ProcessingControlWidget::setParameterDescriptors);

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
	const QString filePath = QFileDialog::getSaveFileName(
		this,
		"Save Parameters",
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
		"CSV Files (*.csv)"
	);
	if (!filePath.isEmpty()) {
		this->applicationController->saveParametersToFile(filePath);
		this->statusBar()->showMessage("Parameters saved", 3000);
	}
}

void MainWindow::loadParameters() {
	const QString filePath = QFileDialog::getOpenFileName(
		this,
		"Load Parameters",
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
		"CSV Files (*.csv)"
	);
	if (!filePath.isEmpty()) {
		this->applicationController->loadParametersFromFile(filePath);
		this->statusBar()->showMessage("Parameters loaded", 3000);
	}
}

void MainWindow::saveOutputData() {
	if (!this->applicationController->hasOutputData()) {
		this->statusBar()->showMessage("No output data to save", 3000);
		return;
	}
	const QString filePath = QFileDialog::getSaveFileName(
		this,
		"Save Output Data",
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
		"ENVI Files (*.img);;TIFF Image (*.tif);;PNG Image (current frame only) (*.png)"
	);
	if (!filePath.isEmpty()) {
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
	this->lastOpenDirInput   = settings.value(KEY_LAST_OPEN_DIR_INPUT, QString()).toString();
	this->lastOpenDirGroundTruth    = settings.value(KEY_LAST_OPEN_DIR_GT,   QString()).toString();
	this->lastNameFilterInput       = settings.value(KEY_LAST_FILTER_INPUT,  QString()).toString();
	this->lastNameFilterGroundTruth = settings.value(KEY_LAST_FILTER_GT,     QString()).toString();
	this->recentInput->setSettings(this->guiSettings->getStoredSettings(this->recentInput->getName()));
	this->recentGroundTruth->setSettings(this->guiSettings->getStoredSettings(this->recentGroundTruth->getName()));

	if (settings.contains(KEY_DOCK_STATE)) {
		this->restoreState(settings.value(KEY_DOCK_STATE).toByteArray());
	}

	const bool showConsole = settings.value(KEY_CONSOLE_VISIBLE, DEF_CONSOLE_VISIBLE).toBool();
	if (this->messageConsoleDock) this->messageConsoleDock->setVisible(showConsole);
	if (this->toggleMessageConsoleAction) this->toggleMessageConsoleAction->setChecked(showConsole);

	this->resize(this->windowSize);
	this->move(this->windowPosition);
	if (settings.value(KEY_WINDOW_MAXIMIZED, DEF_WINDOW_MAXIMIZED).toBool()) {
		this->showMaximized();
	}

	// Load widget settings
	this->consoleWidget()->setSettings(this->guiSettings->getStoredSettings(this->consoleWidget()->getName()));
	this->sessionViewer->setSettings(this->guiSettings->getStoredSettings(this->sessionViewer->getName()));
	this->psfGenerationWidget->setSettings(this->guiSettings->getStoredSettings(this->psfGenerationWidget->getName()));
	this->processingControlWidget->setSettings(this->guiSettings->getStoredSettings(this->processingControlWidget->getName()));
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
	settings[KEY_LAST_OPEN_DIR_INPUT] = this->lastOpenDirInput;
	settings[KEY_LAST_OPEN_DIR_GT]    = this->lastOpenDirGroundTruth;
	settings[KEY_LAST_FILTER_INPUT]   = this->lastNameFilterInput;
	settings[KEY_LAST_FILTER_GT]      = this->lastNameFilterGroundTruth;
	settings[KEY_DOCK_STATE]       = this->saveState();
	settings[KEY_CONSOLE_VISIBLE]  = (this->messageConsoleDock && this->messageConsoleDock->isVisible());
	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);

	this->guiSettings->storeSettings(this->consoleWidget()->getName(), this->consoleWidget()->getSettings());
	this->guiSettings->storeSettings(this->sessionViewer->getName(), this->sessionViewer->getSettings());
	this->guiSettings->storeSettings(this->psfGenerationWidget->getName(), this->psfGenerationWidget->getSettings());
	this->guiSettings->storeSettings(this->processingControlWidget->getName(), this->processingControlWidget->getSettings());
	this->guiSettings->storeSettings(this->recentInput->getName(),       this->recentInput->getSettings());
	this->guiSettings->storeSettings(this->recentGroundTruth->getName(), this->recentGroundTruth->getSettings());
}

MessageConsoleWidget *MainWindow::consoleWidget() const
{
	if (!this->messageConsoleDock) return nullptr;
	return qobject_cast<MessageConsoleWidget*>(this->messageConsoleDock->widget());
}
