#ifndef DATACROSSSECTIONWIDGET_H
#define DATACROSSSECTIONWIDGET_H

#include <QWidget>
#include <QThread>
#include <QAtomicInteger>
#include "displaysettings.h"

class ImageData;
class ImageRenderWorker;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsLineItem;
class QSlider;
class QSpinBox;
class QLabel;

class DataCrossSectionWidget : public QWidget
{
	Q_OBJECT
public:
	explicit DataCrossSectionWidget(QWidget* parent = nullptr);
	~DataCrossSectionWidget();

	void setInputData(const ImageData* data);
	void setOutputData(const ImageData* data);
	void setDisplaySettings(const DisplaySettings& settings);
	void setFrameLineVisible(bool visible);
	bool isFrameLineVisible() const { return this->showFrameLine; }
	int currentYPosition() const;

public slots:
	void setCurrentFrame(int frame);
	void setYPosition(int y);
	void refreshPanels();

signals:
	void frameChangeRequested(int frame);
	void yPositionChanged(int yIndex);

private:
	struct Panel {
		QGraphicsView* view = nullptr;
		QGraphicsScene* scene = nullptr;
		QGraphicsPixmapItem* pixmapItem = nullptr;
		QGraphicsLineItem* frameLine = nullptr;
		QLabel* titleLabel = nullptr;
		QString baseTitle;
		int lastWidth = 0;
		int lastHeight = 0;
		// Render infrastructure (matches ImageDataViewer pattern)
		QThread* renderThread = nullptr;
		ImageRenderWorker* renderWorker = nullptr;
		QAtomicInteger<quint64> latestRequestId;
	};

	void initPanel(Panel& panel, const QString& title);
	void updatePanel(Panel& panel, const ImageData* data, int yIndex);
	void fitPanelToView(Panel& panel);
	void updateFrameLine(Panel& panel, int frame, int numFrames);
	void dispatchPanelRender(Panel& panel, const ImageData* data, int yIndex);
	void handlePanelRendered(Panel& panel, const QImage& image, quint64 reqId);
	QByteArray extractXZSliceBytes(const ImageData* data, int yIndex);

	void setupUI();
	void connectSignals();
	void updateYControls();
	void clearPanel(Panel& panel);

	bool eventFilter(QObject* obj, QEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

	Panel inputPanel;
	Panel outputPanel;
	QSlider* ySlider;
	QSpinBox* ySpinBox;
	const ImageData* inputData;
	const ImageData* outputData;
	int currentFrame;
	bool showFrameLine;
	DisplaySettings displaySettings;
	bool pendingRefresh;
	bool draggingFrameLine;
	Panel* draggingPanel;
};

#endif // DATACROSSSECTIONWIDGET_H
