#include "deconvolutionsettingswidget.h"
#include "core/psf/deconvolver.h"
#include "core/processing/volumetricdeconvolver.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include "gui/verticalscrollarea.h"
#include <QEvent>

namespace {
	const QString SETTINGS_GROUP = QStringLiteral("deconvolution_settings");

	// Key names
	const QString KEY_ALGORITHM             = QStringLiteral("algorithm");
	const QString KEY_ITERATIONS            = QStringLiteral("iterations");
	const QString KEY_ITERATIONS_RL         = QStringLiteral("iterations_rl");
	const QString KEY_ITERATIONS_LW         = QStringLiteral("iterations_landweber");
	const QString KEY_ITERATIONS_RL3D       = QStringLiteral("iterations_rl3d");
	const QString KEY_RELAXATION_FACTOR     = QStringLiteral("relaxation_factor");
	const QString KEY_REGULARIZATION_FACTOR = QStringLiteral("regularization_factor");
	const QString KEY_NOISE_TO_SIGNAL       = QStringLiteral("noise_to_signal_factor");
	const QString KEY_PADDING_MODE          = QStringLiteral("padding_mode");
	const QString KEY_ACCELERATION          = QStringLiteral("acceleration_enabled");
	const QString KEY_LIVE_MODE             = QStringLiteral("live_mode");

	// Default values
	const int    DEF_ALGORITHM             = 0;
	const int    DEF_ITERATIONS            = 128;
	const int    DEF_ITERATIONS_RL         = 128;
	const int    DEF_ITERATIONS_LW         = 128;
	const int    DEF_ITERATIONS_RL3D       = 16;
	const double DEF_RELAXATION_FACTOR     = 0.65;
	const double DEF_REGULARIZATION_FACTOR = 0.005;
	const double DEF_NOISE_TO_SIGNAL       = 0.01;
	const int    DEF_PADDING_MODE          = VolumetricDeconvolver::MIRROR_PAD;
	const int    DEF_ACCELERATION          = VolumetricDeconvolver::ACCEL_BIGGS_ANDREWS;
	const bool   DEF_LIVE_MODE             = true;
}


DeconvolutionSettingsWidget::DeconvolutionSettingsWidget(QWidget* parent)
	: QWidget(parent), previousAlgorithm(DEF_ALGORITHM)
{
	this->iterationsPerAlgorithm[Deconvolver::RICHARDSON_LUCY]    = DEF_ITERATIONS_RL;
	this->iterationsPerAlgorithm[Deconvolver::LANDWEBER]          = DEF_ITERATIONS_LW;
	this->iterationsPerAlgorithm[Deconvolver::RICHARDSON_LUCY_3D] = DEF_ITERATIONS_RL3D;
	this->setupUI();
}

DeconvolutionSettingsWidget::~DeconvolutionSettingsWidget()
{
}

