#include "imagerenderworker.h"

#include <QVector>
#include <QElapsedTimer>
#include <algorithm>
#include <limits>
#include <cmath>
#include <omp.h>
#include <cstring>

namespace {
	const double DEFAULT_MIN_VALUE = 0.0;
	const double DEFAULT_MAX_VALUE = 255.0;
	const double P_LOW = 0.02; //for percentile based auto-range
	const double P_HIGH = 0.98; //for percentile based auto-range

	#define ENABLE_WORKER_TIMING 0
}

ImageRenderWorker::ImageRenderWorker(QObject* parent)
	: QObject(parent)
{
}

void ImageRenderWorker::setLatestRequestIdSource(const QAtomicInteger<quint64>* src)
{
	this->latestIdSource = src;
}

template <typename Fn>
bool ImageRenderWorker::dispatchByType(EnviDataType dt, const void* ptr, int count, const Fn& fn) const
{
	switch (dt) {
		case UNSIGNED_CHAR_8BIT:		fn(static_cast<const unsigned char*>(ptr), count); return true;
		case UNSIGNED_SHORT_16BIT:		fn(static_cast<const unsigned short*>(ptr), count); return true;
		case SIGNED_SHORT_16BIT:		fn(static_cast<const short*>(ptr), count); return true;
		case UNSIGNED_INT_32BIT:		fn(static_cast<const unsigned int*>(ptr), count); return true;
		case SIGNED_INT_32BIT:			fn(static_cast<const int*>(ptr), count); return true;
		case FLOAT_32BIT:				fn(static_cast<const float*>(ptr), count); return true;
		case DOUBLE_64BIT:				fn(static_cast<const double*>(ptr), count); return true;
		case SIGNED_LONG_INT_64BIT:		fn(static_cast<const long long*>(ptr), count); return true;
		case UNSIGNED_LONG_INT_64BIT:	fn(static_cast<const unsigned long long*>(ptr), count); return true;
		default:						return false;
	}
}

void ImageRenderWorker::renderFrame(const RenderRequest& req)
{
#if ENABLE_WORKER_TIMING
	QElapsedTimer tAll; tAll.start();
	QElapsedTimer tStage;
#endif

	// Validate payload
	if (req.frameBytes.isEmpty() || req.width <= 0 || req.height <= 0) {
		emit this->frameRendered(QImage(), req.requestId);
		return;
	}

	const void* srcPtr = static_cast<const void*>(req.frameBytes.constData());
	const int count = req.width * req.height;
	const quint64 curId = req.requestId;

	double dataMin = DEFAULT_MIN_VALUE;
	double dataMax = DEFAULT_MAX_VALUE;
	double minV = DEFAULT_MIN_VALUE;
	double maxV = DEFAULT_MAX_VALUE;

#if ENABLE_WORKER_TIMING
	tStage.start();
#endif
	// Always compute actual data range (for dataRangeComputed signal)
	const bool okMinMax = this->dispatchByType(req.dataType, srcPtr, count, [&](auto* p, int n) {
		this->minMaxTyped(p, n, dataMin, dataMax, curId);
	});
	if (!okMinMax) {
		emit this->frameRendered(QImage(), req.requestId);
		return;
	}
	if (isCancelled(curId)) {
		emit this->frameRendered(QImage(), req.requestId);
		return;
	}

	emit this->dataRangeComputed(dataMin, dataMax, req.requestId);

	if (req.useAutoRange) {
		if (req.usePercentile) {
			this->dispatchByType(req.dataType, srcPtr, count, [&](auto* p, int n) {
				this->percentileBoundsTyped(p, n, P_LOW, P_HIGH, minV, maxV, curId);
			});
		} else {
			minV = dataMin;
			maxV = dataMax;
		}
	} else {
		minV = req.manualMin;
		maxV = req.manualMax;
	}

#if ENABLE_WORKER_TIMING
	const qint64 msRange = tStage.elapsed();
#endif

#if ENABLE_WORKER_TIMING
	tStage.restart();
#endif
	if (isCancelled(curId)) {
		emit this->frameRendered(QImage(), req.requestId);
		return;
	}

	// Choose image format: indexed (with color table) or grayscale
	const bool useColorTable = !req.colorTable.isEmpty();
	const QImage::Format fmt = useColorTable ? QImage::Format_Indexed8 : QImage::Format_Grayscale8;
	QImage img(req.width, req.height, fmt);
	if (img.isNull()) {
		emit this->frameRendered(QImage(), req.requestId);
		return;
	}
	if (useColorTable) {
		img.setColorTable(req.colorTable);
	}

	// Check if QImage has padding and generate temp buffer if needed
	const bool hasPadding = img.bytesPerLine() != req.width;	// Indexed8/Grayscale8: 1 byte per pixel
	QVector<uchar> temp;
	uchar* dst = hasPadding ? (temp.resize(count), temp.data()) : img.bits();

	// Scale typed source into 8-bit destination (linear or log)
	const bool okScale = this->dispatchByType(req.dataType, srcPtr, count, [&](auto* p, int n) {
		if (req.logScale) {
			this->scaleToU8LogTyped(p, n, minV, maxV, dst, curId);
		} else {
			this->scaleToU8Typed(p, n, minV, maxV, dst, curId);
		}
	});

	if (!okScale) {
		img.fill(0);
	} else if (hasPadding) {
		// Copy each row into padded scanlines
		for (int y = 0; y < req.height; ++y) {
			std::memcpy(img.scanLine(y), dst + y * req.width, req.width);
		}
	}


#if ENABLE_WORKER_TIMING
	const qint64 msScale = tStage.elapsed();
	const qint64 msAll = tAll.elapsed();
	qDebug("RenderWorker: range=%lld ms, scale=%lld ms, total=%lld ms, size=%dx%d",
	       static_cast<long long>(msRange), static_cast<long long>(msScale),
	       static_cast<long long>(msAll), req.width, req.height);
#endif

	emit this->frameRendered(img, req.requestId);
}

