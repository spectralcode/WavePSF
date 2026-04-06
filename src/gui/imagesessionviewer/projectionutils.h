#ifndef PROJECTIONUTILS_H
#define PROJECTIONUTILS_H

#include <QByteArray>
#include <QVector>
#include <cmath>
#include <cstring>
#include <type_traits>
#include "data/imagedata.h"

namespace ProjectionUtils {

// Minimum number of output pixels before enabling OpenMP parallelization 
static constexpr int OMP_PIXEL_THRESHOLD = 65535; //1048576; // 256x256=65536 seems to be good. ImageRenderWorker however uses 1024x1024. todo: maybe use same values in both places. but there is no right/wrong because it depends on data size as well as used hardware

// MIP across z (all frames) → one XY image.
// For each pixel (x,y): result = max over all frames.
// Returns QByteArray of width*height samples in native data type.
template <typename T>
static QByteArray computeMIPXYTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int count = width * height;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}
	if (!framePtrs[0]) return QByteArray();

	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	// Initialize from first frame
	std::memcpy(dst, framePtrs[0], count * sizeof(T));

	// Max with remaining frames — sequential over frames, parallel over pixels
	#pragma omp parallel if(count >= OMP_PIXEL_THRESHOLD)
	{
		for (int f = 1; f < frames; ++f) {
			if (!framePtrs[f]) continue;
			const T* src = framePtrs[f];
			#pragma omp for schedule(static)
			for (int i = 0; i < count; ++i) {
				if (src[i] > dst[i]) dst[i] = src[i];
			}
		}
	}
	return result;
}

// MIP across y (all rows) → one XZ image.
// For each (x, z): result = max over all y rows.
// Returns QByteArray of width*frames samples in native data type.
template <typename T>
static QByteArray computeMIPXZTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int total = width * frames;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}

	QByteArray result(total * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	// Each frame z writes to independent output row — parallel over z
	#pragma omp parallel for if(total >= OMP_PIXEL_THRESHOLD) schedule(static)
	for (int z = 0; z < frames; ++z) {
		const T* frame = framePtrs[z];
		if (!frame) continue;
		T* row = dst + z * width;
		// Initialize from first row of frame
		std::memcpy(row, frame, width * sizeof(T));
		// Max with remaining rows — sequential, cache-friendly row access
		for (int y = 1; y < height; ++y) {
			const T* src = frame + y * width;
			for (int x = 0; x < width; ++x) {
				if (src[x] > row[x]) row[x] = src[x];
			}
		}
	}
	return result;
}

// Min across z (all frames) → one XY image.
template <typename T>
static QByteArray computeMinXYTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int count = width * height;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}
	if (!framePtrs[0]) return QByteArray();

	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	std::memcpy(dst, framePtrs[0], count * sizeof(T));

	#pragma omp parallel if(count >= OMP_PIXEL_THRESHOLD)
	{
		for (int f = 1; f < frames; ++f) {
			if (!framePtrs[f]) continue;
			const T* src = framePtrs[f];
			#pragma omp for schedule(static)
			for (int i = 0; i < count; ++i) {
				if (src[i] < dst[i]) dst[i] = src[i];
			}
		}
	}
	return result;
}

// Min across y (all rows) → one XZ image.
template <typename T>
static QByteArray computeMinXZTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int total = width * frames;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}

	QByteArray result(total * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	#pragma omp parallel for if(total >= OMP_PIXEL_THRESHOLD) schedule(static)
	for (int z = 0; z < frames; ++z) {
		const T* frame = framePtrs[z];
		if (!frame) continue;
		T* row = dst + z * width;
		std::memcpy(row, frame, width * sizeof(T));
		for (int y = 1; y < height; ++y) {
			const T* src = frame + y * width;
			for (int x = 0; x < width; ++x) {
				if (src[x] < row[x]) row[x] = src[x];
			}
		}
	}
	return result;
}

// Average across z (all frames) → one XY image.
template <typename T>
static QByteArray computeAvgXYTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int count = width * height;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}
	if (!framePtrs[0]) return QByteArray();

	int validCount = 0;
	for (int f = 0; f < frames; ++f) {
		if (framePtrs[f]) ++validCount;
	}
	if (validCount == 0) return QByteArray();
	const double divisor = static_cast<double>(validCount);

	// Accumulate — sequential over frames, parallel over pixels
	QVector<double> acc(count, 0.0);
	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	#pragma omp parallel if(count >= OMP_PIXEL_THRESHOLD)
	{
		for (int f = 0; f < frames; ++f) {
			if (!framePtrs[f]) continue;
			const T* src = framePtrs[f];
			#pragma omp for schedule(static)
			for (int i = 0; i < count; ++i) {
				acc[i] += static_cast<double>(src[i]);
			}
		}
		#pragma omp for schedule(static)
		for (int i = 0; i < count; ++i) {
			double avg = acc[i] / divisor;
			if (std::is_integral<T>::value) {
				dst[i] = static_cast<T>(std::round(avg));
			} else {
				dst[i] = static_cast<T>(avg);
			}
		}
	}
	return result;
}

