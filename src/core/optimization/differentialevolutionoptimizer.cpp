#include "differentialevolutionoptimizer.h"
#include <QRandomGenerator>
#include <QtMath>
#include <limits>

namespace {
	const QString KEY_MUTATION_FACTOR  = QStringLiteral("mutation_factor");
	const QString KEY_CROSSOVER_RATE   = QStringLiteral("crossover_rate");
	const QString KEY_POPULATION_SIZE  = QStringLiteral("population_size");
	const QString KEY_MAX_GENERATIONS  = QStringLiteral("max_generations");
}


DifferentialEvolutionOptimizer::DifferentialEvolutionOptimizer()
	: mutationFactor(0.8)
	, crossoverRate(0.9)
	, populationSize(0)
	, maxGenerations(300)
{
}

QString DifferentialEvolutionOptimizer::typeName() const
{
	return QStringLiteral("Differential Evolution");
}

QVector<OptimizerParameter> DifferentialEvolutionOptimizer::getParameterDescriptors() const
{
	return {
		{ KEY_MUTATION_FACTOR, "Mutation Factor (F)",
		  "Scale factor for difference vectors.\nHigher = larger mutation steps.\nTypical: 0.5-1.0.",
		  0.0, 2.0, 0.01, 0.8, 2 },
		{ KEY_CROSSOVER_RATE, "Crossover Rate (CR)",
		  "Probability of taking each dimension from the mutant.\nHigher = more dimensions changed per trial.\nTypical: 0.1-1.0.",
		  0.0, 1.0, 0.01, 0.9, 2 },
		{ KEY_POPULATION_SIZE, "Population Size (0=auto)",
		  "Number of individuals in the population.\n0 = automatic (N+2, min 6).",
		  0, 1000, 1, 0, 0 },
		{ KEY_MAX_GENERATIONS, "Max Generations",
		  "Maximum number of generations before stopping.",
		  1, 100000, 10, 300, 0 }
	};
}

QVariantMap DifferentialEvolutionOptimizer::serializeSettings() const
{
	QVariantMap map;
	map[KEY_MUTATION_FACTOR] = this->mutationFactor;
	map[KEY_CROSSOVER_RATE]  = this->crossoverRate;
	map[KEY_POPULATION_SIZE] = this->populationSize;
	map[KEY_MAX_GENERATIONS] = this->maxGenerations;
	return map;
}

void DifferentialEvolutionOptimizer::deserializeSettings(const QVariantMap& settings)
{
	this->mutationFactor = settings.value(KEY_MUTATION_FACTOR, this->mutationFactor).toDouble();
	this->crossoverRate  = settings.value(KEY_CROSSOVER_RATE,  this->crossoverRate).toDouble();
	this->populationSize = settings.value(KEY_POPULATION_SIZE, this->populationSize).toInt();
	this->maxGenerations = settings.value(KEY_MAX_GENERATIONS, this->maxGenerations).toInt();
}

