#include "psfgeneratorfactory.h"
#include "composedpsfgenerator.h"
#include "zernikegenerator.h"
#include "deformablemirror/deformablemirrorgenerator.h"
#include "psfcalculator.h"
#include "richardswolfcalculator.h"
#include "filepsfgenerator.h"


IPSFGenerator* PSFGeneratorFactory::create(const QString& typeName, QObject* parent)
{
	if (typeName == QLatin1String("Deformable Mirror")) {
		return new ComposedPSFGenerator(
			typeName,
			new DeformableMirrorGenerator(nullptr),
			new PSFCalculator(1.0, 1.0, nullptr),
			parent);
	}
	if (typeName == QLatin1String("3D PSF Microscopy")) {
		return new ComposedPSFGenerator(
			typeName,
			new ZernikeGenerator(nullptr),
			new RichardsWolfCalculator(nullptr),
			parent);
	}
	if (typeName == QLatin1String("From File")) {
		return new FilePSFGenerator(parent);
	}
	// Default: Zernike
	return new ComposedPSFGenerator(
		QStringLiteral("Zernike"),
		new ZernikeGenerator(nullptr),
		new PSFCalculator(1.0, 1.0, nullptr),
		parent);
}

QStringList PSFGeneratorFactory::availableTypeNames()
{
	return { QStringLiteral("Zernike"),
			 QStringLiteral("Deformable Mirror"),
			 QStringLiteral("3D PSF Microscopy"),
			 QStringLiteral("From File") };
}
