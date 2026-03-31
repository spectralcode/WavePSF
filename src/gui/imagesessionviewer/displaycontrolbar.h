#ifndef DISPLAYCONTROLBAR_H
#define DISPLAYCONTROLBAR_H

#include <QWidget>
#include "displaysettings.h"

class QComboBox;
class QCheckBox;
class RangeSlider;

class DisplayControlBar : public QWidget
{
	Q_OBJECT
public:
	explicit DisplayControlBar(QWidget* parent = nullptr);

	DisplaySettings getSettings() const;
	void setSettings(const DisplaySettings& settings);
	void setSliderRange(double min, double max);
	void setInputFrameRange(double min, double max);
	void setOutputFrameRange(double min, double max);
	void setIntegerMode(bool intMode);

signals:
	void settingsChanged(const DisplaySettings& settings);
	void resetToInputStackRequested();
	void resetToOutputStackRequested();

protected:
	void contextMenuEvent(QContextMenuEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	void emitSettings();
	void updateManualControlsEnabled();
	void populateLutCombo();

	QComboBox* lutCombo;
	RangeSlider* rangeSlider;
	QComboBox* autoRangeCombo;
	QCheckBox* logCheck;
	bool blockEmit;
	double lastInputFrameMin;
	double lastInputFrameMax;
	double lastOutputFrameMin;
	double lastOutputFrameMax;
};

#endif // DISPLAYCONTROLBAR_H
