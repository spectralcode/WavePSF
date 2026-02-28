#ifndef DECONVOLVER_H
#define DECONVOLVER_H

#include <QObject>
#include <arrayfire.h>

class Deconvolver : public QObject
{
	Q_OBJECT
public:
	explicit Deconvolver(int iterations = 128, QObject* parent = nullptr);
	~Deconvolver() override;

	af::array deconvolve(const af::array& blurredInput, const af::array& psf);

	void setIterations(int iterations);
	int getIterations() const;

signals:
	void error(QString message);

private:
	int iterations;

	af::array zeroPadPSF(const af::array& psf, int targetRows, int targetCols) const;
};

#endif // DECONVOLVER_H
