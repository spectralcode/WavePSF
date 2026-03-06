#include "cmaesoptimizer.h"
#include <QRandomGenerator>
#include <QtMath>
#include <limits>
#include <algorithm>

namespace {
	const QString KEY_INITIAL_SIGMA    = QStringLiteral("initial_sigma");
	const QString KEY_POPULATION_SIZE  = QStringLiteral("population_size");
	const QString KEY_MAX_GENERATIONS  = QStringLiteral("max_generations");
}


CMAESOptimizer::CMAESOptimizer()
	: initialSigma(0.3)
	, populationSize(0)
	, maxGenerations(300)
{
}

QString CMAESOptimizer::typeName() const
{
	return QStringLiteral("CMA-ES");
}

QVector<OptimizerParameter> CMAESOptimizer::getParameterDescriptors() const
{
	return {
		{ KEY_INITIAL_SIGMA,   "Initial Sigma",
		  "Initial step size as fraction of parameter range.\n0.3 = 30% of each parameter's bounds.\nTypical: 0.1-0.5.",
		  0.001, 10.0,   0.01, 0.3, 3 },
		{ KEY_POPULATION_SIZE, "Population Size (0=auto)",
		  "Number of candidate solutions per generation.\n0 = automatic (4 + 3*ln(N)).",
		  0,     500,    1,    0,   0 },
		{ KEY_MAX_GENERATIONS, "Max Generations",
		  "Maximum number of evolutionary generations before stopping.",
		  1,     100000, 10,   300, 0 }
	};
}

QVariantMap CMAESOptimizer::serializeSettings() const
{
	QVariantMap map;
	map[KEY_INITIAL_SIGMA]    = this->initialSigma;
	map[KEY_POPULATION_SIZE]  = this->populationSize;
	map[KEY_MAX_GENERATIONS]  = this->maxGenerations;
	return map;
}

void CMAESOptimizer::deserializeSettings(const QVariantMap& settings)
{
	this->initialSigma   = settings.value(KEY_INITIAL_SIGMA,   this->initialSigma).toDouble();
	this->populationSize = settings.value(KEY_POPULATION_SIZE, this->populationSize).toInt();
	this->maxGenerations = settings.value(KEY_MAX_GENERATIONS, this->maxGenerations).toInt();
}

