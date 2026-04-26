#ifndef IMAGEDATAACCESSOR_H
#define IMAGEDATAACCESSOR_H

#include <QObject>
#include <QRect>
#include <QList>
#include <QPoint>
#include <arrayfire.h>
#include "imagepatch.h"
#include "imagedata.h"

class ImageDataAccessor : public QObject
{
	Q_OBJECT

public:
	explicit ImageDataAccessor(ImageData* imageData, bool readOnly = true, QObject* parent = nullptr);
	~ImageDataAccessor();

	// Frame-level operations (optimized with caching)
	af::array getFrame(int frameNr);  // Note: removed const to enable caching
	void writeFrame(int frameNr, const af::array& frameData);

	// New patch extraction method that tracks borders
	ImagePatch getExtendedPatch(int patchX, int patchY, int frameNr);

	// Write back using ImagePatch (handles core extraction automatically)
	void writePatchResult(const ImagePatch& originalPatch, const af::array& processedData);

	// Batch operations for performance
	void writeMultiplePatches(int frameNr, const QList<QPoint>& patchCoords, const QList<af::array>& patchData);
	void writeMultiplePatchResults(int frameNr, const QList<ImagePatch>& originalPatches, const QList<af::array>& processedData);
	void flushFrame(int frameNr);  // Force write cached frame back to ImageData

	// Patch grid configuration
	void configurePatchGrid(int cols, int rows, int borderExtension = 0);

	// Patch grid information
	int getPatchGridCols() const;
	int getPatchGridRows() const;
	int getPatchBorderExtension() const;
	QRect getCorePatchBounds(int patchX, int patchY) const;

	// Data information
	int getWidth() const;
	int getHeight() const;
	int getFrames() const;
	bool isReadOnly() const;
	bool isValid() const;

	// Cache management
	void clearCache();
	bool isFrameCached(int frameNr) const;

private:
	ImageData* imageData;
	bool readOnly;

	// Performance optimization: frame caching
	af::array cachedFrame;
	int cachedFrameNumber;
	bool frameModified;

	// Patch grid configuration
	int patchGridCols;
	int patchGridRows;
	int patchBorderExtension;

	// Memory management
	void* tempBuffer;
	size_t tempBufferSize;

	// Core conversion methods (optimized for different data types)
	af::array convertToArrayFire(void* data, int width, int height) const;
	void convertFromArrayFire(const af::array& afData, void* data, int width, int height) const;

	// Cache management
	void loadFrameToCache(int frameNr);
	void writeFrameFromCache();
	void ensureTempBuffer(size_t requiredSize);

	// Patch operations
	QRect calculateCorePatchBounds(int patchX, int patchY) const;
	void writePatchAtPosition(const QRect& imagePos, const af::array& patchData);
	bool writePatchResultToCache(const ImagePatch& originalPatch, const af::array& processedData);

	// Validation helpers
	bool isValidPatchCoordinate(int patchX, int patchY) const;
	bool isValidFrame(int frameNr) const;

signals:
	void dataWritten(int frameNr);
	void patchWritten(int patchX, int patchY, int frameNr);
	void frameLoaded(int frameNr);
};

#endif // IMAGEDATAACCESSOR_H
