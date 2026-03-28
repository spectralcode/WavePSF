#ifndef VOLUMETRICDECONVOLVER_H
#define VOLUMETRICDECONVOLVER_H

#include <QObject>
#include <QStringList>
#include <arrayfire.h>

class VolumetricDeconvolver : public QObject
{
	Q_OBJECT
public:
	enum PaddingMode {
		ZERO_PAD = 0,
		MIRROR_PAD
	};

	enum AccelerationMode {
		ACCEL_NONE = 0,
		ACCEL_BIGGS_ANDREWS
	};

	explicit VolumetricDeconvolver(QObject* parent = nullptr);

	// 3D Richardson-Lucy deconvolution.
	// volume and psf are 3D af::arrays [H, W, D].
	af::array deconvolve(const af::array& volume, const af::array& psf, int iterations);

	void setPaddingMode(PaddingMode mode);
	void setAccelerationMode(AccelerationMode mode);

	static QStringList getAccelerationModeNames();
	static QStringList getPaddingModeNames();

signals:
	void iterationCompleted(int currentIteration, int totalIterations);
	void error(QString message);

private:
	// Zero-pad PSF to volume size and shift center to origin
	af::array preparePSF(const af::array& psf, int volH, int volW, int volD) const;

	// Pad volume to computation size using the current padding mode
	af::array padVolume(const af::array& volume, int fH, int fW, int fD) const;

	// Mirror-reflect volume along each dimension to fill target size
	static af::array mirrorPad3D(const af::array& vol, int fH, int fW, int fD);

	// Find the next FFT-friendly size (factorable into 2, 3, 5, 7)
	static int nextGoodFFTSize(int n);

	PaddingMode paddingMode;
	AccelerationMode accelerationMode;
};

#endif // VOLUMETRICDECONVOLVER_H
