#ifndef PSFPREVIEWWIDGET_H
#define PSFPREVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <arrayfire.h>

class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;

class PSFPreviewWidget : public QWidget
{
	Q_OBJECT
public:
	explicit PSFPreviewWidget(QWidget* parent = nullptr);
	~PSFPreviewWidget() override;

public slots:
	void updateImage(af::array psf);

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;

private:
	void fitToView();
	void saveImage();
	void scaleView(qreal scaleFactor);

	QGraphicsView* view;
	QGraphicsScene* scene;
	QGraphicsPixmapItem* pixmapItem;
	QImage currentImage;
	int lastWidth;
	int lastHeight;
};

#endif // PSFPREVIEWWIDGET_H
