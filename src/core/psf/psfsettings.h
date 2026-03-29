#ifndef PSFSETTINGS_H
#define PSFSETTINGS_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QVariantMap>

struct PSFSettings {
	QString generatorTypeName = "Zernike";
	QMap<QString, QVariantMap> allGeneratorSettings; // type name → serialized composed settings
	int gridSize = 128;
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
