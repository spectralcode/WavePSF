#include "psfsettings.h"
#include <QStringList>
#include <algorithm>

namespace {
	// Key names
	const QString KEY_GENERATOR_TYPE_NAME         = QStringLiteral("generator_type_name");
	const QString KEY_GENERATOR_SETTINGS          = QStringLiteral("generator_settings");
	const QString KEY_ALL_GENERATOR_SETTINGS      = QStringLiteral("all_generator_settings");
	const QString KEY_NOLL_INDEX_SPEC             = QStringLiteral("noll_index_spec");
	const QString KEY_GLOBAL_MIN_COEFFICIENT      = QStringLiteral("global_min_coefficient");
	const QString KEY_GLOBAL_MAX_COEFFICIENT      = QStringLiteral("global_max_coefficient");
	const QString KEY_COEFFICIENT_STEP            = QStringLiteral("coefficient_step");
	const QString KEY_COEFFICIENT_RANGE_OVERRIDES = QStringLiteral("coefficient_range_overrides");
	const QString KEY_GRID_SIZE                   = QStringLiteral("grid_size");
	const QString KEY_PHASE_SCALE                 = QStringLiteral("phase_scale");
	const QString KEY_APERTURE_RADIUS             = QStringLiteral("aperture_radius");
	const QString KEY_NORMALIZATION_MODE          = QStringLiteral("normalization_mode");
	const QString KEY_PADDING_FACTOR              = QStringLiteral("padding_factor");
	const QString KEY_APERTURE_GEOMETRY           = QStringLiteral("aperture_geometry");
	const QString KEY_RANGE_MIN                   = QStringLiteral("min");
	const QString KEY_RANGE_MAX                   = QStringLiteral("max");

	// Default values (match PSFSettings struct defaults)
	const QString DEF_GENERATOR_TYPE_NAME    = QStringLiteral("Zernike");
	const QString DEF_NOLL_INDEX_SPEC        = QStringLiteral("2-21");
	const double  DEF_GLOBAL_MIN_COEFFICIENT = -3.0;
	const double  DEF_GLOBAL_MAX_COEFFICIENT =  3.0;
	const double  DEF_COEFFICIENT_STEP       =  0.1;
	const int     DEF_GRID_SIZE              =  128;
	const double  DEF_PHASE_SCALE            =  1.0;
	const double  DEF_APERTURE_RADIUS        =  1.0;
	const int     DEF_NORMALIZATION_MODE     =  0;
	const int     DEF_PADDING_FACTOR         =  1;
	const int     DEF_APERTURE_GEOMETRY      =  0;
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
	map[KEY_GENERATOR_TYPE_NAME]    = settings.generatorTypeName;
	map[KEY_GENERATOR_SETTINGS]     = settings.generatorSettings;
	map[KEY_NOLL_INDEX_SPEC]        = settings.nollIndexSpec;
	map[KEY_GLOBAL_MIN_COEFFICIENT] = settings.globalMinCoefficient;
	map[KEY_GLOBAL_MAX_COEFFICIENT] = settings.globalMaxCoefficient;
	map[KEY_COEFFICIENT_STEP]       = settings.coefficientStep;
	map[KEY_GRID_SIZE]              = settings.gridSize;
	map[KEY_PHASE_SCALE]            = settings.phaseScale;
	map[KEY_APERTURE_RADIUS]        = settings.apertureRadius;
	map[KEY_NORMALIZATION_MODE]     = settings.normalizationMode;
	map[KEY_PADDING_FACTOR]         = settings.paddingFactor;
	map[KEY_APERTURE_GEOMETRY]      = settings.apertureGeometry;

	// Serialize range overrides
	QVariantMap overrides;
	for (auto it = settings.coefficientRangeOverrides.constBegin();
		 it != settings.coefficientRangeOverrides.constEnd(); ++it) {
		QVariantMap range;
		range[KEY_RANGE_MIN] = it.value().first;
		range[KEY_RANGE_MAX] = it.value().second;
		overrides[QString::number(it.key())] = range;
	}
	map[KEY_COEFFICIENT_RANGE_OVERRIDES] = overrides;

	// Serialize per-type generator settings
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
	PSFSettings settings;

	settings.generatorTypeName    = map.value(KEY_GENERATOR_TYPE_NAME,    DEF_GENERATOR_TYPE_NAME).toString();
	settings.generatorSettings    = map.value(KEY_GENERATOR_SETTINGS).toMap();
	settings.nollIndexSpec        = map.value(KEY_NOLL_INDEX_SPEC,         DEF_NOLL_INDEX_SPEC).toString();
	settings.globalMinCoefficient = map.value(KEY_GLOBAL_MIN_COEFFICIENT,  DEF_GLOBAL_MIN_COEFFICIENT).toDouble();
	settings.globalMaxCoefficient = map.value(KEY_GLOBAL_MAX_COEFFICIENT,  DEF_GLOBAL_MAX_COEFFICIENT).toDouble();
	settings.coefficientStep      = map.value(KEY_COEFFICIENT_STEP,        DEF_COEFFICIENT_STEP).toDouble();
	settings.gridSize             = map.value(KEY_GRID_SIZE,               DEF_GRID_SIZE).toInt();
	settings.phaseScale           = map.value(KEY_PHASE_SCALE,             DEF_PHASE_SCALE).toDouble();
	settings.apertureRadius       = map.value(KEY_APERTURE_RADIUS,         DEF_APERTURE_RADIUS).toDouble();
	settings.normalizationMode    = map.value(KEY_NORMALIZATION_MODE,      DEF_NORMALIZATION_MODE).toInt();
	settings.paddingFactor        = map.value(KEY_PADDING_FACTOR,          DEF_PADDING_FACTOR).toInt();
	settings.apertureGeometry     = map.value(KEY_APERTURE_GEOMETRY,       DEF_APERTURE_GEOMETRY).toInt();

	// Deserialize range overrides
	if (map.contains(KEY_COEFFICIENT_RANGE_OVERRIDES)) {
		QVariantMap overrides = map[KEY_COEFFICIENT_RANGE_OVERRIDES].toMap();
		for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
			QVariantMap range = it.value().toMap();
			int nollIndex = it.key().toInt();
			double minVal = range[KEY_RANGE_MIN].toDouble();
			double maxVal = range[KEY_RANGE_MAX].toDouble();
			settings.coefficientRangeOverrides[nollIndex] = qMakePair(minVal, maxVal);
		}
	}

	// Deserialize per-type generator settings
	if (map.contains(KEY_ALL_GENERATOR_SETTINGS)) {
		QVariantMap allGenMap = map[KEY_ALL_GENERATOR_SETTINGS].toMap();
		for (auto it = allGenMap.constBegin(); it != allGenMap.constEnd(); ++it) {
			settings.allGeneratorSettings[it.key()] = it.value().toMap();
		}
	}

	return settings;
}
