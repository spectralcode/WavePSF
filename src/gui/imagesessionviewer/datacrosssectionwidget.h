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

public slots:
	void setCurrentFrame(int frame);

private:
	struct Panel {
		QGraphicsView* view;
		QGraphicsScene* scene;
		QGraphicsPixmapItem* pixmapItem;
		QGraphicsLineItem* frameLine;
		QLabel* titleLabel;
		int lastWidth;
		int lastHeight;
	};

	Panel createPanel(const QString& title);
	void updatePanel(Panel& panel, const ImageData* data, int yIndex);
	void fitPanelToView(Panel& panel);
	void updateFrameLine(Panel& panel, int frame, int numFrames);
	QImage extractXZSlice(const ImageData* data, int yIndex);

	bool eventFilter(QObject* obj, QEvent* event) override;
	void showEvent(QShowEvent* event) override;

	Panel inputPanel;
	Panel outputPanel;
	QSlider* ySlider;
	QSpinBox* ySpinBox;
	const ImageData* inputData;
	const ImageData* outputData;
	int currentFrame;
};

#endif // DATACROSSSECTIONWIDGET_H
