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
};

#endif // WAVEFRONTPARAMETER_H
