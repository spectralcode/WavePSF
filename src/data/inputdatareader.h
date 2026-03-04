#ifndef INPUTDATAREADER_H
#define INPUTDATAREADER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <memory>
#include <arrayfire.h>
#include "imagedata.h"

// ENVI field names
//todo: make a envi class to read write and store all these properties
#define ACQUISITION_TIME "acquisition time"
#define BAND_NAMES "band names"
#define BANDS "bands"
#define BBL "bbl"
#define BYTE_ORDER "byte order"
#define CLASS_LOOKUP "class lookup"
#define CLASS_NAMES "class names"
#define CLASSES "classes"
#define CLOUD_COVER "cloud cover"
#define COMPLEX_FUNCTION "complex function"
#define COORDINATE_SYSTEM_STRING "coordinate system string"
#define DATA_GAIN_VALUES "data gain values"
#define DATA_IGNORE_VALUE "data ignore value"
#define DATA_OFFSET_VALUES "data offset values"
#define DATA_REFLECTANCE_GAIN_VALUES "data reflectance gain values"
#define DATA_REFLECTANCE_OFFSET_VALUES "data reflectance offset values"
#define DATA_TYPE "data type"
#define DEFAULT_BANDS "default bands"
#define DEFAULT_STRETCH "default stretch"
#define DEM_BAND "dem band"
#define DEM_FILE "dem file"
#define DESCRIPTION "description"
#define FILE_TYPE "file type"
#define FWHM "fwhm"
#define GEO_POINTS "geo points"
#define HEADER_OFFSET "header offset"
#define INTERLEAVE "interleave"
#define LINES "lines"
#define MAP_INFO "map info"
#define PIXEL_SIZE "pixel size"
#define PROJECTION_INFO "projection info"
#define READ_PROCEDURES "read procedures"
#define REFLECTANCE_SCALE_FACTOR "reflectance scale factor"
#define RPC_INFO "rpc info"
#define SAMPLES "samples"
#define SECURITY_TAG "security tag"
#define SENSOR_TYPE "sensor type"
#define SOLAR_IRRADIANCE "solar irradiance"
#define SPECTRA_NAMES "spectra names"
#define SUN_AZIMUTH "sun azimuth"
#define SUN_ELEVATION "sun elevation"
#define WAVELENGTH "wavelength"
#define WAVELENGTH_UNITS "wavelength units"
#define X_START "x start"
#define Y_START "y start"
#define Z_PLOT_AVERAGE "z plot average"
#define Z_PLOT_RANGE "z plot range"
#define Z_PLOT_TITLES "z plot titles"


class InputDataReader : public QObject
{
	Q_OBJECT

public:
	explicit InputDataReader(QObject* parent = nullptr);
	~InputDataReader();

	// Main interface
	ImageData* loadFile(const QString& filePath);

private:
	// ENVI file handling
	ImageData* loadEnviFiles(const QString& hdrFilePath);
	QMap<QString, QString> parseEnviHeader(const QString& hdrFilePath);
	ImageData* loadEnviData(const QString& dataFilePath, const QMap<QString, QString>& enviMap);

	// Standard image handling
	ImageData* loadStandardImage(const QString& imagePath);
#ifdef WAVEPSF_LIBTIFF_BACKEND
	ImageData* loadTiffWithLibtiff(const QString& imagePath);
#endif
	ImageData* loadTiffWithArrayFire(const QString& imagePath);

	// Utility methods
	size_t getBytesPerSample(EnviDataType dataType);
	QString detectDataFile(const QString& hdrFilePath);
	void copyArrayFireDataToBuffer(const af::array& afData, void* buffer, EnviDataType enviType, int width, int height, int frames);

signals:
	void progressChanged(int percentage);
	void fileLoaded(const QString& filePath);
};

#endif // INPUTDATAREADER_H
