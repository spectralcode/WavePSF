#ifndef DECONVOLUTIONSETTINGSWIDGET_H
#define DECONVOLUTIONSETTINGSWIDGET_H

#include <QWidget>
#include <QVariantMap>

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;

class DeconvolutionSettingsWidget : public QWidget
{
	Q_OBJECT
public:
	explicit DeconvolutionSettingsWidget(QWidget* parent = nullptr);
	~DeconvolutionSettingsWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

signals:
	void algorithmChanged(int algorithm);
	void iterationsChanged(int iterations);
	void relaxationFactorChanged(float factor);
	void regularizationFactorChanged(float factor);
	void noiseToSignalFactorChanged(float factor);
	void paddingModeChanged(int mode);
	void accelerationModeChanged(int mode);
	void liveModeChanged(bool enabled);
	void deconvolutionRequested();

private slots:
	void onAlgorithmChanged(int index);
	void updateParameterVisibility(int algorithmIndex);

private:
	void setupUI();
	void installScrollGuard(QWidget* widget);
	bool eventFilter(QObject* obj, QEvent* event) override;

	QComboBox* algorithmComboBox;
	QSpinBox* iterationsSpinBox;
	QDoubleSpinBox* relaxationFactorSpinBox;
	QDoubleSpinBox* regularizationFactorSpinBox;
	QDoubleSpinBox* noiseToSignalFactorSpinBox;
	QCheckBox* liveModeCheckBox;
	QPushButton* deconvolveButton;

	// Labels for parameter rows (for show/hide)
	QLabel* iterationsLabel;
	QLabel* relaxationLabel;
	QLabel* regularizationLabel;
	QLabel* noiseToSignalLabel;
	QLabel* paddingModeLabel;
	QComboBox* paddingModeComboBox;
	QLabel* accelerationModeLabel;
	QComboBox* accelerationModeComboBox;
};

#endif // DECONVOLUTIONSETTINGSWIDGET_H
