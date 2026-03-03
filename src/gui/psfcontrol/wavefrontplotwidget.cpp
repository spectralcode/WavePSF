#include "wavefrontplotwidget.h"
#include "qcustomplot.h"
#include "gui/qcppaletteobserver.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QtMath>
#include <QShowEvent>
#include "gui/plotutils.h"


WavefrontPlotWidget::WavefrontPlotWidget(QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

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

	// Default gradient
	this->applyWavefrontGradient();

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

	// Context menu
	this->setupContextMenu();

	// Palette-aware theming
	new QCPPaletteObserver(this->plot);
}

WavefrontPlotWidget::~WavefrontPlotWidget()
{
}

QVariantMap WavefrontPlotWidget::getSettings() const
{
	QVariantMap settings;
	settings.insert("autoScale", this->autoScaleAction->isChecked());
	settings.insert("symmetricZero", this->symmetricZeroAction->isChecked());
	settings.insert("showGrid", this->showGridAction->isChecked());
	settings.insert("showAxis", this->showAxisAction->isChecked());
	settings.insert("showColorScale", this->showColorScaleAction->isChecked());

	int colormapIndex = 13;
	QList<QAction*> actions = this->gradientGroup->actions();
	for (int i = 0; i < actions.size(); i++) {
		if (actions[i]->isChecked()) {
			colormapIndex = i;
			break;
		}
	}
	settings.insert("colormapIndex", colormapIndex);
	return settings;
}

void WavefrontPlotWidget::setSettings(const QVariantMap& settings)
{
	if (settings.isEmpty()) return;

	// Block signals to prevent cascading replots
	this->autoScaleAction->blockSignals(true);
	this->symmetricZeroAction->blockSignals(true);
	this->showGridAction->blockSignals(true);
	this->showAxisAction->blockSignals(true);
	this->showColorScaleAction->blockSignals(true);

	this->autoScaleAction->setChecked(settings.value("autoScale", true).toBool());
	this->symmetricZeroAction->setChecked(settings.value("symmetricZero", true).toBool());
	this->showGridAction->setChecked(settings.value("showGrid", true).toBool());
	this->showAxisAction->setChecked(settings.value("showAxis", true).toBool());
	this->showColorScaleAction->setChecked(settings.value("showColorScale", true).toBool());

	int colormapIndex = settings.value("colormapIndex", 13).toInt();
	QList<QAction*> actions = this->gradientGroup->actions();
	if (colormapIndex >= 0 && colormapIndex < actions.size()) {
		actions[colormapIndex]->setChecked(true);
	}

	this->autoScaleAction->blockSignals(false);
	this->symmetricZeroAction->blockSignals(false);
	this->showGridAction->blockSignals(false);
	this->showAxisAction->blockSignals(false);
	this->showColorScaleAction->blockSignals(false);

	// Manually apply visual state since signals were blocked
	bool showGrid = this->showGridAction->isChecked();
	this->plot->xAxis->grid()->setVisible(showGrid);
	this->plot->yAxis->grid()->setVisible(showGrid);

	bool showAxis = this->showAxisAction->isChecked();
	this->plot->xAxis->setVisible(showAxis);
	this->plot->yAxis->setVisible(showAxis);
	if (showAxis) {
		this->plot->axisRect()->setAutoMargins(QCP::msAll);
	} else {
		this->plot->axisRect()->setAutoMargins(QCP::msNone);
		this->plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
	}

	bool showColorScale = this->showColorScaleAction->isChecked();
	if (showColorScale) {
		this->colorScale->setVisible(true);
		this->plot->plotLayout()->addElement(0, 1, this->colorScale);
	} else {
		this->plot->plotLayout()->take(this->colorScale);
		this->plot->plotLayout()->simplify();
		this->colorScale->setVisible(false);
	}

	this->applyGradient(colormapIndex);
	this->applyDataRange();
	this->plot->replot();
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

	this->applyDataRange();
	this->plot->rescaleAxes();
	this->enforceAspectRatio();
	this->plot->replot();
}

