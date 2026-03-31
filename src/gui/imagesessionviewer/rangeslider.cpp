#include "rangeslider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLineEdit>
#include <QValidator>
#include <QtGlobal>
#include <cmath>

RangeSlider::RangeSlider(QWidget* parent)
	: QWidget(parent)
	, rangeMin(0.0)
	, rangeMax(255.0)
	, lowValue(0.0)
	, highValue(255.0)
	, integerMode(true)
	, dragging(None)
	, dragOffset(0.0)
	, expanded(false)
	, activeEditor(nullptr)
	, editTarget(None)
{
	setMouseTracking(true);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMaximumWidth(300);
}

void RangeSlider::setRange(double absMin, double absMax)
{
	if (absMin > absMax) std::swap(absMin, absMax);
	this->rangeMin = absMin;
	this->rangeMax = absMax;
	this->lowValue = qBound(absMin, this->lowValue, absMax);
	this->highValue = qBound(absMin, this->highValue, absMax);
	update();
}

void RangeSlider::setValues(double low, double high)
{
	if (low > high) std::swap(low, high);
	this->lowValue = qBound(this->rangeMin, low, this->rangeMax);
	this->highValue = qBound(this->rangeMin, high, this->rangeMax);
	update();
}

void RangeSlider::setGradient(const QVector<QRgb>& lut)
{
	this->lutColors = lut;
	update();
}

void RangeSlider::setHistogram(const HistogramData& hist)
{
	this->histogram = hist;
	update();
}

void RangeSlider::clearHistogram()
{
	this->histogram.bins.clear();
	update();
}

void RangeSlider::setIntegerMode(bool intMode)
{
	this->integerMode = intMode;
	update();
}

void RangeSlider::setExpanded(bool exp)
{
	if (this->expanded == exp) return;
	this->expanded = exp;
	updateGeometry();
	update();
}

QString RangeSlider::formatValue(double v) const
{
	if (this->integerMode) {
		return QString::number(qRound(v));
	}
	return QString::number(v, 'f', 2);
}

QRect RangeSlider::barRect() const
{
	int y = (height() - barH()) / 2;
	return QRect(MARGIN, y, width() - 2 * MARGIN, barH());
}

int RangeSlider::valueToX(double value) const
{
	QRect bar = barRect();
	double range = this->rangeMax - this->rangeMin;
	if (range <= 0.0 || bar.width() <= 1) return bar.left();
	double frac = qBound(0.0, (value - this->rangeMin) / range, 1.0);
	return bar.left() + qRound(frac * (bar.width() - 1));
}

double RangeSlider::xToValue(int x) const
{
	QRect bar = barRect();
	if (bar.width() <= 1) return this->rangeMin;
	double frac = static_cast<double>(x - bar.left()) / (bar.width() - 1);
	frac = qBound(0.0, frac, 1.0);
	return this->rangeMin + frac * (this->rangeMax - this->rangeMin);
}