void DeconvolutionSettingsWidget::setupUI()
{
	QHBoxLayout* mainLayout = new QHBoxLayout(this);

	VerticalScrollArea* scrollArea = new VerticalScrollArea(this);

	QWidget* controlsWidget = new QWidget(scrollArea);
	QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);

	// Form layout for algorithm parameters
	QFormLayout* formLayout = new QFormLayout();

	// Algorithm selection
	this->algorithmComboBox = new QComboBox(controlsWidget);
	this->algorithmComboBox->addItems(Deconvolver::getAlgorithmNames());
	this->installScrollGuard(this->algorithmComboBox);
	formLayout->addRow(tr("Algorithm:"), this->algorithmComboBox);

	// Iterations
	this->iterationsLabel = new QLabel(tr("Iterations:"), controlsWidget);
	this->iterationsSpinBox = new QSpinBox(controlsWidget);
	this->iterationsSpinBox->setRange(1, 10240);
	this->iterationsSpinBox->setValue(DEF_ITERATIONS);
	this->installScrollGuard(this->iterationsSpinBox);
	formLayout->addRow(this->iterationsLabel, this->iterationsSpinBox);

	// Relaxation factor (Landweber)
	this->relaxationLabel = new QLabel(tr("Relaxation Factor:"), controlsWidget);
	this->relaxationFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->relaxationFactorSpinBox->setRange(0.0001, 1000.0);
	this->relaxationFactorSpinBox->setDecimals(4);
	this->relaxationFactorSpinBox->setValue(DEF_RELAXATION_FACTOR);
	this->relaxationFactorSpinBox->setSingleStep(0.01);
	this->installScrollGuard(this->relaxationFactorSpinBox);
	formLayout->addRow(this->relaxationLabel, this->relaxationFactorSpinBox);

	// Regularization factor (Tikhonov)
	this->regularizationLabel = new QLabel(tr("Regularization:"), controlsWidget);
	this->regularizationFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->regularizationFactorSpinBox->setRange(0.0001, 1.0);
	this->regularizationFactorSpinBox->setDecimals(4);
	this->regularizationFactorSpinBox->setValue(DEF_REGULARIZATION_FACTOR);
	this->regularizationFactorSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->regularizationFactorSpinBox);
	formLayout->addRow(this->regularizationLabel, this->regularizationFactorSpinBox);

	// Noise-to-signal factor (Wiener)
	this->noiseToSignalLabel = new QLabel(tr("NSR Factor:"), controlsWidget);
	this->noiseToSignalFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->noiseToSignalFactorSpinBox->setRange(0.0001, 1.0);
	this->noiseToSignalFactorSpinBox->setDecimals(4);
	this->noiseToSignalFactorSpinBox->setValue(DEF_NOISE_TO_SIGNAL);
	this->noiseToSignalFactorSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->noiseToSignalFactorSpinBox);
	formLayout->addRow(this->noiseToSignalLabel, this->noiseToSignalFactorSpinBox);

	// Volume padding mode (3D RL only)
	this->paddingModeLabel = new QLabel(tr("Volume Padding:"), controlsWidget);
	this->paddingModeComboBox = new QComboBox(controlsWidget);
	this->paddingModeComboBox->addItems(VolumetricDeconvolver::getPaddingModeNames());
	this->paddingModeComboBox->setCurrentIndex(DEF_PADDING_MODE);
	this->installScrollGuard(this->paddingModeComboBox);
	formLayout->addRow(this->paddingModeLabel, this->paddingModeComboBox);

	// Acceleration mode (3D RL only)
	this->accelerationModeLabel = new QLabel(tr("Acceleration:"), controlsWidget);
	this->accelerationModeComboBox = new QComboBox(controlsWidget);
	this->accelerationModeComboBox->addItems(VolumetricDeconvolver::getAccelerationModeNames());
	this->accelerationModeComboBox->setCurrentIndex(DEF_ACCELERATION);
	this->installScrollGuard(this->accelerationModeComboBox);
	formLayout->addRow(this->accelerationModeLabel, this->accelerationModeComboBox);

	controlsLayout->addLayout(formLayout);
	controlsLayout->addStretch();

	scrollArea->setWidget(controlsWidget);
	mainLayout->addWidget(scrollArea, 0);

	// Right side: action controls (always visible, outside scroll area)
	QVBoxLayout* actionLayout = new QVBoxLayout();
	this->liveModeCheckBox = new QCheckBox(tr("Auto-deconvolve on changes"), this);
	actionLayout->addWidget(this->liveModeCheckBox);
	this->deconvolveButton = new QPushButton(tr("Deconvolve"), this);
	actionLayout->addWidget(this->deconvolveButton);
	actionLayout->addStretch();
	mainLayout->addLayout(actionLayout);

	// Set initial visibility
	this->updateParameterVisibility(0);

	// Connect signals
	connect(this->algorithmComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &DeconvolutionSettingsWidget::onAlgorithmChanged);

	connect(this->iterationsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, [this](int value) {
				this->iterationsPerAlgorithm[this->algorithmComboBox->currentIndex()] = value;
				emit this->iterationsChanged(value);
			});

	connect(this->relaxationFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->relaxationFactorChanged(static_cast<float>(value)); });

	connect(this->regularizationFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->regularizationFactorChanged(static_cast<float>(value)); });

	connect(this->noiseToSignalFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->noiseToSignalFactorChanged(static_cast<float>(value)); });

	connect(this->paddingModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &DeconvolutionSettingsWidget::paddingModeChanged);

	connect(this->accelerationModeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &DeconvolutionSettingsWidget::accelerationModeChanged);

	connect(this->liveModeCheckBox, &QCheckBox::toggled,
			this, [this](bool checked) {
				this->deconvolveButton->setEnabled(!checked);
				emit this->liveModeChanged(checked);
			});

	connect(this->deconvolveButton, &QPushButton::clicked,
			this, &DeconvolutionSettingsWidget::deconvolutionRequested);
}

