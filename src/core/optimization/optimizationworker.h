#ifndef OPTIMIZATIONWORKER_H
#define OPTIMIZATIONWORKER_H

#include <QObject>
#include <QVector>
#include <QAtomicInt>
#include <QMetaType>
#include <arrayfire.h>
#include "core/psf/psfsettings.h"

// Single optimization job (one patch on one frame)
struct OptimizationJob {
	int frameNr = 0;
	int patchX = 0;
	int patchY = 0;
	af::array inputPatch;
	af::array groundTruthPatch;  // empty if no ground truth
	QVector<double> startCoefficients;
};

struct OptimizationConfig {
	// SA parameters
	double startTemperature = 1.0;
	double endTemperature = 0.001;
	double coolingFactor = 0.95;
	double startPerturbance = 0.05;
	double endPerturbance = 0.001;
	int iterationsPerTemperature = 10;

	// Which coefficient indices to optimize (indices into the coefficient vector)
	QVector<int> selectedCoefficientIndices;

	// Per-coefficient bounds (filled by controller from WavefrontParameter descriptors)
	QVector<double> minBounds;
	QVector<double> maxBounds;

	// PSF pipeline config (filled by controller)
	PSFSettings psfSettings;

	// Deconvolution settings (filled by controller)
	int deconvAlgorithm = 0;
	int deconvIterations = 128;
	float deconvRelaxationFactor = 0.65f;
	float deconvRegularizationFactor = 0.005f;
	float deconvNoiseToSignalFactor = 0.01f;

	// Metric selection
	bool useReferenceMetric = false;
	int imageMetric = 0;
	int referenceMetric = 0;
	double metricMultiplier = 1.0;

	// Batch specification (filled by UI, used by controller to build jobs)
	int mode = 0;  // 0 = single patch, 1 = batch
	QString patchSpec;   // e.g. "0, 2, 4-7"
	QString frameSpec;   // e.g. "0-500:50"
	int startCoefficientSource = 0;  // 0=current stored, 1=from frame, 2=frame offset, 3=random, 4=zeros, 5=previous result, 6=from patch
	int sourceParam = 0;  // frame number (source=1), frame offset (source=2), or patch index (source=6)

	// Live preview (used by controller, not by worker)
	bool livePreview = false;
	int livePreviewInterval = 10;  // update every N outer iterations

	// Jobs to process (built by controller; single-patch: 1 job; batch: many jobs)
	QVector<OptimizationJob> jobs;
};

struct OptimizationProgress {
	int currentJobIndex = 0;
	int totalJobs = 0;
	int outerIteration = 0;
	double currentMetric = 0.0;
	double bestMetric = 0.0;
	double temperature = 0.0;
	int currentFrameNr = 0;
	int currentPatchX = 0;
	int currentPatchY = 0;
	QVector<double> currentBestCoefficients;
	QVector<double> currentCoefficients;  // current working state (for live preview)
};

struct OptimizationJobResult {
	int frameNr = 0;
	int patchX = 0;
	int patchY = 0;
	QVector<double> bestCoefficients;
	double bestMetric = 0.0;
};

struct OptimizationResult {
	QVector<OptimizationJobResult> jobResults;
	int totalOuterIterations = 0;
	bool wasCancelled = false;
};

Q_DECLARE_METATYPE(OptimizationConfig)
Q_DECLARE_METATYPE(OptimizationProgress)
Q_DECLARE_METATYPE(OptimizationResult)


class OptimizationWorker : public QObject
{
	Q_OBJECT
public:
	explicit OptimizationWorker(QObject* parent = nullptr);
	~OptimizationWorker() override;

	// Thread-safe cancellation (called from main thread)
	void requestCancel();

public slots:
	void runOptimization(const OptimizationConfig& config);

signals:
	void progressUpdated(OptimizationProgress progress);
	void optimizationFinished(OptimizationResult result);
	void error(QString message);

private:
	QAtomicInt cancelRequested;

	void perturbCoefficients(QVector<double>& coefficients,
							 const QVector<int>& selectedIndices,
							 double perturbance,
							 const QVector<double>& minBounds,
							 const QVector<double>& maxBounds);

	double randomDouble(double low, double high);
};

#endif // OPTIMIZATIONWORKER_H
