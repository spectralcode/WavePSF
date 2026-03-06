#include "neldermeadoptimizer.h"
#include <QtMath>
#include <algorithm>

namespace {
	const QString KEY_MAX_ITERATIONS   = QStringLiteral("max_iterations");
	const QString KEY_INITIAL_STEP_SIZE = QStringLiteral("initial_step_size");
}


NelderMeadOptimizer::NelderMeadOptimizer()
	: maxIterations(1000)
	, initialStepSize(0.05)
{
}

QString NelderMeadOptimizer::typeName() const
{
	return QStringLiteral("Nelder-Mead");
}

QVector<OptimizerParameter> NelderMeadOptimizer::getParameterDescriptors() const
{
	return {
		{ KEY_MAX_ITERATIONS,    "Max Iterations",
		  "Maximum number of simplex operations before stopping.",
		  1, 100000, 10, 1000, 0 },
		{ KEY_INITIAL_STEP_SIZE, "Initial Step Size",
		  "Simplex size as fraction of parameter range.\nSmaller = finer local search.\nLarger = broader exploration.\nTypical: 0.01-0.2.",
		  0.001, 1.0, 0.001, 0.05, 3 }
	};
}

QVariantMap NelderMeadOptimizer::serializeSettings() const
{
	QVariantMap map;
	map[KEY_MAX_ITERATIONS]    = this->maxIterations;
	map[KEY_INITIAL_STEP_SIZE] = this->initialStepSize;
	return map;
}

void NelderMeadOptimizer::deserializeSettings(const QVariantMap& settings)
{
	this->maxIterations   = settings.value(KEY_MAX_ITERATIONS,    this->maxIterations).toInt();
	this->initialStepSize = settings.value(KEY_INITIAL_STEP_SIZE, this->initialStepSize).toDouble();
}

