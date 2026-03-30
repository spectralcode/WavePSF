#ifndef DISPLAYSETTINGS_H
#define DISPLAYSETTINGS_H

#include <QString>

enum class AutoRangeMode { Off, PerFrame, PerVolume };

struct DisplaySettings
{
	AutoRangeMode autoRangeMode = AutoRangeMode::PerFrame;
	double rangeMin = 0.0;
	double rangeMax = 255.0;
	bool logScale = false;
	QString lutName = QStringLiteral("Grayscale");
};

#endif // DISPLAYSETTINGS_H