// Average across y (all rows) → one XZ image.
template <typename T>
static QByteArray computeAvgXZTyped(const ImageData* data)
{
	const int width = data->getWidth();
	const int height = data->getHeight();
	const int frames = data->getFrames();
	const int total = width * frames;

	QVector<const T*> framePtrs(frames, nullptr);
	for (int f = 0; f < frames; ++f) {
		framePtrs[f] = static_cast<const T*>(data->getData(f));
	}
	const double divisor = static_cast<double>(height);

	QByteArray result(total * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	// Each frame z writes to independent output row — parallel over z
	#pragma omp parallel for if(total >= OMP_PIXEL_THRESHOLD) schedule(static)
	for (int z = 0; z < frames; ++z) {
		const T* frame = framePtrs[z];
		if (!frame) continue;
		T* outRow = dst + z * width;
		// Thread-local accumulator — sequential row access within frame
		QVector<double> acc(width, 0.0);
		for (int y = 0; y < height; ++y) {
			const T* src = frame + y * width;
			for (int x = 0; x < width; ++x) {
				acc[x] += static_cast<double>(src[x]);
			}
		}
		for (int x = 0; x < width; ++x) {
			double avg = acc[x] / divisor;
			if (std::is_integral<T>::value) {
				outRow[x] = static_cast<T>(std::round(avg));
			} else {
				outRow[x] = static_cast<T>(avg);
			}
		}
	}
	return result;
}

// Type-dispatched MIP across z → XY image
inline QByteArray computeMIPXY(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeMIPXYTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeMIPXYTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeMIPXYTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeMIPXYTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeMIPXYTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeMIPXYTyped<float>(data);
		case DOUBLE_64BIT:				return computeMIPXYTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeMIPXYTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeMIPXYTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

// Type-dispatched MIP across y → XZ image
inline QByteArray computeMIPXZ(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeMIPXZTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeMIPXZTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeMIPXZTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeMIPXZTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeMIPXZTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeMIPXZTyped<float>(data);
		case DOUBLE_64BIT:				return computeMIPXZTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeMIPXZTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeMIPXZTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

// Type-dispatched Min across z → XY image
inline QByteArray computeMinXY(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeMinXYTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeMinXYTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeMinXYTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeMinXYTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeMinXYTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeMinXYTyped<float>(data);
		case DOUBLE_64BIT:				return computeMinXYTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeMinXYTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeMinXYTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

// Type-dispatched Min across y → XZ image
inline QByteArray computeMinXZ(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeMinXZTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeMinXZTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeMinXZTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeMinXZTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeMinXZTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeMinXZTyped<float>(data);
		case DOUBLE_64BIT:				return computeMinXZTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeMinXZTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeMinXZTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

// Type-dispatched Average across z → XY image
inline QByteArray computeAvgXY(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeAvgXYTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeAvgXYTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeAvgXYTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeAvgXYTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeAvgXYTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeAvgXYTyped<float>(data);
		case DOUBLE_64BIT:				return computeAvgXYTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeAvgXYTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeAvgXYTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

// Type-dispatched Average across y → XZ image
inline QByteArray computeAvgXZ(const ImageData* data)
{
	if (!data || data->getFrames() <= 0) return QByteArray();
	switch (data->getDataType()) {
		case UNSIGNED_CHAR_8BIT:		return computeAvgXZTyped<unsigned char>(data);
		case UNSIGNED_SHORT_16BIT:		return computeAvgXZTyped<unsigned short>(data);
		case SIGNED_SHORT_16BIT:		return computeAvgXZTyped<short>(data);
		case SIGNED_INT_32BIT:			return computeAvgXZTyped<int>(data);
		case UNSIGNED_INT_32BIT:		return computeAvgXZTyped<unsigned int>(data);
		case FLOAT_32BIT:				return computeAvgXZTyped<float>(data);
		case DOUBLE_64BIT:				return computeAvgXZTyped<double>(data);
		case SIGNED_LONG_INT_64BIT:		return computeAvgXZTyped<long long>(data);
		case UNSIGNED_LONG_INT_64BIT:	return computeAvgXZTyped<unsigned long long>(data);
		default:						return QByteArray();
	}
}

} // namespace ProjectionUtils

#endif // PROJECTIONUTILS_H
