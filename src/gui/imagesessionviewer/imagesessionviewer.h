#ifndef IMAGESESSIONVIEWER_H
#define IMAGESESSIONVIEWER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QSplitter>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QTimer>
#include "controller/imagesession.h"

// Forward declarations
class ImageDataViewer;
class SettingsFileManager;

class ImageSessionViewer : public QWidget
{
	Q_OBJECT

public:
	explicit ImageSessionViewer(QWidget* parent = nullptr);
	~ImageSessionViewer();

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& m);

public slots:
	// Session updates from ApplicationController
	void updateImageSession(ImageSession* imageSession);
	void setCurrentFrame(int frame);
	void highlightPatch(int x, int y);
	void highlightPatches(const QVector<int>& patchLinearIds);
	void configurePatchGrid(int cols, int rows, int borderExtension);
	void refreshOutputViewer();

private slots:
	// Internal UI interactions
	void setFrameFromSlider(int frame);
	void setFrameFromSpinBox(int frame);
	void setAutoRange(bool enabled);
	void setMinValue(double value);
	void setMaxValue(double value);
	void applyPatchGridSettings();

	// Viewer interactions
	void handleInputPatchSelected(int patchId);
	void handleOutputPatchSelected(int patchId);
	void handleInputFileDropped(const QString& filePath);
	void handleFrameChangedFromViewer(int frame);

signals:
	// Requests to ApplicationController
	void frameChangeRequested(int frame);
	void patchChangeRequested(int x, int y);
	void patchGridConfigurationRequested(int cols, int rows, int borderExtension);
	void inputFileDropRequested(const QString& filePath);

	// Coefficient operations (forwarded from viewer context menus)
	void copyCoefficientsRequested();
	void pasteCoefficientsRequested();
	void resetCoefficientsRequested();

private:
	void setupUI();
	void setupFrameControls();
	void setupDisplayRangeControls();
	void setupPatchGridControls();
	void setupImageViewers();
	void connectSignals();
	void updateFrameControls();
	void updatePatchGridControls();
	void syncViewersToSession();
	void updateDataInViewers();

	// Current session reference (not owned)
	ImageSession* imageSession;

	// Display range settings
	double displayRangeMin;
	double displayRangeMax;
	bool autoRangeEnabled;

	// Main layout
	QSplitter* mainSplitter;
	QWidget* controlsWidget;
	QWidget* viewersWidget;

	// Frame controls
	QGroupBox* frameControlsGroup;
	QLabel* frameInfoLabel;
	QSlider* frameSlider;
	QSpinBox* frameSpinBox;

	// Display range controls
	QGroupBox* displayRangeGroup;
	QCheckBox* autoRangeCheckBox;
	QDoubleSpinBox* minValueSpinBox;
	QDoubleSpinBox* maxValueSpinBox;

	// Patch grid controls
	QGroupBox* patchGridGroup;
	QLabel* patchGridInfoLabel;
	QSpinBox* patchColsSpinBox;
	QSpinBox* patchRowsSpinBox;
	QSpinBox* borderExtensionSpinBox;

	// Image viewers
	ImageDataViewer* inputViewer;
	ImageDataViewer* outputViewer;

	// State tracking
	bool updatingControls;
};

#endif // IMAGESESSIONVIEWER_H
