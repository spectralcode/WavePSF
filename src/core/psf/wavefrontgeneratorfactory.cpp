#include "wavefrontgeneratorfactory.h"
#include "zernikegenerator.h"
#include "deformablemirror/deformablemirrorgenerator.h"


IWavefrontGenerator* WavefrontGeneratorFactory::create(const QString& typeName, QObject* parent)
{
	if (typeName == QLatin1String("Deformable Mirror")) {
		return new DeformableMirrorGenerator(parent);
	}
	// Default: Zernike
	return new ZernikeGenerator(2, 21, parent);
}

QStringList WavefrontGeneratorFactory::availableTypeNames()
{
	return { QStringLiteral("Zernike"), QStringLiteral("Deformable Mirror") };
}
