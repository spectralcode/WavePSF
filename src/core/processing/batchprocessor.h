#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QObject>

class ImageSession;
class PSFModule;
class WavefrontParameterTable;
class QWidget;

class BatchProcessor : public QObject
{
	Q_OBJECT
public:
	explicit BatchProcessor(QObject* parent = nullptr);

	// Deconvolves all patches across all frames using stored coefficients
	// or file-based PSFs (via FilePSFGenerator).
	// For 3D algorithms, processes per-patch subvolumes instead of per-frame patches.
	// Shows a modal progress dialog (parented to parentWidget).
	// Returns true if completed (not cancelled).
	bool executeBatchDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		QWidget* parentWidget = nullptr);

private:
	bool executeBatchVolumetricDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		QWidget* parentWidget);
};

#endif // BATCHPROCESSOR_H
