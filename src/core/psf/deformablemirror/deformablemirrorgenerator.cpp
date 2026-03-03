#include "deformablemirrorgenerator.h"
#include <QtMath>


DeformableMirrorGenerator::DeformableMirrorGenerator(QObject* parent)
	: QObject(parent)
	, actuatorRows(8)
	, actuatorCols(8)
	, couplingCoefficient(0.15)
	, gaussianIndex(2.0)
	, commandMin(-1.0)
	, commandMax(1.0)
	, commandStep(0.01)
	, cachedGridSize(0)
{
	this->rebuildActuatorLayout();
}

DeformableMirrorGenerator::~DeformableMirrorGenerator()
{
}

QString DeformableMirrorGenerator::typeName() const
{
	return QStringLiteral("Deformable Mirror");
}

QVariantMap DeformableMirrorGenerator::serializeSettings() const
{
	QVariantMap map;
	map["actuator_rows"] = this->actuatorRows;
	map["actuator_cols"] = this->actuatorCols;
	map["coupling_coefficient"] = this->couplingCoefficient;
	map["gaussian_index"] = this->gaussianIndex;
	map["command_min"] = this->commandMin;
	map["command_max"] = this->commandMax;
	map["command_step"] = this->commandStep;
	return map;
}

void DeformableMirrorGenerator::deserializeSettings(const QVariantMap& settings)
{
	int rows = settings.value("actuator_rows", this->actuatorRows).toInt();
	int cols = settings.value("actuator_cols", this->actuatorCols).toInt();
	this->couplingCoefficient = settings.value("coupling_coefficient", this->couplingCoefficient).toDouble();
	this->gaussianIndex = settings.value("gaussian_index", this->gaussianIndex).toDouble();
	this->commandMin = settings.value("command_min", this->commandMin).toDouble();
	this->commandMax = settings.value("command_max", this->commandMax).toDouble();
	this->commandStep = settings.value("command_step", this->commandStep).toDouble();
	this->cachedGridSize = 0; // Invalidate influence cache (coupling/gaussian may have changed)
	this->setActuatorGrid(rows, cols);
}

QVector<WavefrontParameter> DeformableMirrorGenerator::getParameterDescriptors() const
{
	QVector<WavefrontParameter> descriptors;
	for (const ActuatorInfo& act : qAsConst(this->activeActuators)) {
		WavefrontParameter param;
		param.id = act.linearIndex;
		param.name = QString("Act (%1,%2)").arg(act.row).arg(act.col);
		param.minValue = this->commandMin;
		param.maxValue = this->commandMax;
		param.step = this->commandStep;
		param.defaultValue = 0.0;
		descriptors.append(param);
	}
	return descriptors;
}

void DeformableMirrorGenerator::setCoefficient(int id, double value)
{
	this->commands[id] = value;
}

double DeformableMirrorGenerator::getCoefficient(int id) const
{
	return this->commands.value(id, 0.0);
}

QVector<double> DeformableMirrorGenerator::getAllCoefficients() const
{
	QVector<double> result;
	for (const ActuatorInfo& act : qAsConst(this->activeActuators)) {
		result.append(this->commands.value(act.linearIndex, 0.0));
	}
	return result;
}

void DeformableMirrorGenerator::setAllCoefficients(const QVector<double>& coefficients)
{
	int count = qMin(coefficients.size(), this->activeActuators.size());
	for (int i = 0; i < count; ++i) {
		this->commands[this->activeActuators[i].linearIndex] = coefficients[i];
	}
}

void DeformableMirrorGenerator::resetCoefficients()
{
	this->commands.clear();
}

af::array DeformableMirrorGenerator::generateWavefront(int gridSize)
{
	if (gridSize != this->cachedGridSize || this->cachedInfluenceFunctions.isEmpty()) {
		this->buildInfluenceCache(gridSize);
	}

	af::array wavefront = af::constant(0.0f, gridSize, gridSize, f32);

	for (int i = 0; i < this->activeActuators.size(); ++i) {
		double cmd = this->commands.value(this->activeActuators[i].linearIndex, 0.0);
		if (qAbs(cmd) > 1e-12) {
			wavefront += static_cast<float>(cmd) * this->cachedInfluenceFunctions[i];
		}
	}

	return wavefront;
}