void DeconvolutionSettingsWidget::onAlgorithmChanged(int index)
{
	// Save iterations for the previous algorithm
	this->iterationsPerAlgorithm[this->previousAlgorithm] = this->iterationsSpinBox->value();

	// Load iterations for the new algorithm
	if (this->iterationsPerAlgorithm.contains(index)) {
		this->iterationsSpinBox->blockSignals(true);
		this->iterationsSpinBox->setValue(this->iterationsPerAlgorithm.value(index));
		this->iterationsSpinBox->blockSignals(false);
	}

	this->previousAlgorithm = index;
	this->updateParameterVisibility(index);
	emit algorithmChanged(index);
	emit iterationsChanged(this->iterationsSpinBox->value());
}

void DeconvolutionSettingsWidget::updateParameterVisibility(int algorithmIndex)
{
	// Hide all algorithm-specific parameters first
	this->relaxationLabel->setVisible(false);
	this->relaxationFactorSpinBox->setVisible(false);
	this->regularizationLabel->setVisible(false);
	this->regularizationFactorSpinBox->setVisible(false);
	this->noiseToSignalLabel->setVisible(false);
	this->noiseToSignalFactorSpinBox->setVisible(false);
	this->paddingModeLabel->setVisible(false);
	this->paddingModeComboBox->setVisible(false);
	this->accelerationModeLabel->setVisible(false);
	this->accelerationModeComboBox->setVisible(false);

	// Iterations are used by RL, Landweber, and 3D RL
	bool showIterations = (algorithmIndex == Deconvolver::RICHARDSON_LUCY ||
						   algorithmIndex == Deconvolver::LANDWEBER ||
						   algorithmIndex == Deconvolver::RICHARDSON_LUCY_3D);
	this->iterationsLabel->setVisible(showIterations);
	this->iterationsSpinBox->setVisible(showIterations);

	// Show algorithm-specific parameters
	switch (algorithmIndex) {
		case Deconvolver::LANDWEBER:
			this->relaxationLabel->setVisible(true);
			this->relaxationFactorSpinBox->setVisible(true);
			break;
		case Deconvolver::TIKHONOV:
			this->regularizationLabel->setVisible(true);
			this->regularizationFactorSpinBox->setVisible(true);
			break;
		case Deconvolver::WIENER:
			this->noiseToSignalLabel->setVisible(true);
			this->noiseToSignalFactorSpinBox->setVisible(true);
			break;
		case Deconvolver::RICHARDSON_LUCY_3D:
			this->paddingModeLabel->setVisible(true);
			this->paddingModeComboBox->setVisible(true);
			this->accelerationModeLabel->setVisible(true);
			this->accelerationModeComboBox->setVisible(true);
			break;
		default:
			break;
	}
}

QString DeconvolutionSettingsWidget::getName() const
{
	return SETTINGS_GROUP;
}