void WavefrontPlotWidget::showEvent(QShowEvent* event)
{
	QWidget::showEvent(event);
	this->resetView();
}

void WavefrontPlotWidget::resetView()
{
	this->plot->rescaleAxes();
	this->enforceAspectRatio();
	this->plot->replot();
}

void WavefrontPlotWidget::enforceAspectRatio()
{
	// Adjust the axis with more room to match the tighter one,
	// so the full data range is always visible with 1:1 aspect ratio
	QCPAxisRect* ar = this->plot->axisRect();
	if (ar->width() > ar->height()) {
		this->plot->xAxis->setScaleRatio(this->plot->yAxis, 1.0);
	} else {
		this->plot->yAxis->setScaleRatio(this->plot->xAxis, 1.0);
	}
}

void WavefrontPlotWidget::applyGradient(int index)
{
	// Indices 0-11 map directly to QCPColorGradient::GradientPreset enum values
	if (index >= 0 && index < 12) {
		this->colorMap->setGradient(
			QCPColorGradient(static_cast<QCPColorGradient::GradientPreset>(index)));
	} else if (index == 12) {
		this->applyBlueWhiteRedGradient();
	} else if (index == 13) {
		this->applyWavefrontGradient();
	} else if (index == 14) {
		this->applySeismicGradient();
	}
	this->plot->replot();
}

void WavefrontPlotWidget::applyBlueWhiteRedGradient()
{
	QCPColorGradient gradient;
	gradient.setColorStopAt(0.0, QColor(0, 0, 180));
	gradient.setColorStopAt(0.5, QColor(255, 255, 255));
	gradient.setColorStopAt(1.0, QColor(180, 0, 0));
	this->colorMap->setGradient(gradient);
}

void WavefrontPlotWidget::applyWavefrontGradient()
{
	QCPColorGradient gradient;
	gradient.setColorStopAt(0.0, Qt::darkBlue);
	gradient.setColorStopAt(0.2, Qt::blue);
	gradient.setColorStopAt(0.5, Qt::white);
	gradient.setColorStopAt(0.6, QColor(255, 0, 127));
	gradient.setColorStopAt(0.8, Qt::red);
	gradient.setColorStopAt(1.0, Qt::yellow);
	gradient.setColorInterpolation(QCPColorGradient::ciRGB);
	this->colorMap->setGradient(gradient);
}

void WavefrontPlotWidget::applySeismicGradient()
{
	// Matplotlib "seismic" colormap: dark blue → blue → white → red → dark red
	QCPColorGradient gradient;
	gradient.setColorStopAt(0.0,  QColor(0, 0, 77));
	gradient.setColorStopAt(0.25, QColor(0, 0, 255));
	gradient.setColorStopAt(0.5,  QColor(255, 255, 255));
	gradient.setColorStopAt(0.75, QColor(255, 0, 0));
	gradient.setColorStopAt(1.0,  QColor(128, 0, 0));
	gradient.setColorInterpolation(QCPColorGradient::ciRGB);
	this->colorMap->setGradient(gradient);
}

