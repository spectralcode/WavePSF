#include "lut.h"
#include <QImage>
#include <QPainter>
#include <QMap>

namespace {

QVector<QRgb> loadFromResource(const QString& name)
{
	const QString path = QStringLiteral(":/colormaps/%1.png").arg(name.toLower());
	QImage img(path);
	if (img.isNull() || img.width() < 256) {
		return QVector<QRgb>();
	}
	QVector<QRgb> table(256);
	for (int i = 0; i < 256; ++i) {
		table[i] = img.pixel(i, 0);
	}
	return table;
}

// Thread-safe lazy cache
QMap<QString, QVector<QRgb>>& cache()
{
	static QMap<QString, QVector<QRgb>> s_cache;
	return s_cache;
}

} // anonymous namespace


QVector<QRgb> LUT::get(const QString& name)
{
	auto& c = cache();
	auto it = c.find(name);
	if (it != c.end()) {
		return it.value();
	}

	QVector<QRgb> table = loadFromResource(name);
	if (table.isEmpty()) {
		// Fallback grayscale
		table.resize(256);
		for (int i = 0; i < 256; ++i) {
			table[i] = qRgb(i, i, i);
		}
	}
	c.insert(name, table);
	return table;
}

QStringList LUT::availableNames()
{
	return { QStringLiteral("Grayscale"), QStringLiteral("Hot"),
			 QStringLiteral("Viridis"), QStringLiteral("Inferno") };
}

QPixmap LUT::getPreviewPixmap(const QString& name, int width, int height)
{
	const QVector<QRgb> table = get(name);
	QImage strip(256, 1, QImage::Format_RGB32);
	for (int i = 0; i < 256; ++i) {
		strip.setPixel(i, 0, table.value(i, qRgb(i, i, i)));
	}
	return QPixmap::fromImage(strip.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
