#ifndef COEFFICIENTWORKSPACE_H
#define COEFFICIENTWORKSPACE_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QString>

class PSFModule;
class ImageSession;
class WavefrontParameterTable;

class CoefficientWorkspace : public QObject
{
	Q_OBJECT

public:
	explicit CoefficientWorkspace(PSFModule* psfModule, ImageSession* imageSession, QObject* parent = nullptr);

	// Core operations
	void store();
	void loadForCurrentPatch();
	int coefficientFrame() const;
	void resize();
	void clearAndResize();

	// Clipboard
	void copy();
	void paste();
	void undoPaste();

	// Reset
	void resetAll();

	// File I/O
	void saveToFile(const QString& filePath);
	bool loadFromFile(const QString& filePath);

	// Generator-switch cache management
	void cacheCurrentTable(const QString& typeName);
	void restoreOrCreateTable(const QString& typeName);
	void clearCache();

	// Table accessor (for external code: BatchProcessor, PSFGridGenerator, etc.)
	WavefrontParameterTable* table() const;

signals:
	void coefficientsLoaded(QVector<double> coefficients);
	void parametersLoaded();

private:
	PSFModule* psfModule;
	ImageSession* imageSession;

	WavefrontParameterTable* currentTable;
	QMap<QString, WavefrontParameterTable*> cachedTables;

	QVector<double> clipboard;
	QVector<double> undoCoefficients;
	int undoFrame;
	int undoPatchX;
	int undoPatchY;
};

#endif // COEFFICIENTWORKSPACE_H
