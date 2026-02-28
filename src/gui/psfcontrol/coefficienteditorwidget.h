#ifndef COEFFICIENTEDITORWIDGET_H
#define COEFFICIENTEDITORWIDGET_H

#include <QWidget>
#include <QVector>
#include <QMap>
#include <QVariantMap>
#include "core/psf/wavefrontparameter.h"

class QSlider;
class QDoubleSpinBox;
class QVBoxLayout;

class CoefficientEditorWidget : public QWidget
{
	Q_OBJECT
public:
	explicit CoefficientEditorWidget(QWidget* parent = nullptr);
	~CoefficientEditorWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void setParameterDescriptors(QVector<WavefrontParameter> descriptors);
	void setValues(const QVector<double>& values);

signals:
	void coefficientChanged(int id, double value);
	void resetRequested();

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
	void handleSliderChanged(int id, int sliderValue);
	void handleSpinBoxChanged(int id, double value);
	void resetAll();
	void updateStepSize(double step);

private:
	static const int SLIDER_SCALE_FACTOR = 1000;

	struct CoefficientRow {
		int id;
		QSlider* slider;
		QDoubleSpinBox* spinBox;
	};

	QVector<WavefrontParameter> descriptors;
	QVector<CoefficientRow> rows;
	QVBoxLayout* scrollLayout;
	QDoubleSpinBox* stepSizeSpinBox;
	bool updatingControls;

	void clearRows();
	void buildRows();
};

#endif // COEFFICIENTEDITORWIDGET_H
