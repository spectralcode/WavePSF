#ifndef PATCHLAYOUT_H
#define PATCHLAYOUT_H

#include <QRect>

struct PatchLayout {
	int imageWidth  = 0;
	int imageHeight = 0;
	int cols        = 0;
	int rows        = 0;

	bool isValid() const {
		return imageWidth > 0 && imageHeight > 0 && cols > 0 && rows > 0;
	}

	int patchCount() const { return cols * rows; }

	QRect patchBounds(int col, int row) const {
		const int baseWidth  = imageWidth  / cols;
		const int baseHeight = imageHeight / rows;
		const int x = col * baseWidth;
		const int y = row * baseHeight;
		const int w = (col == cols - 1) ? (imageWidth  - x) : baseWidth;
		const int h = (row == rows - 1) ? (imageHeight - y) : baseHeight;
		return QRect(x, y, w, h);
	}

	QRect patchBounds(int patchId) const {
		return patchBounds(patchId % cols, patchId / cols);
	}
};

#endif // PATCHLAYOUT_H
