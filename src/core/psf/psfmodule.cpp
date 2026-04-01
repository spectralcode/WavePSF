#include "psfmodule.h"
#include "ipsfgenerator.h"
#include "psfgeneratorfactory.h"
#include "deconvolver.h"
#include "utils/afdevicemanager.h"
#include "utils/logging.h"


PSFModule::PSFModule(AFDeviceManager* afDeviceManager, QObject* parent)
	: QObject(parent)
	, gridSize(128)
	, currentFrame(0)
	, currentPatchIdx(0)
{
	connect(afDeviceManager, &AFDeviceManager::aboutToChangeDevice,
			this, &PSFModule::clearCachedArrays);

	// Pre-populate allGeneratorSettings with defaults from every known mode.
	// This ensures SettingsDialog always finds a non-empty map for any type,
	// even before it has been used or saved to INI.
	for (const QString& typeName : PSFGeneratorFactory::availableTypeNames()) {
		IPSFGenerator* gen = PSFGeneratorFactory::create(typeName, nullptr);
		this->allGeneratorSettings[typeName] = gen->serializeSettings();
		delete gen;
	}

	this->generator = PSFGeneratorFactory::create(QStringLiteral("Zernike"), this);
	this->deconvolver = new Deconvolver(128, this);

	connect(this->deconvolver, &Deconvolver::error, this, &PSFModule::error);
	connect(this->deconvolver, &Deconvolver::iterationCompleted,
			this, &PSFModule::deconvolutionIterationCompleted);

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
	return this->generator->getLastWavefront();
}

af::array PSFModule::getCurrentPSF() const
{
	return this->currentPSF;
}

IPSFGenerator* PSFModule::getGenerator() const
{
	return this->generator;
}

QStringList PSFModule::availablePSFModes()
{
	return PSFGeneratorFactory::availableTypeNames();
}

af::array PSFModule::extractFrame(const af::array& psf, int frame)
{
	if (psf.numdims() > 2 && psf.dims(2) > 1) {
		int maxZ = static_cast<int>(psf.dims(2)) - 1;
		int z = qBound(0, frame, maxZ);
		return psf(af::span, af::span, z);
	}
	return psf;
}

int PSFModule::getCurrentFrame() const
{
	return this->currentFrame;
}

PSFSettings PSFModule::getPSFSettings() const
{
	PSFSettings s;
	s.generatorTypeName = this->generator->typeName();
	s.gridSize = this->gridSize;
	s.allGeneratorSettings = this->allGeneratorSettings;
	s.allGeneratorSettings[s.generatorTypeName] = this->generator->serializeSettings();
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
	if (this->generator->getAllCoefficients() == coefficients && !this->currentPSF.isempty()) {
		return;
	}
	this->generator->setAllCoefficients(coefficients);
	this->regeneratePipeline();
}

void PSFModule::setCurrentPatch(int frame, int patchIdx)
{
	this->currentFrame = frame;
	this->currentPatchIdx = patchIdx;
}

void PSFModule::refreshPSF()
{
	this->regeneratePipeline();
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
	af::array psf = PSFModule::extractFrame(this->getCurrentPSF(), this->currentFrame);
	if (psf.isempty()) {
		emit error(tr("No PSF available for deconvolution."));
		return af::array();
	}
	return this->deconvolver->deconvolve(input, psf);
}

af::array PSFModule::deconvolve(const af::array& input, const af::array& psf)
{
	return this->deconvolver->deconvolve(input, psf);
}

bool PSFModule::is3DAlgorithm() const
{
	return this->deconvolver->is3DAlgorithm();
}

void PSFModule::switchGenerator(const QString& typeName)
{
	if (this->generator->typeName() == typeName) {
		return;
	}

	// Cache outgoing generator settings
	this->allGeneratorSettings[this->generator->typeName()] =
		this->generator->serializeSettings();
	delete this->generator;
	this->generator = PSFGeneratorFactory::create(typeName, this);

	// Restore from cache if previously used
	QVariantMap cached = this->allGeneratorSettings.value(typeName);
	if (!cached.isEmpty()) {
		this->generator->deserializeSettings(cached);
	}

	emit generatorChanged(typeName);
	emit parameterDescriptorsChanged(this->generator->getParameterDescriptors());
	this->currentPSF = af::array();
}

