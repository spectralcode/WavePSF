#ifndef PSFCONTROLWIDGET_H
#define PSFCONTROLWIDGET_H

#include <QGroupBox>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"

class CoefficientEditorWidget;
class WavefrontPlotWidget;
class PSFPreviewWidget;
class QTabWidget;

class PSFControlWidget : public QGroupBox
{
	Q_OBJECT
public:
	explicit PSFControlWidget(QWidget* parent = nullptr);
	~PSFControlWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void updateWavefront(af::array wavefront);
	void updatePSF(af::array psf);

signals:
	void coefficientChanged(int id, double value);
	void resetRequested();

private:
	QTabWidget* tabWidget;
	CoefficientEditorWidget* coeffEditor;
	WavefrontPlotWidget* wavefrontPlot;
	PSFPreviewWidget* psfPreview;
};

#endif // PSFCONTROLWIDGET_H
