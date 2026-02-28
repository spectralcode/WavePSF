#include "psfcontrolwidget.h"
#include "coefficienteditorwidget.h"
#include "wavefrontplotwidget.h"
#include "psfpreviewwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>

namespace {
	const char* SETTINGS_GROUP = "psf_control";
}


PSFControlWidget::PSFControlWidget(QWidget* parent)
	: QGroupBox(tr("PSF Control"), parent)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);

	this->tabWidget = new QTabWidget(this);
	mainLayout->addWidget(this->tabWidget);

	// PSF Generation tab
	QWidget* generationTab = new QWidget(this->tabWidget);
	QHBoxLayout* genLayout = new QHBoxLayout(generationTab);

	this->coeffEditor = new CoefficientEditorWidget(generationTab);
	this->wavefrontPlot = new WavefrontPlotWidget(generationTab);
	this->psfPreview = new PSFPreviewWidget(generationTab);

	genLayout->addWidget(this->coeffEditor, 1);
	genLayout->addWidget(this->wavefrontPlot, 1);
	genLayout->addWidget(this->psfPreview, 1);

	this->tabWidget->addTab(generationTab, tr("PSF Generation"));

	// Placeholder tabs for future milestones
	this->tabWidget->addTab(new QWidget(this->tabWidget), tr("Deconvolution"));
	this->tabWidget->addTab(new QWidget(this->tabWidget), tr("Optimization"));
	this->tabWidget->addTab(new QWidget(this->tabWidget), tr("Interpolation"));

	// Forward signals from coefficient editor
	connect(this->coeffEditor, &CoefficientEditorWidget::coefficientChanged,
			this, &PSFControlWidget::coefficientChanged);
	connect(this->coeffEditor, &CoefficientEditorWidget::resetRequested,
			this, &PSFControlWidget::resetRequested);
}

PSFControlWidget::~PSFControlWidget()
{
}

QString PSFControlWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap PSFControlWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("coefficients", this->coeffEditor->getSettings());
	return settings;
}

void PSFControlWidget::setSettings(const QVariantMap& settings)
{
	if (settings.contains("coefficients")) {
		this->coeffEditor->setSettings(settings.value("coefficients").toMap());
	}
}

void PSFControlWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->coeffEditor->setParameterDescriptors(descriptors);
}

void PSFControlWidget::updateWavefront(af::array wavefront)
{
	this->wavefrontPlot->updatePlot(wavefront);
}

void PSFControlWidget::updatePSF(af::array psf)
{
	this->psfPreview->updateImage(psf);
}
