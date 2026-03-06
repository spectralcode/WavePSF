#include "coefficienteditorwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QScrollArea>
#include <QEvent>

namespace {
	const QString SETTINGS_GROUP = QStringLiteral("coefficient_editor");
}


CoefficientEditorWidget::CoefficientEditorWidget(QWidget* parent)
	: QWidget(parent)
	, scrollLayout(nullptr)
	, stepSizeSpinBox(nullptr)
	, updatingControls(false)
{
	QVBoxLayout* mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);

	// Top row: Reset button + Step size control
	QHBoxLayout* topRow = new QHBoxLayout();

	QPushButton* resetButton = new QPushButton(tr("Reset All"), this);
	connect(resetButton, &QPushButton::clicked, this, &CoefficientEditorWidget::resetAll);
	topRow->addWidget(resetButton);

	topRow->addStretch();

	QLabel* stepLabel = new QLabel(tr("Step:"), this);
	topRow->addWidget(stepLabel);

	this->stepSizeSpinBox = new QDoubleSpinBox(this);
	this->stepSizeSpinBox->setRange(0.0001, 0.1);
	this->stepSizeSpinBox->setSingleStep(0.001);
	this->stepSizeSpinBox->setDecimals(4);
	this->stepSizeSpinBox->setValue(0.001);
	connect(this->stepSizeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
			this, &CoefficientEditorWidget::updateStepSize);
	topRow->addWidget(this->stepSizeSpinBox);

	mainLayout->addLayout(topRow);

	QScrollArea* scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	QWidget* scrollContent = new QWidget(scrollArea);
	this->scrollLayout = new QVBoxLayout(scrollContent);
	this->scrollLayout->setContentsMargins(4, 0, 4, 0);
	this->scrollLayout->setSpacing(10);
	this->scrollLayout->addStretch();

	scrollArea->setWidget(scrollContent);
	mainLayout->addWidget(scrollArea);
}

CoefficientEditorWidget::~CoefficientEditorWidget()
{
}

QString CoefficientEditorWidget::getName() const
{
	return SETTINGS_GROUP;
}

void CoefficientEditorWidget::setValues(const QVector<double>& values)
{
	this->updatingControls = true;
	for (int i = 0; i < qMin(values.size(), this->rows.size()); i++) {
		this->rows[i].spinBox->setValue(values.at(i));
		this->rows[i].slider->setValue(static_cast<int>(values.at(i) * SLIDER_SCALE_FACTOR));
	}
	this->updatingControls = false;
}

void CoefficientEditorWidget::setParameterDescriptors(QVector<WavefrontParameter> descriptors)
{
	this->descriptors = descriptors;

	// Sync the step size spinbox from descriptors (all share the same step value)
	if (!descriptors.isEmpty()) {
		this->stepSizeSpinBox->setValue(descriptors.first().step);
	}

	this->clearRows();
	this->buildRows();
}

