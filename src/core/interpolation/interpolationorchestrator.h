#ifndef INTERPOLATIONORCHESTRATOR_H
#define INTERPOLATIONORCHESTRATOR_H

#include <QObject>
#include "tableinterpolator.h"

class WavefrontParameterTable;

class InterpolationOrchestrator : public QObject
{
	Q_OBJECT
public:
	explicit InterpolationOrchestrator(QObject* parent = nullptr);

	void setPolynomialOrder(int order);

	// Run interpolation along an axis, build result with full table readback.
	// Returns the result (also emitted as interpolationCompleted signal).
	InterpolationResult interpolateInX(WavefrontParameterTable* table, int frame, int patchX, int patchY);
	InterpolationResult interpolateInY(WavefrontParameterTable* table, int frame, int patchX, int patchY);
	InterpolationResult interpolateInZ(WavefrontParameterTable* table, int patchX, int patchY);
	void interpolateAllInZ(WavefrontParameterTable* table);

private:
	TableInterpolator* tableInterpolator;
};

#endif // INTERPOLATIONORCHESTRATOR_H
