#ifndef ZERNIKEGENERATOR_H
#define ZERNIKEGENERATOR_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QPair>
#include <arrayfire.h>
#include "iwavefrontgenerator.h"

class ZernikeGenerator : public QObject, public IWavefrontGenerator
{
	Q_OBJECT
public:
	explicit ZernikeGenerator(int minNollIndex = 2, int maxNollIndex = 21, QObject* parent = nullptr);
	~ZernikeGenerator() override;

	// IWavefrontGenerator interface
	QString typeName() const override;
	QVariantMap serializeSettings() const override;
	void deserializeSettings(const QVariantMap& settings) override;
	QVector<WavefrontParameter> getParameterDescriptors() const override;
	void setCoefficient(int id, double value) override;
	double getCoefficient(int id) const override;
	QVector<double> getAllCoefficients() const override;
	void setAllCoefficients(const QVector<double>& coefficients) override;
	void resetCoefficients() override;
	af::array generateWavefront(int gridSize) override;

	// Noll index control
	void setNollIndices(const QVector<int>& indices);
	QVector<int> getNollIndices() const;

	// Range control
	void setGlobalRange(double minValue, double maxValue);
	void setStepValue(double step);
	void setParameterRange(int nollIndex, double minValue, double maxValue);
	void clearParameterRange(int nollIndex);
	void clearAllParameterRanges();
	double getGlobalMinValue() const;
	double getGlobalMaxValue() const;
	double getStepValue() const;
	QMap<int, QPair<double,double>> getRangeOverrides() const;

	// Zernike-specific static utilities
	static int getNollN(int nollIndex);
	static int getNollM(int nollIndex);
	static QString getName(int nollIndex);

private:
	struct ZernikeBasis {
		int nollIndex;
		int n;
		int m;
		bool isEven;
		QVector<double> radialCoeffs;
		QVector<int> radialExponents;
	};

	QVector<int> nollIndices;
	double globalMinValue;
	double globalMaxValue;
	double stepValue;
	QMap<int, QPair<double,double>> rangeOverrides;
	QVector<ZernikeBasis> basisDefinitions;
	QMap<int, double> coefficients;

	// Cached GPU arrays (invalidated when grid size changes)
	int cachedGridSize;
	QVector<af::array> cachedBasisArrays;

	void initializeBasisDefinitions();
	void buildBasisCache(int gridSize);
	af::array evaluateBasisOnGrid(const ZernikeBasis& basis, const af::array& r, const af::array& theta) const;

	static double factorial(int n);
};

#endif // ZERNIKEGENERATOR_H
