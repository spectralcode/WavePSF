#ifndef OPTIMIZATIONWIDGET_H
#define OPTIMIZATIONWIDGET_H

#include <QWidget>
#include <QVariantMap>
#include <QVector>
#include <QElapsedTimer>
#include "core/psf/wavefrontparameter.h"
#include "core/optimization/optimizationworker.h"

class QVBoxLayout;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QCheckBox;
class QGroupBox;
class QCustomPlot;

class OptimizationWidget : public QWidget
{
	Q_OBJECT
public:
	explicit OptimizationWidget(QWidget* parent = nullptr);
	~OptimizationWidget() override;

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

public slots:
	void setParameterDescriptors(const QVector<WavefrontParameter>& descriptors);
	void setGroundTruthAvailable(bool available);
	void updateProgress(const OptimizationProgress& progress);
	void onOptimizationFinished(const OptimizationResult& result);
	void onOptimizationStarted();

signals:
	void optimizationRequested(OptimizationConfig config);
	void optimizationCancelRequested();
	void patchSelectionChanged(QVector<int> patchLinearIds);

private slots:
	void onModeChanged(int index);
	void onMetricModeChanged(int index);
	void onStartCoefficientSourceChanged(int index);
	void onStartClicked();
	void onCancelClicked();
	void onPatchTextChanged(const QString& text);
	void resetPlotView();
	void showPlotContextMenu(const QPoint& pos);

private:
	void setupUI();
	void setupModeSection(QVBoxLayout* layout);
	void setupBatchSection(QVBoxLayout* layout);
	void setupInitialValuesSection(QVBoxLayout* layout);
	void setupSAParametersSection(QVBoxLayout* layout);
	void setupMetricSection(QVBoxLayout* layout);
	void setupCoefficientSection(QVBoxLayout* layout);
	void setupControlSection(QVBoxLayout* layout);
	void setupStatusSection(QVBoxLayout* layout);
	void setupPlotSection();

	void installScrollGuard(QWidget* widget);
	bool eventFilter(QObject* obj, QEvent* event) override;

	OptimizationConfig buildConfig() const;
	void setRunning(bool running);

	// Parameter descriptors (needed for bounds)
	QVector<WavefrontParameter> parameterDescriptors;
	bool groundTruthAvailable;
	bool isRunning;

	// Mode
	QComboBox* modeComboBox;

	// Batch settings
	QGroupBox* batchGroup;
	QLineEdit* patchesLineEdit;
	QLineEdit* framesLineEdit;

	// Initial values (always visible)
	QGroupBox* initialValuesGroup;
	QComboBox* startCoeffSourceComboBox;
	QLabel* sourceParamLabel;
	QSpinBox* sourceParamSpinBox;

	// SA parameters
	QDoubleSpinBox* startTempSpinBox;
	QDoubleSpinBox* endTempSpinBox;
	QDoubleSpinBox* coolingFactorSpinBox;
	QDoubleSpinBox* perturbanceSpinBox;
	QSpinBox* itersPerTempSpinBox;

	// Metric
	QComboBox* metricModeComboBox;
	QComboBox* metricTypeComboBox;

	// Coefficient specification
	QLineEdit* coefficientSpecLineEdit;

	// Controls
	QPushButton* startButton;
	QPushButton* cancelButton;

	// Live preview
	QCheckBox* livePreviewCheckBox;
	QSpinBox* livePreviewIntervalSpinBox;

	// Status
	QLabel* statusLabel;
	QLabel* batchStatusLabel;
	QLabel* temperatureLabel;
	QLabel* iterationLabel;
	QLabel* bestMetricLabel;

	// Plot
	QCustomPlot* metricPlot;
	QVector<double> plotIterations;
	QVector<double> plotMetricValues;
	QElapsedTimer replotTimer;
};

#endif // OPTIMIZATIONWIDGET_H
