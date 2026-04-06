#include "coefficientworkspace.h"
#include "core/psf/psfmodule.h"
#include "core/psf/ipsfgenerator.h"
#include "controller/imagesession.h"
#include "data/wavefrontparametertable.h"

CoefficientWorkspace::CoefficientWorkspace(PSFModule* psfModule, ImageSession* imageSession, QObject* parent)
	: QObject(parent)
	, psfModule(psfModule)
	, imageSession(imageSession)
	, currentTable(new WavefrontParameterTable(this))
	, undoFrame(0)
	, undoPatchX(0)
	, undoPatchY(0)
{
}

int CoefficientWorkspace::coefficientFrame() const
{
	// In 3D generator mode, coefficients are frame-independent (shared wavefront)
	if (this->psfModule != nullptr && this->psfModule->getGenerator()->is3D()) {
		return 0;
	}
	return this->imageSession != nullptr ? this->imageSession->getCurrentFrame() : 0;
}

void CoefficientWorkspace::store()
{
	if (this->currentTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->imageSession->hasInputData()) {
		return;
	}
	int frame = this->coefficientFrame();
	int patchIdx = this->currentTable->patchIndex(this->imageSession->getCurrentPatch().x(), this->imageSession->getCurrentPatch().y());
	this->currentTable->setCoefficients(frame, patchIdx, this->psfModule->getAllCoefficients());
}

void CoefficientWorkspace::loadForCurrentPatch()
{
	if (this->currentTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->imageSession->hasInputData()) {
		return;
	}

	int frame = this->coefficientFrame();
	int patchIdx = this->currentTable->patchIndex(this->imageSession->getCurrentPatch().x(), this->imageSession->getCurrentPatch().y());
	this->psfModule->setCurrentPatch(frame, patchIdx);

	if (this->psfModule->getGenerator()->supportsCoefficients()) {
		QVector<double> coeffs = this->currentTable->getCoefficients(frame, patchIdx);
		if (!coeffs.isEmpty()) {
			this->psfModule->setAllCoefficients(coeffs);
			emit coefficientsLoaded(coeffs);
		}
	} else {
		this->psfModule->refreshPSF();
	}
}

void CoefficientWorkspace::resize()
{
	if (this->currentTable == nullptr || this->psfModule == nullptr || this->imageSession == nullptr) {
		return;
	}
	if (!this->imageSession->hasInputData()) {
		return;
	}
	int frames = this->imageSession->getInputFrames();
	int cols = this->imageSession->getPatchGridCols();
	int rows = this->imageSession->getPatchGridRows();
	int coeffs = this->psfModule->getAllCoefficients().size();

	// Only resize (and zero-fill) when dimensions actually changed
	if (frames != this->currentTable->getNumberOfFrames()
		|| cols != this->currentTable->getNumberOfPatchesInX()
		|| rows != this->currentTable->getNumberOfPatchesInY()
		|| coeffs != this->currentTable->getCoefficientsPerPatch()) {
		this->currentTable->resize(frames, cols, rows, coeffs);
	}
}

void CoefficientWorkspace::clearAndResize()
{
	if (this->currentTable == nullptr) return;
	this->currentTable->clear();
	this->resize();
}

void CoefficientWorkspace::copy()
{
	this->store();
	if (this->psfModule != nullptr) {
		this->clipboard = this->psfModule->getAllCoefficients();
	}
}

void CoefficientWorkspace::paste()
{
	if (this->clipboard.isEmpty() || this->psfModule == nullptr) return;

	// Save undo state before overwriting
	this->undoCoefficients = this->psfModule->getAllCoefficients();
	this->undoFrame = this->imageSession != nullptr ? this->imageSession->getCurrentFrame() : 0;
	this->undoPatchX = this->imageSession != nullptr ? this->imageSession->getCurrentPatch().x() : 0;
	this->undoPatchY = this->imageSession != nullptr ? this->imageSession->getCurrentPatch().y() : 0;

	this->psfModule->setAllCoefficients(this->clipboard);
	this->store();
	emit coefficientsLoaded(this->clipboard);
}

void CoefficientWorkspace::undoPaste()
{
	if (this->undoCoefficients.isEmpty() || this->psfModule == nullptr || this->currentTable == nullptr) return;

	// Write undo coefficients back to the parameter table
	int patchIdx = this->currentTable->patchIndex(this->undoPatchX, this->undoPatchY);
	this->currentTable->setCoefficients(this->undoFrame, patchIdx, this->undoCoefficients);

	// If we're still on the same patch, update sliders + PSF
	int currentFrame = this->imageSession != nullptr ? this->imageSession->getCurrentFrame() : -1;
	int currentPatchX = this->imageSession != nullptr ? this->imageSession->getCurrentPatch().x() : -1;
	int currentPatchY = this->imageSession != nullptr ? this->imageSession->getCurrentPatch().y() : -1;

	if (currentFrame == this->undoFrame &&
	    currentPatchX == this->undoPatchX &&
	    currentPatchY == this->undoPatchY) {
		this->psfModule->setAllCoefficients(this->undoCoefficients);
		emit coefficientsLoaded(this->undoCoefficients);
	}

	this->undoCoefficients.clear();
}

void CoefficientWorkspace::resetAll()
{
	if (this->currentTable == nullptr) return;
	this->currentTable->resetAllCoefficients();
	this->loadForCurrentPatch();
}

void CoefficientWorkspace::saveToFile(const QString& filePath)
{
	this->store();
	if (this->currentTable != nullptr) {
		this->currentTable->saveToFile(filePath);
	}
}

bool CoefficientWorkspace::loadFromFile(const QString& filePath)
{
	if (this->currentTable == nullptr) return false;
	if (this->currentTable->loadFromFile(filePath)) {
		this->loadForCurrentPatch();
		emit parametersLoaded();
		return true;
	}
	return false;
}

void CoefficientWorkspace::cacheCurrentTable(const QString& typeName)
{
	if (this->currentTable == nullptr) return;
	this->cachedTables[typeName] = this->currentTable;
	this->currentTable = nullptr;
}

void CoefficientWorkspace::restoreOrCreateTable(const QString& typeName)
{
	if (this->cachedTables.contains(typeName)) {
		this->currentTable = this->cachedTables.take(typeName);
	} else {
		this->currentTable = new WavefrontParameterTable(this);
		this->resize();
	}
}

void CoefficientWorkspace::clearCache()
{
	qDeleteAll(this->cachedTables);
	this->cachedTables.clear();
}

WavefrontParameterTable* CoefficientWorkspace::table() const
{
	return this->currentTable;
}
