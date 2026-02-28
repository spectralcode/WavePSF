#ifndef PSFCALCULATOR_H
#define PSFCALCULATOR_H

#include <QObject>
#include <arrayfire.h>

class PSFCalculator : public QObject
{
	Q_OBJECT
public:
	explicit PSFCalculator(double lambda = 0.055, double apertureRadius = 0.4, QObject* parent = nullptr);
	~PSFCalculator() override;

	af::array computePSF(const af::array& wavefront);

	void setLambda(double lambda);
	double getLambda() const;
	void setApertureRadius(double radius);
	double getApertureRadius() const;

private:
	double lambda;
	double apertureRadius;

	int cachedGridSize;
	af::array cachedApertureMask;

	void buildApertureCache(int gridSize);
	static af::array fftshift2D(const af::array& input);
};

#endif // PSFCALCULATOR_H
