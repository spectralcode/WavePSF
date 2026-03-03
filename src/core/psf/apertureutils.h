#ifndef APERTUREUTILS_H
#define APERTUREUTILS_H

#include <arrayfire.h>

namespace ApertureUtils {
	enum Geometry { Circle = 0, Rectangle = 1, Triangle = 2 };

	// Returns a float mask array (1.0 inside aperture, 0.0 outside)
	// on a normalized [-1,1] grid of the given size.
	// radius scales the aperture (1.0 = full grid, 0.5 = half).
	af::array buildMask(int gridSize, Geometry geometry, double radius = 1.0);

	// CPU-side check for a single normalized coordinate
	bool isInsideAperture(double x, double y, Geometry geometry, double radius = 1.0);
}

#endif // APERTUREUTILS_H
