#ifndef ZERNIKEGENERATOR_H
#define ZERNIKEGENERATOR_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <arrayfire.h>
#include "iwavefrontgenerator.h"

class ZernikeGenerator : public QObject, public IWavefrontGenerator
{
	Q_OBJECT
public:
	explicit ZernikeGenerator(int minNollIndex = 2, int maxNollIndex = 21, QObject* parent = nullptr);
	~ZernikeGenerator() override;

	// IWavefrontGenerator interface
	QVector<WavefrontParameter> getParameterDescriptors() const override;
	void setCoefficient(int id, double value) override;
	double getCoefficient(int id) const override;
	QVector<double> getAllCoefficients() const override;
	void setAllCoefficients(const QVector<double>& coefficients) override;
	void resetCoefficients() override;
	af::array generateWavefront(int gridSize) override;

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

	int minNoll;
	int maxNoll;
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
