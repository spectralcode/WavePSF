#include "zernikegenerator.h"
#include <QStringList>
#include <QtMath>
#include <algorithm>

namespace {
	const QString KEY_NOLL_INDEX_SPEC = QStringLiteral("noll_index_spec");
	const QString KEY_GLOBAL_MIN      = QStringLiteral("global_min");
	const QString KEY_GLOBAL_MAX      = QStringLiteral("global_max");
	const QString KEY_STEP            = QStringLiteral("step");
	const QString KEY_RANGE_OVERRIDES = QStringLiteral("range_overrides");
	const QString KEY_RANGE_MIN       = QStringLiteral("min");
	const QString KEY_RANGE_MAX       = QStringLiteral("max");

	const double DEF_GLOBAL_MIN = -0.03;
	const double DEF_GLOBAL_MAX =  0.03;
	const double DEF_STEP       =  0.001;
}


ZernikeGenerator::ZernikeGenerator(int minNollIndex, int maxNollIndex, QObject* parent)
	: QObject(parent)
	, globalMinValue(DEF_GLOBAL_MIN)
	, globalMaxValue(DEF_GLOBAL_MAX)
	, stepValue(DEF_STEP)
	, cachedGridSize(0)
{
	// Build indices list from min..max range
	for (int i = minNollIndex; i <= maxNollIndex; ++i) {
		this->nollIndices.append(i);
	}
	this->initializeBasisDefinitions();
}

ZernikeGenerator::~ZernikeGenerator()
{
}

QString ZernikeGenerator::typeName() const
{
	return QStringLiteral("Zernike");
}

QVariantMap ZernikeGenerator::serializeSettings() const
{
	QVariantMap map;
	map[KEY_NOLL_INDEX_SPEC] = formatNollIndexSpec(this->nollIndices);
	map[KEY_GLOBAL_MIN]      = this->globalMinValue;
	map[KEY_GLOBAL_MAX]      = this->globalMaxValue;
	map[KEY_STEP]            = this->stepValue;

	QVariantMap overrides;
	for (auto it = this->rangeOverrides.constBegin(); it != this->rangeOverrides.constEnd(); ++it) {
		QVariantMap range;
		range[KEY_RANGE_MIN] = it.value().first;
		range[KEY_RANGE_MAX] = it.value().second;
		overrides[QString::number(it.key())] = range;
	}
	map[KEY_RANGE_OVERRIDES] = overrides;
	return map;
}

void ZernikeGenerator::deserializeSettings(const QVariantMap& settings)
{
	if (settings.contains(KEY_NOLL_INDEX_SPEC)) {
		QVector<int> indices = parseNollIndexSpec(settings[KEY_NOLL_INDEX_SPEC].toString());
		if (!indices.isEmpty() && indices != this->nollIndices) {
			this->setNollIndices(indices);
		}
	}
	this->setGlobalRange(
		settings.value(KEY_GLOBAL_MIN, DEF_GLOBAL_MIN).toDouble(),
		settings.value(KEY_GLOBAL_MAX, DEF_GLOBAL_MAX).toDouble()
	);
	this->setStepValue(settings.value(KEY_STEP, DEF_STEP).toDouble());
	this->clearAllParameterRanges();
	QVariantMap overrides = settings.value(KEY_RANGE_OVERRIDES).toMap();
	for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
		QVariantMap range = it.value().toMap();
		int nollIndex = it.key().toInt();
		this->setParameterRange(nollIndex, range[KEY_RANGE_MIN].toDouble(), range[KEY_RANGE_MAX].toDouble());
	}
}

QVector<WavefrontParameter> ZernikeGenerator::getParameterDescriptors() const
{
	QVector<WavefrontParameter> descriptors;
	for (const ZernikeBasis& basis : qAsConst(this->basisDefinitions)) {
		WavefrontParameter param;
		param.id = basis.nollIndex;
		param.name = getName(basis.nollIndex);
		if (this->rangeOverrides.contains(basis.nollIndex)) {
			param.minValue = this->rangeOverrides[basis.nollIndex].first;
			param.maxValue = this->rangeOverrides[basis.nollIndex].second;
		} else {
			param.minValue = this->globalMinValue;
			param.maxValue = this->globalMaxValue;
		}
		param.step = this->stepValue;
		param.defaultValue = 0.0;
		descriptors.append(param);
	}
	return descriptors;
}

void ZernikeGenerator::setCoefficient(int id, double value)
{
	this->coefficients[id] = value;
}

double ZernikeGenerator::getCoefficient(int id) const
{
	return this->coefficients.value(id, 0.0);
}

QVector<double> ZernikeGenerator::getAllCoefficients() const
{
	QVector<double> result;
	for (const ZernikeBasis& basis : qAsConst(this->basisDefinitions)) {
		result.append(this->coefficients.value(basis.nollIndex, 0.0));
	}
	return result;
}

