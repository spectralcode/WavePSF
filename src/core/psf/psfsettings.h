#ifndef PSFSETTINGS_H
#define PSFSETTINGS_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QVariantMap>

struct PSFSettings {
	// --- Zernike generator settings (generator-specific) ---
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
	double apertureRadius = 0.4;    // normalized [0, 1]
	int normalizationMode = 0;      // 0=Sum, 1=Peak, 2=None
};

// Parse Noll index spec string into sorted list of indices
// Examples: "2-21" -> {2,3,...,21}, "1-5, 7, 11" -> {1,2,3,4,5,7,11}
QVector<int> parseNollIndexSpec(const QString& spec);

// Convert sorted list of indices back to compact spec string
// Examples: {2,3,4,5,7,11} -> "2-5, 7, 11"
QString formatNollIndexSpec(const QVector<int>& indices);

// Serialization helpers for settings persistence
QVariantMap serializePSFSettings(const PSFSettings& settings);
PSFSettings deserializePSFSettings(const QVariantMap& map);

#endif // PSFSETTINGS_H
