#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QObject>

class ImageSession;
class PSFModule;
class WavefrontParameterTable;
class PSFFileManager;
class QWidget;

class BatchProcessor : public QObject
{
	Q_OBJECT
public:
	explicit BatchProcessor(QObject* parent = nullptr);

	// Deconvolves all patches across all frames using stored coefficients
	// or external PSFs (from override map / custom folder).
	// For 3D algorithms, processes per-patch subvolumes instead of per-frame patches.
	// Shows a modal progress dialog (parented to parentWidget).
	// Returns true if completed (not cancelled).
	bool executeBatchDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		PSFFileManager* psfFileManager,
		QWidget* parentWidget = nullptr);

private:
	bool executeBatchVolumetricDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		PSFFileManager* psfFileManager,
		QWidget* parentWidget);
};

#endif // BATCHPROCESSOR_H
