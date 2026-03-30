#ifndef LUT_H
#define LUT_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QPixmap>
#include <QRgb>

namespace LUT {
	QVector<QRgb> get(const QString& name);
	QStringList availableNames();
	QPixmap getPreviewPixmap(const QString& name, int width, int height);
}

#endif // LUT_H