OptimizerResult CMAESOptimizer::run(
	std::function<double(const QVector<double>&)> objective,
	const QVector<double>& initialCoefficients,
	const QVector<int>& selectedIndices,
	const QVector<double>& lowerBounds,
	const QVector<double>& upperBounds,
	QAtomicInt& cancelFlag,
	OptimizerProgressCallback progressCallback)
{
	const int N = selectedIndices.size();

	// Handle degenerate cases
	if (N == 0) {
		OptimizerResult result;
		result.bestCoefficients = initialCoefficients;
		result.bestMetric = objective(initialCoefficients);
		result.totalIterations = 0;
		return result;
	}

	// Population size
	const int lambda = (this->populationSize > 0) ? this->populationSize
												   : (4 + static_cast<int>(3.0 * qLn(N)));
	const int mu = lambda / 2;

	// Recombination weights
	QVector<double> weights(mu);
	double sumWeights = 0.0;
	for (int i = 0; i < mu; ++i) {
		weights[i] = qLn(mu + 0.5) - qLn(i + 1.0);
		sumWeights += weights[i];
	}
	for (int i = 0; i < mu; ++i) {
		weights[i] /= sumWeights;
	}
	double mueff = 0.0;
	for (int i = 0; i < mu; ++i) {
		mueff += weights[i] * weights[i];
	}
	mueff = 1.0 / mueff;

	// Adaptation parameters
	double cc = (4.0 + mueff / N) / (N + 4.0 + 2.0 * mueff / N);
	double cs = (mueff + 2.0) / (N + mueff + 5.0);
	double c1 = 2.0 / ((N + 1.3) * (N + 1.3) + mueff);
	double cmu = qMin(1.0 - c1, 2.0 * (mueff - 2.0 + 1.0 / mueff)
					   / ((N + 2.0) * (N + 2.0) + mueff));
	double damps = 1.0 + 2.0 * qMax(0.0, qSqrt((mueff - 1.0) / (N + 1.0)) - 1.0) + cs;
	double chiN = qSqrt(static_cast<double>(N))
				  * (1.0 - 1.0 / (4.0 * N) + 1.0 / (21.0 * N * N));

	// Extract bounds and compute ranges for internal normalization.
	// All CMA-ES state (mean, C, ps, pc, sigma) operates in normalized [0,1]
	// space so that sigma is scale-independent across different parameter ranges.
	QVector<double> lo(N), hi(N), range(N);
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		lo[i] = (idx < lowerBounds.size()) ? lowerBounds[idx] : -0.3;
		hi[i] = (idx < upperBounds.size()) ? upperBounds[idx] : 0.3;
		range[i] = hi[i] - lo[i];
		if (range[i] < 1e-15) range[i] = 1e-15;
	}

	// Initial mean in normalized [0, 1] space
	QVector<double> mean(N);
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		double val = qBound(lo[i], initialCoefficients[idx], hi[i]);
		mean[i] = (val - lo[i]) / range[i];
	}

	double sigma = this->initialSigma;

	// Evolution paths
	QVector<double> pc(N, 0.0);
	QVector<double> ps(N, 0.0);

	// Covariance matrix C = I (stored as flat NxN, row-major)
	QVector<double> C(N * N, 0.0);
	for (int i = 0; i < N; ++i) {
		C[i * N + i] = 1.0;
	}

	// Eigenvectors B and sqrt-eigenvalues D
	QVector<double> B(N * N, 0.0);
	QVector<double> D(N);
	for (int i = 0; i < N; ++i) {
		B[i * N + i] = 1.0;
		D[i] = 1.0;
	}

	// Inverse sqrt-eigenvalues for ps update
	QVector<double> invD(N);
	for (int i = 0; i < N; ++i) {
		invD[i] = 1.0 / D[i];
	}

	int eigenUpdateInterval = qMax(1, N / 10);

	// Global best tracking (stored in actual coefficient space)
	QVector<double> globalBestCoeffs = initialCoefficients;
	double globalBestMetric = (std::numeric_limits<double>::max)();

	// Population storage (x and y in normalized [0,1] space)
	struct Individual {
		QVector<double> x;   // N-dimensional normalized space
		QVector<double> y;   // x = mean + sigma * y
		double fitness;
	};
	QVector<Individual> population(lambda);

	QVector<double> fullCoeffs = initialCoefficients;
	const int maxResampleAttempts = 10;

	int gen = 0;
	for (gen = 0; gen < this->maxGenerations; ++gen) {
		if (cancelFlag.loadAcquire()) break;

		// Sample lambda offspring
		for (int k = 0; k < lambda; ++k) {
			if (cancelFlag.loadAcquire()) break;

			population[k].x.resize(N);
			population[k].y.resize(N);

			// Resample until feasible (within [0,1]^N) to avoid
			// covariance distortion from boundary clamping.
			bool feasible = false;
			for (int attempt = 0; attempt <= maxResampleAttempts; ++attempt) {
				QVector<double> z(N);
				for (int i = 0; i < N; ++i) {
					z[i] = randomGaussian();
				}

				// y = B * D * z,  x = mean + sigma * y
				bool inBounds = true;
				for (int i = 0; i < N; ++i) {
					double sum = 0.0;
					for (int j = 0; j < N; ++j) {
						sum += B[i * N + j] * D[j] * z[j];
					}
					population[k].y[i] = sum;
					population[k].x[i] = mean[i] + sigma * sum;
					if (population[k].x[i] < 0.0 || population[k].x[i] > 1.0) {
						inBounds = false;
					}
				}

				if (inBounds || attempt == maxResampleAttempts) {
					feasible = inBounds;
					break;
				}
			}

			// Fallback: clamp and recompute y
			if (!feasible) {
				for (int i = 0; i < N; ++i) {
					population[k].x[i] = qBound(0.0, population[k].x[i], 1.0);
					population[k].y[i] = (sigma > 1e-20)
						? (population[k].x[i] - mean[i]) / sigma : 0.0;
				}
			}

			// Convert normalized x to actual space for objective evaluation
			for (int i = 0; i < N; ++i) {
				fullCoeffs[selectedIndices[i]] = lo[i] + population[k].x[i] * range[i];
			}
			population[k].fitness = objective(fullCoeffs);
		}

		if (cancelFlag.loadAcquire()) break;

		// Sort by fitness (ascending = best first)
		std::sort(population.begin(), population.end(),
				  [](const Individual& a, const Individual& b) {
			return a.fitness < b.fitness;
		});

		// Track global best (convert to actual space)
		if (population[0].fitness < globalBestMetric) {
			globalBestMetric = population[0].fitness;
			for (int i = 0; i < N; ++i) {
				globalBestCoeffs[selectedIndices[i]] =
					lo[i] + population[0].x[i] * range[i];
			}
		}

		// Compute new mean (in normalized space)
		QVector<double> meanOld = mean;
		for (int i = 0; i < N; ++i) {
			mean[i] = 0.0;
			for (int k = 0; k < mu; ++k) {
				mean[i] += weights[k] * population[k].x[i];
			}
		}

		// Mean displacement (used for path updates)
		QVector<double> meanDiff(N);
		for (int i = 0; i < N; ++i) {
			meanDiff[i] = (sigma > 1e-20)
				? (mean[i] - meanOld[i]) / sigma : 0.0;
		}

		// Update ps (sigma evolution path)
		// ps = (1-cs)*ps + sqrt(cs*(2-cs)*mueff) * C^(-1/2) * meanDiff
		// C^(-1/2) * meanDiff = B * invD * B^T * meanDiff
		QVector<double> artmp(N, 0.0);
		// B^T * meanDiff
		for (int i = 0; i < N; ++i) {
			double sum = 0.0;
			for (int j = 0; j < N; ++j) {
				sum += B[j * N + i] * meanDiff[j];
			}
			artmp[i] = invD[i] * sum;
		}
		// B * artmp
		QVector<double> Cinvsqrt_meanDiff(N, 0.0);
		for (int i = 0; i < N; ++i) {
			double sum = 0.0;
			for (int j = 0; j < N; ++j) {
				sum += B[i * N + j] * artmp[j];
			}
			Cinvsqrt_meanDiff[i] = sum;
		}

		double sqrtCsMueff = qSqrt(cs * (2.0 - cs) * mueff);
		for (int i = 0; i < N; ++i) {
			ps[i] = (1.0 - cs) * ps[i] + sqrtCsMueff * Cinvsqrt_meanDiff[i];
		}

		// Compute |ps|^2
		double psNorm2 = 0.0;
		for (int i = 0; i < N; ++i) {
			psNorm2 += ps[i] * ps[i];
		}
		double psNorm = qSqrt(psNorm2);

		// hsig: stall indicator
		double hsigThreshold = (1.4 + 2.0 / (N + 1.0)) * chiN
							   * qSqrt(1.0 - qPow(1.0 - cs, 2.0 * (gen + 1)));
		double hsig = (psNorm < hsigThreshold) ? 1.0 : 0.0;

		// Update pc (covariance evolution path)
		double sqrtCcMueff = qSqrt(cc * (2.0 - cc) * mueff);
		for (int i = 0; i < N; ++i) {
			pc[i] = (1.0 - cc) * pc[i] + hsig * sqrtCcMueff * meanDiff[i];
		}

		// Update covariance matrix C
		double c1a = c1 * (1.0 - (1.0 - hsig * hsig) * cc * (2.0 - cc));
		for (int i = 0; i < N; ++i) {
			for (int j = 0; j <= i; ++j) {
				double rankOneUpdate = pc[i] * pc[j];
				double rankMuUpdate = 0.0;
				for (int k = 0; k < mu; ++k) {
					rankMuUpdate += weights[k] * population[k].y[i] * population[k].y[j];
				}
				C[i * N + j] = (1.0 - c1a - cmu) * C[i * N + j]
							   + c1 * rankOneUpdate
							   + cmu * rankMuUpdate;
				C[j * N + i] = C[i * N + j]; // symmetric
			}
		}

		// Update sigma
		sigma *= qExp((cs / damps) * (psNorm / chiN - 1.0));

		// Eigendecomposition of C (periodically)
		if ((gen + 1) % eigenUpdateInterval == 0 || gen == 0) {
			eigenDecomposition(C, N, B, D);
			for (int i = 0; i < N; ++i) {
				D[i] = qSqrt(qMax(D[i], 1e-20));
				invD[i] = 1.0 / D[i];
			}
		}

		// Report progress (convert to actual space)
		OptimizerProgress progress;
		progress.iteration = gen + 1;
		progress.currentMetric = population[0].fitness;
		progress.bestMetric = globalBestMetric;
		progress.bestCoefficients = globalBestCoeffs;
		for (int i = 0; i < N; ++i) {
			fullCoeffs[selectedIndices[i]] =
				lo[i] + population[0].x[i] * range[i];
		}
		progress.currentCoefficients = fullCoeffs;
		progress.algorithmStatus = QString("sigma: %1").arg(sigma, 0, 'g', 4);
		progressCallback(progress);
	}

	OptimizerResult result;
	result.bestCoefficients = globalBestCoeffs;
	result.bestMetric = globalBestMetric;
	result.totalIterations = gen;
	return result;
}

