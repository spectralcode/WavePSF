#include "imagemetriccalculator.h"
#include <cmath>
#include <limits>


double ImageMetricCalculator::calculate(const af::array& image, ImageMetric metric)
{
	try {
		switch (metric) {
		case TOTAL_INTENSITY:
			return af::sum<float>(image);

		case VARIANCE:
			return af::var<float>(image, AF_VARIANCE_POPULATION);

		case STDEV:
			return af::stdev<float>(image, AF_VARIANCE_POPULATION);

		case SQUARED_SUM:
			return af::sum<float>(af::pow(image, 2));

		default:
			return af::sum<float>(image);
		}
	} catch (af::exception&) {
		return (std::numeric_limits<double>::max)();
	}
}

double ImageMetricCalculator::calculate(const af::array& image, const af::array& reference,
										ReferenceMetric metric)
{
	try {
		switch (metric) {
		case NORMALIZED_SUM_SQUARED_DIFFERENCES: {
			af::array img = image.as(f32);
			af::array ref = reference.as(f32);
			float sumSqDiff = af::sum<float>(af::pow(img - ref, 2));
			float normFactor = std::sqrt(
				af::sum<float>(af::pow(img, 2)) * af::sum<float>(af::pow(ref, 2)));
			if (normFactor == 0.0f) return (std::numeric_limits<double>::max)();
			return static_cast<double>(sumSqDiff / normFactor);
		}

		case NORMALIZED_CROSS_CORRELATION: {
			af::array img = image.as(f32);
			af::array ref = reference.as(f32);
			float meanImg = af::mean<float>(img);
			float meanRef = af::mean<float>(ref);
			img = img - meanImg;
			ref = ref - meanRef;
			float normFactor = std::sqrt(
				af::sum<float>(af::pow(img, 2)) * af::sum<float>(af::pow(ref, 2)));
			if (normFactor == 0.0f) return (std::numeric_limits<double>::max)();
			float dotProduct = af::sum<float>(img * ref);
			return static_cast<double>(dotProduct / normFactor);
		}

		case SUM_OF_HAMMING_DISTANCE_BINARY: {
			af::array bin1 = otsuThreshold(image);
			af::array bin2 = otsuThreshold(reference);
			af::array diff = af::abs(bin1 - bin2);
			return static_cast<double>(af::sum<float>(diff));
		}

		case SUM_DIFFERENCES: {
			return static_cast<double>(
				af::sum<float>(image) - af::sum<float>(reference));
		}

		default:
			return static_cast<double>(
				af::sum<float>(image) - af::sum<float>(reference));
		}
	} catch (af::exception&) {
		return (std::numeric_limits<double>::max)();
	}
}

QStringList ImageMetricCalculator::imageMetricNames()
{
	return QStringList()
		<< QStringLiteral("Total Intensity")
		<< QStringLiteral("Variance")
		<< QStringLiteral("Standard Deviation")
		<< QStringLiteral("Squared Sum");
}

QStringList ImageMetricCalculator::referenceMetricNames()
{
	return QStringList()
		<< QStringLiteral("Normalized Sum of Squared Differences")
		<< QStringLiteral("Normalized Cross Correlation")
		<< QStringLiteral("Binary Hamming Distance")
		<< QStringLiteral("Sum of Differences");
}

// Otsu thresholding ported from reference implementation
// Source: https://arrayfire.com/blog/image-editing-using-arrayfire-part-3-2/
af::array ImageMetricCalculator::otsuThreshold(const af::array& image)
{
	af::array gray;
	if (image.dims(2) > 1) {
		gray = af::colorSpace(image, AF_GRAY, AF_RGB);
	} else {
		gray = image;
	}

	af::array hist = af::histogram(gray, 256, 0.0f, 65535.0f);
	af::array wts = af::seq(256);

	af::array wtB = af::accum(hist);
	af::array wtF = gray.elements() - wtB;
	af::array sumB = af::accum(wts * hist);
	af::array meanB = sumB / wtB;
	af::array meanF = (sumB(255) - sumB) / wtF;
	af::array mDiff = meanB - meanF;

	af::array interClsVar = wtB * wtF * mDiff * mDiff;
	float maxVal = af::max<float>(interClsVar);
	af::array threshIdx = af::where(interClsVar == maxVal);
	float threshold = threshIdx.elements() > 0 ? threshIdx.scalar<float>() : 0.0f;

	return (gray > (threshold / 255.0f)).as(f32);
}
