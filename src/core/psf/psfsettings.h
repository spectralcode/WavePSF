#ifndef PSFSETTINGS_H
#define PSFSETTINGS_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QVariantMap>

struct PSFSettings {
	// --- Generator identity ---
	QString generatorTypeName = "Zernike";
	QVariantMap generatorSettings; // generator-specific config (serialized via IWavefrontGenerator)
	QMap<QString, QVariantMap> allGeneratorSettings; // type name → serialized settings for ALL known generators

	// --- Zernike generator settings (convenience accessors, kept for UI compatibility) ---
	// Flexible Noll index specification: "2-21", "1-5, 7, 11", "4, 11, 15-21"
	QString nollIndexSpec = "2-21";

	// Coefficient range
	double globalMinCoefficient = -3.0;
	double globalMaxCoefficient = 3.0;
	double coefficientStep = 0.1;
	QMap<int, QPair<double,double>> coefficientRangeOverrides; // nollIndex -> (min, max)

	// --- Common PSF calculator settings (shared by all generators) ---
	int gridSize = 128;
	double phaseScale = 1.0;    	// phase scaling factor: φ = phaseScale × Wavefront (rad per wavefront unit)
	double apertureRadius = 1.0;    // normalized [0, 1]
	int normalizationMode = 0;      // 0=Sum, 1=Peak, 2=None
	int paddingFactor = 1;          // FFT zero-padding factor (1=none, 2, 4, 8)
	int apertureGeometry = 0;       // 0=Circle, 1=Rectangle, 2=Triangle

	// --- PSF model selection ---
	int psfModel = 0;              // 0=ScalarFourier, 1=Microscopy3D

	// --- Richards-Wolf calculator settings (used when psfModel == 1) ---
	QVariantMap rwSettings;
};

// Parse 0-based index spec string into sorted list of indices
// Examples: "0-3, 5, 7-10" -> {0,1,2,3,5,7,8,9,10}
QVector<int> parseIndexSpec(const QString& spec);

// Parse frame spec string into sorted list of frame numbers
// Supports step notation with colon: "0-500:50" -> {0,50,100,...,500}
// Also supports comma-separated and mixed: "0-100:10, 200, 300-400:25"
QVector<int> parseFrameSpec(const QString& spec);

// Serialization helpers for settings persistence
QVariantMap serializePSFSettings(const PSFSettings& settings);
PSFSettings deserializePSFSettings(const QVariantMap& map);

#endif // PSFSETTINGS_H
