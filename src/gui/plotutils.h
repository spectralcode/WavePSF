#ifndef PLOTUTILS_H
#define PLOTUTILS_H

class QCustomPlot;
class QCPColorMap;
class QWidget;

namespace PlotUtils {
	void savePlotToDisk(QCustomPlot* plot, QWidget* parent);
	void saveColorMapToDisk(QCustomPlot* plot, QCPColorMap* colorMap, QWidget* parent);
}

#endif // PLOTUTILS_H
