#include "psfsettings.h"
#include <QStringList>
#include <algorithm>


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
	map["generator_type_name"] = settings.generatorTypeName;
	map["generator_settings"] = settings.generatorSettings;
	map["noll_index_spec"] = settings.nollIndexSpec;
	map["global_min_coefficient"] = settings.globalMinCoefficient;
	map["global_max_coefficient"] = settings.globalMaxCoefficient;
	map["coefficient_step"] = settings.coefficientStep;
	map["grid_size"] = settings.gridSize;
	map["wavelength_nm"] = settings.wavelengthNm;
	map["aperture_radius"] = settings.apertureRadius;
	map["normalization_mode"] = settings.normalizationMode;
	map["padding_factor"] = settings.paddingFactor;

	// Serialize range overrides
	QVariantMap overrides;
	for (auto it = settings.coefficientRangeOverrides.constBegin();
		 it != settings.coefficientRangeOverrides.constEnd(); ++it) {
		QVariantMap range;
		range["min"] = it.value().first;
		range["max"] = it.value().second;
		overrides[QString::number(it.key())] = range;
	}
	map["coefficient_range_overrides"] = overrides;

	return map;
}

PSFSettings deserializePSFSettings(const QVariantMap& map)
{
	PSFSettings settings;

	if (map.contains("generator_type_name")) {
		settings.generatorTypeName = map["generator_type_name"].toString();
	}
	if (map.contains("generator_settings")) {
		settings.generatorSettings = map["generator_settings"].toMap();
	}
	if (map.contains("noll_index_spec")) {
		settings.nollIndexSpec = map["noll_index_spec"].toString();
	}
	if (map.contains("global_min_coefficient")) {
		settings.globalMinCoefficient = map["global_min_coefficient"].toDouble();
	}
	if (map.contains("global_max_coefficient")) {
		settings.globalMaxCoefficient = map["global_max_coefficient"].toDouble();
	}
	if (map.contains("coefficient_step")) {
		settings.coefficientStep = map["coefficient_step"].toDouble();
	}
	if (map.contains("grid_size")) {
		settings.gridSize = map["grid_size"].toInt();
	}
	if (map.contains("wavelength_nm")) {
		settings.wavelengthNm = map["wavelength_nm"].toDouble();
	}
	if (map.contains("aperture_radius")) {
		settings.apertureRadius = map["aperture_radius"].toDouble();
	}
	if (map.contains("normalization_mode")) {
		settings.normalizationMode = map["normalization_mode"].toInt();
	}
	if (map.contains("padding_factor")) {
		settings.paddingFactor = map["padding_factor"].toInt();
	}

	// Deserialize range overrides
	if (map.contains("coefficient_range_overrides")) {
		QVariantMap overrides = map["coefficient_range_overrides"].toMap();
		for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
			QVariantMap range = it.value().toMap();
			int nollIndex = it.key().toInt();
			double min = range["min"].toDouble();
			double max = range["max"].toDouble();
			settings.coefficientRangeOverrides[nollIndex] = qMakePair(min, max);
		}
	}

	return settings;
}
