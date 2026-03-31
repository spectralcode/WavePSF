#ifndef SLICEVIEWERWIDGET_H
#define SLICEVIEWERWIDGET_H

#include <QWidget>
#include <QImage>
#include <QList>

class QAction;
class QMenu;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QSlider;
class QSpinBox;
class QLabel;

class SliceViewerWidget : public QWidget
{
	Q_OBJECT
public:
	explicit SliceViewerWidget(const QString& axisLabel, const QString& defaultTitle,
	                           QWidget* parent = nullptr);

	void setImage(const QImage& img);
	void setSliderRange(int min, int max);
	void setSliderValue(int value);
	int sliderValue() const;
	int sliderMaximum() const;
	void setTitle(const QString& text);
	void setValueLabel(const QString& text);
	void addContextMenuAction(QAction* action);
	void addContextMenuSubmenu(QMenu* submenu);

signals:
	void sliceChanged(int value);

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;

private:
	void onSliderValueChanged(int value);
	void onSpinBoxValueChanged(int value);
	void fitToView();
	void scaleView(qreal scaleFactor);
	void saveImage();
	void goToCenter();

	QLabel* titleLabel;
	QGraphicsView* view;
	QGraphicsScene* scene;
	QGraphicsPixmapItem* pixmapItem;
	QSlider* slider;
	QSpinBox* spinBox;
	int lastWidth;
	int lastHeight;
	QList<QAction*> extraActions;
	QList<QMenu*> extraSubmenus;
};

#endif // SLICEVIEWERWIDGET_H
