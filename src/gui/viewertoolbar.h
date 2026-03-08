#ifndef VIEWERTOOLBAR_H
#define VIEWERTOOLBAR_H

#include <QToolBar>
#include <QAction>
#include <QVariantMap>
#include "stylemanager.h"

class ViewerToolBar : public QToolBar
{
	Q_OBJECT

public:
	explicit ViewerToolBar(StyleManager* styleManager, QWidget* parent = nullptr);

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& settings);

signals:
	void rotateRequested();
	void flipHRequested();
	void flipVRequested();
	void syncViewsToggled(bool enabled);
	void showPatchGridToggled(bool visible);
	void showAxisToggled(bool visible);

private:
	QAction* actionRotate90;
	QAction* actionFlipH;
	QAction* actionFlipV;
	QAction* actionSyncViews;
	QAction* actionShowPatchGrid;
	QAction* actionShowAxis;

	void updateIcons();
	static QIcon svgIconColored(const QString& path, const QColor& color, int size = 24);
};

#endif // VIEWERTOOLBAR_H