// ----- Templates with cooperative cancellation -----

template<typename T>
void ImageRenderWorker::minMaxTyped(const T* data, int count, double& outMin, double& outMax, quint64 curId) const
{
	double mn = std::numeric_limits<double>::infinity();
	double mx = -std::numeric_limits<double>::infinity();

	for (int i = 0; i < count; ++i) {
		if (isCancelledPeriodic(curId, i)) return;
		const double v = static_cast<double>(data[i]);
		if (qIsNaN(v) || !qIsFinite(v)) continue;
		if (v < mn) mn = v;
		if (v > mx) mx = v;
	}

	if (!qIsFinite(mn) || !qIsFinite(mx)) {
		outMin = DEFAULT_MIN_VALUE;
		outMax = DEFAULT_MAX_VALUE;
	} else if (!(mx > mn)) {
		outMin = mn;
		outMax = mn + 1.0;
	} else {
		outMin = mn;
		outMax = mx;
	}
}

template<typename T>
void ImageRenderWorker::percentileBoundsTyped(const T* data, int count, double lowP, double highP, double& outLow, double& outHigh, quint64 curId) const
{
	QVector<double> buf;
	buf.reserve(count);
	for (int i = 0; i < count; ++i) {
		if (isCancelledPeriodic(curId, i)) return;
		const double v = static_cast<double>(data[i]);
		if (qIsNaN(v) || !qIsFinite(v)) continue;
		buf.append(v);
	}

	if (buf.isEmpty()) {
		outLow = DEFAULT_MIN_VALUE;
		outHigh = DEFAULT_MAX_VALUE;
		return;
	}

	lowP = qBound(0.0, lowP, 1.0);
	highP = qBound(0.0, highP, 1.0);
	if (highP < lowP) qSwap(lowP, highP);

	const int n = buf.size();
	const int kLow = qBound(0, static_cast<int>(lowP * (n - 1)), n - 1);
	const int kHigh = qBound(0, static_cast<int>(highP * (n - 1)), n - 1);

	double* begin = buf.data();
	double* end = begin + n;

	if (isCancelled(curId)) return;
	std::nth_element(begin, begin + kLow, end);
	const double lowVal = *(begin + kLow);

	if (isCancelled(curId)) return;
	std::nth_element(begin, begin + kHigh, end);
	const double highVal = *(begin + kHigh);

	if (!(highVal > lowVal)) {
		outLow = lowVal;
		outHigh = lowVal + 1.0;
	} else {
		outLow = lowVal;
		outHigh = highVal;
	}
}

template<typename T>
void ImageRenderWorker::scaleToU8Typed(const T* src, int count, double minV, double maxV, uchar* dst, quint64 curId) const
{
	//if a newer request already exists, bail before doing any work.
	if (this->latestIdSource && this->latestIdSource->loadAcquire() != curId) return;

	const double scale = 255.0 / (maxV - minV);
	const double offs = -minV * scale;

#pragma omp parallel for if(count >= 1048576) schedule(static) //use OpenMP parallelization only if frame is larger than 1024x1024 (=1048576)
	for (int i = 0; i < count; ++i) {
		double sv = static_cast<double>(src[i]) * scale + offs;
		sv = sv < 0.0 ? 0.0 : (sv > 255.0 ? 255.0 : sv);
		dst[i] = static_cast<uchar>(sv + 0.5);
	}
}

template<typename T>
void ImageRenderWorker::scaleToU8LogTyped(const T* src, int count, double minV, double maxV, uchar* dst, quint64 curId) const
{
	if (this->latestIdSource && this->latestIdSource->loadAcquire() != curId) return;

	const double logMin = std::log(1.0 + std::max(0.0, minV));
	const double logMax = std::log(1.0 + std::max(0.0, maxV));
	const double logRange = logMax - logMin;
	const double scale = (logRange > 0.0) ? (255.0 / logRange) : 0.0;

#pragma omp parallel for if(count >= 1048576) schedule(static)
	for (int i = 0; i < count; ++i) {
		double v = std::log(1.0 + std::max(0.0, static_cast<double>(src[i])));
		double sv = (v - logMin) * scale;
		sv = sv < 0.0 ? 0.0 : (sv > 255.0 ? 255.0 : sv);
		dst[i] = static_cast<uchar>(sv + 0.5);
	}
}
