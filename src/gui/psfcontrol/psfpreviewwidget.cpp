#include "psfpreviewwidget.h"
#include "gui/lut.h"
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QResizeEvent>
#include <QtMath>


PSFPreviewWidget::PSFPreviewWidget(QWidget* parent)
	: QWidget(parent)
	, lastWidth(0)
	, lastHeight(0)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	this->scene = new QGraphicsScene(this);
	this->scene->setItemIndexMethod(QGraphicsScene::NoIndex);

	this->view = new QGraphicsView(this->scene, this);
	this->view->setCacheMode(QGraphicsView::CacheBackground);
	this->view->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
	this->view->setRenderHint(QPainter::Antialiasing);
	this->view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	this->view->setDragMode(QGraphicsView::ScrollHandDrag);
	this->view->setMinimumSize(64, 64);

	this->pixmapItem = new QGraphicsPixmapItem();
	this->scene->addItem(this->pixmapItem);

	layout->addWidget(this->view);

	// Install event filter on the viewport to handle mouse interactions
	this->view->viewport()->installEventFilter(this);
}

PSFPreviewWidget::~PSFPreviewWidget()
{
}

void PSFPreviewWidget::updateImage(af::array psf)
{
	if (psf.isempty()) return;

	this->lastPSF = psf;

	// Extract focal plane if PSF is 3D
	if (psf.numdims() > 2 && psf.dims(2) > 1) {
		int centerZ = static_cast<int>(psf.dims(2)) / 2;
		psf = psf(af::span, af::span, centerZ);
	}

	int rows = psf.dims(0);
	int cols = psf.dims(1);

	// Copy to host
	QVector<float> hostData(rows * cols);
	psf.as(f32).host(hostData.data());

	// Find max for log-scale normalization
	float maxVal = 0.0f;
	for (float v : qAsConst(hostData)) {
		if (v > maxVal) maxVal = v;
	}

	if (maxVal <= 0.0f) return;

	// Convert to QImage using log scale for better visibility
	this->currentImage = QImage(cols, rows, QImage::Format_Indexed8);
	this->currentImage.setColorTable(LUT::get(this->lutName));
	float logMax = qLn(1.0f + maxVal);

	for (int y = 0; y < rows; ++y) {
		uchar* scanLine = this->currentImage.scanLine(y);
		for (int x = 0; x < cols; ++x) {
			float val = hostData[y + x * rows]; // column-major: element(row=y, col=x)
			float logVal = qLn(1.0f + qMax(0.0f, val));
			scanLine[x] = static_cast<uchar>(255.0f * logVal / logMax);
		}
	}

	// Update the graphics item
	this->pixmapItem->setPixmap(QPixmap::fromImage(this->currentImage));

	// Auto-fit when image size changes
	if (this->lastWidth != cols || this->lastHeight != rows) {
		this->lastWidth = cols;
		this->lastHeight = rows;
		this->fitToView();
	}
}

void PSFPreviewWidget::clearPreview()
{
	this->pixmapItem->setPixmap(QPixmap());
	this->currentImage = QImage();
	this->lastPSF = af::array();
	this->lastWidth = 0;
	this->lastHeight = 0;
}

bool PSFPreviewWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (obj != this->view->viewport()) {
		return QWidget::eventFilter(obj, event);
	}

	switch (event->type()) {
		case QEvent::Wheel: {
			QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
			double factor = (wheelEvent->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
			this->scaleView(factor);
			return true;
		}

		case QEvent::MouseButtonDblClick: {
			this->fitToView();
			return true;
		}

		case QEvent::ContextMenu: {
			QContextMenuEvent* contextEvent = static_cast<QContextMenuEvent*>(event);
			QMenu menu(this);
			QMenu* colormapMenu = menu.addMenu(tr("Colormap"));
			for (const QString& name : LUT::availableNames()) {
				QAction* action = colormapMenu->addAction(QIcon(LUT::getPreviewPixmap(name, 64, 12)), name);
				action->setCheckable(true);
				action->setChecked(name == this->lutName);
			}
			QAction* saveAction = menu.addAction(tr("Save image..."));
			connect(saveAction, &QAction::triggered, this, &PSFPreviewWidget::saveImage);
			QAction* chosen = menu.exec(contextEvent->globalPos());
			if (chosen && colormapMenu->actions().contains(chosen)) {
				this->setLutName(chosen->text());
			}
			return true;
		}

		default:
			break;
	}

	return QWidget::eventFilter(obj, event);
}

void PSFPreviewWidget::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	if (!this->currentImage.isNull()) {
		this->fitToView();
	}
}

void PSFPreviewWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	if (!this->currentImage.isNull()) {
		this->fitToView();
	}
}

void PSFPreviewWidget::setLutName(const QString& name)
{
	this->lutName = name;
	if (!this->lastPSF.isempty()) {
		this->updateImage(this->lastPSF);
	}
}

void PSFPreviewWidget::fitToView()
{
	this->scene->setSceneRect(this->scene->itemsBoundingRect());
	this->view->fitInView(this->scene->sceneRect(), Qt::KeepAspectRatio);
	this->view->ensureVisible(this->pixmapItem);
	this->view->centerOn(this->scene->itemsBoundingRect().center());
}

void PSFPreviewWidget::saveImage()
{
	QString fileName = QFileDialog::getSaveFileName(this, tr("Save PSF Image"),
		QString(), tr("PNG Image (*.png)"));
	if (!fileName.isEmpty()) {
		this->pixmapItem->pixmap().save(fileName, "PNG");
	}
}

void PSFPreviewWidget::scaleView(qreal scaleFactor)
{
	qreal factor = this->view->transform().scale(scaleFactor, scaleFactor)
		.mapRect(QRectF(0, 0, 1, 1)).width();
	if (factor < 0.07 || factor > 100.0) {
		return;
	}
	this->view->scale(scaleFactor, scaleFactor);
}