OptimizerResult DifferentialEvolutionOptimizer::run(
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

	// Population size: keep small for expensive objectives.
	// N+2 gives enough vectors for mutation while keeping eval count
	// comparable to SA (~1500-2500 total for 300 generations).
	const int NP = (this->populationSize > 0) ? this->populationSize
											   : qMax(6, N + 2);
	const double F = this->mutationFactor;
	const double CR = this->crossoverRate;

	// Extract bounds for selected indices
	QVector<double> lo(N), hi(N);
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		lo[i] = (idx < lowerBounds.size()) ? lowerBounds[idx] : -0.3;
		hi[i] = (idx < upperBounds.size()) ? upperBounds[idx] : 0.3;
	}

	// Initialize population: NP individuals, each with N selected dimensions
	QVector<QVector<double>> pop(NP, QVector<double>(N));
	QVector<double> fitness(NP);

	// First individual = initial coefficients (clamped to bounds)
	for (int i = 0; i < N; ++i) {
		int idx = selectedIndices[i];
		pop[0][i] = qBound(lo[i], initialCoefficients[idx], hi[i]);
	}

	// Remaining individuals: perturbations around starting point so that
	// difference vectors are immediately meaningful (not random noise).
	for (int k = 1; k < NP; ++k) {
		for (int i = 0; i < N; ++i) {
			double range = hi[i] - lo[i];
			double spread = range * 0.3;
			double val = pop[0][i] + randomDouble(-spread, spread);
			pop[k][i] = qBound(lo[i], val, hi[i]);
		}
	}

	// Evaluate initial population
	QVector<double> fullCoeffs = initialCoefficients;
	for (int k = 0; k < NP; ++k) {
		if (cancelFlag.loadAcquire()) break;
		for (int i = 0; i < N; ++i) {
			fullCoeffs[selectedIndices[i]] = pop[k][i];
		}
		fitness[k] = objective(fullCoeffs);
	}

	// Find initial best and track its index
	int bestIdx = 0;
	for (int k = 1; k < NP; ++k) {
		if (fitness[k] < fitness[bestIdx]) bestIdx = k;
	}

	QVector<double> globalBestCoeffs = initialCoefficients;
	double globalBestMetric = fitness[bestIdx];
	for (int i = 0; i < N; ++i) {
		globalBestCoeffs[selectedIndices[i]] = pop[bestIdx][i];
	}

	// Main DE loop (DE/current-to-best/1/bin)
	QVector<double> trial(N);
	int gen = 0;
	for (gen = 0; gen < this->maxGenerations; ++gen) {
		if (cancelFlag.loadAcquire()) break;

		for (int k = 0; k < NP; ++k) {
			if (cancelFlag.loadAcquire()) break;

			// Pick 2 distinct random indices, different from k and bestIdx
			int r1, r2;
			do { r1 = randomInt(0, NP - 1); } while (r1 == k);
			do { r2 = randomInt(0, NP - 1); } while (r2 == k || r2 == r1);

			// Guaranteed crossover dimension
			int jrand = randomInt(0, N - 1);

			// DE/current-to-best/1/bin:
			// v = x_k + F * (x_best - x_k) + F * (x_r1 - x_r2)
			for (int i = 0; i < N; ++i) {
				if (randomDouble(0.0, 1.0) < CR || i == jrand) {
					trial[i] = pop[k][i]
						+ F * (pop[bestIdx][i] - pop[k][i])
						+ F * (pop[r1][i] - pop[r2][i]);
				} else {
					trial[i] = pop[k][i];
				}
				trial[i] = qBound(lo[i], trial[i], hi[i]);
			}

			// Evaluate trial
			for (int i = 0; i < N; ++i) {
				fullCoeffs[selectedIndices[i]] = trial[i];
			}
			double trialFitness = objective(fullCoeffs);

			// Greedy selection
			if (trialFitness <= fitness[k]) {
				pop[k] = trial;
				fitness[k] = trialFitness;

				// Update global best
				if (trialFitness < globalBestMetric) {
					globalBestMetric = trialFitness;
					bestIdx = k;
					for (int i = 0; i < N; ++i) {
						globalBestCoeffs[selectedIndices[i]] = trial[i];
					}
				}
			}
		}

		if (cancelFlag.loadAcquire()) break;

		// Update bestIdx for next generation (in case greedy selection changed it)
		for (int k = 0; k < NP; ++k) {
			if (fitness[k] < fitness[bestIdx]) bestIdx = k;
		}

		// Report progress
		OptimizerProgress progress;
		progress.iteration = gen + 1;
		progress.bestMetric = globalBestMetric;
		progress.bestCoefficients = globalBestCoeffs;
		progress.currentMetric = fitness[bestIdx];
		for (int i = 0; i < N; ++i) {
			fullCoeffs[selectedIndices[i]] = pop[bestIdx][i];
		}
		progress.currentCoefficients = fullCoeffs;
		progress.algorithmStatus = QString("F: %1  CR: %2")
			.arg(F, 0, 'f', 2).arg(CR, 0, 'f', 2);
		progressCallback(progress);
	}

	OptimizerResult result;
	result.bestCoefficients = globalBestCoeffs;
	result.bestMetric = globalBestMetric;
	result.totalIterations = gen;
	return result;
}

double DifferentialEvolutionOptimizer::randomDouble(double low, double high)
{
	double r = QRandomGenerator::global()->generateDouble();
	return low + r * (high - low);
}

int DifferentialEvolutionOptimizer::randomInt(int low, int high)
{
	return QRandomGenerator::global()->bounded(low, high + 1);
}
