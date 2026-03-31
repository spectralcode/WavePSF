#ifndef HISTOGRAMDATA_H
#define HISTOGRAMDATA_H

#include <QVector>
#include <QMetaType>

struct HistogramData
{
	double domainMin = 0.0;
	double domainMax = 1.0;
	QVector<int> bins;
};

Q_DECLARE_METATYPE(HistogramData)

#endif // HISTOGRAMDATA_H