void RangeSlider::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QRect bar = barRect();
	int lutSize = this->lutColors.size();
	int xLow = valueToX(this->lowValue);
	int xHigh = valueToX(this->highValue);

	// Determine first and last LUT colors
	QRgb firstColor = (lutSize >= 256) ? this->lutColors.first() : qRgb(0, 0, 0);
	QRgb lastColor = (lutSize >= 256) ? this->lutColors.last() : qRgb(255, 255, 255);

	// Fill region left of low handle with clamped first color
	if (xLow > bar.left()) {
		p.fillRect(QRect(bar.left(), bar.top(), xLow - bar.left(), barH()), QColor(firstColor));
	}

	// Fill region right of high handle with clamped last color
	if (xHigh < bar.right()) {
		p.fillRect(QRect(xHigh, bar.top(), bar.right() - xHigh + 1, barH()), QColor(lastColor));
	}

	// Draw LUT gradient only between the two handles
	int gradientWidth = xHigh - xLow;
	if (gradientWidth > 0) {
		QImage strip(gradientWidth, 1, QImage::Format_RGB32);
		for (int x = 0; x < gradientWidth; ++x) {
			int idx = x * 255 / qMax(1, gradientWidth - 1);
			if (lutSize >= 256) {
				strip.setPixel(x, 0, this->lutColors.value(idx, qRgb(idx, idx, idx)));
			} else {
				strip.setPixel(x, 0, qRgb(idx, idx, idx));
			}
		}
		QPixmap gradPx = QPixmap::fromImage(strip.scaled(gradientWidth, barH(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
		p.drawPixmap(xLow, bar.top(), gradPx);
	}

	// Draw histogram overlay (clipped to bar)
	const int numBins = this->histogram.bins.size();
	if (numBins > 0) {
		int maxBin = 0;
		for (int i = 0; i < numBins; ++i) {
			if (this->histogram.bins[i] > maxBin) maxBin = this->histogram.bins[i];
		}
		if (maxBin > 0) {
			p.save();
			p.setClipRect(bar);
			const double sqrtMax = std::sqrt(static_cast<double>(maxBin));
			const double domainRange = this->histogram.domainMax - this->histogram.domainMin;
			if (domainRange > 0.0) {
				const double binWidth = domainRange / numBins;
				p.setPen(Qt::NoPen);
				p.setBrush(QColor(255, 255, 255, 100));
				QVector<QPoint> envelope;
				envelope.reserve(numBins);
				for (int i = 0; i < numBins; ++i) {
					const double binLeft = this->histogram.domainMin + i * binWidth;
					const double binRight = binLeft + binWidth;
					const int x1 = valueToX(binLeft);
					const int x2 = valueToX(binRight);
					const int xMid = (x1 + x2) / 2;
					if (this->histogram.bins[i] == 0) {
						envelope.append(QPoint(xMid, bar.bottom()));
						continue;
					}
					const int spanW = qMax(1, x2 - x1);
					const double h = std::sqrt(static_cast<double>(this->histogram.bins[i])) / sqrtMax * barH() * 0.8;
					const int barY = bar.bottom() - static_cast<int>(h);
					p.drawRect(x1, barY, spanW, static_cast<int>(h));
					envelope.append(QPoint(xMid, barY));
				}
				// Dark envelope polyline for visibility on bright LUTs
				if (!envelope.isEmpty()) {
					p.save();
					p.setRenderHint(QPainter::Antialiasing, false);
					p.setBrush(Qt::NoBrush);
					QPen pen(QColor(0, 0, 0, 140), 1);
					pen.setCosmetic(true);
					p.setPen(pen);
					p.drawPolyline(envelope.constData(), envelope.size());
					p.restore();
				}
			}
			p.restore();
		}
	}

	// Draw bar border
	p.setPen(QPen(palette().mid().color(), 1));
	p.setBrush(Qt::NoBrush);
	p.drawRect(bar);

	// Draw handles
	auto drawHandle = [&](int cx, bool active) {
		int hy = (height() - handleH()) / 2;
		QRect hr(cx - HANDLE_W / 2, hy, HANDLE_W, handleH());
		p.setPen(QPen(active ? palette().highlight().color() : palette().dark().color(), 1));
		p.setBrush(palette().button());
		p.drawRoundedRect(hr, 2, 2);
	};
	drawHandle(xLow, this->dragging == Low);
	drawHandle(xHigh, this->dragging == High);

	// Gray overlay when disabled (auto modes)
	if (!isEnabled()) {
		p.fillRect(rect(), QColor(0, 0, 0, 120));
	}

	// Draw value tooltip above the active handle during drag
	if (this->dragging == Low || this->dragging == High || this->dragging == Both) {
		auto drawTooltip = [&](int cx, double value) {
			QString text = formatValue(value);
			QFont f = font();
			QFontMetrics fm(f);
			int tw = fm.horizontalAdvance(text) + 6;
			int th = fm.height() + 4;
			int tx = cx - tw / 2;
			int ty = bar.top() - th - 4;

			// Keep within widget bounds
			tx = qBound(0, tx, width() - tw);
			ty = qMax(0, ty);

			QRect tooltipRect(tx, ty, tw, th);
			p.setPen(Qt::NoPen);
			p.setBrush(QColor(50, 50, 50, 210));
			p.drawRoundedRect(tooltipRect, 3, 3);
			p.setFont(f);
			p.setPen(Qt::white);
			p.drawText(tooltipRect, Qt::AlignCenter, text);
		};

		if (this->dragging == Low) {
			drawTooltip(xLow, this->lowValue);
		} else if (this->dragging == High) {
			drawTooltip(xHigh, this->highValue);
		} else if (this->dragging == Both) {
			drawTooltip(xLow, this->lowValue);
			drawTooltip(xHigh, this->highValue);
		}
	}
}

void RangeSlider::mousePressEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton) return;

	// Close any open editor on click elsewhere
	if (this->activeEditor) {
		commitEditor(false);
	}

	int mx = event->pos().x();
	int xLow = valueToX(this->lowValue);
	int xHigh = valueToX(this->highValue);

	int distLow = std::abs(mx - xLow);
	int distHigh = std::abs(mx - xHigh);

	const bool overlap = (xLow == xHigh);
	const bool nearHandle = (distLow < HANDLE_W + 4);

	// When handles overlap, split into left half (Low) / right half (High)
	if (overlap && nearHandle) {
		this->dragging = (mx < xLow) ? Low : High;
	} else if (distLow <= distHigh && distLow < HANDLE_W + 4) {
		this->dragging = Low;
	} else if (distHigh < HANDLE_W + 4) {
		this->dragging = High;
	} else if (mx > xLow && mx < xHigh) {
		this->dragging = Both;
		this->dragOffset = xToValue(mx) - this->lowValue;
	} else {
		this->dragging = None;
	}
	update();
}