void PSFModule::applyPSFSettings(const PSFSettings& settings)
{
	// Merge saved per-type settings on top of pre-populated defaults
	for (auto it = settings.allGeneratorSettings.constBegin();
		 it != settings.allGeneratorSettings.constEnd(); ++it) {
		this->allGeneratorSettings[it.key()] = it.value();
	}

	// Validate mode name
	QString modeName = settings.generatorTypeName;
	if (!PSFGeneratorFactory::availableTypeNames().contains(modeName)) {
		modeName = QStringLiteral("Zernike");
	}

	// Switch generator if needed
	if (this->generator->typeName() != modeName) {
		this->allGeneratorSettings[this->generator->typeName()] =
			this->generator->serializeSettings();
		delete this->generator;
		this->generator = PSFGeneratorFactory::create(modeName, this);
		emit generatorChanged(modeName);
	}

	// Apply settings from cache
	QVariantMap cached = this->allGeneratorSettings.value(modeName);
	if (!cached.isEmpty()) {
		this->generator->deserializeSettings(cached);
	}

	this->gridSize = settings.gridSize;

	// Only re-broadcast descriptors if they actually changed
	QVector<WavefrontParameter> newDescriptors = this->generator->getParameterDescriptors();
	if (newDescriptors != this->cachedDescriptors) {
		this->cachedDescriptors = newDescriptors;
		emit parameterDescriptorsChanged(newDescriptors);
	}

	this->regeneratePipeline();
}

void PSFModule::applyInlineSettings(const QVariantMap& settings)
{
	if (!settings.isEmpty()) {
		this->generator->applyInlineSettings(settings);
	}
	this->allGeneratorSettings[this->generator->typeName()] =
		this->generator->serializeSettings();
	this->regeneratePipeline();
}

void PSFModule::setNumOutputPlanes(int numPlanes)
{
	this->generator->setNumOutputPlanes(numPlanes);
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

void PSFModule::setVolumePaddingMode(int mode)
{
	this->deconvolver->setVolumePaddingMode(mode);
	emit deconvolutionSettingsChanged();
}

void PSFModule::setAccelerationMode(int mode)
{
	this->deconvolver->setAccelerationMode(mode);
	emit deconvolutionSettingsChanged();
}

void PSFModule::requestDeconvolutionCancel()
{
	this->deconvolver->requestDeconvolutionCancel();
}

void PSFModule::resetDeconvolutionCancel()
{
	this->deconvolver->resetDeconvolutionCancel();
}

bool PSFModule::wasDeconvolutionCancelled() const
{
	return this->deconvolver->wasDeconvolutionCancelled();
}

af::array PSFModule::computePSFFromCoefficients(const QVector<double>& coefficients)
{
	QVector<double> saved = this->generator->getAllCoefficients();
	this->generator->setAllCoefficients(coefficients);
	PSFRequest req;
	req.gridSize = this->gridSize;
	af::array psf = this->generator->generatePSF(req);
	this->generator->setAllCoefficients(saved);
	return psf;
}

void PSFModule::clearCachedArrays()
{
	this->currentPSF = af::array();
	this->generator->invalidateCache();
}

void PSFModule::regeneratePipeline()
{
	try {
		PSFRequest req;
		req.gridSize = this->gridSize;
		req.frame = this->currentFrame;
		req.patchIdx = this->currentPatchIdx;
		this->currentPSF = this->generator->generatePSF(req);
		if (this->generator->hasWavefront()) {
			emit wavefrontUpdated(this->generator->getLastWavefront());
		}
		emit psfUpdated(this->currentPSF);
	} catch (af::exception& e) {
		emit error(tr("PSF pipeline error: ") + QString(e.what()));
	}
}
