#include "psfmodule.h"
#include "zernikegenerator.h"
#include "wavefrontgeneratorfactory.h"
#include "psfcalculator.h"
#include "deconvolver.h"
#include "utils/logging.h"


PSFModule::PSFModule(QObject* parent)
	: QObject(parent)
	, gridSize(128)
	, usingExternalPSF(false)
{
	this->generator = new ZernikeGenerator(2, 21, this);
	this->calculator = new PSFCalculator(0.055, 0.4, this);
	this->deconvolver = new Deconvolver(128, this);

	connect(this->deconvolver, &Deconvolver::error, this, &PSFModule::error);

	// Warm up ArrayFire: first GPU operation triggers backend init + JIT compilation.
	// Run a small dummy pipeline so the cost is paid at startup, not on first slider move.
	this->regeneratePipeline();
}

PSFModule::~PSFModule()
{
}

QVector<WavefrontParameter> PSFModule::getParameterDescriptors() const
{
	return this->generator->getParameterDescriptors();
}

af::array PSFModule::getCurrentWavefront() const
{
	return this->currentWavefront;
}

af::array PSFModule::getCurrentPSF() const
{
	if (this->usingExternalPSF && !this->externalPSF.isempty()) {
		return this->externalPSF;
	}
	return this->currentPSF;
}

bool PSFModule::isUsingExternalPSF() const
{
	return this->usingExternalPSF;
}

PSFSettings PSFModule::getPSFSettings() const
{
	PSFSettings s;
	s.generatorTypeName = this->generator->typeName();
	s.generatorSettings = this->generator->serializeSettings();

	// Populate Zernike convenience fields for UI compatibility
	ZernikeGenerator* zg = dynamic_cast<ZernikeGenerator*>(this->generator);
	if (zg) {
		s.nollIndexSpec = ZernikeGenerator::formatNollIndexSpec(zg->getNollIndices());
		s.globalMinCoefficient = zg->getGlobalMinValue();
		s.globalMaxCoefficient = zg->getGlobalMaxValue();
		s.coefficientStep = zg->getStepValue();
		s.coefficientRangeOverrides = zg->getRangeOverrides();
	}
	s.gridSize = this->gridSize;
	s.wavelengthNm = this->calculator->getLambda() * 1000.0;  // µm → nm
	s.apertureRadius = this->calculator->getApertureRadius();
	s.normalizationMode = static_cast<int>(this->calculator->getNormalizationMode());
	return s;
}

QString PSFModule::getGeneratorTypeName() const
{
	return this->generator->typeName();
}

QVector<double> PSFModule::getAllCoefficients() const
{
	return this->generator->getAllCoefficients();
}

void PSFModule::setCoefficient(int id, double value)
{
	this->generator->setCoefficient(id, value);
	this->regeneratePipeline();
}

void PSFModule::setAllCoefficients(const QVector<double>& coefficients)
{
	this->usingExternalPSF = false;
	this->generator->setAllCoefficients(coefficients);
	this->regeneratePipeline();
}

void PSFModule::setExternalPSF(const af::array& psf)
{
	this->externalPSF = psf;
	this->usingExternalPSF = true;
	emit psfUpdated(psf);
}

void PSFModule::resetCoefficients()
{
	this->generator->resetCoefficients();
	this->regeneratePipeline();
}

void PSFModule::setGridSize(int size)
{
	if (size > 0 && size != this->gridSize) {
		this->gridSize = size;
		this->regeneratePipeline();
	}
}

af::array PSFModule::deconvolve(const af::array& input)
{
	af::array psf = this->getCurrentPSF();
	if (psf.isempty()) {
		emit error(tr("No PSF available for deconvolution."));
		return af::array();
	}
	return this->deconvolver->deconvolve(input, psf);
}

