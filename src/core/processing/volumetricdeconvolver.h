#ifndef VOLUMETRICDECONVOLVER_H
#define VOLUMETRICDECONVOLVER_H

#include <QObject>
#include <arrayfire.h>

class VolumetricDeconvolver : public QObject
{
	Q_OBJECT
public:
	explicit VolumetricDeconvolver(QObject* parent = nullptr);

	// 3D Richardson-Lucy deconvolution.
	// volume and psf are 3D af::arrays [H, W, D].
	af::array deconvolve(const af::array& volume, const af::array& psf, int iterations);

	// Peak GPU memory estimate (bytes) for a given volume size.
	static size_t estimateGPUMemory(int height, int width, int depth);

signals:
	void iterationCompleted(int currentIteration, int totalIterations);
	void error(QString message);

private:
	// Zero-pad PSF to volume size and shift center to origin
	af::array preparePSF(const af::array& psf, int volH, int volW, int volD) const;

	// Find the next FFT-friendly size (factorable into 2, 3, 5, 7)
	static int nextGoodFFTSize(int n);
};

#endif // VOLUMETRICDECONVOLVER_H
