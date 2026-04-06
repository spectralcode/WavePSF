#ifndef PROJECTIONUTILS_H
#define PROJECTIONUTILS_H

#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <type_traits>
#include "data/imagedata.h"

namespace ProjectionUtils {

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

	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	// Initialize from first frame
	const T* first = static_cast<const T*>(data->getData(0));
	if (!first) return QByteArray();
	std::memcpy(dst, first, count * sizeof(T));

	// Max with remaining frames
	for (int f = 1; f < frames; ++f) {
		const T* src = static_cast<const T*>(data->getData(f));
		if (!src) continue;
		for (int i = 0; i < count; ++i) {
			if (src[i] > dst[i]) dst[i] = src[i];
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

	QByteArray result(width * frames * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	for (int z = 0; z < frames; ++z) {
		const T* frame = static_cast<const T*>(data->getData(z));
		if (!frame) continue;
		T* row = dst + z * width;
		// Initialize from first row
		std::memcpy(row, frame, width * sizeof(T));
		// Max with remaining rows
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

	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	const T* first = static_cast<const T*>(data->getData(0));
	if (!first) return QByteArray();
	std::memcpy(dst, first, count * sizeof(T));

	for (int f = 1; f < frames; ++f) {
		const T* src = static_cast<const T*>(data->getData(f));
		if (!src) continue;
		for (int i = 0; i < count; ++i) {
			if (src[i] < dst[i]) dst[i] = src[i];
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

	QByteArray result(width * frames * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());

	for (int z = 0; z < frames; ++z) {
		const T* frame = static_cast<const T*>(data->getData(z));
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

	QVector<double> acc(count, 0.0);
	for (int f = 0; f < frames; ++f) {
		const T* src = static_cast<const T*>(data->getData(f));
		if (!src) continue;
		for (int i = 0; i < count; ++i) {
			acc[i] += static_cast<double>(src[i]);
		}
	}

	QByteArray result(count * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());
	const double divisor = static_cast<double>(frames);
	for (int i = 0; i < count; ++i) {
		double avg = acc[i] / divisor;
		if (std::is_integral<T>::value) {
			dst[i] = static_cast<T>(std::round(avg));
		} else {
			dst[i] = static_cast<T>(avg);
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

	QVector<double> acc(width * frames, 0.0);
	for (int z = 0; z < frames; ++z) {
		const T* frame = static_cast<const T*>(data->getData(z));
		if (!frame) continue;
		double* accRow = acc.data() + z * width;
		for (int y = 0; y < height; ++y) {
			const T* src = frame + y * width;
			for (int x = 0; x < width; ++x) {
				accRow[x] += static_cast<double>(src[x]);
			}
		}
	}

	QByteArray result(width * frames * static_cast<int>(sizeof(T)), '\0');
	T* dst = reinterpret_cast<T*>(result.data());
	const double divisor = static_cast<double>(height);
	const int total = width * frames;
	for (int i = 0; i < total; ++i) {
		double avg = acc[i] / divisor;
		if (std::is_integral<T>::value) {
			dst[i] = static_cast<T>(std::round(avg));
		} else {
			dst[i] = static_cast<T>(avg);
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
