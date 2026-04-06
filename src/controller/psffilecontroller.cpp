#include "psffilecontroller.h"
#include "core/psf/psffilemanager.h"
#include "core/psf/filepsfgenerator.h"
#include "core/psf/psfmodule.h"
#include "utils/logging.h"

PSFFileController::PSFFileController(PSFModule* psfModule, QObject* parent)
	: QObject(parent)
	, psfModule(psfModule)
{
	this->psfFileManager = new PSFFileManager(this);
}

void PSFFileController::savePSFToFile(const QString& filePath)
{
	if (this->psfModule == nullptr) return;
	this->psfFileManager->savePSFToFile(filePath, this->psfModule);
}

void PSFFileController::setAutoSavePSF(bool enabled)
{
	this->psfFileManager->setAutoSavePSF(enabled);
}

void PSFFileController::setPSFSaveFolder(const QString& folder)
{
	this->psfFileManager->setPSFSaveFolder(folder);
}

void PSFFileController::autoSaveIfEnabled(int frame, int patchIdx)
{
	if (this->psfModule == nullptr) return;
	this->psfFileManager->autoSaveIfEnabled(frame, patchIdx, this->psfModule);
}

void PSFFileController::setFilePSFSource(const QString& path)
{
	if (this->psfModule == nullptr) return;
	FilePSFGenerator* fileGen = dynamic_cast<FilePSFGenerator*>(this->psfModule->getGenerator());
	if (fileGen == nullptr) return;
	fileGen->setSource(path);
	this->psfModule->refreshPSF();
	this->refreshFileInfo();
}

void PSFFileController::refreshFileInfo()
{
	if (this->psfModule == nullptr) return;
	FilePSFGenerator* fileGen = dynamic_cast<FilePSFGenerator*>(this->psfModule->getGenerator());
	if (fileGen != nullptr) {
		emit filePSFInfoUpdated(fileGen->getFileInfo());
	}
}