void ZernikeGenerator::setAllCoefficients(const QVector<double>& coefficients)
{
	int count = qMin(coefficients.size(), this->basisDefinitions.size());
	for (int i = 0; i < count; ++i) {
		this->coefficients[this->basisDefinitions[i].nollIndex] = coefficients[i];
	}
}

void ZernikeGenerator::resetCoefficients()
{
	this->coefficients.clear();
}

af::array ZernikeGenerator::generateWavefront(int gridSize)
{
	if (gridSize != this->cachedGridSize || this->cachedBasisArrays.isEmpty()) {
		this->buildBasisCache(gridSize);
	}

	af::array wavefront = af::constant(0.0f, gridSize, gridSize, f32);

	for (int i = 0; i < this->basisDefinitions.size(); ++i) {
		double coeff = this->coefficients.value(this->basisDefinitions[i].nollIndex, 0.0);
		if (qAbs(coeff) > 1e-12) {
			wavefront += static_cast<float>(coeff) * this->cachedBasisArrays[i];
		}
	}

	return wavefront;
}

void ZernikeGenerator::setNollIndices(const QVector<int>& indices)
{
	this->nollIndices = indices;
	this->cachedGridSize = 0; // invalidate GPU cache
	this->initializeBasisDefinitions();
}

QVector<int> ZernikeGenerator::getNollIndices() const
{
	return this->nollIndices;
}

void ZernikeGenerator::setGlobalRange(double minValue, double maxValue)
{
	this->globalMinValue = minValue;
	this->globalMaxValue = maxValue;
}

void ZernikeGenerator::setStepValue(double step)
{
	this->stepValue = step;
}

void ZernikeGenerator::setParameterRange(int nollIndex, double minValue, double maxValue)
{
	this->rangeOverrides[nollIndex] = qMakePair(minValue, maxValue);
}

void ZernikeGenerator::clearParameterRange(int nollIndex)
{
	this->rangeOverrides.remove(nollIndex);
}

void ZernikeGenerator::clearAllParameterRanges()
{
	this->rangeOverrides.clear();
}

double ZernikeGenerator::getGlobalMinValue() const
{
	return this->globalMinValue;
}

double ZernikeGenerator::getGlobalMaxValue() const
{
	return this->globalMaxValue;
}

double ZernikeGenerator::getStepValue() const
{
	return this->stepValue;
}

QMap<int, QPair<double,double>> ZernikeGenerator::getRangeOverrides() const
{
	return this->rangeOverrides;
}

int ZernikeGenerator::getNollN(int nollIndex)
{
	if (nollIndex < 1) { nollIndex = 1; }
	return static_cast<int>(qFloor(qSqrt(2.0 * static_cast<double>(nollIndex) - 1.0) + 0.5)) - 1;
}

int ZernikeGenerator::getNollM(int nollIndex)
{
	if (nollIndex < 1) { nollIndex = 1; }
	int n = getNollN(nollIndex);
	int s = n % 2;
	int me = 2 * static_cast<int>(qFloor((2 * nollIndex + 1.0 - n * (n + 1)) / 4.0));
	int mo = 2 * static_cast<int>(qFloor((2 * (nollIndex + 1.0) - n * (n + 1)) / 4.0)) - 1;
	int meo = (mo * s + me * (1 - s)) * (1 - 2 * (nollIndex % 2));
	return meo;
}

QString ZernikeGenerator::getName(int nollIndex)
{
	switch (nollIndex) {
		case 1:  return QStringLiteral("Piston");
		case 2:  return QStringLiteral("Tip");
		case 3:  return QStringLiteral("Tilt");
		case 4:  return QStringLiteral("Defocus");
		case 5:  return QStringLiteral("Oblique primary astigmatism");
		case 6:  return QStringLiteral("Vertical primary astigmatism");
		case 7:  return QStringLiteral("Vertical coma");
		case 8:  return QStringLiteral("Horizontal coma");
		case 9:  return QStringLiteral("Vertical trefoil");
		case 10: return QStringLiteral("Oblique trefoil");
		case 11: return QStringLiteral("Primary spherical");
		case 12: return QStringLiteral("Vertical secondary astigmatism");
		case 13: return QStringLiteral("Oblique secondary astigmatism");
		case 14: return QStringLiteral("Vertical quadrafoil");
		case 15: return QStringLiteral("Oblique quadrafoil");
		case 16: return QStringLiteral("Horizontal secondary coma");
		case 17: return QStringLiteral("Vertical secondary coma");
		case 18: return QStringLiteral("Oblique secondary trefoil");
		case 19: return QStringLiteral("Vertical secondary trefoil");
		case 20: return QStringLiteral("Oblique pentafoil");
		case 21: return QStringLiteral("Vertical pentafoil");
		default: return QStringLiteral("Higher order (%1)").arg(nollIndex);
	}
}

double ZernikeGenerator::factorial(int n)
{
	return (n < 2) ? 1.0 : n * factorial(n - 1);
}

