#ifndef TABLEINTERPOLATOR_H
#define TABLEINTERPOLATOR_H

#include <QObject>
#include <QVector>
#include <QString>

class WavefrontParameterTable;

struct InterpolationSlice {
	int coefficientIndex;
	QVector<double> inputPositions;
	QVector<double> inputValues;
};

struct InterpolationResult {
	QVector<InterpolationSlice> slices;
	QVector<double> allPositions;
	QVector<QVector<double>> allValues;
	int totalCoefficients;
	QString axisLabel;
};

class TableInterpolator : public QObject
{
	Q_OBJECT
public:
	explicit TableInterpolator(QObject* parent = nullptr);

	void setPolynomialOrder(int order);
	int polynomialOrder() const;

	QVector<InterpolationSlice> interpolateInX(WavefrontParameterTable* table, int frame, int patchX, int patchY);
	QVector<InterpolationSlice> interpolateInY(WavefrontParameterTable* table, int frame, int patchX, int patchY);
	QVector<InterpolationSlice> interpolateInZ(WavefrontParameterTable* table, int patchX, int patchY);
	void interpolateAllInZ(WavefrontParameterTable* table);

private:
	int order;

	static QVector<double> polyFit(const QVector<double>& positions,
								   const QVector<double>& values, int order);
	static double polyEval(const QVector<double>& coeffs, double x);
};

Q_DECLARE_METATYPE(InterpolationResult)

#endif // TABLEINTERPOLATOR_H
