#ifndef PSFGENERATIONWIDGET_H
#define PSFGENERATIONWIDGET_H

#include <QGroupBox>
#include <QVector>
#include <QVariantMap>
#include <arrayfire.h>
#include "core/psf/wavefrontparameter.h"
#include "core/psf/psfsettings.h"

class CoefficientEditorWidget;
class WavefrontPlotWidget;
class PSFPreviewWidget;
class PSF3DPreviewWidget;
class RWSettingsWidget;
class QComboBox;
class QStackedWidget;

class PSFGenerationWidget : public QGroupBox
{
	Q_OBJECT
public:
	explicit PSFGenerationWidget(QWidget* parent = nullptr);
	~PSFGenerationWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);
	PSFSettings getPSFSettings() const;

	CoefficientEditorWidget* coefficientEditor() const;

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void setCoefficients(const QVector<double>& values);
	void updateWavefront(af::array wavefront);
	void updatePSF(af::array psf);
	void setPSFSettings(const PSFSettings& settings);
	void setPSFMode(const QString& modeName);

signals:
	void coefficientChanged(int id, double value);
	void resetRequested();
	void psfModeChangeRequested(QString modeName);
	void inlineSettingsChanged(QVariantMap settings);

private:
	QComboBox* generatorTypeCombo;
	CoefficientEditorWidget* coeffEditor;
	WavefrontPlotWidget* wavefrontPlot;
	PSFPreviewWidget* psfPreview;
	PSF3DPreviewWidget* psf3DPreview;
	QStackedWidget* psfPreviewStack;
	RWSettingsWidget* rwSettingsWidget;
	PSFSettings currentSettings;
};

#endif // PSFGENERATIONWIDGET_H
