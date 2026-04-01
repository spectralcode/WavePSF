#include "composedpsfgenerator.h"
#include "iwavefrontgenerator.h"
#include "ipsfpropagator.h"
#include <QSet>

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

af::array ComposedPSFGenerator::generatePSF(const PSFRequest& request)
{
	this->lastWavefront = this->generator->generateWavefront(request.gridSize);
	return this->propagator_->computePSF(this->lastWavefront, request.gridSize);
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

bool ComposedPSFGenerator::supportsRangeOverrides() const
{
	return this->generator->supportsRangeOverrides();
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

QVariantMap ComposedPSFGenerator::extractDialogValues(const QVariantMap& persisted) const
{
	QVariantMap flat;
	QVariantMap gs = persisted.value(KEY_GENERATOR_SETTINGS).toMap();
	QVariantMap ps = persisted.value(KEY_PROPAGATOR_SETTINGS).toMap();
	for (auto it = gs.constBegin(); it != gs.constEnd(); ++it) flat[it.key()] = it.value();
	for (auto it = ps.constBegin(); it != ps.constEnd(); ++it) flat[it.key()] = it.value();
	return flat;
}

QVariantMap ComposedPSFGenerator::mergeDialogValues(
	const QVariantMap& basePersisted,
	const QVariantMap& flatDialogValues) const
{
	QVariantMap result = basePersisted;
	QVariantMap genSettings = result.value(KEY_GENERATOR_SETTINGS).toMap();
	QVariantMap propSettings = result.value(KEY_PROPAGATOR_SETTINGS).toMap();

	// Route descriptor-owned keys to the correct sub-map via storageSection
	QSet<QString> descriptorKeys;
	for (const NumericSettingDescriptor& desc : this->getSettingsDescriptors()) {
		descriptorKeys.insert(desc.key);
		if (!flatDialogValues.contains(desc.key)) continue;
		if (desc.storageSection == SettingsStorageSection::Generator) {
			genSettings[desc.key] = flatDialogValues[desc.key];
		} else {
			propSettings[desc.key] = flatDialogValues[desc.key];
		}
	}

	// Route remaining keys not matched by any descriptor into generator_settings.
	// This handles Zernike custom UI keys (noll_index_spec, global_min, etc.)
	// which are not part of getSettingsDescriptors() but belong to the generator side.
	for (auto it = flatDialogValues.constBegin(); it != flatDialogValues.constEnd(); ++it) {
		if (!descriptorKeys.contains(it.key()))
			genSettings[it.key()] = it.value();
	}

	result[KEY_GENERATOR_SETTINGS] = genSettings;
	result[KEY_PROPAGATOR_SETTINGS] = propSettings;
	return result;
}
