#include "viewertoolbar.h"

#include <QIcon>
#include <QPainter>

namespace {
	const QString SETTINGS_GROUP         = QStringLiteral("viewer_toolbar");
	const QString KEY_SYNC_VIEWS         = QStringLiteral("sync_views");
	const QString KEY_SHOW_PATCH_GRID    = QStringLiteral("show_patch_grid");
	const bool    DEF_SYNC_VIEWS         = false;
	const bool    DEF_SHOW_PATCH_GRID    = true;
}

ViewerToolBar::ViewerToolBar(StyleManager* styleManager, QWidget* parent)
	: QToolBar(tr("Viewer Controls"), parent)
{
	this->setObjectName(QStringLiteral("viewerControlsToolBar"));
	this->setIconSize(QSize(20, 20));

	this->actionRotate90 = new QAction(tr("Rotate 90\xC2\xB0"), this);
	this->actionFlipH    = new QAction(tr("Flip Horizontal"), this);
	this->actionFlipV    = new QAction(tr("Flip Vertical"), this);

	this->actionSyncViews = new QAction(tr("Sync Views"), this);
	this->actionSyncViews->setCheckable(true);
	this->actionSyncViews->setChecked(DEF_SYNC_VIEWS);

	this->actionShowPatchGrid = new QAction(tr("Show Patch Grid"), this);
	this->actionShowPatchGrid->setCheckable(true);
	this->actionShowPatchGrid->setChecked(DEF_SHOW_PATCH_GRID);

	this->addAction(this->actionRotate90);
	this->addAction(this->actionFlipV);
	this->addAction(this->actionFlipH);
	this->addSeparator();
	this->addAction(this->actionShowPatchGrid);
	this->addAction(this->actionSyncViews);

	connect(this->actionRotate90,      &QAction::triggered, this, &ViewerToolBar::rotateRequested);
	connect(this->actionFlipH,         &QAction::triggered, this, &ViewerToolBar::flipHRequested);
	connect(this->actionFlipV,         &QAction::triggered, this, &ViewerToolBar::flipVRequested);
	connect(this->actionSyncViews,     &QAction::toggled,   this, &ViewerToolBar::syncViewsToggled);
	connect(this->actionShowPatchGrid, &QAction::toggled,   this, &ViewerToolBar::showPatchGridToggled);

	connect(styleManager, &StyleManager::styleChanged,
	        this, [this](StyleManager::StyleMode) { this->updateIcons(); });
	this->updateIcons();
}

QString ViewerToolBar::getName() const
{
	return SETTINGS_GROUP;
}

QVariantMap ViewerToolBar::getSettings() const
{
	QVariantMap m;
	m.insert(KEY_SYNC_VIEWS,      this->actionSyncViews->isChecked());
	m.insert(KEY_SHOW_PATCH_GRID, this->actionShowPatchGrid->isChecked());
	return m;
}

void ViewerToolBar::setSettings(const QVariantMap& settings)
{
	const bool syncViews   = settings.value(KEY_SYNC_VIEWS,      DEF_SYNC_VIEWS).toBool();
	const bool showGrid    = settings.value(KEY_SHOW_PATCH_GRID, DEF_SHOW_PATCH_GRID).toBool();

	// setChecked emits toggled → connected slots on ImageSessionViewer fire
	this->actionSyncViews->setChecked(syncViews);
	this->actionShowPatchGrid->setChecked(showGrid);
}

void ViewerToolBar::updateIcons()
{
	const QColor c = this->palette().windowText().color();
	this->actionRotate90->setIcon(      svgIconColored(QStringLiteral(":/icons/toolbar/rotate-ccw.svg"), c));
	this->actionFlipH->setIcon(         svgIconColored(QStringLiteral(":/icons/toolbar/flip-h.svg"),    c));
	this->actionFlipV->setIcon(         svgIconColored(QStringLiteral(":/icons/toolbar/flip-v.svg"),    c));
	this->actionSyncViews->setIcon(     svgIconColored(QStringLiteral(":/icons/toolbar/link.svg"),      c));
	this->actionShowPatchGrid->setIcon( svgIconColored(QStringLiteral(":/icons/toolbar/grid.svg"),      c));
}

QIcon ViewerToolBar::svgIconColored(const QString& path, const QColor& color, int size)
{
	QPixmap pm = QIcon(path).pixmap(QSize(size, size));
	if (pm.isNull())
		return QIcon();
	QPainter painter(&pm);
	painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
	painter.fillRect(pm.rect(), color);
	return QIcon(pm);
}
