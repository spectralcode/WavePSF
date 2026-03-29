#include "composedpsfgenerator.h"
#include "iwavefrontgenerator.h"
#include "ipsfpropagator.h"

namespace {
	const QString KEY_GENERATOR_SETTINGS  = QStringLiteral("generator_settings");
	const QString KEY_PROPAGATOR_SETTINGS = QStringLiteral("propagator_settings");
}


ComposedPSFGenerator::ComposedPSFGenerator(const QString& typeName,
										   IWavefrontGenerator* generator,
										   IPSFPropagator* propagator,
										   QObject* parent)
	: QObject(parent)
	, name(typeName)
	, generator(generator)
	, propagator_(propagator)
{
}

ComposedPSFGenerator::~ComposedPSFGenerator()
{
	delete this->generator;
	// propagator_ may be a QObject child — only delete if not parented
	QObject* propObj = dynamic_cast<QObject*>(this->propagator_);
	if (!propObj || !propObj->parent()) {
		delete this->propagator_;
	}
}

QString ComposedPSFGenerator::typeName() const
{
	return this->name;
}

QVariantMap ComposedPSFGenerator::serializeSettings() const
{
	QVariantMap settings;
	settings[KEY_GENERATOR_SETTINGS]  = this->generator->serializeSettings();
	settings[KEY_PROPAGATOR_SETTINGS] = this->propagator_->serializeSettings();
	return settings;
}

void ComposedPSFGenerator::deserializeSettings(const QVariantMap& settings)
{
	QVariantMap genSettings = settings.value(KEY_GENERATOR_SETTINGS).toMap();
	if (!genSettings.isEmpty()) {
		this->generator->deserializeSettings(genSettings);
	}
	QVariantMap propSettings = settings.value(KEY_PROPAGATOR_SETTINGS).toMap();
	if (!propSettings.isEmpty()) {
		this->propagator_->deserializeSettings(propSettings);
	}
}

QVector<NumericSettingDescriptor> ComposedPSFGenerator::getSettingsDescriptors() const
{
	return this->generator->getSettingsDescriptors() +
		   this->propagator_->getSettingsDescriptors();
}

QVector<WavefrontParameter> ComposedPSFGenerator::getParameterDescriptors() const
{
	return this->generator->getParameterDescriptors();
}

void ComposedPSFGenerator::setCoefficient(int id, double value)
{
	this->generator->setCoefficient(id, value);
}

double ComposedPSFGenerator::getCoefficient(int id) const
{
	return this->generator->getCoefficient(id);
}

QVector<double> ComposedPSFGenerator::getAllCoefficients() const
{
	return this->generator->getAllCoefficients();
}

void ComposedPSFGenerator::setAllCoefficients(const QVector<double>& coefficients)
{
	this->generator->setAllCoefficients(coefficients);
}

void ComposedPSFGenerator::resetCoefficients()
{
	this->generator->resetCoefficients();
}

af::array ComposedPSFGenerator::generatePSF(int gridSize)
{
	this->lastWavefront = this->generator->generateWavefront(gridSize);
	return this->propagator_->computePSF(this->lastWavefront, gridSize);
}

af::array ComposedPSFGenerator::getLastWavefront() const
{
	return this->lastWavefront;
}

int ComposedPSFGenerator::getApertureGeometry() const
{
	return this->propagator_->getApertureGeometry();
}

double ComposedPSFGenerator::getApertureRadius() const
{
	return this->propagator_->getApertureRadius();
}

bool ComposedPSFGenerator::is3D() const
{
	return this->propagator_->is3D();
}

void ComposedPSFGenerator::applyInlineSettings(const QVariantMap& settings)
{
	this->propagator_->applyInlineSettings(settings);
}

void ComposedPSFGenerator::setNumOutputPlanes(int numPlanes)
{
	this->propagator_->setNumOutputPlanes(numPlanes);
}

void ComposedPSFGenerator::invalidateCache()
{
	this->generator->invalidateCache();
	this->propagator_->invalidateCache();
	this->lastWavefront = af::array();
}

IWavefrontGenerator* ComposedPSFGenerator::wavefrontGenerator() const
{
	return this->generator;
}

IPSFPropagator* ComposedPSFGenerator::propagator() const
{
	return this->propagator_;
}