QVariantMap DeconvolutionSettingsWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert(KEY_ALGORITHM,             this->algorithmComboBox->currentIndex());
	settings.insert(KEY_ITERATIONS,            this->iterationsSpinBox->value());
	settings.insert(KEY_ITERATIONS_RL,         this->iterationsPerAlgorithm.value(Deconvolver::RICHARDSON_LUCY, DEF_ITERATIONS_RL));
	settings.insert(KEY_ITERATIONS_LW,         this->iterationsPerAlgorithm.value(Deconvolver::LANDWEBER, DEF_ITERATIONS_LW));
	settings.insert(KEY_ITERATIONS_RL3D,       this->iterationsPerAlgorithm.value(Deconvolver::RICHARDSON_LUCY_3D, DEF_ITERATIONS_RL3D));
	settings.insert(KEY_RELAXATION_FACTOR,     this->relaxationFactorSpinBox->value());
	settings.insert(KEY_REGULARIZATION_FACTOR, this->regularizationFactorSpinBox->value());
	settings.insert(KEY_NOISE_TO_SIGNAL,       this->noiseToSignalFactorSpinBox->value());
	settings.insert(KEY_PADDING_MODE,          this->paddingModeComboBox->currentIndex());
	settings.insert(KEY_ACCELERATION,          this->accelerationModeComboBox->currentIndex());
	settings.insert(KEY_LIVE_MODE,             this->liveModeCheckBox->isChecked());
	return settings;
}

void DeconvolutionSettingsWidget::setSettings(const QVariantMap& settings)
{
	int fallback = settings.value(KEY_ITERATIONS, DEF_ITERATIONS).toInt();
	this->iterationsPerAlgorithm[Deconvolver::RICHARDSON_LUCY]    = settings.value(KEY_ITERATIONS_RL,   fallback).toInt();
	this->iterationsPerAlgorithm[Deconvolver::LANDWEBER]          = settings.value(KEY_ITERATIONS_LW,   fallback).toInt();
	this->iterationsPerAlgorithm[Deconvolver::RICHARDSON_LUCY_3D] = settings.value(KEY_ITERATIONS_RL3D, fallback).toInt();
	this->algorithmComboBox->setCurrentIndex(      settings.value(KEY_ALGORITHM,             DEF_ALGORITHM).toInt());
	int currentAlgo = this->algorithmComboBox->currentIndex();
	this->previousAlgorithm = currentAlgo;
	this->iterationsSpinBox->setValue(this->iterationsPerAlgorithm.value(currentAlgo, fallback));
	this->relaxationFactorSpinBox->setValue(       settings.value(KEY_RELAXATION_FACTOR,     DEF_RELAXATION_FACTOR).toDouble());
	this->regularizationFactorSpinBox->setValue(   settings.value(KEY_REGULARIZATION_FACTOR, DEF_REGULARIZATION_FACTOR).toDouble());
	this->noiseToSignalFactorSpinBox->setValue(    settings.value(KEY_NOISE_TO_SIGNAL,        DEF_NOISE_TO_SIGNAL).toDouble());
	this->paddingModeComboBox->setCurrentIndex(   settings.value(KEY_PADDING_MODE,           DEF_PADDING_MODE).toInt());
	this->accelerationModeComboBox->setCurrentIndex(settings.value(KEY_ACCELERATION,          DEF_ACCELERATION).toInt());
	this->liveModeCheckBox->setChecked(            settings.value(KEY_LIVE_MODE,              DEF_LIVE_MODE).toBool());
}

void DeconvolutionSettingsWidget::installScrollGuard(QWidget* widget)
{
	widget->setFocusPolicy(Qt::StrongFocus);
	widget->installEventFilter(this);
}

bool DeconvolutionSettingsWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::Wheel) {
		QWidget* widget = qobject_cast<QWidget*>(obj);
		if (widget && !widget->hasFocus()) {
			event->ignore();
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}
