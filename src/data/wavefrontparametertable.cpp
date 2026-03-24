#include "wavefrontparametertable.h"
#include "utils/logging.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>

WavefrontParameterTable::WavefrontParameterTable(QObject* parent)
	: QObject(parent)
	, numberOfFrames(0)
	, numberOfPatchesInX(0)
	, numberOfPatchesInY(0)
	, patchesPerFrame(0)
	, coefficientsPerPatch(0)
{
}

void WavefrontParameterTable::resize(int frames, int patchesInX, int patchesInY, int coeffsPerPatch)
{
	if (frames < 0 || patchesInX < 0 || patchesInY < 0 || coeffsPerPatch < 0) {
		return;
	}

	int newPatchesPerFrame = patchesInX * patchesInY;

	this->table.resize(frames);
	for (int f = 0; f < frames; f++) {
		int oldPatchCount = this->table[f].size();
		this->table[f].resize(newPatchesPerFrame);
		for (int p = 0; p < newPatchesPerFrame; p++) {
			int oldCoeffCount = this->table[f][p].size();
			if (p >= oldPatchCount || oldCoeffCount == 0) {
				// New patch: zero-fill entirely
				this->table[f][p].fill(0.0, coeffsPerPatch);
			} else if (oldCoeffCount != coeffsPerPatch) {
				// Existing patch with different coeff count: preserve for switching back
				this->table[f][p].resize(coeffsPerPatch);
				for (int c = oldCoeffCount; c < coeffsPerPatch; c++) {
					this->table[f][p][c] = 0.0;
				}
			}
		}
	}

	this->numberOfFrames = frames;
	this->numberOfPatchesInX = patchesInX;
	this->numberOfPatchesInY = patchesInY;
	this->patchesPerFrame = newPatchesPerFrame;
	this->coefficientsPerPatch = coeffsPerPatch;
}

void WavefrontParameterTable::clear()
{
	this->table.clear();
	this->numberOfFrames = 0;
	this->numberOfPatchesInX = 0;
	this->numberOfPatchesInY = 0;
	this->patchesPerFrame = 0;
	this->coefficientsPerPatch = 0;
}

void WavefrontParameterTable::resetAllCoefficients()
{
	for (int f = 0; f < this->numberOfFrames; f++) {
		for (int p = 0; p < this->patchesPerFrame; p++) {
			this->table[f][p].fill(0.0);
		}
	}
}

void WavefrontParameterTable::setCoefficient(int frame, int patch, int index, double value)
{
	if (frame >= 0 && frame < this->numberOfFrames
		&& patch >= 0 && patch < this->patchesPerFrame
		&& index >= 0 && index < this->coefficientsPerPatch) {
		this->table[frame][patch][index] = value;
	}
}

double WavefrontParameterTable::getCoefficient(int frame, int patch, int index) const
{
	if (frame >= 0 && frame < this->numberOfFrames
		&& patch >= 0 && patch < this->patchesPerFrame
		&& index >= 0 && index < this->coefficientsPerPatch) {
		return this->table[frame][patch][index];
	}
	return 0.0;
}

void WavefrontParameterTable::setCoefficients(int frame, int patch, const QVector<double>& coeffs)
{
	if (frame >= 0 && frame < this->numberOfFrames
		&& patch >= 0 && patch < this->patchesPerFrame) {
		int count = qMin(coeffs.size(), this->coefficientsPerPatch);
		for (int i = 0; i < count; i++) {
			this->table[frame][patch][i] = coeffs.at(i);
		}
	}
}

QVector<double> WavefrontParameterTable::getCoefficients(int frame, int patch) const
{
	if (frame >= 0 && frame < this->numberOfFrames
		&& patch >= 0 && patch < this->patchesPerFrame) {
		return this->table[frame][patch];
	}
	return QVector<double>();
}

int WavefrontParameterTable::patchIndex(int x, int y) const
{
	return y * this->numberOfPatchesInX + x;
}