void WavefrontPlotWidget::setupContextMenu()
{
	this->plot->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this->plot, &QWidget::customContextMenuRequested,
			this, &WavefrontPlotWidget::showContextMenu);

	this->contextMenu = new QMenu(this);

	QAction* saveAction = new QAction(tr("Save Plot as..."), this);
	connect(saveAction, &QAction::triggered, this, [this]() {
		PlotUtils::saveColorMapToDisk(this->plot, this->colorMap, this);
	});
	this->contextMenu->addAction(saveAction);

	this->contextMenu->addSeparator();

	this->autoScaleAction = new QAction("Auto-Scale Range", this);
	this->autoScaleAction->setCheckable(true);
	this->autoScaleAction->setChecked(true);
	connect(this->autoScaleAction, &QAction::toggled, this, [this]() {
		this->applyDataRange();
		this->plot->replot();
	});
	this->contextMenu->addAction(this->autoScaleAction);

	this->symmetricZeroAction = new QAction("Symmetric Zero", this);
	this->symmetricZeroAction->setCheckable(true);
	this->symmetricZeroAction->setChecked(true);
	connect(this->symmetricZeroAction, &QAction::toggled, this, [this]() {
		this->applyDataRange();
		this->plot->replot();
	});
	this->contextMenu->addAction(this->symmetricZeroAction);

	this->contextMenu->addSeparator();

	this->showGridAction = new QAction(tr("Show Grid"), this);
	this->showGridAction->setCheckable(true);
	this->showGridAction->setChecked(true);
	connect(this->showGridAction, &QAction::toggled, this, [this](bool checked) {
		this->plot->xAxis->grid()->setVisible(checked);
		this->plot->yAxis->grid()->setVisible(checked);
		this->plot->replot();
	});
	this->contextMenu->addAction(this->showGridAction);

	this->showAxisAction = new QAction(tr("Show Axis"), this);
	this->showAxisAction->setCheckable(true);
	this->showAxisAction->setChecked(true);
	connect(this->showAxisAction, &QAction::toggled, this, [this](bool checked) {
		this->plot->xAxis->setVisible(checked);
		this->plot->yAxis->setVisible(checked);
		if (checked) {
			this->plot->axisRect()->setAutoMargins(QCP::msAll);
		} else {
			this->plot->axisRect()->setAutoMargins(QCP::msNone);
			this->plot->axisRect()->setMargins(QMargins(0, 0, 0, 0));
		}
		this->plot->replot();
	});
	this->contextMenu->addAction(this->showAxisAction);

	this->showColorScaleAction = new QAction(tr("Show Color Scale"), this);
	this->showColorScaleAction->setCheckable(true);
	this->showColorScaleAction->setChecked(true);
	connect(this->showColorScaleAction, &QAction::toggled, this, [this](bool visible) {
		if (visible) {
			this->colorScale->setVisible(true);
			this->plot->plotLayout()->addElement(0, 1, this->colorScale);
		} else {
			this->plot->plotLayout()->take(this->colorScale);
			this->plot->plotLayout()->simplify();
			this->colorScale->setVisible(false);
		}
		this->plot->replot();
	});
	this->contextMenu->addAction(this->showColorScaleAction);

	this->contextMenu->addSeparator();

	// Color map submenu
	QMenu* colorMapMenu = new QMenu(tr("Color Map"), this->contextMenu);
	this->gradientGroup = new QActionGroup(this);
	this->gradientGroup->setExclusive(true);

	QStringList names = {
		tr("Grayscale"), tr("Hot"), tr("Cold"), tr("Night"), tr("Candy"),
		tr("Geography"), tr("Ion"), tr("Thermal"), tr("Polar"), tr("Spectrum"),
		tr("Jet"), tr("Hues"), tr("Blue-White-Red"), tr("Wavefront"), tr("Seismic")
	};

	for (int i = 0; i < names.size(); i++) {
		QAction* action = colorMapMenu->addAction(names[i]);
		action->setCheckable(true);
		action->setChecked(i == 13); // Wavefront default
		this->gradientGroup->addAction(action);
		connect(action, &QAction::triggered, this, [this, i]() {
			this->applyGradient(i);
		});
	}

	this->contextMenu->addMenu(colorMapMenu);
}

void WavefrontPlotWidget::showContextMenu(const QPoint& pos)
{
	this->contextMenu->exec(this->plot->mapToGlobal(pos));
}

void WavefrontPlotWidget::applyDataRange()
{
	if (this->autoScaleAction->isChecked()) {
		this->colorMap->rescaleDataRange();
	}
	if (this->symmetricZeroAction->isChecked()) {
		QCPRange dataRange = this->colorMap->dataRange();
		double maxAbs = qMax(qAbs(dataRange.lower), qAbs(dataRange.upper));
		if (maxAbs > 0.0) {
			this->colorMap->setDataRange(QCPRange(-maxAbs, maxAbs));
		}
	}
}
