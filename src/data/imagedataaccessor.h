#ifndef IMAGEDATAACCESSOR_H
#define IMAGEDATAACCESSOR_H

#include <QObject>
#include <QRect>
#include <QList>
#include <QPoint>
#include <arrayfire.h>
#include "imagedata.h"

struct BorderExtension {
	bool leftExtended;
	bool rightExtended;
	bool topExtended;
	bool bottomExtended;
	int extensionSize;  // How many pixels were requested for extension

	BorderExtension() : leftExtended(false), rightExtended(false),
						topExtended(false), bottomExtended(false), extensionSize(0) {}
};

struct ImagePatch {
	af::array data;              // Extended patch data (with borders)
	QRect coreArea;              // Core patch area within the extended data (relative coordinates)
	QRect imagePosition;         // Where core patch maps to in the full image (absolute coordinates)
	BorderExtension borders;     // Which borders were actually extended

	// Convenience methods
	af::array extractCore() const {
		if (data.isempty()) return af::array();
		return data(af::seq(coreArea.x(), coreArea.x() + coreArea.width() - 1),
					af::seq(coreArea.y(), coreArea.y() + coreArea.height() - 1));
	}

	bool isValid() const {
		return !data.isempty() && coreArea.isValid() && imagePosition.isValid();
	}
};

class ImageDataAccessor : public QObject
{
	Q_OBJECT

public:
	// Synchronization modes for CPU↔GPU data transfers
	enum SyncMode {
		IMMEDIATE,	// Write to CPU immediately after each GPU update (live viewing)
		MANUAL,		// Only write to CPU when explicitly requested (optimization)
		DISABLED	// Never write to CPU automatically (pure GPU workflow)
	};

	explicit ImageDataAccessor(ImageData* imageData, bool readOnly = true, QObject* parent = nullptr);
	~ImageDataAccessor();

	// Frame-level operations (optimized with caching)
	af::array getFrame(int frameNr);  // Note: removed const to enable caching
	void writeFrame(int frameNr, const af::array& frameData);

	// New patch extraction method that tracks borders
	ImagePatch getExtendedPatch(int patchX, int patchY, int frameNr);

	// Write back only the core area from processed patch
	void writeCoreArea(int patchX, int patchY, int frameNr, const af::array& processedPatch);

	// Write back using ImagePatch (handles core extraction automatically)
	void writePatchResult(const ImagePatch& originalPatch, const af::array& processedData);

	// Legacy patch operations (for backward compatibility - return core area only)
	af::array getPatch(int patchX, int patchY, int frameNr);
	void writePatch(int patchX, int patchY, int frameNr, const af::array& patchData);

	// Batch operations for performance
	void writeMultiplePatches(int frameNr, const QList<QPoint>& patchCoords, const QList<af::array>& patchData);
	void flushFrame(int frameNr);  // Force write cached frame back to ImageData

	// Synchronization control for optimization workflows
	void setSyncMode(SyncMode mode);
	SyncMode getSyncMode() const;
	void forceSyncToCPU();  // Manual sync for MANUAL/DISABLED modes
	void forceSyncToGPU();  // Reload from CPU (invalidate cache)

	// Patch grid configuration
	void configurePatchGrid(int cols, int rows, int borderExtension = 0);

	// Patch grid information
	int getPatchGridCols() const;
	int getPatchGridRows() const;
	int getPatchBorderExtension() const;
	QRect getPatchBounds(int patchX, int patchY) const;
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

	// Synchronization control
	SyncMode syncMode;

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

	// Validation helpers
	bool isValidPatchCoordinate(int patchX, int patchY) const;
	bool isValidFrame(int frameNr) const;

signals:
	void dataWritten(int frameNr);
	void patchWritten(int patchX, int patchY, int frameNr);
	void frameLoaded(int frameNr);
};

#endif // IMAGEDATAACCESSOR_H
