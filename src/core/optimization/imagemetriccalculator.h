#ifndef IMAGEMETRICCALCULATOR_H
#define IMAGEMETRICCALCULATOR_H

#include <arrayfire.h>
#include <QStringList>

class ImageMetricCalculator
{
public:
	// Single-image metrics (minimize these)
	enum ImageMetric {
		TOTAL_INTENSITY = 0,
		VARIANCE = 1,
		STDEV = 2,
		SQUARED_SUM = 3,
		LAPLACIAN_VARIANCE = 4,
		TENENGRAD = 5,
		BRENNER_GRADIENT = 6,
		SHANNON_ENTROPY = 7,
		TOTAL_VARIATION = 8,
		KURTOSIS = 9
	};

	// Reference-comparison metrics (minimize these; NCC is negated)
	enum ReferenceMetric {
		NORMALIZED_SUM_SQUARED_DIFFERENCES = 0,
		NORMALIZED_CROSS_CORRELATION = 1,
		SUM_OF_HAMMING_DISTANCE_BINARY = 2,
		SUM_DIFFERENCES = 3
	};

	// Calculate single-image metric
	static double calculate(const af::array& image, ImageMetric metric);

	// Calculate reference-comparison metric
	static double calculate(const af::array& image, const af::array& reference,
							ReferenceMetric metric);

	// Human-readable names for UI combo boxes
	static QStringList imageMetricNames();
	static QStringList referenceMetricNames();

	// Descriptions for UI
	static QStringList imageMetricDescriptions();
	static QStringList referenceMetricDescriptions();

private:
	static af::array otsuThreshold(const af::array& image);
};

#endif // IMAGEMETRICCALCULATOR_H
