#include "psfmodule.h"
#include "zernikegenerator.h"
#include "psfcalculator.h"
#include "deconvolver.h"
#include "utils/logging.h"


PSFModule::PSFModule(QObject* parent)
	: QObject(parent)
	, gridSize(128)
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
	return this->currentPSF;
}

void PSFModule::setCoefficient(int id, double value)
{
	this->generator->setCoefficient(id, value);
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
	if (this->currentPSF.isempty()) {
		emit error(tr("No PSF available for deconvolution."));
		return af::array();
	}
	return this->deconvolver->deconvolve(input, this->currentPSF);
}

void PSFModule::regeneratePipeline()
{
	try {
		this->currentWavefront = this->generator->generateWavefront(this->gridSize);
		emit wavefrontUpdated(this->currentWavefront);

		this->currentPSF = this->calculator->computePSF(this->currentWavefront);
		emit psfUpdated(this->currentPSF);
	} catch (af::exception& e) {
		emit error(tr("PSF pipeline error: ") + QString(e.what()));
	}
}
