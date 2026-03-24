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
	// Shows a modal progress dialog (parented to parentWidget).
	// Returns true if completed (not cancelled).
	bool executeBatchDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		PSFFileManager* psfFileManager,
		QWidget* parentWidget = nullptr);
};

#endif // BATCHPROCESSOR_H
