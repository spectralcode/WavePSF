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

	// Deconvolves all patches across all frames using stored coefficients.
	// Shows a modal progress dialog (parented to parentWidget).
	// Returns true if completed (not cancelled).
	bool executeBatchDeconvolution(
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		QWidget* parentWidget = nullptr);
};

#endif // BATCHPROCESSOR_H