void CMAESOptimizer::eigenDecomposition(const QVector<double>& matrix, int n,
										QVector<double>& eigenvectors,
										QVector<double>& eigenvalues)
{
	// Jacobi eigenvalue algorithm for symmetric matrices
	eigenvalues.resize(n);
	eigenvectors.resize(n * n);

	// Copy matrix to work array (will be modified)
	QVector<double> A = matrix;

	// Initialize eigenvectors to identity
	eigenvectors.fill(0.0);
	for (int i = 0; i < n; ++i) {
		eigenvectors[i * n + i] = 1.0;
	}

	const int maxSweeps = 100;
	const double eps = 1e-15;

	for (int sweep = 0; sweep < maxSweeps; ++sweep) {
		// Check for convergence: sum of off-diagonal elements
		double offDiagSum = 0.0;
		for (int i = 0; i < n; ++i) {
			for (int j = i + 1; j < n; ++j) {
				offDiagSum += A[i * n + j] * A[i * n + j];
			}
		}
		if (offDiagSum < eps) break;

		for (int p = 0; p < n; ++p) {
			for (int q = p + 1; q < n; ++q) {
				double apq = A[p * n + q];
				if (qAbs(apq) < eps) continue;

				double app = A[p * n + p];
				double aqq = A[q * n + q];
				double tau = (aqq - app) / (2.0 * apq);

				double t;
				if (tau >= 0.0) {
					t = 1.0 / (tau + qSqrt(1.0 + tau * tau));
				} else {
					t = -1.0 / (-tau + qSqrt(1.0 + tau * tau));
				}

				double c = 1.0 / qSqrt(1.0 + t * t);
				double s = t * c;

				// Update A
				A[p * n + p] = app - t * apq;
				A[q * n + q] = aqq + t * apq;
				A[p * n + q] = 0.0;
				A[q * n + p] = 0.0;

				for (int r = 0; r < n; ++r) {
					if (r == p || r == q) continue;
					double arp = A[r * n + p];
					double arq = A[r * n + q];
					A[r * n + p] = c * arp - s * arq;
					A[p * n + r] = A[r * n + p];
					A[r * n + q] = s * arp + c * arq;
					A[q * n + r] = A[r * n + q];
				}

				// Update eigenvectors
				for (int r = 0; r < n; ++r) {
					double vrp = eigenvectors[r * n + p];
					double vrq = eigenvectors[r * n + q];
					eigenvectors[r * n + p] = c * vrp - s * vrq;
					eigenvectors[r * n + q] = s * vrp + c * vrq;
				}
			}
		}
	}

	// Extract eigenvalues from diagonal
	for (int i = 0; i < n; ++i) {
		eigenvalues[i] = A[i * n + i];
	}
}

double CMAESOptimizer::randomGaussian()
{
	// Box-Muller transform
	double u1 = QRandomGenerator::global()->generateDouble();
	double u2 = QRandomGenerator::global()->generateDouble();
	// Avoid log(0)
	if (u1 < 1e-15) u1 = 1e-15;
	return qSqrt(-2.0 * qLn(u1)) * qCos(2.0 * M_PI * u2);
}

double CMAESOptimizer::randomDouble(double low, double high)
{
	double r = QRandomGenerator::global()->generateDouble();
	return low + r * (high - low);
}
