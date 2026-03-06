#ifndef DEFORMABLEMIRRORGENERATOR_H
#define DEFORMABLEMIRRORGENERATOR_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <arrayfire.h>
#include "core/psf/iwavefrontgenerator.h"

class DeformableMirrorGenerator : public QObject, public IWavefrontGenerator
{
	Q_OBJECT
public:
	explicit DeformableMirrorGenerator(QObject* parent = nullptr);
	~DeformableMirrorGenerator() override;

	// IWavefrontGenerator interface
	QString typeName() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	QVector<WavefrontGeneratorSetting> getSettingsDescriptors() const override;
	QVector<WavefrontParameter> getParameterDescriptors() const override;
	void setCoefficient(int id, double value) override;
	double getCoefficient(int id) const override;
	QVector<double> getAllCoefficients() const override;
	void setAllCoefficients(const QVector<double>& coefficients) override;
	void resetCoefficients() override;
	af::array generateWavefront(int gridSize) override;

	// DM configuration
	void setActuatorGrid(int rows, int cols);
	int getActuatorGridRows() const;
	int getActuatorGridCols() const;

	void setCouplingCoefficient(double coupling);
	double getCouplingCoefficient() const;

	void setGaussianIndex(double index);
	double getGaussianIndex() const;

	void setCommandRange(double minValue, double maxValue);
	double getCommandMin() const;
	double getCommandMax() const;

	void setCommandStep(double step);
	double getCommandStep() const;

private:
	struct ActuatorInfo {
		int linearIndex;	// 0-based index in the parameter list
		int row;
		int col;
		double x;			// normalized position [-1, 1]
		double y;			// normalized position [-1, 1]
	};

	void rebuildActuatorLayout();

	int actuatorRows;
	int actuatorCols;
	double couplingCoefficient;
	double gaussianIndex;
	double commandMin;
	double commandMax;
	double commandStep;

	QVector<ActuatorInfo> activeActuators;	// only actuators inside the pupil
	QMap<int, double> commands;				// linearIndex -> command value

	// Cached GPU arrays
	int cachedGridSize;
	QVector<af::array> cachedInfluenceFunctions;

	void buildInfluenceCache(int gridSize);
};

#endif // DEFORMABLEMIRRORGENERATOR_H