void PSFModule::setGeneratorType(const QString& typeName)
{
	if (this->generator->typeName() == typeName) {
		return;
	}

	// Save current generator settings before switching
	QVariantMap oldSettings = this->generator->serializeSettings();

	// Delete old generator and create new one
	delete this->generator;
	this->generator = dynamic_cast<IWavefrontGenerator*>(
		WavefrontGeneratorFactory::create(typeName, this));

	emit generatorTypeChanged(typeName);
	emit parameterDescriptorsChanged(this->generator->getParameterDescriptors());
	this->regeneratePipeline();
}

void PSFModule::applyPSFSettings(const PSFSettings& settings)
{
	// Switch generator type if needed
	if (this->generator->typeName() != settings.generatorTypeName) {
		delete this->generator;
		this->generator = dynamic_cast<IWavefrontGenerator*>(
			WavefrontGeneratorFactory::create(settings.generatorTypeName, this));
		emit generatorTypeChanged(settings.generatorTypeName);
	}

	// Apply generator-specific settings
	if (!settings.generatorSettings.isEmpty()) {
		this->generator->deserializeSettings(settings.generatorSettings);
	}

	// Also apply Zernike convenience fields when the generator is Zernike
	ZernikeGenerator* zg = dynamic_cast<ZernikeGenerator*>(this->generator);
	bool indicesChanged = false;
	if (zg) {
		QVector<int> newIndices = ZernikeGenerator::parseNollIndexSpec(settings.nollIndexSpec);
		if (zg->getNollIndices() != newIndices) {
			zg->setNollIndices(newIndices);
			indicesChanged = true;
		}
		zg->setGlobalRange(settings.globalMinCoefficient, settings.globalMaxCoefficient);
		zg->setStepValue(settings.coefficientStep);
		zg->clearAllParameterRanges();
		for (auto it = settings.coefficientRangeOverrides.constBegin();
			 it != settings.coefficientRangeOverrides.constEnd(); ++it) {
			zg->setParameterRange(it.key(), it.value().first, it.value().second);
		}
	}

	this->gridSize = settings.gridSize;
	this->calculator->setLambda(settings.wavelengthNm / 1000.0);  // nm → µm
	this->calculator->setApertureRadius(settings.apertureRadius);
	this->calculator->setNormalizationMode(
		static_cast<PSFCalculator::NormalizationMode>(settings.normalizationMode));

	// Only re-broadcast descriptors if they actually changed
	QVector<WavefrontParameter> newDescriptors = this->generator->getParameterDescriptors();
	if (newDescriptors != this->cachedDescriptors) {
		this->cachedDescriptors = newDescriptors;
		emit parameterDescriptorsChanged(newDescriptors);
	}

	// Regenerate pipeline
	this->regeneratePipeline();

	if (indicesChanged) {
		emit nollIndicesChanged();
	}
}

void PSFModule::setDeconvolutionAlgorithm(int algorithm)
{
	this->deconvolver->setAlgorithm(static_cast<Deconvolver::Algorithm>(algorithm));
	emit deconvolutionSettingsChanged();
}

void PSFModule::setDeconvolutionIterations(int iterations)
{
	this->deconvolver->setIterations(iterations);
	emit deconvolutionSettingsChanged();
}

void PSFModule::setDeconvolutionRelaxationFactor(float factor)
{
	this->deconvolver->setRelaxationFactor(factor);
	emit deconvolutionSettingsChanged();
}

void PSFModule::setDeconvolutionRegularizationFactor(float factor)
{
	this->deconvolver->setRegularizationFactor(factor);
	emit deconvolutionSettingsChanged();
}

void PSFModule::setDeconvolutionNoiseToSignalFactor(float factor)
{
	this->deconvolver->setNoiseToSignalFactor(factor);
	emit deconvolutionSettingsChanged();
}

void PSFModule::regeneratePipeline()
{
	this->usingExternalPSF = false;
	try {
		this->currentWavefront = this->generator->generateWavefront(this->gridSize);
		emit wavefrontUpdated(this->currentWavefront);

		this->currentPSF = this->calculator->computePSF(this->currentWavefront);
		emit psfUpdated(this->currentPSF);
	} catch (af::exception& e) {
		emit error(tr("PSF pipeline error: ") + QString(e.what()));
	}
}
