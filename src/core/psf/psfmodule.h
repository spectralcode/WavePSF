#ifndef PSFMODULE_H
#define PSFMODULE_H

#include <QObject>
#include <QVector>
#include <arrayfire.h>
#include "wavefrontparameter.h"

class IWavefrontGenerator;
class PSFCalculator;
class Deconvolver;

class PSFModule : public QObject
{
	Q_OBJECT
public:
	explicit PSFModule(QObject* parent = nullptr);
	~PSFModule() override;

	QVector<WavefrontParameter> getParameterDescriptors() const;
	af::array getCurrentWavefront() const;
	af::array getCurrentPSF() const;

public slots:
	void setCoefficient(int id, double value);
	void resetCoefficients();
	void setGridSize(int size);
	af::array deconvolve(const af::array& input);

signals:
	void wavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void parameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
	void error(QString message);

private:
	void regeneratePipeline();

	IWavefrontGenerator* generator;
	PSFCalculator* calculator;
	Deconvolver* deconvolver;

	int gridSize;
	af::array currentWavefront;
	af::array currentPSF;
};

#endif // PSFMODULE_H
