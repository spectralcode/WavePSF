#include "psfsettings.h"
#include <QStringList>
#include <algorithm>

namespace {
	const QString KEY_GENERATOR_TYPE_NAME    = QStringLiteral("generator_type_name");
	const QString KEY_ALL_GENERATOR_SETTINGS = QStringLiteral("all_generator_settings");
	const QString KEY_GRID_SIZE              = QStringLiteral("grid_size");
}


QVector<int> parseIndexSpec(const QString& spec)
{
	QVector<int> result;
	QStringList parts = spec.split(',', QString::SkipEmptyParts);

	for (const QString& part : qAsConst(parts)) {
		QString trimmed = part.trimmed();
		if (trimmed.isEmpty()) {
			continue;
		}

		int dashIndex = trimmed.indexOf('-');
		if (dashIndex > 0) {
			// Range: "2-10"
			bool okStart = false, okEnd = false;
			int start = trimmed.left(dashIndex).trimmed().toInt(&okStart);
			int end = trimmed.mid(dashIndex + 1).trimmed().toInt(&okEnd);
			if (okStart && okEnd && start >= 0 && end >= start) {
				for (int i = start; i <= end; ++i) {
					if (!result.contains(i)) {
						result.append(i);
					}
				}
			}
		} else {
			// Single index: "7"
			bool ok = false;
			int val = trimmed.toInt(&ok);
			if (ok && val >= 0 && !result.contains(val)) {
				result.append(val);
			}
		}
	}

	std::sort(result.begin(), result.end());
	return result;
}

QVector<int> parseFrameSpec(const QString& spec)
{
	QVector<int> result;
	QStringList parts = spec.split(',', QString::SkipEmptyParts);

	for (const QString& part : qAsConst(parts)) {
		QString trimmed = part.trimmed();
		if (trimmed.isEmpty()) {
			continue;
		}

		int dashIndex = trimmed.indexOf('-');
		int colonIndex = trimmed.indexOf(':');

		if (dashIndex > 0) {
			// Range with optional step: "0-500" or "0-500:50"
			QString rangeStr = (colonIndex > dashIndex)
				? trimmed.left(colonIndex).trimmed()
				: trimmed;

			bool okStart = false, okEnd = false;
			int start = rangeStr.left(dashIndex).trimmed().toInt(&okStart);
			int end = rangeStr.mid(dashIndex + 1).trimmed().toInt(&okEnd);

			int step = 1;
			if (colonIndex > dashIndex) {
				bool okStep = false;
				step = trimmed.mid(colonIndex + 1).trimmed().toInt(&okStep);
				if (!okStep || step < 1) step = 1;
			}

			if (okStart && okEnd && start >= 0 && end >= start) {
				for (int i = start; i <= end; i += step) {
					if (!result.contains(i)) {
						result.append(i);
					}
				}
			}
		} else {
			// Single frame number: "200"
			bool ok = false;
			int val = trimmed.toInt(&ok);
			if (ok && val >= 0 && !result.contains(val)) {
				result.append(val);
			}
		}
	}

	std::sort(result.begin(), result.end());
	return result;
}

QVariantMap serializePSFSettings(const PSFSettings& settings)
{
	QVariantMap map;
	map[KEY_GENERATOR_TYPE_NAME] = settings.generatorTypeName;
	map[KEY_GRID_SIZE]           = settings.gridSize;

	QVariantMap allGenMap;
	for (auto it = settings.allGeneratorSettings.constBegin();
		 it != settings.allGeneratorSettings.constEnd(); ++it) {
		allGenMap[it.key()] = it.value();
	}
	map[KEY_ALL_GENERATOR_SETTINGS] = allGenMap;

	return map;
}

PSFSettings deserializePSFSettings(const QVariantMap& map)
{
	PSFSettings s;

	s.generatorTypeName = map.value(KEY_GENERATOR_TYPE_NAME, s.generatorTypeName).toString();
	s.gridSize          = map.value(KEY_GRID_SIZE,           s.gridSize).toInt();

	QVariantMap allGenMap = map.value(KEY_ALL_GENERATOR_SETTINGS).toMap();
	for (auto it = allGenMap.constBegin(); it != allGenMap.constEnd(); ++it) {
		s.allGeneratorSettings[it.key()] = it.value().toMap();
	}

	return s;
}
