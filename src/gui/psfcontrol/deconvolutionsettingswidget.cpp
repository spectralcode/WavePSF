#include "deconvolutionsettingswidget.h"
#include "core/psf/deconvolver.h"
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
#include <QScrollArea>
#include <QEvent>

namespace {
	const char* SETTINGS_GROUP = "deconvolution_settings";
}


DeconvolutionSettingsWidget::DeconvolutionSettingsWidget(QWidget* parent)
	: QWidget(parent)
{
	this->setupUI();
}

DeconvolutionSettingsWidget::~DeconvolutionSettingsWidget()
{
}

void DeconvolutionSettingsWidget::setupUI()
{
	QHBoxLayout* mainLayout = new QHBoxLayout(this);

	QScrollArea* scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setMinimumWidth(220);
	scrollArea->setMaximumWidth(380);

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
	this->iterationsSpinBox->setValue(128);
	this->installScrollGuard(this->iterationsSpinBox);
	formLayout->addRow(this->iterationsLabel, this->iterationsSpinBox);

	// Relaxation factor (Landweber)
	this->relaxationLabel = new QLabel(tr("Relaxation Factor:"), controlsWidget);
	this->relaxationFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->relaxationFactorSpinBox->setRange(0.0001, 1000.0);
	this->relaxationFactorSpinBox->setDecimals(4);
	this->relaxationFactorSpinBox->setValue(0.65);
	this->relaxationFactorSpinBox->setSingleStep(0.01);
	this->installScrollGuard(this->relaxationFactorSpinBox);
	formLayout->addRow(this->relaxationLabel, this->relaxationFactorSpinBox);

	// Regularization factor (Tikhonov)
	this->regularizationLabel = new QLabel(tr("Regularization:"), controlsWidget);
	this->regularizationFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->regularizationFactorSpinBox->setRange(0.0001, 1.0);
	this->regularizationFactorSpinBox->setDecimals(4);
	this->regularizationFactorSpinBox->setValue(0.005);
	this->regularizationFactorSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->regularizationFactorSpinBox);
	formLayout->addRow(this->regularizationLabel, this->regularizationFactorSpinBox);

	// Noise-to-signal factor (Wiener)
	this->noiseToSignalLabel = new QLabel(tr("NSR Factor:"), controlsWidget);
	this->noiseToSignalFactorSpinBox = new QDoubleSpinBox(controlsWidget);
	this->noiseToSignalFactorSpinBox->setRange(0.0001, 1.0);
	this->noiseToSignalFactorSpinBox->setDecimals(4);
	this->noiseToSignalFactorSpinBox->setValue(0.01);
	this->noiseToSignalFactorSpinBox->setSingleStep(0.001);
	this->installScrollGuard(this->noiseToSignalFactorSpinBox);
	formLayout->addRow(this->noiseToSignalLabel, this->noiseToSignalFactorSpinBox);

	controlsLayout->addLayout(formLayout);

	// Separator
	QFrame* separator = new QFrame(controlsWidget);
	separator->setFrameShape(QFrame::HLine);
	separator->setFrameShadow(QFrame::Sunken);
	controlsLayout->addWidget(separator);

	// Live mode checkbox
	this->liveModeCheckBox = new QCheckBox(tr("Live Deconvolution"), controlsWidget);
	this->liveModeCheckBox->setChecked(false);
	controlsLayout->addWidget(this->liveModeCheckBox);

	// Deconvolve button
	this->deconvolveButton = new QPushButton(tr("Deconvolve"), controlsWidget);
	controlsLayout->addWidget(this->deconvolveButton);

	controlsLayout->addStretch();

	scrollArea->setWidget(controlsWidget);
	mainLayout->addWidget(scrollArea, 0);
	mainLayout->addStretch(1);

	// Set initial visibility
	this->updateParameterVisibility(0);

	// Connect signals
	connect(this->algorithmComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &DeconvolutionSettingsWidget::onAlgorithmChanged);

	connect(this->iterationsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &DeconvolutionSettingsWidget::iterationsChanged);

	connect(this->relaxationFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->relaxationFactorChanged(static_cast<float>(value)); });

	connect(this->regularizationFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->regularizationFactorChanged(static_cast<float>(value)); });

	connect(this->noiseToSignalFactorSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, [this](double value) { emit this->noiseToSignalFactorChanged(static_cast<float>(value)); });

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
	this->updateParameterVisibility(index);
	emit algorithmChanged(index);
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

	// Iterations are used by RL and Landweber
	bool showIterations = (algorithmIndex == Deconvolver::RICHARDSON_LUCY ||
						   algorithmIndex == Deconvolver::LANDWEBER);
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
		default:
			break;
	}
}

QString DeconvolutionSettingsWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap DeconvolutionSettingsWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("algorithm", this->algorithmComboBox->currentIndex());
	settings.insert("iterations", this->iterationsSpinBox->value());
	settings.insert("relaxationFactor", this->relaxationFactorSpinBox->value());
	settings.insert("regularizationFactor", this->regularizationFactorSpinBox->value());
	settings.insert("noiseToSignalFactor", this->noiseToSignalFactorSpinBox->value());
	settings.insert("liveMode", this->liveModeCheckBox->isChecked());
	return settings;
}

void DeconvolutionSettingsWidget::setSettings(const QVariantMap& settings)
{
	if (settings.contains("algorithm")) {
		this->algorithmComboBox->setCurrentIndex(settings.value("algorithm").toInt());
	}
	if (settings.contains("iterations")) {
		this->iterationsSpinBox->setValue(settings.value("iterations").toInt());
	}
	if (settings.contains("relaxationFactor")) {
		this->relaxationFactorSpinBox->setValue(settings.value("relaxationFactor").toDouble());
	}
	if (settings.contains("regularizationFactor")) {
		this->regularizationFactorSpinBox->setValue(settings.value("regularizationFactor").toDouble());
	}
	if (settings.contains("noiseToSignalFactor")) {
		this->noiseToSignalFactorSpinBox->setValue(settings.value("noiseToSignalFactor").toDouble());
	}
	if (settings.contains("liveMode")) {
		this->liveModeCheckBox->setChecked(settings.value("liveMode").toBool());
	}
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
