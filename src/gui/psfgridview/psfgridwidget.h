#ifndef PSFGRIDWIDGET_H
#define PSFGRIDWIDGET_H

#include <QWidget>
#include <QVariantMap>
#include <QTransform>
#include "core/psf/psfgridgenerator.h"

class QCheckBox;
class QPushButton;
class QProgressBar;
class QSpinBox;
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
	void rotate90();
	void flipH();
	void flipV();
	void updateSinglePSF(af::array psf, int patchX, int patchY);
	void invalidateGrid();
	void generationStarted(int totalPatches);
	void generationProgressUpdated(int completedPatches, int totalPatches);
	void handleVisibilityChanged(bool visible);
	void applyViewTransform(QTransform transform, QPointF center);
	void setSyncActive(bool active);

signals:
	void generateRequested(int frame, int cropSize);
	void cancelGenerationRequested();
	void updatePatchRequested();
	void patchClicked(int x, int y);

private slots:
	void onUpdatePatchClicked();
	void onUpdateFrameClicked();
	void showContextMenu(const QPoint& pos);

private:
	void setupUI();
	void finishGeneration();
	void startPendingFrameUpdate();
	void setStatus(const QString& text);
	void updateHighlight();
	QPair<int, int> cellAtScenePos(QPointF scenePos) const;
	void saveMosaicAs(const QString& format);
	void saveMosaicAsTif(const QString& filePath);

	bool eventFilter(QObject* obj, QEvent* event) override;

	// Layout
	QSplitter* splitter;

	// Controls
	QPushButton* updatePatchButton;
	QCheckBox* autoUpdatePatchCheckBox;
	QPushButton* updateFrameButton;
	QCheckBox* autoUpdateFrameCheckBox;
	QProgressBar* progressBar;
	QSpinBox* cropSizeSpinBox;

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
	bool syncActive;
	bool generationInProgress;
	bool discardPendingResult;
	bool frameUpdatePending;
	bool manualPatchUpdate;
	bool gridOutdated;
	bool resetGridOnNextPatchUpdate;
	QTransform viewOrientation;
	PSFGridResult lastResult;
};

#endif // PSFGRIDWIDGET_H