OptimizerResult NelderMeadOptimizer::run(
	std::function<double(const QVector<double>&)> objective,
	const QVector<double>& initialCoefficients,
	const QVector<int>& selectedIndices,
	const QVector<double>& lowerBounds,
	const QVector<double>& upperBounds,
	QAtomicInt& cancelFlag,
	OptimizerProgressCallback progressCallback)
{
	const int N = selectedIndices.size();

	// Handle degenerate case
	if (N == 0) {
		OptimizerResult result;
		result.bestCoefficients = initialCoefficients;
		result.bestMetric = objective(initialCoefficients);
		result.totalIterations = 0;
		return result;
	}

	// Extract bounds for selected indices
	QVector<double> lo(N), hi(N);
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		lo[i] = (idx < lowerBounds.size()) ? lowerBounds[idx] : -0.3;
		hi[i] = (idx < upperBounds.size()) ? upperBounds[idx] : 0.3;
	}

	// Evaluation helper: maps N-dimensional point to full coefficient vector
	QVector<double> fullCoeffs = initialCoefficients;
	auto eval = [&](const QVector<double>& point) -> double {
		for (int i = 0; i < N; ++i)
			fullCoeffs[selectedIndices[i]] = point[i];
		return objective(fullCoeffs);
	};

	// Build initial simplex: N+1 vertices
	const int NV = N + 1;
	QVector<QVector<double>> simplex(NV, QVector<double>(N));
	QVector<double> values(NV);

	// Vertex 0 = initial coefficients (clamped to bounds)
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		simplex[0][i] = qBound(lo[i], initialCoefficients[idx], hi[i]);
	}

	// Vertices 1..N: perturb one dimension each by initialStepSize * range
	for (int j = 0; j < N; ++j) {
		simplex[j + 1] = simplex[0];
		double range = hi[j] - lo[j];
		double step = range * this->initialStepSize;
		double perturbed = simplex[0][j] + step;
		if (perturbed > hi[j])
			perturbed = simplex[0][j] - step;
		simplex[j + 1][j] = qBound(lo[j], perturbed, hi[j]);
	}

	// Evaluate initial simplex
	for (int v = 0; v < NV; ++v) {
		if (cancelFlag.loadAcquire()) break;
		values[v] = eval(simplex[v]);
	}

	// Track global best
	QVector<double> globalBestCoeffs = initialCoefficients;
	double globalBestMetric = values[0];
	for (int v = 0; v < NV; ++v) {
		if (values[v] < globalBestMetric) {
			globalBestMetric = values[v];
			for (int i = 0; i < N; ++i)
				globalBestCoeffs[selectedIndices[i]] = simplex[v][i];
		}
	}

	// Standard Nelder-Mead coefficients
	const double alpha = 1.0;         // reflection
	const double gamma = 2.0;         // expansion
	const double rho   = 0.5;         // contraction
	const double shrinkFactor = 0.5;  // shrink
	const double tol = 1e-12;

	QVector<int> order(NV);
	for (int v = 0; v < NV; ++v) order[v] = v;
	QVector<double> centroid(N), reflected(N), expanded(N), contracted(N);

	int iter = 0;
	for (iter = 0; iter < this->maxIterations; ++iter) {
		if (cancelFlag.loadAcquire()) break;

		// Sort vertices by fitness (ascending = best first)
		std::sort(order.begin(), order.end(), [&](int a, int b) {
			return values[a] < values[b];
		});

		int iBest = order[0];
		int iWorst = order[NV - 1];
		int iSecondWorst = order[NV - 2];

		// Update global best
		if (values[iBest] < globalBestMetric) {
			globalBestMetric = values[iBest];
			for (int i = 0; i < N; ++i)
				globalBestCoeffs[selectedIndices[i]] = simplex[iBest][i];
		}

		// Convergence check: value spread
		if (values[iWorst] - values[iBest] < tol) break;

		// Centroid of all vertices except worst
		for (int i = 0; i < N; ++i) {
			centroid[i] = 0.0;
			for (int v = 0; v < NV - 1; ++v)
				centroid[i] += simplex[order[v]][i];
			centroid[i] /= (NV - 1);
		}

		// 1. Reflection
		for (int i = 0; i < N; ++i)
			reflected[i] = qBound(lo[i],
				centroid[i] + alpha * (centroid[i] - simplex[iWorst][i]), hi[i]);
		double fReflected = eval(reflected);

		if (fReflected < values[iSecondWorst] && fReflected >= values[iBest]) {
			// Accept reflection
			simplex[iWorst] = reflected;
			values[iWorst] = fReflected;
		}
		else if (fReflected < values[iBest]) {
			// 2. Expansion
			for (int i = 0; i < N; ++i)
				expanded[i] = qBound(lo[i],
					centroid[i] + gamma * (reflected[i] - centroid[i]), hi[i]);
			double fExpanded = eval(expanded);

			if (fExpanded < fReflected) {
				simplex[iWorst] = expanded;
				values[iWorst] = fExpanded;
			} else {
				simplex[iWorst] = reflected;
				values[iWorst] = fReflected;
			}
		}
		else {
			// 3. Contraction
			bool doShrink = false;

			if (fReflected < values[iWorst]) {
				// Outside contraction
				for (int i = 0; i < N; ++i)
					contracted[i] = qBound(lo[i],
						centroid[i] + rho * (reflected[i] - centroid[i]), hi[i]);
				double fContracted = eval(contracted);

				if (fContracted <= fReflected) {
					simplex[iWorst] = contracted;
					values[iWorst] = fContracted;
				} else {
					doShrink = true;
				}
			} else {
				// Inside contraction
				for (int i = 0; i < N; ++i)
					contracted[i] = qBound(lo[i],
						centroid[i] + rho * (simplex[iWorst][i] - centroid[i]), hi[i]);
				double fContracted = eval(contracted);

				if (fContracted < values[iWorst]) {
					simplex[iWorst] = contracted;
					values[iWorst] = fContracted;
				} else {
					doShrink = true;
				}
			}

			// 4. Shrink toward best
			if (doShrink) {
				for (int v = 0; v < NV; ++v) {
					if (order[v] == iBest) continue;
					int vi = order[v];
					for (int i = 0; i < N; ++i)
						simplex[vi][i] = qBound(lo[i],
							simplex[iBest][i] + shrinkFactor * (simplex[vi][i] - simplex[iBest][i]),
							hi[i]);
					if (cancelFlag.loadAcquire()) break;
					values[vi] = eval(simplex[vi]);
				}
			}
		}

		// Report progress
		double minVal = values[0], maxVal = values[0];
		int curBestIdx = 0;
		for (int v = 1; v < NV; ++v) {
			if (values[v] < minVal) { minVal = values[v]; curBestIdx = v; }
			if (values[v] > maxVal) maxVal = values[v];
		}

		OptimizerProgress progress;
		progress.iteration = iter + 1;
		progress.bestMetric = globalBestMetric;
		progress.bestCoefficients = globalBestCoeffs;
		progress.currentMetric = minVal;
		for (int i = 0; i < N; ++i)
			fullCoeffs[selectedIndices[i]] = simplex[curBestIdx][i];
		progress.currentCoefficients = fullCoeffs;
		progress.algorithmStatus = QString("Spread: %1")
			.arg(maxVal - minVal, 0, 'e', 2);
		progressCallback(progress);
	}

	OptimizerResult result;
	result.bestCoefficients = globalBestCoeffs;
	result.bestMetric = globalBestMetric;
	result.totalIterations = iter;
	return result;
}