bool WavefrontParameterTable::saveToFile(const QString& filePath) const
{
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		LOG_WARNING() << "Failed to open file for writing:" << filePath;
		return false;
	}

	QTextStream stream(&file);

	// Header
	stream << "Date" << ";" << QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz") << "\n";
	stream << "Number of frames" << ";" << QString::number(this->numberOfFrames) << "\n";
	stream << "Patches per frame" << ";" << QString::number(this->patchesPerFrame) << ";";
	stream << "Patches in X" << ";" << QString::number(this->numberOfPatchesInX) << ";";
	stream << "Patches in Y" << ";" << QString::number(this->numberOfPatchesInY) << "\n";
	stream << "Coefficients per patch" << ";" << QString::number(this->coefficientsPerPatch) << "\n";
	stream << "\n";
	stream << "Frame Nr." << ";" << "Patch Nr." << ";" << "Coeff Index" << ";" << "Value" << "\n";

	// Data
	for (int i = 0; i < this->numberOfFrames; i++) {
		for (int j = 0; j < this->patchesPerFrame; j++) {
			for (int k = 0; k < this->coefficientsPerPatch; k++) {
				double coeff = this->table.at(i).at(j).at(k);
				stream << QString::number(i) << ";"
					   << QString::number(j) << ";"
					   << QString::number(k) << ";"
					   << QString::number(coeff, 'f', 12) << "\n";
			}
		}
	}

	stream.flush();
	file.close();
	LOG_INFO() << "Parameters saved to:" << filePath;
	return true;
}

bool WavefrontParameterTable::loadFromFile(const QString& filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		LOG_WARNING() << "Failed to open file for reading:" << filePath;
		return false;
	}

	QTextStream stream(&file);

	// Parse header
	int frames = 0;
	int patches = 0;
	int patchesX = 0;
	int patchesY = 0;
	int coeffs = 0;

	// Line 1: Date
	stream.readLine();
	// Line 2: Number of frames
	QString line = stream.readLine().trimmed();
	frames = line.section(";", 1, 1).trimmed().toInt();
	// Line 3: Patches per frame;N;Patches in X;NX;Patches in Y;NY
	line = stream.readLine().trimmed();
	patches = line.section(";", 1, 1).trimmed().toInt();
	patchesX = line.section(";", 3, 3).trimmed().toInt();
	patchesY = line.section(";", 5, 5).trimmed().toInt();
	// Line 4: Coefficients per patch
	line = stream.readLine().trimmed();
	coeffs = line.section(";", 1, 1).trimmed().toInt();
	// Line 5: blank
	stream.readLine();
	// Line 6: column headers
	stream.readLine();

	if (frames <= 0 || patches <= 0 || coeffs <= 0) {
		LOG_WARNING() << "Invalid parameter file header:" << filePath;
		file.close();
		return false;
	}

	// Resize table
	this->resize(frames, patchesX, patchesY, coeffs);

	// Parse data rows
	while (!stream.atEnd()) {
		line = stream.readLine().trimmed();
		if (line.isEmpty()) continue;

		int frameNr = line.section(";", 0, 0).toInt();
		int patchNr = line.section(";", 1, 1).toInt();
		int coeffIdx = line.section(";", 2, 2).toInt();
		double value = line.section(";", 3, 3).toDouble();

		this->setCoefficient(frameNr, patchNr, coeffIdx, value);
	}

	file.close();
	LOG_INFO() << "Parameters loaded from:" << filePath
			   << "(" << frames << " frames," << patches << " patches," << coeffs << " coefficients)";
	return true;
}

int WavefrontParameterTable::getNumberOfFrames() const
{
	return this->numberOfFrames;
}

int WavefrontParameterTable::getNumberOfPatchesInX() const
{
	return this->numberOfPatchesInX;
}

int WavefrontParameterTable::getNumberOfPatchesInY() const
{
	return this->numberOfPatchesInY;
}

int WavefrontParameterTable::getPatchesPerFrame() const
{
	return this->patchesPerFrame;
}

int WavefrontParameterTable::getCoefficientsPerPatch() const
{
	return this->coefficientsPerPatch;
}
