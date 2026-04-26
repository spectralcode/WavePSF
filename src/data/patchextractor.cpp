#include "patchextractor.h"
#include "utils/logging.h"

ImagePatch PatchExtractor::extractExtendedPatch(
	const af::array& frame,
	const PatchLayout& layout,
	int patchX,
	int patchY,
	int borderExtension)
{
	ImagePatch result;

	if (frame.isempty() || !layout.isValid()) {
		return result;
	}

	if (patchX < 0 || patchX >= layout.cols || patchY < 0 || patchY >= layout.rows) {
		LOG_WARNING() << "PatchExtractor: invalid patch coordinates:" << patchX << patchY;
		return result;
	}

	const QRect coreBounds = layout.patchBounds(patchX, patchY);
	const long long imageWidth = static_cast<long long>(frame.dims(0));
	const long long imageHeight = static_cast<long long>(frame.dims(1));

	long long xPos = coreBounds.x();
	long long yPos = coreBounds.y();
	long long width = coreBounds.width();
	long long height = coreBounds.height();

	long long newXPos = xPos;
	long long newYPos = yPos;
	long long newWidth = width;
	long long newHeight = height;

	result.borders.extensionSize = borderExtension;

	if (xPos - borderExtension >= 0) {
		newXPos = xPos - borderExtension;
		newWidth += borderExtension;
		result.borders.leftExtended = true;
	}

	if (xPos + width + borderExtension <= imageWidth) {
		newWidth += borderExtension;
		result.borders.rightExtended = true;
	}

	if (yPos - borderExtension >= 0) {
		newYPos = yPos - borderExtension;
		newHeight += borderExtension;
		result.borders.topExtended = true;
	}

	if (yPos + height + borderExtension <= imageHeight) {
		newHeight += borderExtension;
		result.borders.bottomExtended = true;
	}

	try {
		result.data = frame(
			af::seq(newXPos, newXPos + newWidth - 1),
			af::seq(newYPos, newYPos + newHeight - 1));

		const long long coreStartX = result.borders.leftExtended ? borderExtension : 0;
		const long long coreStartY = result.borders.topExtended ? borderExtension : 0;
		result.coreArea = QRect(coreStartX, coreStartY, width, height);
		result.imagePosition = QRect(xPos, yPos, width, height);

		LOG_DEBUG() << "PatchExtractor: extracted patch (" << patchX << "," << patchY << ")"
					<< "extended size:" << newWidth << "x" << newHeight
					<< "borders: L=" << result.borders.leftExtended
					<< "R=" << result.borders.rightExtended
					<< "T=" << result.borders.topExtended
					<< "B=" << result.borders.bottomExtended;
	} catch (const af::exception& e) {
		LOG_WARNING() << "PatchExtractor: ArrayFire exception during extended patch extraction:" << e.what();
		result = ImagePatch();
	}

	return result;
}
