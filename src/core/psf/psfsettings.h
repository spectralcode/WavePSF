#ifndef PSFSETTINGS_H
#define PSFSETTINGS_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QVariantMap>

struct PSFSettings {
	QString generatorTypeName = "Zernike";
	QMap<QString, QVariantMap> allGeneratorSettings; // type name → serialized per-generator settings
	int gridSize = 128;

	// Runtime capabilities of the active generator (set by PSFModule, not serialized)
	bool is3D = false;
	bool hasInlineSettings = false;
	bool isFileBased = false;
	int apertureGeometry = 0;
	double apertureRadius = 1.0;
	QVariantMap inlineSettingsValues;  // flat map for inline settings widget (e.g., RW parameters)
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