bool CoefficientEditorWidget::eventFilter(QObject* obj, QEvent* event)
{
	if (event->type() == QEvent::Wheel) {
		QSlider* slider = qobject_cast<QSlider*>(obj);
		if (slider) {
			event->ignore();
			return true;
		}
		QDoubleSpinBox* spinBox = qobject_cast<QDoubleSpinBox*>(obj);
		if (spinBox && !spinBox->hasFocus()) {
			event->ignore();
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void CoefficientEditorWidget::handleSliderChanged(int id, int sliderValue)
{
	if (this->updatingControls) return;

	double value = static_cast<double>(sliderValue) / SLIDER_SCALE_FACTOR;

	this->updatingControls = true;
	for (CoefficientRow& row : this->rows) {
		if (row.id == id) {
			row.spinBox->setValue(value);
			break;
		}
	}
	this->updatingControls = false;

	emit coefficientChanged(id, value);
}

void CoefficientEditorWidget::handleSpinBoxChanged(int id, double value)
{
	if (this->updatingControls) return;

	this->updatingControls = true;
	for (CoefficientRow& row : this->rows) {
		if (row.id == id) {
			row.slider->setValue(static_cast<int>(value * SLIDER_SCALE_FACTOR));
			break;
		}
	}
	this->updatingControls = false;

	emit coefficientChanged(id, value);
}

void CoefficientEditorWidget::resetAll()
{
	this->updatingControls = true;
	for (CoefficientRow& row : this->rows) {
		double defaultVal = 0.0;
		for (const WavefrontParameter& desc : qAsConst(this->descriptors)) {
			if (desc.id == row.id) {
				defaultVal = desc.defaultValue;
				break;
			}
		}
		row.spinBox->setValue(defaultVal);
		row.slider->setValue(static_cast<int>(defaultVal * SLIDER_SCALE_FACTOR));
	}
	this->updatingControls = false;

	emit resetRequested();
}

void CoefficientEditorWidget::updateStepSize(double step)
{
	int scaledStep = qMax(1, static_cast<int>(step * SLIDER_SCALE_FACTOR));
	for (CoefficientRow& row : this->rows) {
		row.slider->setSingleStep(scaledStep);
		row.spinBox->setSingleStep(step);
	}
}

void CoefficientEditorWidget::clearRows()
{
	this->rows.clear();
	// Remove all items from scroll layout except the stretch at the end
	while (this->scrollLayout->count() > 1) {
		QLayoutItem* item = this->scrollLayout->takeAt(0);
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}
}

void CoefficientEditorWidget::buildRows()
{
	double currentStep = this->stepSizeSpinBox->value();

	for (const WavefrontParameter& desc : qAsConst(this->descriptors)) {
		QWidget* rowWidget = new QWidget(this);
		QVBoxLayout* rowLayout = new QVBoxLayout(rowWidget);
		rowLayout->setContentsMargins(0, 2, 0, 2);
		rowLayout->setSpacing(2);

		// Line 1: Noll index and name
		QLabel* label = new QLabel(QString("%1. %2").arg(desc.id).arg(desc.name), rowWidget);
		label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

		// Line 2: Slider + SpinBox
		QSlider* slider = new QSlider(Qt::Horizontal, rowWidget);
		slider->setRange(
			static_cast<int>(desc.minValue * SLIDER_SCALE_FACTOR),
			static_cast<int>(desc.maxValue * SLIDER_SCALE_FACTOR)
		);
		slider->setValue(static_cast<int>(desc.defaultValue * SLIDER_SCALE_FACTOR));
		slider->setSingleStep(qMax(1, static_cast<int>(currentStep * SLIDER_SCALE_FACTOR)));
		slider->setFocusPolicy(Qt::StrongFocus);
		slider->installEventFilter(this);

		QDoubleSpinBox* spinBox = new QDoubleSpinBox(rowWidget);
		spinBox->setRange(desc.minValue, desc.maxValue);
		spinBox->setSingleStep(currentStep);
		spinBox->setDecimals(4);
		spinBox->setValue(desc.defaultValue);
		spinBox->setFocusPolicy(Qt::StrongFocus);
		spinBox->installEventFilter(this);

		rowLayout->addWidget(label);
		QHBoxLayout* controlsRow = new QHBoxLayout();
		controlsRow->setContentsMargins(0, 0, 0, 0);
		controlsRow->addWidget(slider, 1);
		controlsRow->addWidget(spinBox);
		rowLayout->addLayout(controlsRow);

		CoefficientRow row;
		row.id = desc.id;
		row.slider = slider;
		row.spinBox = spinBox;
		this->rows.append(row);

		// Connect signals using lambdas to capture the id
		int id = desc.id;
		connect(slider, &QSlider::valueChanged, this, [this, id](int value) {
			this->handleSliderChanged(id, value);
		});
		connect(spinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, id](double value) {
			this->handleSpinBoxChanged(id, value);
		});

		// Insert before the stretch
		this->scrollLayout->insertWidget(this->scrollLayout->count() - 1, rowWidget);
	}
}
