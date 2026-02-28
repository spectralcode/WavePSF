#ifndef IMAGEDATAVIEWER_H
#define IMAGEDATAVIEWER_H

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFocusEvent>
#include <QThread>
#include <QAtomicInteger>
#include "graphicsview.h"
#include "imagerenderworker.h"

// Forward declaration
class ImageData;

// Comments in English.
// Tabs are used for indentation.

class ImageDataViewer : public QWidget
{
	Q_OBJECT

public:
	explicit ImageDataViewer(const QString& viewerName, QWidget *parent = nullptr);
	~ImageDataViewer();

	// Data connection
	void connectImageData(const ImageData* imageData);
	void disconnectImageData();
	void connectReferenceImageData(const ImageData* referenceImageData);
	const ImageData* getImageData() const;

	// Frame control
	void setCurrentFrame(int frame);
	int getFrameNr() const;

	// Display range controls
	void setAutoRange(bool enabled);
	bool isAutoRange() const;
	void setDisplayRange(double minValue, double maxValue);
	void updateDisplayRange();

	// Patch grid
	void configurePatchGrid(int cols, int rows);
	void setPatchGridVisible(bool visible);
	void highlightPatch(int patchId);

	// Utility
	void reset();

	void enableInputDataDrops(bool enable = true);

protected:
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;
	QSize sizeHint() const override;

private slots:
	void displayFrame(int frameNr);
	void refresh();
	void readCurrentFrameValueAt(int x, int y);
	void selectPatch(int patchId);
	void beginReferencePreview();
	void endReferencePreview();

	void updateRenderedImage(const QImage& image, quint64 requestId); //receive image from worker

	void dispatchRenderNow();

public slots:
	void showReference(bool show);

private:
	void setupUI();
	void connectSignals();
	void updateInfoDisplay();
	const ImageData* getCurrentDataSource() const;

	int getValidFrameForDataSource(int requestedFrame, const ImageData* dataSource) const;

	static int sampleSizeFor(EnviDataType dt);
	static void copyFrameToBytes(QByteArray& out, const void* src, int count, EnviDataType dt);

	GraphicsView* frameView;
	QLabel* labelViewerName;
	QLabel* labelFrameName;
	QLabel* labelX;
	QLabel* labelXCoordinate;
	QLabel* labelY;
	QLabel* labelYCoordinate;
	QLabel* labelValue;
	QLabel* labelPixelValue;

	const ImageData* imageData;
	const ImageData* referenceImageData;
	QString viewerName;
	QString originalViewerName;
	int mousePosX;
	int mousePosY;
	int currentFrame;

	bool autoRangeEnabled;
	double manualMinValue;
	double manualMaxValue;

	bool showingReference;

	QThread* renderThread;
	ImageRenderWorker* renderWorker;
	QAtomicInteger<quint64> latestRequestId;
	bool renderBusy;
	bool hasPending;

signals:
	void currentFrameChanged(int frameNr);
	void currentWavelengthChanged(qreal wavelength);
	void imageDataConnected();
	void patchSelectionChanged(int patchId);
	void deletePressed();
	void copyPressed();
	void pastePressed();
	void togglePressed();
	void toggleReleased();

	void renderRequested(const RenderRequest& req);

	void inputFileDropRequested(const QString& filePath);
};

#endif // IMAGEDATAVIEWER_H
