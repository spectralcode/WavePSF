#ifndef DATACROSSSECTIONWIDGET_H
#define DATACROSSSECTIONWIDGET_H

#include <QWidget>
#include <QImage>

class ImageData;
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

	void setInputData(const ImageData* data);
	void setOutputData(const ImageData* data);
	void setDisplaySettings(bool autoRange, double minValue, double maxValue);
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
		QGraphicsView* view;
		QGraphicsScene* scene;
		QGraphicsPixmapItem* pixmapItem;
		QGraphicsLineItem* frameLine;
		QLabel* titleLabel;
		QString baseTitle;
		int lastWidth;
		int lastHeight;
	};

	Panel createPanel(const QString& title);
	void updatePanel(Panel& panel, const ImageData* data, int yIndex);
	void fitPanelToView(Panel& panel);
	void updateFrameLine(Panel& panel, int frame, int numFrames);
	QImage extractXZSlice(const ImageData* data, int yIndex);

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
	bool autoRangeEnabled;
	double manualMinValue;
	double manualMaxValue;
	bool pendingRefresh;
	bool draggingFrameLine;
	Panel* draggingPanel;
};

#endif // DATACROSSSECTIONWIDGET_H
