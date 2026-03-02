#include "interpolationwidget.h"
#include "qcustomplot.h"
#include "gui/qcppaletteobserver.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include "gui/verticalscrollarea.h"
#include <QEvent>
#include <QMenu>
#include "gui/plotutils.h"

InterpolationWidget::InterpolationWidget(QWidget* parent)
	: QWidget(parent)
	, hasResult(false)
{
	this->setupUI();
}

InterpolationWidget::~InterpolationWidget()
{
}

void InterpolationWidget::setupUI()
{
	QHBoxLayout* mainLayout = new QHBoxLayout(this);

	// Left side: controls in scroll area
	VerticalScrollArea* scrollArea = new VerticalScrollArea(this);

	QWidget* controlsWidget = new QWidget(scrollArea);
	QVBoxLayout* controlsLayout = new QVBoxLayout(controlsWidget);

	// Polynomial order
	QGroupBox* settingsGroup = new QGroupBox(tr("Settings"), controlsWidget);
	QGridLayout* settingsLayout = new QGridLayout(settingsGroup);

	settingsLayout->addWidget(new QLabel(tr("Polynomial Order:"), settingsGroup), 0, 0);
	this->polynomialOrderSpinBox = new QSpinBox(settingsGroup);
	this->polynomialOrderSpinBox->setRange(1, 6);
	this->polynomialOrderSpinBox->setValue(3);
	this->installScrollGuard(this->polynomialOrderSpinBox);
	settingsLayout->addWidget(this->polynomialOrderSpinBox, 0, 1);

	controlsLayout->addWidget(settingsGroup);

	// Interpolation buttons
	QGroupBox* actionsGroup = new QGroupBox(tr("Interpolate"), controlsWidget);
	QVBoxLayout* actionsLayout = new QVBoxLayout(actionsGroup);

	this->interpolateXButton = new QPushButton(tr("Interpolate in X"), actionsGroup);
	this->interpolateXButton->setToolTip(tr("Fit polynomial along patch columns for the current row and frame"));
	actionsLayout->addWidget(this->interpolateXButton);

	this->interpolateYButton = new QPushButton(tr("Interpolate in Y"), actionsGroup);
	this->interpolateYButton->setToolTip(tr("Fit polynomial along patch rows for the current column and frame"));
	actionsLayout->addWidget(this->interpolateYButton);

	this->interpolateZButton = new QPushButton(tr("Interpolate in Z"), actionsGroup);
	this->interpolateZButton->setToolTip(tr("Fit polynomial across frames for the current patch"));
	actionsLayout->addWidget(this->interpolateZButton);

	this->interpolateAllZButton = new QPushButton(tr("Interpolate All in Z"), actionsGroup);
	this->interpolateAllZButton->setToolTip(tr("Fit polynomial across frames for all patches"));
	actionsLayout->addWidget(this->interpolateAllZButton);

	controlsLayout->addWidget(actionsGroup);

	// Plot coefficient selector
	QGroupBox* plotGroup = new QGroupBox(tr("Plot"), controlsWidget);
	QGridLayout* plotControlLayout = new QGridLayout(plotGroup);

	plotControlLayout->addWidget(new QLabel(tr("Coefficient:"), plotGroup), 0, 0);
	this->coefficientSpinBox = new QSpinBox(plotGroup);
	this->coefficientSpinBox->setRange(0, 0);
	this->coefficientSpinBox->setValue(0);
	this->coefficientSpinBox->setEnabled(false);
	this->installScrollGuard(this->coefficientSpinBox);
	plotControlLayout->addWidget(this->coefficientSpinBox, 0, 1);

	controlsLayout->addWidget(plotGroup);

	// Status
	this->statusLabel = new QLabel(tr("No interpolation performed yet."), controlsWidget);
	this->statusLabel->setWordWrap(true);
	controlsLayout->addWidget(this->statusLabel);

	controlsLayout->addStretch();

	// Right side: plot
	this->plot = new QCustomPlot(this);
	this->plot->setMinimumSize(200, 150);
	this->plot->xAxis->setLabel(tr("Position"));
	this->plot->yAxis->setLabel(tr("Coefficient Value"));

	// Graph 0: fitted curve (line)
	this->plot->addGraph();
	this->plot->graph(0)->setPen(QPen(QColor(200, 50, 50), 2));
	this->plot->graph(0)->setName(tr("Fitted Curve"));

	// Graph 1: input data points (scatter)
	this->plot->addGraph();
	this->plot->graph(1)->setPen(QPen(QColor(50, 100, 200), 1));
	this->plot->graph(1)->setLineStyle(QCPGraph::lsNone);
	this->plot->graph(1)->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 7));
	this->plot->graph(1)->setName(tr("Data Points"));

	this->plot->legend->setVisible(true);
	this->plot->legend->setFont(QFont(font().family(), 8));

	// Context menu
	this->plot->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this->plot, &QWidget::customContextMenuRequested,
			this, &InterpolationWidget::showPlotContextMenu);

	// Palette-aware theming
	new QCPPaletteObserver(this->plot);

	// Layout assembly
	scrollArea->setWidget(controlsWidget);
	mainLayout->addWidget(scrollArea, 0);
	mainLayout->addWidget(this->plot, 1);

	// Connections
	connect(this->polynomialOrderSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &InterpolationWidget::polynomialOrderChanged);
	connect(this->interpolateXButton, &QPushButton::clicked,
			this, &InterpolationWidget::interpolateInXRequested);
	connect(this->interpolateYButton, &QPushButton::clicked,
			this, &InterpolationWidget::interpolateInYRequested);
	connect(this->interpolateZButton, &QPushButton::clicked,
			this, &InterpolationWidget::interpolateInZRequested);
	connect(this->interpolateAllZButton, &QPushButton::clicked,
			this, &InterpolationWidget::interpolateAllInZRequested);
	connect(this->coefficientSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
			this, &InterpolationWidget::onCoefficientSelectionChanged);
}

