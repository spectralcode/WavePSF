// Regression test for large-file and short-read safety of the ENVI loader:
// byte offsets and total sizes were computed in 32-bit int, so realistic
// hyperspectral cubes (e.g. 2048x2048x600 @ 2 bytes = 4.8 GiB) overflowed
// into heap corruption (BIP) or a silently truncated read reported as
// success (BSQ), and any short/truncated file was accepted with an
// uninitialized tail.
// Covered here:
// - EnviLayout arithmetic for the 4.8 GiB cube, without allocating it.
// - Round trip of tiny BSQ/BIL/BIP cubes through the public loadFile()
//   (compatibility protection for the loader rewrite; passes before it).
// - Truncated files must be rejected (red before the fix: short reads
//   were reported as success).
#include "data/envilayout.h"
#include "data/inputdatareader.h"
#include "data/imagedata.h"
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QScopedPointer>
#include <QString>
#include <QTemporaryDir>
#include <cstdio>
#include <limits>

namespace {

const int cubeWidth = 2;
const int cubeHeight = 2;
const int cubeBands = 3;

quint16 sampleValue(int row, int col, int band)
{
	return static_cast<quint16>((band + 1) * 100 + row * 10 + col);
}

// Little-endian 16-bit samples in the given interleave order.
QByteArray cubeBytes(const QString& interleave)
{
	QByteArray bytes;
	auto appendSample = [&bytes](quint16 value) {
		bytes.append(static_cast<char>(value & 0xFF));
		bytes.append(static_cast<char>((value >> 8) & 0xFF));
	};
	if (interleave == "bsq") {
		for (int b = 0; b < cubeBands; ++b)
			for (int r = 0; r < cubeHeight; ++r)
				for (int c = 0; c < cubeWidth; ++c)
					appendSample(sampleValue(r, c, b));
	} else if (interleave == "bil") {
		for (int r = 0; r < cubeHeight; ++r)
			for (int b = 0; b < cubeBands; ++b)
				for (int c = 0; c < cubeWidth; ++c)
					appendSample(sampleValue(r, c, b));
	} else { // bip
		for (int r = 0; r < cubeHeight; ++r)
			for (int c = 0; c < cubeWidth; ++c)
				for (int b = 0; b < cubeBands; ++b)
					appendSample(sampleValue(r, c, b));
	}
	return bytes;
}

QByteArray headerText(const QString& interleave)
{
	return QByteArray("ENVI\n"
		"samples = 2\n"
		"lines = 2\n"
		"bands = 3\n"
		"header offset = 0\n"
		"data type = 12\n"
		"interleave = ") + interleave.toLatin1() + QByteArray("\n"
		"byte order = 0\n");
}

bool writeFile(const QString& path, const QByteArray& content)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	return file.write(content) == content.size();
}

bool testLayoutArithmetic()
{
	// The canonical overflowing cube: 2048 x 2048 x 600 @ 2 bytes.
	EnviLayout layout;
	if (!EnviLayout::calculate(2048, 2048, 600, 2, layout)) {
		std::fprintf(stderr, "FAIL [layout]: 4.8 GiB cube rejected, expected valid\n");
		return false;
	}
	if (layout.totalBytes != 5033164800ULL) {
		std::fprintf(stderr, "FAIL [layout]: totalBytes=%llu, expected 5033164800\n",
			static_cast<unsigned long long>(layout.totalBytes));
		return false;
	}
	size_t lastOffset = layout.bipDestinationOffset(layout.samplesPerFrame - 1, 599);
	if (lastOffset != layout.totalBytes - 2) {
		std::fprintf(stderr, "FAIL [layout]: last BIP offset=%llu, expected totalBytes-2=%llu\n",
			static_cast<unsigned long long>(lastOffset),
			static_cast<unsigned long long>(layout.totalBytes - 2));
		return false;
	}
	if (lastOffset <= static_cast<size_t>((std::numeric_limits<int>::max)())) {
		std::fprintf(stderr, "FAIL [layout]: last BIP offset does not exceed INT_MAX; "
			"test would not guard the 32-bit regression\n");
		return false;
	}

	EnviLayout rejected;
	const int intMax = (std::numeric_limits<int>::max)();
	if (EnviLayout::calculate(intMax, intMax, intMax, 8, rejected)) {
		std::fprintf(stderr, "FAIL [layout]: overflowing dimensions accepted\n");
		return false;
	}
	// 2^20 cubed at 8 bytes = 2^63 bytes: passes size_t but must hit the qint64 cap.
	if (EnviLayout::calculate(1048576, 1048576, 1048576, 8, rejected)) {
		std::fprintf(stderr, "FAIL [layout]: total above INT64_MAX accepted\n");
		return false;
	}
	if (EnviLayout::calculate(0, 2, 3, 2, rejected)
			|| EnviLayout::calculate(2, -1, 3, 2, rejected)
			|| EnviLayout::calculate(2, 2, 3, 0, rejected)) {
		std::fprintf(stderr, "FAIL [layout]: degenerate dimensions accepted\n");
		return false;
	}
	std::printf("layout arithmetic ok\n");
	return true;
}