void DeformableMirrorGenerator::setActuatorGrid(int rows, int cols)
{
	if (rows < 2) rows = 2;
	if (cols < 2) cols = 2;
	if (rows != this->actuatorRows || cols != this->actuatorCols) {
		this->actuatorRows = rows;
		this->actuatorCols = cols;
		this->cachedGridSize = 0;
		this->rebuildActuatorLayout();
	}
}

int DeformableMirrorGenerator::getActuatorGridRows() const
{
	return this->actuatorRows;
}

int DeformableMirrorGenerator::getActuatorGridCols() const
{
	return this->actuatorCols;
}

void DeformableMirrorGenerator::setCouplingCoefficient(double coupling)
{
	this->couplingCoefficient = coupling;
	this->cachedGridSize = 0; // invalidate cache
}

double DeformableMirrorGenerator::getCouplingCoefficient() const
{
	return this->couplingCoefficient;
}

void DeformableMirrorGenerator::setGaussianIndex(double index)
{
	this->gaussianIndex = index;
	this->cachedGridSize = 0; // invalidate cache
}

double DeformableMirrorGenerator::getGaussianIndex() const
{
	return this->gaussianIndex;
}

void DeformableMirrorGenerator::setCommandRange(double minValue, double maxValue)
{
	this->commandMin = minValue;
	this->commandMax = maxValue;
}

double DeformableMirrorGenerator::getCommandMin() const
{
	return this->commandMin;
}

double DeformableMirrorGenerator::getCommandMax() const
{
	return this->commandMax;
}

void DeformableMirrorGenerator::setCommandStep(double step)
{
	this->commandStep = step;
}

double DeformableMirrorGenerator::getCommandStep() const
{
	return this->commandStep;
}

void DeformableMirrorGenerator::rebuildActuatorLayout()
{
	this->activeActuators.clear();
	this->commands.clear();
	this->cachedInfluenceFunctions.clear();

	// Actuators are placed on a regular rectangular grid spanning [-1, 1]
	double spacingX = (this->actuatorCols > 1) ? 2.0 / (this->actuatorCols - 1) : 0.0;
	double spacingY = (this->actuatorRows > 1) ? 2.0 / (this->actuatorRows - 1) : 0.0;

	int linearIndex = 0;
	for (int r = 0; r < this->actuatorRows; ++r) {
		for (int c = 0; c < this->actuatorCols; ++c) {
			double x = -1.0 + c * spacingX;
			double y = -1.0 + r * spacingY;

			ActuatorInfo info;
			info.linearIndex = linearIndex;
			info.row = r;
			info.col = c;
			info.x = x;
			info.y = y;
			this->activeActuators.append(info);
			this->commands[linearIndex] = 0.0;

			++linearIndex;
		}
	}
}

void DeformableMirrorGenerator::buildInfluenceCache(int gridSize)
{
	this->cachedInfluenceFunctions.clear();
	this->cachedGridSize = gridSize;

	// Build normalized 2D coordinate grids: range [-1, 1]
	// Standard convention: x = columns (dim 1), y = rows (dim 0)
	af::array X = (2.0f * af::range(af::dim4(gridSize, gridSize), 1).as(f32) / (gridSize - 1) - 1.0f);
	af::array Y = (2.0f * af::range(af::dim4(gridSize, gridSize), 0).as(f32) / (gridSize - 1) - 1.0f);

	// Actuator spacing in normalized coordinates
	double spacingX = (this->actuatorCols > 1) ? 2.0 / (this->actuatorCols - 1) : 2.0;
	double spacingY = (this->actuatorRows > 1) ? 2.0 / (this->actuatorRows - 1) : 2.0;
	// Use the average spacing for the influence function width
	float spacing = static_cast<float>((spacingX + spacingY) / 2.0);

	float logW = static_cast<float>(qLn(this->couplingCoefficient));
	float a = static_cast<float>(this->gaussianIndex);

	for (const ActuatorInfo& act : qAsConst(this->activeActuators)) {
		float ax = static_cast<float>(act.x);
		float ay = static_cast<float>(act.y);

		af::array dist = af::sqrt((X - ax) * (X - ax) + (Y - ay) * (Y - ay));
		af::array normDist = dist / spacing;
		af::array exponent = logW * af::pow(normDist, a);
		af::array influence = af::exp(exponent);
		af::eval(influence);
		this->cachedInfluenceFunctions.append(influence);
	}
}
