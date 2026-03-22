#ifndef PSFGRIDWIDGET_H
#define PSFGRIDWIDGET_H

#include <QWidget>
#include <QVariantMap>
#include "core/psf/psfgridgenerator.h"

class QPushButton;
class QSpinBox;
class QLabel;
class QSplitter;
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QGraphicsRectItem;

class PSFGridWidget : public QWidget
{
	Q_OBJECT
public:
	explicit PSFGridWidget(QWidget* parent = nullptr);

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void displayPSFGrid(const PSFGridResult& result);
	void setCurrentPatch(int x, int y);
	void setPatchGridDimensions(int cols, int rows, int borderExtension);
	void setCurrentFrame(int frame);

signals:
	void generateRequested(int frame, int cropSize);
	void patchClicked(int x, int y);

private slots:
	void onGenerateClicked();
	void showContextMenu(const QPoint& pos);

private:
	void setupUI();
	void updateHighlight();
	QPair<int, int> cellAtScenePos(QPointF scenePos) const;
	void saveMosaicAs(const QString& format);
	void saveMosaicAsTif(const QString& filePath);

	bool eventFilter(QObject* obj, QEvent* event) override;

	// Layout
	QSplitter* splitter;

	// Controls
	QPushButton* generateButton;
	QSpinBox* cropSizeSpinBox;
	QLabel* infoLabel;

	// Graphics view (zoom/pan)
	QGraphicsView* graphicsView;
	QGraphicsScene* graphicsScene;
	QGraphicsPixmapItem* mosaicItem;
	QGraphicsRectItem* highlightRect;

	// State
	int currentPatchX;
	int currentPatchY;
	int currentFrame;
	int patchCols;
	int patchRows;
	PSFGridResult lastResult;
};

#endif // PSFGRIDWIDGET_H
