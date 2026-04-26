#ifndef PATCHDECONVOLUTIONPROCESSOR_H
#define PATCHDECONVOLUTIONPROCESSOR_H

#include <QVector>
#include <arrayfire.h>

class IPSFGenerator;
class Deconvolver;

struct PatchDeconvolutionInput {
	af::array inputPatch;
	af::array psf;
	int frameNr = 0;
	int patchIdx = -1;
	int gridSize = 128;
	QVector<double> coefficients;
	bool generatePSFIfMissing = false;
};

class PatchDeconvolutionProcessor
{
public:
	static af::array process(
		const PatchDeconvolutionInput& input,
		IPSFGenerator* generator,
		Deconvolver* deconvolver);

	static af::array extractPSFFrame(const af::array& psf, int frameNr);
};

#endif // PATCHDECONVOLUTIONPROCESSOR_H
