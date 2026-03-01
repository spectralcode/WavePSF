#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/settingsfilemanager.h"
#include "controller/applicationcontroller.h"
#include "gui/imagesessionviewer/imagesessionviewer.h"
#include "gui/psfcontrol/psfcontrolwidget.h"
#include "gui/psfcontrol/psfsettingsdialog.h"
#include "utils/logging.h"
#include "utils/supportedfilechecker.h"
#include "gui/messageconsole/messagerouter.h"
#include "gui/messageconsole/messageconsoledock.h"


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
#include <QSplitter>

namespace {
	const char* SETTINGS_GROUP = "main_window";
	const char* WINDOW_SIZE_KEY = "window_size";
	const char* WINDOW_POSITION_KEY = "window_position";
	const char* LAST_OPEN_DIR_INPUT_KEY = "last_open_dir_input";
	const char* LAST_OPEN_DIR_GROUND_TRUTH_KEY = "last_open_dir_ground_truth";
	const char* LAST_NAME_FILTER_INPUT_KEY = "last_name_filter_input";
	const char* LAST_NAME_FILTER_GROUND_TRUTH_KEY = "last_name_filter_ground_truth";
	const char* DOCK_STATE_KEY = "dock_state_v1";
	const char* MESSAGE_CONSOLE_VISIBLE_KEY = "message_console_visible";
	const char* SPLITTER_STATE_KEY = "central_splitter_state";
}

MainWindow::MainWindow(SettingsFileManager* guiSettings,
					   StyleManager* styleManager, ApplicationController* applicationController, QWidget *parent)
	: QMainWindow(parent), ui(new Ui::MainWindow),
	  guiSettings(guiSettings), styleManager(styleManager), applicationController(applicationController),
	  fileMenu(nullptr), viewMenu(nullptr), extrasMenu(nullptr), styleMenu(nullptr),
	  openImageDataAction(nullptr), openGroundTruthAction(nullptr),
	  saveParametersAction(nullptr), loadParametersAction(nullptr),
	  centralSplitter(nullptr), sessionViewer(nullptr), psfControlWidget(nullptr) {
	MessageRouter::instance()->install();
	this->ui->setupUi(this);
	this->setupMenuBar();
	this->setupCentralWidget();
	this->connectApplicationController();
	this->connectImageSessionViewer();
	this->connectPSFControlWidget();
	this->loadSettings();

	// Apply loaded PSF settings to the controller (loadSettings only deserializes
	// into PSFControlWidget, this actually activates them in PSFModule)
	this->currentPSFSettings = this->psfControlWidget->getPSFSettings();
	this->applicationController->applyPSFSettings(this->currentPSFSettings);

	// Broadcast initial state after all connections are made
	this->applicationController->broadcastCurrentState();

	//style manager init
	connect(this->styleManager, &StyleManager::styleChanged, this, &MainWindow::updateStyleMenuChecks);
	this->updateStyleMenuChecks(this->styleManager->getStyleMode());

	LOG_INFO() << tr("test info");
	LOG_WARNING() << tr("test warning");
	LOG_DEBUG() << "test debug";
}

MainWindow::~MainWindow() {
	delete this->ui;
}

void MainWindow::closeEvent(QCloseEvent *event) {
	this->saveSettings();
	event->accept();
}

