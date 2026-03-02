#include "plotutils.h"
#include "qcustomplot.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDir>

namespace PlotUtils {

static bool saveCSV(QCustomPlot* plot, const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
		return false;
	}
	QTextStream stream(&file);

	int graphCount = plot->graphCount();
	for (int g = 0; g < graphCount; ++g) {
		QCPGraph* graph = plot->graph(g);
		if (!graph || graph->data()->isEmpty()) continue;

		// Graph header
		QString name = graph->name();
		if (name.isEmpty()) {
			name = QStringLiteral("Graph %1").arg(g);
		}
		stream << "# " << name << "\n";

		// Column headers from axis labels
		QString xLabel = plot->xAxis->label();
		QString yLabel = plot->yAxis->label();
		if (xLabel.isEmpty()) xLabel = QStringLiteral("X");
		if (yLabel.isEmpty()) yLabel = QStringLiteral("Y");
		stream << xLabel << ";" << yLabel << "\n";

		// Data rows
		for (auto it = graph->data()->constBegin(); it != graph->data()->constEnd(); ++it) {
			stream << QString::number(it->key, 'g', 10) << ";"
				   << QString::number(it->value, 'g', 10) << "\n";
		}

		// Blank line between graphs
		if (g < graphCount - 1) {
			stream << "\n";
		}
	}

	file.close();
	return true;
}

void savePlotToDisk(QCustomPlot* plot, QWidget* parent)
{
	QString filters(QStringLiteral("CSV (*.csv);;Image (*.png);;Vector graphic (*.pdf)"));
	QString defaultFilter(QStringLiteral("CSV (*.csv)"));
	QString fileName = QFileDialog::getSaveFileName(
		parent, QObject::tr("Save Plot as..."),
		QDir::currentPath(), filters, &defaultFilter);

	if (fileName.isEmpty()) {
		return;
	}

	// Determine format from file extension
	bool saved = false;
	if (fileName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)) {
		saved = plot->savePng(fileName);
	} else if (fileName.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
		saved = plot->savePdf(fileName);
	} else if (fileName.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
		saved = saveCSV(plot, fileName);
	} else {
		// Fallback: use selected filter
		if (defaultFilter.contains(QLatin1String("png"))) {
			saved = plot->savePng(fileName);
		} else if (defaultFilter.contains(QLatin1String("pdf"))) {
			saved = plot->savePdf(fileName);
		} else {
			saved = saveCSV(plot, fileName);
		}
	}
	Q_UNUSED(saved);
}

static bool saveColorMapCSV(QCPColorMap* colorMap, const QString& fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
		return false;
	}
	QTextStream stream(&file);

	QCPColorMapData* data = colorMap->data();
	int nx = data->keySize();
	int ny = data->valueSize();

	// Write as 2D matrix: rows = y (value axis), columns = x (key axis)
	// First row: header with x coordinates
	stream << "y\\x";
	for (int xi = 0; xi < nx; ++xi) {
		double xCoord, yDummy;
		data->cellToCoord(xi, 0, &xCoord, &yDummy);
		stream << ";" << QString::number(xCoord, 'g', 10);
	}
	stream << "\n";

	// Data rows
	for (int yi = 0; yi < ny; ++yi) {
		double xDummy, yCoord;
		data->cellToCoord(0, yi, &xDummy, &yCoord);
		stream << QString::number(yCoord, 'g', 10);
		for (int xi = 0; xi < nx; ++xi) {
			stream << ";" << QString::number(data->cell(xi, yi), 'g', 10);
		}
		stream << "\n";
	}

	file.close();
	return true;
}

void saveColorMapToDisk(QCustomPlot* plot, QCPColorMap* colorMap, QWidget* parent)
{
	QString filters(QStringLiteral("CSV (*.csv);;Image (*.png);;Vector graphic (*.pdf)"));
	QString defaultFilter(QStringLiteral("CSV (*.csv)"));
	QString fileName = QFileDialog::getSaveFileName(
		parent, QObject::tr("Save Plot as..."),
		QDir::currentPath(), filters, &defaultFilter);

	if (fileName.isEmpty()) {
		return;
	}

	bool saved = false;
	if (fileName.endsWith(QLatin1String(".png"), Qt::CaseInsensitive)) {
		saved = plot->savePng(fileName);
	} else if (fileName.endsWith(QLatin1String(".pdf"), Qt::CaseInsensitive)) {
		saved = plot->savePdf(fileName);
	} else if (fileName.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive)) {
		saved = saveColorMapCSV(colorMap, fileName);
	} else {
		if (defaultFilter.contains(QLatin1String("png"))) {
			saved = plot->savePng(fileName);
		} else if (defaultFilter.contains(QLatin1String("pdf"))) {
			saved = plot->savePdf(fileName);
		} else {
			saved = saveColorMapCSV(colorMap, fileName);
		}
	}
	Q_UNUSED(saved);
}

} // namespace PlotUtils