void ZernikeGenerator::initializeBasisDefinitions()
{
	this->basisDefinitions.clear();
	this->coefficients.clear();

	for (int noll : qAsConst(this->nollIndices)) {
		ZernikeBasis basis;
		basis.nollIndex = noll;
		basis.n = getNollN(noll);
		basis.m = qAbs(getNollM(noll));
		basis.isEven = getNollM(noll) >= 0;

		int upperBound = (basis.n - basis.m) / 2;
		double sign = -1.0;
		for (int k = 0; k <= upperBound; ++k) {
			sign *= -1.0;
			basis.radialExponents.append(basis.n - 2 * k);
			basis.radialCoeffs.append(
				sign * factorial(basis.n - k) /
				(factorial(k) * factorial((basis.n + basis.m) / 2 - k) * factorial((basis.n - basis.m) / 2 - k))
			);
		}

		this->basisDefinitions.append(basis);
		this->coefficients[noll] = 0.0;
	}
}

void ZernikeGenerator::buildBasisCache(int gridSize)
{
	this->cachedBasisArrays.clear();
	this->cachedGridSize = gridSize;

	// Build normalized 2D coordinate grids: range [-1, 1]
	// Standard convention: x = columns (dim 1), y = rows (dim 0)
	af::array x = (2.0f * af::range(af::dim4(gridSize, gridSize), 1).as(f32) / (gridSize - 1) - 1.0f);
	af::array y = (2.0f * af::range(af::dim4(gridSize, gridSize), 0).as(f32) / (gridSize - 1) - 1.0f);

	af::array r = af::sqrt(x * x + y * y);
	af::array theta = af::atan2(y, x);

	for (const ZernikeBasis& basis : qAsConst(this->basisDefinitions)) {
		af::array basisArray = this->evaluateBasisOnGrid(basis, r, theta);
		af::eval(basisArray); // force GPU computation now, not lazily on first use //todo: check if this makes a difference on different machines
		this->cachedBasisArrays.append(basisArray);
	}
}

af::array ZernikeGenerator::evaluateBasisOnGrid(const ZernikeBasis& basis, const af::array& r, const af::array& theta) const
{
	af::array radial = af::constant(0.0f, r.dims(), f32);
	for (int k = 0; k < basis.radialCoeffs.size(); ++k) {
		radial += static_cast<float>(basis.radialCoeffs[k]) * af::pow(r, basis.radialExponents[k]);
	}

	af::array angular;
	if (basis.m == 0) {
		angular = af::constant(1.0f, r.dims(), f32);
	} else if (basis.isEven) {
		angular = af::cos(static_cast<float>(basis.m) * theta);
	} else {
		angular = af::sin(static_cast<float>(basis.m) * theta);
	}

	double kroneckerDelta = (basis.m == 0) ? 1.0 : 0.0;
	float normFactor = static_cast<float>(qSqrt(2.0 * (basis.n + 1.0) / (1.0 + kroneckerDelta)));

	af::array mask = (r <= 1.0f).as(f32);

	return normFactor * radial * angular * mask;
}

QVector<int> ZernikeGenerator::parseNollIndexSpec(const QString& spec)
{
	QVector<int> result;
	QStringList parts = spec.split(',', QString::SkipEmptyParts);

	for (const QString& part : qAsConst(parts)) {
		QString trimmed = part.trimmed();
		if (trimmed.isEmpty()) {
			continue;
		}

		int dashIndex = trimmed.indexOf('-');
		if (dashIndex > 0) {
			// Range: "2-21"
			bool okStart = false, okEnd = false;
			int start = trimmed.left(dashIndex).trimmed().toInt(&okStart);
			int end = trimmed.mid(dashIndex + 1).trimmed().toInt(&okEnd);
			if (okStart && okEnd && start >= 1 && end >= start) {
				for (int i = start; i <= end; ++i) {
					if (!result.contains(i)) {
						result.append(i);
					}
				}
			}
		} else {
			// Single index: "7"
			bool ok = false;
			int val = trimmed.toInt(&ok);
			if (ok && val >= 1 && !result.contains(val)) {
				result.append(val);
			}
		}
	}

	std::sort(result.begin(), result.end());
	return result;
}

QString ZernikeGenerator::formatNollIndexSpec(const QVector<int>& indices)
{
	if (indices.isEmpty()) {
		return QString();
	}

	QStringList parts;
	int rangeStart = indices[0];
	int rangeEnd = indices[0];

	for (int i = 1; i < indices.size(); ++i) {
		if (indices[i] == rangeEnd + 1) {
			rangeEnd = indices[i];
		} else {
			// Flush current range
			if (rangeStart == rangeEnd) {
				parts.append(QString::number(rangeStart));
			} else {
				parts.append(QString("%1-%2").arg(rangeStart).arg(rangeEnd));
			}
			rangeStart = indices[i];
			rangeEnd = indices[i];
		}
	}

	// Flush last range
	if (rangeStart == rangeEnd) {
		parts.append(QString::number(rangeStart));
	} else {
		parts.append(QString("%1-%2").arg(rangeStart).arg(rangeEnd));
	}

	return parts.join(", ");
}
