#include "apertureutils.h"
#include <cmath>

af::array ApertureUtils::buildMask(int gridSize, Geometry geometry, double radius)
{
	af::array x = 2.0f * af::range(af::dim4(gridSize, gridSize), 1).as(f32) / (gridSize - 1) - 1.0f;
	af::array y = 2.0f * af::range(af::dim4(gridSize, gridSize), 0).as(f32) / (gridSize - 1) - 1.0f;
	float r = static_cast<float>(radius);

	switch (geometry) {
	case Rectangle:
		return (af::abs(x) <= r && af::abs(y) <= r).as(f32);

	case Triangle: {
		// Equilateral triangle inscribed in circle of given radius
		float sqrt3 = 1.7320508f;
		af::array bottomEdge = (y >= -0.5f * r);
		af::array rightEdge = (sqrt3 * x + y <= r);
		af::array leftEdge = (-sqrt3 * x + y <= r);
		return (bottomEdge && rightEdge && leftEdge).as(f32);
	}

	case Circle:
	default: {
		af::array dist = af::sqrt(x * x + y * y);
		return (dist <= r).as(f32);
	}
	}
}

bool ApertureUtils::isInsideAperture(double x, double y, Geometry geometry, double radius)
{
	switch (geometry) {
	case Rectangle:
		return std::abs(x) <= radius && std::abs(y) <= radius;

	case Triangle: {
		double sqrt3 = 1.7320508;
		return (y >= -0.5 * radius) && (sqrt3 * x + y <= radius) && (-sqrt3 * x + y <= radius);
	}

	case Circle:
	default:
		return std::sqrt(x * x + y * y) <= radius;
	}
}
