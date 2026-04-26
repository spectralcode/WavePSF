#ifndef IMAGEPATCH_H
#define IMAGEPATCH_H

#include <QRect>
#include <arrayfire.h>

struct BorderExtension {
	bool leftExtended = false;
	bool rightExtended = false;
	bool topExtended = false;
	bool bottomExtended = false;
	int extensionSize = 0;
};

struct ImagePatch {
	af::array data;              // Extended patch data (with borders)
	QRect coreArea;              // Core patch area within the extended data (relative coordinates)
	QRect imagePosition;         // Where core patch maps to in the full image (absolute coordinates)
	BorderExtension borders;     // Which borders were actually extended

	af::array extractCore() const {
		if (data.isempty()) return af::array();
		return data(af::seq(coreArea.x(), coreArea.x() + coreArea.width() - 1),
					af::seq(coreArea.y(), coreArea.y() + coreArea.height() - 1));
	}

	bool isValid() const {
		return !data.isempty() && coreArea.isValid() && imagePosition.isValid();
	}
};

#endif // IMAGEPATCH_H
