#include "tableinterpolator.h"
#include "data/wavefrontparametertable.h"
#include <QtMath>
#include <cmath>

TableInterpolator::TableInterpolator(QObject* parent)
	: QObject(parent)
	, order(3)
{
}

void TableInterpolator::setPolynomialOrder(int order)
{
	if (order >= 1) {
		this->order = order;
	}
}

int TableInterpolator::polynomialOrder() const
{
	return this->order;
}

QVector<InterpolationSlice> TableInterpolator::interpolateInX(
	WavefrontParameterTable* table, int frame, int patchX, int patchY)
{
	Q_UNUSED(patchX);
	QVector<InterpolationSlice> slices;
	if (table == nullptr) return slices;

	int width = table->getNumberOfPatchesInX();
	int numCoeffs = table->getCoefficientsPerPatch();

	for (int c = 0; c < numCoeffs; c++) {
		// Collect non-zero values along X for this coefficient
		QVector<double> positions;
		QVector<double> values;

		for (int x = 0; x < width; x++) {
			int patch = table->patchIndex(x, patchY);
			double val = table->getCoefficient(frame, patch, c);
			if (!qFuzzyIsNull(val)) {
				positions.append(x);
				values.append(val);
			}
		}

		if (positions.isEmpty()) continue;

		// Store input data for plotting
		InterpolationSlice slice;
		slice.coefficientIndex = c;
		slice.inputPositions = positions;
		slice.inputValues = values;
		slices.append(slice);

		// Fit polynomial and evaluate at all X positions
		QVector<double> coeffs = polyFit(positions, values, this->order);
		for (int x = 0; x < width; x++) {
			int patch = table->patchIndex(x, patchY);
			table->setCoefficient(frame, patch, c, polyEval(coeffs, x));
		}
	}

	return slices;
}

QVector<InterpolationSlice> TableInterpolator::interpolateInY(
	WavefrontParameterTable* table, int frame, int patchX, int patchY)
{
	Q_UNUSED(patchY);
	QVector<InterpolationSlice> slices;
	if (table == nullptr) return slices;

	int height = table->getNumberOfPatchesInY();
	int numCoeffs = table->getCoefficientsPerPatch();

	for (int c = 0; c < numCoeffs; c++) {
		QVector<double> positions;
		QVector<double> values;

		for (int y = 0; y < height; y++) {
			int patch = table->patchIndex(patchX, y);
			double val = table->getCoefficient(frame, patch, c);
			if (!qFuzzyIsNull(val)) {
				positions.append(y);
				values.append(val);
			}
		}

		if (positions.isEmpty()) continue;

		InterpolationSlice slice;
		slice.coefficientIndex = c;
		slice.inputPositions = positions;
		slice.inputValues = values;
		slices.append(slice);

		QVector<double> coeffs = polyFit(positions, values, this->order);
		for (int y = 0; y < height; y++) {
			int patch = table->patchIndex(patchX, y);
			table->setCoefficient(frame, patch, c, polyEval(coeffs, y));
		}
	}

	return slices;
}

QVector<InterpolationSlice> TableInterpolator::interpolateInZ(
	WavefrontParameterTable* table, int patchX, int patchY)
{
	QVector<InterpolationSlice> slices;
	if (table == nullptr) return slices;

	int numFrames = table->getNumberOfFrames();
	int numCoeffs = table->getCoefficientsPerPatch();
	int patch = table->patchIndex(patchX, patchY);

	for (int c = 0; c < numCoeffs; c++) {
		QVector<double> positions;
		QVector<double> values;

		for (int f = 0; f < numFrames; f++) {
			double val = table->getCoefficient(f, patch, c);
			if (!qFuzzyIsNull(val)) {
				positions.append(f);
				values.append(val);
			}
		}

		if (positions.isEmpty()) continue;

		InterpolationSlice slice;
		slice.coefficientIndex = c;
		slice.inputPositions = positions;
		slice.inputValues = values;
		slices.append(slice);

		QVector<double> coeffs = polyFit(positions, values, this->order);
		for (int f = 0; f < numFrames; f++) {
			table->setCoefficient(f, patch, c, polyEval(coeffs, f));
		}
	}

	return slices;
}

void TableInterpolator::interpolateAllInZ(WavefrontParameterTable* table)
{
	if (table == nullptr) return;

	int patchesX = table->getNumberOfPatchesInX();
	int patchesY = table->getNumberOfPatchesInY();

	for (int y = 0; y < patchesY; y++) {
		for (int x = 0; x < patchesX; x++) {
			this->interpolateInZ(table, x, y);
		}
	}
}

QVector<double> TableInterpolator::polyFit(
	const QVector<double>& positions, const QVector<double>& values, int order)
{
	int n = positions.size();
	if (n == 0) return QVector<double>(order + 1, 0.0);

	// Clamp order to number of data points - 1
	int effectiveOrder = qMin(order, n - 1);
	int m = effectiveOrder + 1;

	// Build Vandermonde matrix A (n x m) and compute A^T*A (m x m) and A^T*b (m)
	QVector<QVector<double>> ATA(m, QVector<double>(m, 0.0));
	QVector<double> ATb(m, 0.0);

	for (int k = 0; k < n; k++) {
		// Compute powers of position[k]
		QVector<double> powers(2 * m, 1.0);
		for (int j = 1; j < 2 * m; j++) {
			powers[j] = powers[j - 1] * positions[k];
		}

		// Accumulate A^T * A
		for (int i = 0; i < m; i++) {
			for (int j = 0; j < m; j++) {
				ATA[i][j] += powers[i + j];
			}
			ATb[i] += powers[i] * values[k];
		}
	}

	// Gaussian elimination with partial pivoting on [ATA | ATb]
	for (int col = 0; col < m; col++) {
		// Find pivot
		int maxRow = col;
		double maxVal = std::fabs(ATA[col][col]);
		for (int row = col + 1; row < m; row++) {
			double absVal = std::fabs(ATA[row][col]);
			if (absVal > maxVal) {
				maxVal = absVal;
				maxRow = row;
			}
		}

		if (maxVal < 1e-15) continue; // singular

		// Swap rows
		if (maxRow != col) {
			ATA[col].swap(ATA[maxRow]);
			std::swap(ATb[col], ATb[maxRow]);
		}

		// Eliminate below
		double pivot = ATA[col][col];
		for (int row = col + 1; row < m; row++) {
			double factor = ATA[row][col] / pivot;
			for (int j = col; j < m; j++) {
				ATA[row][j] -= factor * ATA[col][j];
			}
			ATb[row] -= factor * ATb[col];
		}
	}

	// Back substitution
	QVector<double> coeffs(m, 0.0);
	for (int i = m - 1; i >= 0; i--) {
		if (std::fabs(ATA[i][i]) < 1e-15) continue;
		double sum = ATb[i];
		for (int j = i + 1; j < m; j++) {
			sum -= ATA[i][j] * coeffs[j];
		}
		coeffs[i] = sum / ATA[i][i];
	}

	// Pad with zeros if effective order was clamped
	while (coeffs.size() < order + 1) {
		coeffs.append(0.0);
	}

	return coeffs;
}

double TableInterpolator::polyEval(const QVector<double>& coeffs, double x)
{
	// Horner's method
	double result = 0.0;
	for (int i = coeffs.size() - 1; i >= 0; i--) {
		result = result * x + coeffs[i];
	}
	return result;
}