bool testInterleaveRoundTrip(InputDataReader& reader, const QString& dirPath, const QString& interleave)
{
	const QString base = QDir(dirPath).filePath(interleave);
	if (!writeFile(base + ".hdr", headerText(interleave))
			|| !writeFile(base + ".raw", cubeBytes(interleave))) {
		std::fprintf(stderr, "FAIL [%s]: could not write fixtures\n", qPrintable(interleave));
		return false;
	}

	ImageData* rawData = nullptr;
	OperationResult result = reader.loadFile(base + ".raw", rawData);
	QScopedPointer<ImageData> data(rawData);
	if (!result.ok || data.isNull()) {
		std::fprintf(stderr, "FAIL [%s]: load failed: %s\n",
			qPrintable(interleave), qPrintable(result.message));
		return false;
	}
	if (data->getWidth() != cubeWidth || data->getHeight() != cubeHeight
			|| data->getFrames() != cubeBands) {
		std::fprintf(stderr, "FAIL [%s]: dimensions %dx%dx%d, expected %dx%dx%d\n",
			qPrintable(interleave), data->getWidth(), data->getHeight(), data->getFrames(),
			cubeWidth, cubeHeight, cubeBands);
		return false;
	}
	for (int b = 0; b < cubeBands; ++b) {
		const quint16* frame = static_cast<const quint16*>(data->getData(b));
		if (frame == nullptr) {
			std::fprintf(stderr, "FAIL [%s]: frame %d is null\n", qPrintable(interleave), b);
			return false;
		}
		for (int r = 0; r < cubeHeight; ++r) {
			for (int c = 0; c < cubeWidth; ++c) {
				quint16 expected = sampleValue(r, c, b);
				quint16 actual = frame[r * cubeWidth + c];
				if (actual != expected) {
					std::fprintf(stderr,
						"FAIL [%s]: frame %d row %d col %d = %u, expected %u\n",
						qPrintable(interleave), b, r, c, actual, expected);
					return false;
				}
			}
		}
	}
	std::printf("%s round trip ok\n", qPrintable(interleave));
	return true;
}

bool testTruncatedRejected(InputDataReader& reader, const QString& dirPath, const QString& interleave)
{
	const QString base = QDir(dirPath).filePath("truncated_" + interleave);
	QByteArray bytes = cubeBytes(interleave);
	bytes.chop(1);
	if (!writeFile(base + ".hdr", headerText(interleave))
			|| !writeFile(base + ".raw", bytes)) {
		std::fprintf(stderr, "FAIL [truncated %s]: could not write fixtures\n", qPrintable(interleave));
		return false;
	}

	ImageData* rawData = nullptr;
	OperationResult result = reader.loadFile(base + ".raw", rawData);
	QScopedPointer<ImageData> data(rawData);
	if (result.ok || !data.isNull()) {
		std::fprintf(stderr,
			"FAIL [truncated %s]: truncated file accepted (ok=%d, data=%s)\n",
			qPrintable(interleave), result.ok ? 1 : 0, data.isNull() ? "null" : "non-null");
		return false;
	}
	std::printf("truncated %s rejected ok\n", qPrintable(interleave));
	return true;
}

} // namespace

int main()
{
	QTemporaryDir tempDir;
	if (!tempDir.isValid()) {
		std::fprintf(stderr, "FAIL: could not create temporary directory\n");
		return 1;
	}

	InputDataReader reader;
	const QStringList interleaves = {"bsq", "bil", "bip"};

	bool ok = testLayoutArithmetic();
	for (const QString& interleave : interleaves) {
		ok = testInterleaveRoundTrip(reader, tempDir.path(), interleave) && ok;
	}
	for (const QString& interleave : interleaves) {
		ok = testTruncatedRejected(reader, tempDir.path(), interleave) && ok;
	}

	if (!ok) {
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}