void MainWindow::setupMenuBar() {
	this->setupFileMenu();
	this->setupViewMenu();
	this->setupExtrasMenu();
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

	this->fileMenu->addSeparator();

	// Save Parameters action
	this->saveParametersAction = new QAction("&Save Parameters...", this);
	this->saveParametersAction->setShortcut(QKeySequence::Save);
	this->saveParametersAction->setStatusTip("Save wavefront parameters to CSV file");
	connect(this->saveParametersAction, &QAction::triggered, this, &MainWindow::saveParameters);
	this->fileMenu->addAction(this->saveParametersAction);

	// Load Parameters action
	this->loadParametersAction = new QAction("&Load Parameters...", this);
	this->loadParametersAction->setStatusTip("Load wavefront parameters from CSV file");
	connect(this->loadParametersAction, &QAction::triggered, this, &MainWindow::loadParameters);
	this->fileMenu->addAction(this->loadParametersAction);

	this->fileMenu->addSeparator();

	// Exit action
	QAction* exitAction = new QAction("E&xit", this);
	exitAction->setShortcut(QKeySequence::Quit);
	exitAction->setStatusTip("Exit the application");
	connect(exitAction, &QAction::triggered, this, &QWidget::close);
	this->fileMenu->addAction(exitAction);
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


	// Toggle action in View menu
	this->toggleMessageConsoleAction = new QAction("Message Console", this);
	this->toggleMessageConsoleAction->setShortcut(QKeySequence(Qt::Key_F12));
	this->toggleMessageConsoleAction->setCheckable(true);
	this->viewMenu->addSeparator();
	this->viewMenu->addAction(this->toggleMessageConsoleAction);

	// Create dock and wire it
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

void MainWindow::openSettings() {
	PSFSettingsDialog dialog(this->currentPSFSettings, this);
	connect(&dialog, &PSFSettingsDialog::settingsApplied,
			this->applicationController, &ApplicationController::applyPSFSettings);
	if (dialog.exec() == QDialog::Accepted) {
		this->applicationController->applyPSFSettings(dialog.getSettings());
	}
}

void MainWindow::setupCentralWidget()
{
	this->sessionViewer = new ImageSessionViewer(this);
	this->psfControlWidget = new PSFControlWidget(this);

	this->centralSplitter = new QSplitter(Qt::Vertical, this);
	this->centralSplitter->addWidget(this->sessionViewer);
	this->centralSplitter->addWidget(this->psfControlWidget);
	this->centralSplitter->setStretchFactor(0, 3);
	this->centralSplitter->setStretchFactor(1, 1);

	this->setCentralWidget(this->centralSplitter);
}

void MainWindow::connectApplicationController() {
	if (this->applicationController != nullptr) {
		// Connect file loading responses
		connect(this->applicationController, &ApplicationController::inputFileLoaded,
				this, &MainWindow::onInputFileLoaded);
		connect(this->applicationController, &ApplicationController::fileLoadError,
				this, &MainWindow::onFileLoadError);

		// Keep local copy of PSF settings in sync for the settings dialog
		connect(this->applicationController, &ApplicationController::psfSettingsUpdated,
				this, [this](const PSFSettings& s) { this->currentPSFSettings = s; });
	}
}

void MainWindow::connectImageSessionViewer() {
	if (this->applicationController != nullptr && this->sessionViewer != nullptr) {
		// ImageSessionViewer requests → ApplicationController actions
		connect(this->sessionViewer, &ImageSessionViewer::frameChangeRequested,
				this->applicationController, &ApplicationController::setCurrentFrame);
		connect(this->sessionViewer, &ImageSessionViewer::patchChangeRequested,
				this->applicationController, &ApplicationController::setCurrentPatch);
		connect(this->sessionViewer, &ImageSessionViewer::patchGridConfigurationRequested,
				this->applicationController, &ApplicationController::configurePatchGrid);
		connect(this->sessionViewer, &ImageSessionViewer::inputFileDropRequested,
				this->applicationController, &ApplicationController::requestOpenInputFile);

		// ApplicationController state changes → ImageSessionViewer updates
		connect(this->applicationController, &ApplicationController::imageSessionChanged,
				this->sessionViewer, &ImageSessionViewer::updateImageSession);
		connect(this->applicationController, &ApplicationController::frameChanged,
				this->sessionViewer, &ImageSessionViewer::setCurrentFrame);
		connect(this->applicationController, &ApplicationController::patchChanged,
				this->sessionViewer, &ImageSessionViewer::highlightPatch);
		connect(this->applicationController, &ApplicationController::patchGridConfigured,
				this->sessionViewer, &ImageSessionViewer::configurePatchGrid);

		LOG_DEBUG() << "ImageSessionViewer signal connections established";
	}
}

void MainWindow::connectPSFControlWidget() {
	if (this->applicationController != nullptr && this->psfControlWidget != nullptr) {
		// PSFControlWidget requests → ApplicationController
		connect(this->psfControlWidget, &PSFControlWidget::coefficientChanged,
				this->applicationController, &ApplicationController::setPSFCoefficient);
		connect(this->psfControlWidget, &PSFControlWidget::resetRequested,
				this->applicationController, &ApplicationController::resetPSFCoefficients);

		// ApplicationController results → PSFControlWidget
		connect(this->applicationController, &ApplicationController::psfWavefrontUpdated,
				this->psfControlWidget, &PSFControlWidget::updateWavefront);
		connect(this->applicationController, &ApplicationController::psfUpdated,
				this->psfControlWidget, &PSFControlWidget::updatePSF);
		connect(this->applicationController, &ApplicationController::psfParameterDescriptorsChanged,
				this->psfControlWidget, &PSFControlWidget::setParameterDescriptors);

		// Deconvolution settings: PSFControlWidget → ApplicationController
		connect(this->psfControlWidget, &PSFControlWidget::deconvAlgorithmChanged,
				this->applicationController, &ApplicationController::setDeconvolutionAlgorithm);
		connect(this->psfControlWidget, &PSFControlWidget::deconvIterationsChanged,
				this->applicationController, &ApplicationController::setDeconvolutionIterations);
		connect(this->psfControlWidget, &PSFControlWidget::deconvRelaxationFactorChanged,
				this->applicationController, &ApplicationController::setDeconvolutionRelaxationFactor);
		connect(this->psfControlWidget, &PSFControlWidget::deconvRegularizationFactorChanged,
				this->applicationController, &ApplicationController::setDeconvolutionRegularizationFactor);
		connect(this->psfControlWidget, &PSFControlWidget::deconvNoiseToSignalFactorChanged,
				this->applicationController, &ApplicationController::setDeconvolutionNoiseToSignalFactor);
		connect(this->psfControlWidget, &PSFControlWidget::deconvLiveModeChanged,
				this->applicationController, &ApplicationController::setDeconvolutionLiveMode);
		connect(this->psfControlWidget, &PSFControlWidget::deconvolutionRequested,
				this->applicationController, &ApplicationController::requestDeconvolution);

		// Deconvolution completed → refresh output viewer
		connect(this->applicationController, &ApplicationController::deconvolutionCompleted,
				this->sessionViewer, &ImageSessionViewer::refreshOutputViewer);

		// Loaded coefficients → update GUI sliders
		connect(this->applicationController, &ApplicationController::coefficientsLoaded,
				this->psfControlWidget, &PSFControlWidget::setCoefficients);

		// PSF settings: ApplicationController → PSFControlWidget (for initial broadcast + updates)
		connect(this->applicationController, &ApplicationController::psfSettingsUpdated,
				this->psfControlWidget, &PSFControlWidget::setPSFSettings);

		// Optimization: PSFControlWidget → ApplicationController
		connect(this->psfControlWidget, &PSFControlWidget::optimizationRequested,
				this->applicationController, &ApplicationController::startOptimization);
		connect(this->psfControlWidget, &PSFControlWidget::optimizationCancelRequested,
				this->applicationController, &ApplicationController::cancelOptimization);

		// Optimization: ApplicationController → PSFControlWidget
		connect(this->applicationController, &ApplicationController::optimizationStarted,
				this->psfControlWidget, &PSFControlWidget::onOptimizationStarted);
		connect(this->applicationController, &ApplicationController::optimizationProgressUpdated,
				this->psfControlWidget, &PSFControlWidget::updateOptimizationProgress);
		connect(this->applicationController, &ApplicationController::optimizationFinished,
				this->psfControlWidget, &PSFControlWidget::onOptimizationFinished);

		// Ground truth availability → optimization widget
		connect(this->applicationController, &ApplicationController::groundTruthFileLoaded,
				this, [this](const QString&) {
					this->psfControlWidget->setGroundTruthAvailable(true);
				});

		// Multi-patch highlighting from optimization widget
		connect(this->psfControlWidget, &PSFControlWidget::optimizationPatchSelectionChanged,
				this->sessionViewer, &ImageSessionViewer::highlightPatches);

		LOG_DEBUG() << "PSFControlWidget signal connections established";
	}
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

	this->windowSize = settings.value(WINDOW_SIZE_KEY, QSize(800, 600)).toSize();
	this->windowPosition = settings.value(WINDOW_POSITION_KEY, QPoint(100, 100)).toPoint();
	this->lastOpenDirInput = settings.value(LAST_OPEN_DIR_INPUT_KEY, QString()).toString();
	this->lastOpenDirGroundTruth = settings.value(LAST_OPEN_DIR_GROUND_TRUTH_KEY, QString()).toString();
	this->lastNameFilterInput = settings.value(LAST_NAME_FILTER_INPUT_KEY, QString()).toString();
	this->lastNameFilterGroundTruth = settings.value(LAST_NAME_FILTER_GROUND_TRUTH_KEY, QString()).toString();

	if (settings.contains(DOCK_STATE_KEY)) {
		this->restoreState(settings.value(DOCK_STATE_KEY).toByteArray());
	}

	const bool showConsole = settings.value(MESSAGE_CONSOLE_VISIBLE_KEY, false).toBool();
	if (this->messageConsoleDock) this->messageConsoleDock->setVisible(showConsole);
	if (this->toggleMessageConsoleAction) this->toggleMessageConsoleAction->setChecked(showConsole);

	if (settings.contains(SPLITTER_STATE_KEY) && this->centralSplitter) {
		this->centralSplitter->restoreState(settings.value(SPLITTER_STATE_KEY).toByteArray());
	}

	this->resize(this->windowSize);
	this->move(this->windowPosition);

	//load widget settings
	this->consoleWidget()->setSettings(this->guiSettings->getStoredSettings(this->consoleWidget()->getName()));
	this->sessionViewer->setSettings(this->guiSettings->getStoredSettings(this->sessionViewer->getName()));
	this->psfControlWidget->setSettings(this->guiSettings->getStoredSettings(this->psfControlWidget->getName()));
}

void MainWindow::saveSettings() {
	this->windowSize = this->size();
	this->windowPosition = this->pos();

	QVariantMap settings;
	settings[WINDOW_SIZE_KEY] = this->windowSize;
	settings[WINDOW_POSITION_KEY] = this->windowPosition;
	settings[LAST_OPEN_DIR_INPUT_KEY] = this->lastOpenDirInput;
	settings[LAST_OPEN_DIR_GROUND_TRUTH_KEY] = this->lastOpenDirGroundTruth;
	settings[LAST_NAME_FILTER_INPUT_KEY] = this->lastNameFilterInput;
	settings[LAST_NAME_FILTER_GROUND_TRUTH_KEY] = this->lastNameFilterGroundTruth;
	settings[DOCK_STATE_KEY] = this->saveState();
	settings[MESSAGE_CONSOLE_VISIBLE_KEY] = (this->messageConsoleDock && this->messageConsoleDock->isVisible());
	if (this->centralSplitter) {
		settings[SPLITTER_STATE_KEY] = this->centralSplitter->saveState();
	}
	this->guiSettings->storeSettings(SETTINGS_GROUP, settings);

	this->guiSettings->storeSettings(this->consoleWidget()->getName(), this->consoleWidget()->getSettings());
	this->guiSettings->storeSettings(this->sessionViewer->getName(), this->sessionViewer->getSettings());
	this->guiSettings->storeSettings(this->psfControlWidget->getName(), this->psfControlWidget->getSettings());
}

MessageConsoleWidget *MainWindow::consoleWidget() const
{
	if (!this->messageConsoleDock) return nullptr;
	return qobject_cast<MessageConsoleWidget*>(this->messageConsoleDock->widget());
}
