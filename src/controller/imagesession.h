#ifndef IMAGESESSION_H
#define IMAGESESSION_H

#include <QObject>
#include <QPoint>
#include "data/imagedata.h"
#include "data/imagedataaccessor.h"

// Forward declarations
class AFDeviceManager;

class ImageSession : public QObject
{
	Q_OBJECT

public:
	explicit ImageSession(AFDeviceManager* afDeviceManager, QObject* parent = nullptr);
	~ImageSession();

	// Data management
	void setInputData(ImageData* inputData);
	void setGroundTruthData(ImageData* groundTruthData);
	void clearAllData();

	// State management
	void setCurrentFrame(int frame);
	void setCurrentPatch(int x, int y);
	void configurePatchGrid(int cols, int rows, int borderExtension = 0);

	// Data access wrappers
	ImagePatch getCurrentInputPatch();
	ImagePatch getCurrentOutputPatch();
	ImagePatch getCurrentGroundTruthPatch();
	void setCurrentOutputPatch(const af::array& data);

	// Arbitrary coordinate patch access (for batch processing)
	ImagePatch getInputPatch(int frameNr, int patchX, int patchY);
	ImagePatch getGroundTruthPatch(int frameNr, int patchX, int patchY);
	void setOutputPatch(int frameNr, int patchX, int patchY, const af::array& processedExtendedData);
	void flushOutput();

	// Frame-level access
	af::array getCurrentInputFrame();
	af::array getCurrentOutputFrame();
	af::array getCurrentGroundTruthFrame();
	void setCurrentOutputFrame(const af::array& frameData);

	// State getters
	int getCurrentFrame() const;
	QPoint getCurrentPatch() const;
	int getPatchGridCols() const;
	int getPatchGridRows() const;
	int getPatchBorderExtension() const;

	// Data information
	bool hasInputData() const;
	bool hasOutputData() const;
	bool hasGroundTruthData() const;

	// Direct data access for viewers
	const ImageData* getInputData() const;
	const ImageData* getOutputData() const;
	const ImageData* getGroundTruthData() const;

	int getInputWidth() const;
	int getInputHeight() const;
	int getInputFrames() const;

	int getOutputWidth() const;
	int getOutputHeight() const;
	int getOutputFrames() const;

	int getGroundTruthWidth() const;
	int getGroundTruthHeight() const;
	int getGroundTruthFrames() const;

	// File output
	void saveOutputToFile(const QString& filePath, int currentFrame);

	// Cache management
	void clearAFCaches();

	// Validation
	bool isValidFrame(int frame) const;
	bool isValidPatch(int x, int y) const;

private:
	void createOutputDataFromInput();
	void deleteInputAndOutputData();
	void validateDataCompatibility(ImageData* newData, const QString& dataType) const;
	void updateAccessorConfigurations();
	int getGroundTruthFrameForCurrentFrame() const;

	// Data objects
	ImageData* inputData;
	ImageData* outputData;
	ImageData* groundTruthData;

	// Accessors
	ImageDataAccessor* inputAccessor;
	ImageDataAccessor* outputAccessor;
	ImageDataAccessor* groundTruthAccessor;

	// Session state
	int currentFrame;
	QPoint currentPatch;
	int patchGridCols;
	int patchGridRows;
	int patchBorderExtension;

signals:
	void frameChanged(int frame);
	void patchChanged(int x, int y);
	void inputDataChanged();
	void outputDataChanged();
	void outputPatchUpdated();
	void groundTruthDataChanged();
	void patchGridConfigured(int cols, int rows, int borderExtension);
};

#endif // IMAGESESSION_H
