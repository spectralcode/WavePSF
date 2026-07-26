#ifndef ENVILAYOUT_H
#define ENVILAYOUT_H

#include <cstddef>
#include <cstdint>

// Byte layout of an ENVI cube, computed with overflow-checked size_t
// arithmetic so that multi-gigabyte dimensions can be validated (and unit
// tested) without allocating anything. Totals are additionally capped to
// INT64_MAX because QFile positions and read counts are qint64.
struct EnviLayout
{
	size_t bytesPerSample = 0;
	size_t samplesPerFrame = 0;       // width * height
	size_t bytesPerFrame = 0;         // samplesPerFrame * bytesPerSample
	size_t bytesPerLine = 0;          // width * bytesPerSample
	size_t bytesPerSpectralFrame = 0; // width * frames * bytesPerSample (one BIL row block)
	size_t bytesPerPixel = 0;         // frames * bytesPerSample (one BIP pixel)
	size_t totalSamples = 0;          // width * height * frames
	size_t totalBytes = 0;            // totalSamples * bytesPerSample

	// Destination byte offset of (pixel, band) in band-sequential storage.
	size_t bipDestinationOffset(size_t pixel, size_t band) const
	{
		return band * this->bytesPerFrame + pixel * this->bytesPerSample;
	}

	static bool multiplyChecked(size_t a, size_t b, size_t& product)
	{
		if (a != 0 && b > SIZE_MAX / a) {
			return false;
		}
		product = a * b;
		return true;
	}

	static bool calculate(int width, int height, int frames, size_t bytesPerSample, EnviLayout& layout)
	{
		if (width <= 0 || height <= 0 || frames <= 0 || bytesPerSample == 0) {
			return false;
		}
		const size_t w = static_cast<size_t>(width);
		const size_t f = static_cast<size_t>(frames);
		EnviLayout result;
		result.bytesPerSample = bytesPerSample;
		if (!multiplyChecked(w, static_cast<size_t>(height), result.samplesPerFrame)
				|| !multiplyChecked(result.samplesPerFrame, bytesPerSample, result.bytesPerFrame)
				|| !multiplyChecked(w, bytesPerSample, result.bytesPerLine)
				|| !multiplyChecked(result.bytesPerLine, f, result.bytesPerSpectralFrame)
				|| !multiplyChecked(f, bytesPerSample, result.bytesPerPixel)
				|| !multiplyChecked(result.samplesPerFrame, f, result.totalSamples)
				|| !multiplyChecked(result.totalSamples, bytesPerSample, result.totalBytes)) {
			return false;
		}
		if (result.totalBytes > static_cast<size_t>(INT64_MAX)) {
			return false;
		}
		layout = result;
		return true;
	}
};

#endif // ENVILAYOUT_H
