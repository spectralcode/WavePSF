#ifndef APPLICATIONCONTROLLER_H
#define APPLICATIONCONTROLLER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"

// Forward declarations
class ImageSession;
class InputDataReader;
class ImageData;
class SettingsFileManager;
class PSFModule;

class ApplicationController : public QObject
{
	Q_OBJECT

public:
	explicit ApplicationController(QObject* parent = nullptr);
	~ApplicationController();

	// Direct session access for modules
	ImageSession* getImageSession() const;

	// Session information access (for compatibility)
	bool hasInputData() const;
	bool hasOutputData() const;
	bool hasGroundTruthData() const;

	int getCurrentFrame() const;
	int getCurrentPatchX() const;
	int getCurrentPatchY() const;
	int getPatchGridCols() const;
	int getPatchGridRows() const;
	int getPatchBorderExtension() const;

	// Data dimensions
	int getInputWidth() const;
	int getInputHeight() const;
	int getInputFrames() const;

	// Broadcast current state for initialization
	void broadcastCurrentState();

public slots:
	// File loading with error handling
	void requestOpenInputFile(const QString& filePath);
	void requestOpenGroundTruthFile(const QString& filePath);

	// Session state management - slots for GUI widgets
	void setCurrentFrame(int frame);
	void setCurrentPatch(int x, int y);
	void configurePatchGrid(int cols, int rows, int borderExtension = 0);

	// PSF pipeline - slots for GUI widgets
	void setPSFCoefficient(int id, double value);
	void resetPSFCoefficients();

private slots:
	// Handle data changes that require session broadcast
	void handleInputDataChanged();
	void handleOutputDataChanged();
	void handleGroundTruthDataChanged();

private:
	// File operations
	bool openInputFile(const QString& filePath);
	bool openGroundTruthFile(const QString& filePath);

	void initializeComponents();
	void connectSessionSignals();
	void connectPSFModuleSignals();
	bool loadFileToSession(const QString& filePath, bool isGroundTruth);

	// Core components
	ImageSession* imageSession;
	InputDataReader* inputDataReader;
	PSFModule* psfModule;

signals:
	// File loading results
	void inputFileLoaded(const QString& filePath);
	void groundTruthFileLoaded(const QString& filePath);
	void fileLoadError(const QString& filePath, const QString& error);

	// Session state changes - emitted for GUI widgets
	void frameChanged(int frame);
	void patchChanged(int x, int y);
	void imageSessionChanged(ImageSession* imageSession);
	void patchGridConfigured(int cols, int rows, int borderExtension);

	// Session events
	void sessionClosed();

	// PSF pipeline results
	void psfWavefrontUpdated(af::array wavefront);
	void psfUpdated(af::array psf);
	void psfParameterDescriptorsChanged(QVector<WavefrontParameter> descriptors);
};

#endif // APPLICATIONCONTROLLER_H
