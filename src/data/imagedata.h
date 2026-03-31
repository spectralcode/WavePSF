#ifndef IMAGEDATA_H
#define IMAGEDATA_H

#include <QObject>
#include <QVector>
#include <QPair>
#include <QString>
#include <QImage>

enum EnviDataType { //todo: rename to PixelDataType
	UNSIGNED_CHAR_8BIT = 1,
	SIGNED_SHORT_16BIT = 2,
	SIGNED_INT_32BIT = 3,
	FLOAT_32BIT = 4,
	DOUBLE_64BIT = 5,
	COMPLEX_FLOAT_2X32BIT = 6,
	COMPLEX_DOUBLE_2X64BIT = 9,
	UNSIGNED_SHORT_16BIT = 12,
	UNSIGNED_INT_32BIT = 13,
	SIGNED_LONG_INT_64BIT = 14,
	UNSIGNED_LONG_INT_64BIT = 15
};

class ImageData : public QObject
{
	Q_OBJECT

public:
	// Constructor for HSI data
	explicit ImageData(void* data, int width, int height, int frames, EnviDataType dataType,
					   const QVector<qreal>& wavelengths, const QString& wavelengthUnit, QObject* parent = nullptr);

	// Constructor for RGB and grayscale data
	explicit ImageData(const QImage& image, QObject* parent = nullptr);

	// Copy constructor
	ImageData(const ImageData& other);

	// Destructor
	~ImageData();

	// Basic properties
	int getWidth() const;
	int getHeight() const;
	int getFrames() const;
	EnviDataType getDataType() const;
	size_t getBytesPerSample() const;

	// Data access
	void* getData() const;
	void* getData(int frameNr) const;

	// Frame writing
	void writeSingleFrame(int frameNr, void* data);
	void writeSingleSubFrame(int frameNr, int x, int y, int width, int height, void* data);

	// Metadata
	QVector<qreal> getWavelengths() const;
	QString getWavelengthUnit() const;
	QStringList getFrameNames() const;

	// Utility
	QPair<double,double> getGlobalRange() const;
	void saveDataToDisk(const QString& filePath);
	void saveAsEnvi(const QString& filePath);
	void saveAsTiff(const QString& filePath);
	void saveFrameAsImage(const QString& filePath, int frameNr);

private:
	int width;
	int height;
	int frames;
	EnviDataType dataType;  // Single data type - no need for conceptual vs ENVI distinction

	void* data;
	QVector<qreal> wavelengths;
	QString wavelengthUnit;
	QStringList frameNames;

	void convertRGBToFrames(const QImage& rgbImage);
	void convertGrayscaleToFrame(const QImage& grayscaleImage);
	void initializeRGBMetadata();
	void initializeGrayscaleMetadata();
	void allocateDataBuffer(size_t totalBytes);
	size_t calculateBytesPerSample() const;
	bool isGrayscaleImage(const QImage& image) const;

	mutable bool globalRangeDirty;
	mutable double globalRangeMin;
	mutable double globalRangeMax;

signals:
	void dataChanged();
};

#endif // IMAGEDATA_H