void RangeSlider::mouseMoveEvent(QMouseEvent* event)
{
	if (this->dragging == None) {
		// Update cursor based on hover position
		int mx = event->pos().x();
		int xLow = valueToX(this->lowValue);
		int xHigh = valueToX(this->highValue);
		bool nearHandle = (std::abs(mx - xLow) < HANDLE_W + 4) || (std::abs(mx - xHigh) < HANDLE_W + 4);
		bool betweenHandles = (mx > xLow + HANDLE_W / 2) && (mx < xHigh - HANDLE_W / 2);
		if (nearHandle) {
			setCursor(Qt::SizeHorCursor);
		} else if (betweenHandles) {
			setCursor(Qt::SizeAllCursor);
		} else {
			setCursor(Qt::ArrowCursor);
		}
		return;
	}

	double val = xToValue(event->pos().x());
	if (this->dragging == Low) {
		val = qBound(this->rangeMin, val, this->highValue);
		if (val != this->lowValue) {
			this->lowValue = val;
			update();
			emit valuesChanged(this->lowValue, this->highValue);
		}
	} else if (this->dragging == High) {
		val = qBound(this->lowValue, val, this->rangeMax);
		if (val != this->highValue) {
			this->highValue = val;
			update();
			emit valuesChanged(this->lowValue, this->highValue);
		}
	} else if (this->dragging == Both) {
		double span = this->highValue - this->lowValue;
		double newLow = val - this->dragOffset;
		// Clamp so both handles stay within range
		if (newLow < this->rangeMin) newLow = this->rangeMin;
		if (newLow + span > this->rangeMax) newLow = this->rangeMax - span;
		double newHigh = newLow + span;
		if (newLow != this->lowValue || newHigh != this->highValue) {
			this->lowValue = newLow;
			this->highValue = newHigh;
			update();
			emit valuesChanged(this->lowValue, this->highValue);
		}
	}
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton && this->dragging != None) {
		this->dragging = None;
		setCursor(Qt::ArrowCursor);
		update();
	}
}

void RangeSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
	if (event->button() != Qt::LeftButton) return;

	int mx = event->pos().x();
	int xLow = valueToX(this->lowValue);
	int xHigh = valueToX(this->highValue);

	DragTarget target = None;
	const bool overlap = (xLow == xHigh);
	const bool nearHandle = (std::abs(mx - xLow) < HANDLE_W + 4);
	if (overlap && nearHandle) {
		target = (mx < xLow) ? Low : High;
	} else if (std::abs(mx - xLow) < HANDLE_W + 4) {
		target = Low;
	} else if (std::abs(mx - xHigh) < HANDLE_W + 4) {
		target = High;
	}

	if (target == None) return;

	// Close any previous editor
	if (this->activeEditor) {
		commitEditor(false);
	}

	this->editTarget = target;
	double currentValue = (target == Low) ? this->lowValue : this->highValue;
	int cx = (target == Low) ? xLow : xHigh;

	this->activeEditor = new QLineEdit(this);
	this->activeEditor->setAlignment(Qt::AlignCenter);
	this->activeEditor->setText(formatValue(currentValue));
	this->activeEditor->selectAll();

	if (this->integerMode) {
		this->activeEditor->setValidator(new QIntValidator(this->activeEditor));
	} else {
		this->activeEditor->setValidator(new QDoubleValidator(this->activeEditor));
	}

	// Position above the handle
	QFont f = font();
	QFontMetrics fm(f);
	int edW = qMax(60, fm.horizontalAdvance(this->activeEditor->text()) + 20);
	int edH = fm.height() + 6;
	int edX = cx - edW / 2;
	int edY = barRect().top() - edH - 4;
	edX = qBound(0, edX, width() - edW);
	edY = qMax(0, edY);

	this->activeEditor->setFont(f);
	this->activeEditor->setGeometry(edX, edY, edW, edH);
	this->activeEditor->show();
	this->activeEditor->setFocus();

	connect(this->activeEditor, &QLineEdit::returnPressed, this, [this]() {
		commitEditor(true);
	});
	// Also commit on focus loss (click elsewhere)
	connect(this->activeEditor, &QLineEdit::editingFinished, this, [this]() {
		commitEditor(true);
	});
}

void RangeSlider::commitEditor(bool accept)
{
	if (!this->activeEditor) return;

	if (accept) {
		bool ok = false;
		double val = this->activeEditor->text().toDouble(&ok);
		if (ok) {
			// Expand slider range if value is outside current bounds
			if (val < this->rangeMin) this->rangeMin = val;
			if (val > this->rangeMax) this->rangeMax = val;

			if (this->editTarget == Low) {
				val = qBound(this->rangeMin, val, this->highValue);
				this->lowValue = val;
			} else if (this->editTarget == High) {
				val = qBound(this->lowValue, val, this->rangeMax);
				this->highValue = val;
			}
			emit valuesChanged(this->lowValue, this->highValue);
		}
	}

	this->activeEditor->deleteLater();
	this->activeEditor = nullptr;
	this->editTarget = None;
	update();
}


bool RangeSlider::isNearHandle(int x) const
{
	return (std::abs(x - valueToX(this->lowValue)) < HANDLE_W + 4)
	    || (std::abs(x - valueToX(this->highValue)) < HANDLE_W + 4);
}

QSize RangeSlider::sizeHint() const
{
	return QSize(200, handleH() + 4);
}

QSize RangeSlider::minimumSizeHint() const
{
	return QSize(80, handleH() + 4);
}
