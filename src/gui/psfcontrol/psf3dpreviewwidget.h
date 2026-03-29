#ifndef PSF3DPREVIEWWIDGET_H
#define PSF3DPREVIEWWIDGET_H

#include <QWidget>
#include <QImage>
#include <arrayfire.h>

class SliceViewerWidget;

class PSF3DPreviewWidget : public QWidget
{
	Q_OBJECT
public:
	explicit PSF3DPreviewWidget(QWidget* parent = nullptr);

public slots:
	void updatePSF(af::array psf3D);
	void setFrameIndex(int frame);

private:
	void renderXYSlice(int zIndex);
	void renderXZSection(int yIndex);
	QImage renderSliceToImage(const af::array& slice2D, float globalMax);
	void saveVolume();
	void reRenderAll();

	af::array psfVolume;
	float volumeMaxVal;
	bool normalizePerSlice;
	bool logScale;
	bool syncToDataFrame;
	SliceViewerWidget* xyPanel;
	SliceViewerWidget* xzPanel;
};

#endif // PSF3DPREVIEWWIDGET_H
