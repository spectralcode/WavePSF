#ifndef OPTIMIZATIONJOBBUILDER_H
#define OPTIMIZATIONJOBBUILDER_H

#include "optimizationworker.h"

class ImageSession;
class PSFModule;
class WavefrontParameterTable;

class OptimizationJobBuilder
{
public:
	// Builds optimization jobs from UI config + session data.
	// Populates config.jobs. Returns true if at least one valid job was built.
	static bool buildJobs(
		OptimizationConfig& config,
		ImageSession* imageSession,
		PSFModule* psfModule,
		WavefrontParameterTable* parameterTable,
		int currentFrame,
		int currentPatchX,
		int currentPatchY);
};

#endif // OPTIMIZATIONJOBBUILDER_H
