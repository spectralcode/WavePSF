#include "interpolationorchestrator.h"
#include "data/wavefrontparametertable.h"

InterpolationOrchestrator::InterpolationOrchestrator(QObject* parent)
	: QObject(parent)
{
	this->tableInterpolator = new TableInterpolator(this);
}

void InterpolationOrchestrator::setPolynomialOrder(int order)
{
	this->tableInterpolator->setPolynomialOrder(order);
}

InterpolationResult InterpolationOrchestrator::interpolateInX(
	WavefrontParameterTable* table, int frame, int patchX, int patchY)
{
	int width = table->getNumberOfPatchesInX();
	int numCoeffs = table->getCoefficientsPerPatch();

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInX(
		table, frame, patchX, patchY);

	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Patch X");

	// Read the full interpolated row from the table
	result.allPositions.resize(width);
	result.allValues.resize(numCoeffs);
	for (int x = 0; x < width; x++) {
		result.allPositions[x] = x;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(width);
		for (int x = 0; x < width; x++) {
			int patch = table->patchIndex(x, patchY);
			result.allValues[c][x] = table->getCoefficient(frame, patch, c);
		}
	}

	return result;
}

InterpolationResult InterpolationOrchestrator::interpolateInY(
	WavefrontParameterTable* table, int frame, int patchX, int patchY)
{
	int height = table->getNumberOfPatchesInY();
	int numCoeffs = table->getCoefficientsPerPatch();

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInY(
		table, frame, patchX, patchY);

	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Patch Y");

	result.allPositions.resize(height);
	result.allValues.resize(numCoeffs);
	for (int y = 0; y < height; y++) {
		result.allPositions[y] = y;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(height);
		for (int y = 0; y < height; y++) {
			int patch = table->patchIndex(patchX, y);
			result.allValues[c][y] = table->getCoefficient(frame, patch, c);
		}
	}

	return result;
}

InterpolationResult InterpolationOrchestrator::interpolateInZ(
	WavefrontParameterTable* table, int patchX, int patchY)
{
	int numFrames = table->getNumberOfFrames();
	int numCoeffs = table->getCoefficientsPerPatch();
	int patch = table->patchIndex(patchX, patchY);

	QVector<InterpolationSlice> slices = this->tableInterpolator->interpolateInZ(
		table, patchX, patchY);

	InterpolationResult result;
	result.slices = slices;
	result.totalCoefficients = numCoeffs;
	result.axisLabel = tr("Frame");

	result.allPositions.resize(numFrames);
	result.allValues.resize(numCoeffs);
	for (int f = 0; f < numFrames; f++) {
		result.allPositions[f] = f;
	}
	for (int c = 0; c < numCoeffs; c++) {
		result.allValues[c].resize(numFrames);
		for (int f = 0; f < numFrames; f++) {
			result.allValues[c][f] = table->getCoefficient(f, patch, c);
		}
	}

	return result;
}

void InterpolationOrchestrator::interpolateAllInZ(WavefrontParameterTable* table)
{
	this->tableInterpolator->interpolateAllInZ(table);
}
