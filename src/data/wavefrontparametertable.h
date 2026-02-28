#ifndef WAVEFRONTPARAMETERTABLE_H
#define WAVEFRONTPARAMETERTABLE_H

#include <QObject>
#include <QVector>
#include <QString>

class WavefrontParameterTable : public QObject
{
	Q_OBJECT
public:
	explicit WavefrontParameterTable(QObject* parent = nullptr);

	// Resizing
	void resize(int frames, int patchesInX, int patchesInY, int coeffsPerPatch);
	void clear();

	// Single coefficient access
	void setCoefficient(int frame, int patch, int index, double value);
	double getCoefficient(int frame, int patch, int index) const;

	// Bulk access (full patch)
	void setCoefficients(int frame, int patch, const QVector<double>& coeffs);
	QVector<double> getCoefficients(int frame, int patch) const;

	// Patch index helper (row-major: y * patchesInX + x)
	int patchIndex(int x, int y) const;

	// File I/O
	bool saveToFile(const QString& filePath) const;
	bool loadFromFile(const QString& filePath);

	// Dimension getters
	int getNumberOfFrames() const;
	int getNumberOfPatchesInX() const;
	int getNumberOfPatchesInY() const;
	int getPatchesPerFrame() const;
	int getCoefficientsPerPatch() const;

private:
	QVector<QVector<QVector<double>>> table; // table[frame][patch][coeffIndex]
	int numberOfFrames;
	int numberOfPatchesInX;
	int numberOfPatchesInY;
	int patchesPerFrame;
	int coefficientsPerPatch;
};

#endif // WAVEFRONTPARAMETERTABLE_H
