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

	// --- Zernike generator settings (convenience accessors, kept for UI compatibility) ---
	// Flexible Noll index specification: "2-21", "1-5, 7, 11", "4, 11, 15-21"
	QString nollIndexSpec = "2-21";

	// Coefficient range
	double globalMinCoefficient = -0.3;
	double globalMaxCoefficient = 0.3;
	double coefficientStep = 0.001;
	QMap<int, QPair<double,double>> coefficientRangeOverrides; // nollIndex -> (min, max)

	// --- Common PSF calculator settings (shared by all generators) ---
	int gridSize = 128;
	double wavelengthNm = 55.0;     // wavelength in nanometers
	double apertureRadius = 1.0;    // normalized [0, 1]
	int normalizationMode = 0;      // 0=Sum, 1=Peak, 2=None
};

// Parse Noll index spec string into sorted list of indices
// Examples: "2-21" -> {2,3,...,21}, "1-5, 7, 11" -> {1,2,3,4,5,7,11}
QVector<int> parseNollIndexSpec(const QString& spec);

// Convert sorted list of indices back to compact spec string
// Examples: {2,3,4,5,7,11} -> "2-5, 7, 11"
QString formatNollIndexSpec(const QVector<int>& indices);

// Parse 0-based index spec string into sorted list of indices
// Same as parseNollIndexSpec but accepts 0-based values
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
