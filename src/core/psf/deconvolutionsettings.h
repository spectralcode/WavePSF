#ifndef DECONVOLUTIONSETTINGS_H
#define DECONVOLUTIONSETTINGS_H

struct DeconvolutionSettings
{
	int algorithm = 0;
	int iterations = 128;
	float relaxationFactor = 0.65f;
	float tikhonovRegularizationFactor = 0.005f;
	float wienerNoiseToSignalFactor = 0.01f;
	int volumePaddingMode = 1;
	int accelerationMode = 1;
	int regularizer3D = 0;
	float regularizationWeight = 0.0001f;
	float voxelSizeY = 1.0f;
	float voxelSizeX = 1.0f;
	float voxelSizeZ = 1.0f;
};

#endif // DECONVOLUTIONSETTINGS_H
