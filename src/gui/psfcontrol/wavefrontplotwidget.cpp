#include "wavefrontplotwidget.h"
#include "qcustomplot.h"
#include <QVBoxLayout>
#include <QComboBox>
#include <QtMath>


WavefrontPlotWidget::WavefrontPlotWidget(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	// Gradient selector combo box
	this->gradientCombo = new QComboBox(this);
	this->setupGradientCombo();
	layout->addWidget(this->gradientCombo);

	this->plot = new QCustomPlot(this);
	layout->addWidget(this->plot);

	// Setup color map
	this->colorMap = new QCPColorMap(this->plot->xAxis, this->plot->yAxis);
	this->colorMap->setInterpolate(false);

	// Setup color scale
	this->colorScale = new QCPColorScale(this->plot);
	this->plot->plotLayout()->addElement(0, 1, this->colorScale);
	this->colorScale->setType(QCPAxis::atRight);
	this->colorMap->setColorScale(this->colorScale);

	// Default gradient (blue-white-red diverging)
	this->applyCustomGradient();

	// Axes: visible with labels, range set on data update
	this->plot->xAxis->setVisible(true);
	this->plot->xAxis->setLabel("x");
	this->plot->yAxis->setVisible(true);
	this->plot->yAxis->setLabel("y");

	// Enable drag and zoom interactions
	this->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

	// Double-click to reset view
	connect(this->plot, &QCustomPlot::mouseDoubleClick,
			this, &WavefrontPlotWidget::resetView);

	// Maintain 1:1 aspect ratio on every replot (zoom, pan, resize)
	connect(this->plot, &QCustomPlot::beforeReplot,
			this, &WavefrontPlotWidget::enforceAspectRatio);

	// Align margins between plot and color scale
	QCPMarginGroup* marginGroup = new QCPMarginGroup(this->plot);
	this->plot->axisRect()->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);
	this->colorScale->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);

	// Gradient selector signal
	connect(this->gradientCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &WavefrontPlotWidget::applyGradient);
}

WavefrontPlotWidget::~WavefrontPlotWidget()
{
}

void WavefrontPlotWidget::updatePlot(af::array wavefront)
{
	if (wavefront.isempty()) return;

	int rows = wavefront.dims(0);
	int cols = wavefront.dims(1);

	// Copy ArrayFire data to host
	QVector<float> hostData(rows * cols);
	wavefront.as(f32).host(hostData.data());

	// Update color map dimensions with normalized [-1, 1] range
	this->colorMap->data()->setSize(cols, rows);
	this->colorMap->data()->setRange(QCPRange(-1, 1), QCPRange(-1, 1));

	// Fill color map data; use per-cell alpha to hide area outside unit circle
	for (int y = 0; y < rows; ++y) {
		double yNorm = 2.0 * y / (rows - 1) - 1.0;
		for (int x = 0; x < cols; ++x) {
			double xNorm = 2.0 * x / (cols - 1) - 1.0;
			double r = qSqrt(xNorm * xNorm + yNorm * yNorm);
			// Column-major: element(row=y, col=x) stored at y + x * rows
			this->colorMap->data()->setCell(x, y, hostData[y + x * rows]);
			this->colorMap->data()->setAlpha(x, y, (r > 1.0) ? 0 : 255);
		}
	}

	this->colorMap->rescaleDataRange();
	this->plot->rescaleAxes();
	this->plot->yAxis->setScaleRatio(this->plot->xAxis, 1.0);
	this->plot->replot();
}

void WavefrontPlotWidget::resetView()
{
	this->plot->rescaleAxes();
	this->plot->yAxis->setScaleRatio(this->plot->xAxis, 1.0);
	this->plot->replot();
}

void WavefrontPlotWidget::enforceAspectRatio()
{
	// Called via beforeReplot — just set the ratio, no replot() needed
	this->plot->yAxis->setScaleRatio(this->plot->xAxis, 1.0);
}

void WavefrontPlotWidget::setupGradientCombo()
{
	this->gradientCombo->addItem(tr("Blue-White-Red"));
	this->gradientCombo->addItem(tr("Grayscale"));
	this->gradientCombo->addItem(tr("Hot"));
	this->gradientCombo->addItem(tr("Cold"));
	this->gradientCombo->addItem(tr("Thermal"));
	this->gradientCombo->addItem(tr("Polar"));
	this->gradientCombo->addItem(tr("Spectrum"));
	this->gradientCombo->addItem(tr("Jet"));
	this->gradientCombo->addItem(tr("Hues"));
}

void WavefrontPlotWidget::applyGradient(int index)
{
	if (index == 0) {
		this->applyCustomGradient();
	} else {
		// Map combo index to QCPColorGradient preset
		static const QCPColorGradient::GradientPreset presets[] = {
			QCPColorGradient::gpGrayscale,  // index 1
			QCPColorGradient::gpHot,        // index 2
			QCPColorGradient::gpCold,       // index 3
			QCPColorGradient::gpThermal,    // index 4
			QCPColorGradient::gpPolar,      // index 5
			QCPColorGradient::gpSpectrum,   // index 6
			QCPColorGradient::gpJet,        // index 7
			QCPColorGradient::gpHues        // index 8
		};
		int presetIndex = index - 1;
		if (presetIndex >= 0 && presetIndex < static_cast<int>(sizeof(presets) / sizeof(presets[0]))) {
			this->colorMap->setGradient(QCPColorGradient(presets[presetIndex]));
		}
	}
	this->plot->replot();
}

void WavefrontPlotWidget::applyCustomGradient()
{
	QCPColorGradient gradient;
	gradient.setColorStopAt(0.0, QColor(0, 0, 180));
	gradient.setColorStopAt(0.5, QColor(255, 255, 255));
	gradient.setColorStopAt(1.0, QColor(180, 0, 0));
	this->colorMap->setGradient(gradient);
}