void InterpolationWidget::updateInterpolationResult(const InterpolationResult& result)
{
	this->lastResult = result;
	this->hasResult = true;

	// Update coefficient spinbox range
	this->coefficientSpinBox->setEnabled(true);
	this->coefficientSpinBox->setMaximum(result.totalCoefficients - 1);

	// Update status
	int filledCount = result.slices.size();
	this->statusLabel->setText(tr("Interpolated %1 coefficients along %2.")
		.arg(filledCount).arg(result.axisLabel));

	this->updatePlot();
}

void InterpolationWidget::onCoefficientSelectionChanged(int index)
{
	Q_UNUSED(index);
	if (this->hasResult) {
		this->updatePlot();
	}
}

void InterpolationWidget::updatePlot()
{
	this->plot->graph(0)->data()->clear();
	this->plot->graph(1)->data()->clear();

	if (!this->hasResult) {
		this->plot->replot();
		return;
	}

	int selectedCoeff = this->coefficientSpinBox->value();

	// Find the fitted curve data for this coefficient
	if (selectedCoeff < this->lastResult.allValues.size()) {
		const QVector<double>& curveValues = this->lastResult.allValues[selectedCoeff];
		this->plot->graph(0)->setData(this->lastResult.allPositions, curveValues);
	}

	// Find the input data for this coefficient
	for (const InterpolationSlice& slice : this->lastResult.slices) {
		if (slice.coefficientIndex == selectedCoeff) {
			this->plot->graph(1)->setData(slice.inputPositions, slice.inputValues);
			break;
		}
	}

	this->plot->xAxis->setLabel(this->lastResult.axisLabel);
	this->plot->rescaleAxes();
	this->plot->replot();
}

void InterpolationWidget::installScrollGuard(QWidget* widget)
{
	widget->setFocusPolicy(Qt::StrongFocus);
	widget->installEventFilter(this);
}

bool InterpolationWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::Wheel) {
		QWidget* widget = qobject_cast<QWidget*>(obj);
		if (widget && !widget->hasFocus()) {
			event->ignore();
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void InterpolationWidget::showPlotContextMenu(const QPoint& pos)
{
	QMenu menu(this);
	QAction* saveAction = menu.addAction(tr("Save Plot as..."));
	connect(saveAction, &QAction::triggered, this, [this]() {
		PlotUtils::savePlotToDisk(this->plot, this);
	});
	menu.exec(this->plot->mapToGlobal(pos));
}
