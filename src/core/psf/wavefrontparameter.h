#ifndef WAVEFRONTPARAMETER_H
#define WAVEFRONTPARAMETER_H

#include <QString>

struct WavefrontParameter {
	int id;
	QString name;
	double minValue;
	double maxValue;
	double step;
	double defaultValue;

	bool operator==(const WavefrontParameter& other) const {
		return id == other.id && name == other.name
			&& minValue == other.minValue && maxValue == other.maxValue
			&& step == other.step && defaultValue == other.defaultValue;
	}
	bool operator!=(const WavefrontParameter& other) const {
		return !(*this == other);
	}
};

#endif // WAVEFRONTPARAMETER_H
