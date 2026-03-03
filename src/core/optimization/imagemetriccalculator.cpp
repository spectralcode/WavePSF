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

		case LAPLACIAN_VARIANCE: {
			float kernel[] = {0,1,0, 1,-4,1, 0,1,0};
			af::array lap = af::convolve2(image.as(f32), af::array(3, 3, kernel));
			return -af::var<double>(lap, AF_VARIANCE_POPULATION);
		}

		case TENENGRAD: {
			float kx[] = {-1,0,1, -2,0,2, -1,0,1};
			float ky[] = {-1,-2,-1, 0,0,0, 1,2,1};
			af::array gx = af::convolve2(image.as(f32), af::array(3, 3, kx));
			af::array gy = af::convolve2(image.as(f32), af::array(3, 3, ky));
			return -af::sum<double>(gx * gx + gy * gy);
		}

		case BRENNER_GRADIENT: {
			af::array img = image.as(f32);
			int cols = img.dims(1);
			af::array shifted = img(af::span, af::seq(2, cols - 1));
			af::array original = img(af::span, af::seq(0, cols - 3));
			af::array diff = shifted - original;
			return -af::sum<double>(diff * diff);
		}

		case SHANNON_ENTROPY: {
			af::array img = image.as(f32);
			af::array hist = af::histogram(img, 256);
			af::array p = hist.as(f32) / af::sum<float>(hist);
			af::array mask = p > 1e-10f;
			af::array logp = af::log(p + 1e-10f);
			return af::sum<double>(-p * logp * mask);
		}

		case TOTAL_VARIATION: {
			af::array img = image.as(f32);
			af::array dx = af::diff1(img, 1);
			af::array dy = af::diff1(img, 0);
			return af::sum<double>(af::abs(dx)) + af::sum<double>(af::abs(dy));
		}

		case KURTOSIS: {
			af::array img = image.as(f32);
			double mean = af::mean<double>(img);
			af::array centered = img - static_cast<float>(mean);
			double var = af::mean<double>(centered * centered);
			if (var < 1e-12) return (std::numeric_limits<double>::max)();
			double m4 = af::mean<double>(af::pow(centered, 4));
			return -(m4 / (var * var));
		}

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
		<< QStringLiteral("Squared Sum")
		<< QStringLiteral("Laplacian Variance")
		<< QStringLiteral("Tenengrad")
		<< QStringLiteral("Brenner Gradient")
		<< QStringLiteral("Shannon Entropy")
		<< QStringLiteral("Total Variation")
		<< QStringLiteral("Kurtosis");
}

QStringList ImageMetricCalculator::imageMetricDescriptions()
{
	return QStringList()
		<< QStringLiteral("Total pixel intensity. sum(I)")
		<< QStringLiteral("Population variance of pixel values. var(I).")
		<< QStringLiteral("Population standard deviation of pixel values. std(I).")
		<< QStringLiteral("Sum of squared pixel values. sum(I^2)")
		<< QStringLiteral("Negative variance of Laplacian-filtered image. -var(L * I)")
		<< QStringLiteral("Negative sum of squared Sobel gradients. -sum(Gx^2 + Gy^2)")
		<< QStringLiteral("Negative sum of squared horizontal pixel differences. -sum((I(x+2) - I(x))^2)")
		<< QStringLiteral("Shannon entropy over 256-bin histogram. -sum(p * log(p))")
		<< QStringLiteral("L1 norm of image gradients. sum(|dI/dx|) + sum(|dI/dy|)")
		<< QStringLiteral("Negative kurtosis of pixel distribution. -m4 / var^2");
}

QStringList ImageMetricCalculator::referenceMetricDescriptions()
{
	return QStringList()
		<< QStringLiteral("Normalized squared difference. sum((I-R)^2) / sqrt(sum(I^2) * sum(R^2))")
		<< QStringLiteral("Normalized cross-correlation [-1, 1]. sum((I-mean(I))*(R-mean(R))) / (norm(I-mean(I)) * norm(R-mean(R))). Recommended multiplier: -100")
		<< QStringLiteral("Hamming distance of Otsu-binarized images. sum(|otsu(I) - otsu(R)|)")
		<< QStringLiteral("Difference of total pixel intensities. sum(I) - sum(R)");
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
