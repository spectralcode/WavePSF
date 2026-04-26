#ifndef DECONVOLUTIONSETTINGSWIDGET_H
#define DECONVOLUTIONSETTINGSWIDGET_H

#include <QWidget>
#include <QMap>
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

public slots:
	void setDeconvolutionRunning(bool running);

signals:
	void algorithmChanged(int algorithm);
	void iterationsChanged(int iterations);
	void relaxationFactorChanged(float factor);
	void tikhonovRegularizationFactorChanged(float factor);
	void wienerNoiseToSignalFactorChanged(float factor);
	void paddingModeChanged(int mode);
	void accelerationModeChanged(int mode);
	void regularizer3DChanged(int mode);
	void regularizationWeightChanged(float weight);
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
	QMap<int, int> iterationsPerAlgorithm;
	int previousAlgorithm;
	QDoubleSpinBox* relaxationFactorSpinBox;
	QDoubleSpinBox* tikhonovRegularizationFactorSpinBox;
	QDoubleSpinBox* wienerNoiseToSignalFactorSpinBox;
	QCheckBox* liveModeCheckBox;
	QPushButton* deconvolveButton;
	bool deconvolutionRunning = false;

	// Labels for parameter rows (for show/hide)
	QLabel* iterationsLabel;
	QLabel* relaxationLabel;
	QLabel* tikhonovRegularizationLabel;
	QLabel* wienerNoiseToSignalLabel;
	QLabel* paddingModeLabel;
	QComboBox* paddingModeComboBox;
	QLabel* accelerationModeLabel;
	QComboBox* accelerationModeComboBox;
	QLabel* regularizerLabel;
	QComboBox* regularizerComboBox;
	QLabel* regularizationWeightLabel;
	QDoubleSpinBox* regularizationWeightSpinBox;
};

#endif // DECONVOLUTIONSETTINGSWIDGET_H
