#ifndef IMAGESESSIONVIEWER_H
#define IMAGESESSIONVIEWER_H

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QGroupBox>
#include <QSplitter>
#include <QTimer>
#include "controller/imagesession.h"
#include "datacrosssectionwidget.h"
#include "displaysettings.h"

// Forward declarations
class ImageDataViewer;
class DisplayControlBar;
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
	void addSidebarWidget(QWidget* widget);
	void addBottomPanel(QWidget* widget);

	// Display settings accessors
	DisplaySettings getDisplaySettings() const;
	void setDisplaySettings(const DisplaySettings& settings);

public slots:
	// Session updates from ApplicationController
	void updateImageSession(ImageSession* imageSession);
	void setCurrentFrame(int frame);
	void highlightPatch(int x, int y);
	void highlightPatches(const QVector<int>& patchLinearIds);
	void configurePatchGrid(int cols, int rows, int borderExtension);
	void refreshOutputViewer();
	void setViewSyncEnabled(bool enabled);
	void setCrossSectionVisible(bool visible);
	DataCrossSectionWidget* getCrossSectionWidget() const { return this->crossSectionWidget; }
	void rotateViewers90();
	void flipViewersH();
	void flipViewersV();
	void setPatchGridVisible(bool visible);
	void setAxisOverlayVisible(bool visible);

private slots:
	// Internal UI interactions
	void setFrameFromSlider(int frame);
	void setFrameFromSpinBox(int frame);
	void setPatchFromSlider(int patchId);
	void setPatchFromSpinBox(int patchId);

	// Viewer interactions
	void handleInputPatchSelected(int patchId);
	void handleOutputPatchSelected(int patchId);
	void handleInputFileDropped(const QString& filePath);
	void handleFrameChangedFromViewer(int frame);

signals:
	// Requests to ApplicationController
	void frameChangeRequested(int frame);
	void patchChangeRequested(int x, int y);
	void inputFileDropRequested(const QString& filePath);

	// Coefficient operations (forwarded from viewer context menus)
	void copyCoefficientsRequested();
	void pasteCoefficientsRequested();
	void resetCoefficientsRequested();

	void navigatePatch(int dx, int dy);
	void crossSectionVisibilityChanged(bool visible);

	// Settings propagation
	void patchGridConfigurationRequested(int cols, int rows, int borderExtension);

	// View transform forwarding (for PSF grid orientation sync)
	void viewerTransformChanged(QTransform transform, QPointF centerInScene);

private:
	void setupUI();
	void setupFrameControls();
	void setupImageViewers();
	void connectSignals();
	void updateFrameControls();
	void syncViewersToSession();
	void updateDataInViewers();
	ImageDataViewer* otherViewer() const;

	// Current session reference (not owned)
	ImageSession* imageSession;

	// Display settings (shared across all viewers)
	DisplaySettings displaySettings;

	// Main layout
	QSplitter* mainSplitter;
	QWidget* controlsWidget;
	QVBoxLayout* sidebarLayout;
	QSplitter* rightSplitter;
	QWidget* viewersWidget;

	// Frame controls
	QGroupBox* frameControlsGroup;
	QSlider* frameSlider;
	QSpinBox* frameSpinBox;

	// Patch navigation controls
	QSlider* patchSlider;
	QSpinBox* patchSpinBox;

	// Image viewers
	ImageDataViewer* inputViewer;
	ImageDataViewer* outputViewer;
	ImageDataViewer* activeViewer;
	DataCrossSectionWidget* crossSectionWidget;
	DisplayControlBar* displayControlBar;

	// State tracking
	bool updatingControls;
	bool viewSyncEnabled;
	QMetaObject::Connection viewSyncConn1;
	QMetaObject::Connection viewSyncConn2;
	bool crossSectionVisible;
	bool patchGridVisible;

	// Last-connected data pointers (used for safe comparison in updateDataInViewers)
	const ImageData* connectedInputData;
	const ImageData* connectedOutputData;
};

#endif // IMAGESESSIONVIEWER_H
