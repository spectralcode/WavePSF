#ifndef PSFFILEINFO_H
#define PSFFILEINFO_H

#include <QString>
#include <QMetaType>

struct PSFFileInfo {
	bool valid = false;
	// Source
	bool isFolder = false;
	int fileCount = 0;
	QString fileFormat;       // "TIFF", "PNG", "JPEG", "BMP", "Mixed"
	// Dimensions (representative — from first patch for multi-patch)
	int width = 0;
	int height = 0;
	int depth = 1;            // z-slices per volume
	// Classification
	int patchCount = 0;
	bool volumetric = false;  // derived from depth > 1
	// Data properties (aggregated across ALL patches/volumes)
	int bitDepth = 0;         // 8, 16, or 32; 0 = mixed/unknown
	double minValue = 0.0;
	double maxValue = 0.0;
	double sum = 0.0;         // for normalization check
};

Q_DECLARE_METATYPE(PSFFileInfo)

#endif // PSFFILEINFO_H
