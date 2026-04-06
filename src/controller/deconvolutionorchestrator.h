#ifndef DECONVOLUTIONORCHESTRATOR_H
#define DECONVOLUTIONORCHESTRATOR_H

#include <QObject>

class PSFModule;
class ImageSession;
class CoefficientWorkspace;
class BatchProcessor;

class DeconvolutionOrchestrator : public QObject
{
	Q_OBJECT

public:
	explicit DeconvolutionOrchestrator(
		PSFModule* psfModule,
		ImageSession* imageSession,
		CoefficientWorkspace* coefficientWorkspace,
		QObject* parent = nullptr);

public slots:
	void runOnCurrentPatch();
	bool runBatch();
	void cancel();

private:
	void runVolumetricOnCurrentPatch();
	void syncVoxelSize();

	PSFModule* psfModule;
	ImageSession* imageSession;
	CoefficientWorkspace* coefficientWorkspace;
	BatchProcessor* batchProcessor;
};

#endif // DECONVOLUTIONORCHESTRATOR_H
