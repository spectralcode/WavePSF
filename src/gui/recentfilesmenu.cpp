#include "recentfilesmenu.h"

#include <QFileInfo>
#include <QAction>

namespace {
	const QString KEY_FILES = QStringLiteral("files");
}

RecentFilesMenu::RecentFilesMenu(const QString& title, const QString& settingsGroup,
                                 QWidget* parent, int maxCount)
	: QObject(parent),
	  recentMenu(new QMenu(title, parent)),
	  settingsName(settingsGroup),
	  maxFiles(maxCount)
{
	this->rebuild();
}

QMenu* RecentFilesMenu::menu() const
{
	return this->recentMenu;
}

QString RecentFilesMenu::getName() const
{
	return this->settingsName;
}

QVariantMap RecentFilesMenu::getSettings() const
{
	QVariantMap map;
	map[KEY_FILES] = this->recentFiles;
	return map;
}

void RecentFilesMenu::setSettings(const QVariantMap& settings)
{
	this->recentFiles = settings.value(KEY_FILES, QStringList()).toStringList();
	this->rebuild();
}

void RecentFilesMenu::addFile(const QString& path)
{
	this->recentFiles.removeAll(path);
	this->recentFiles.prepend(path);
	while (this->recentFiles.size() > this->maxFiles)
		this->recentFiles.removeLast();
	this->rebuild();
}

void RecentFilesMenu::rebuild()
{
	this->recentMenu->clear();
	if (this->recentFiles.isEmpty()) {
		QAction* empty = this->recentMenu->addAction(tr("No recent files"));
		empty->setEnabled(false);
		return;
	}
	for (const QString& path : qAsConst(this->recentFiles)) {
		QAction* a = this->recentMenu->addAction(QFileInfo(path).fileName());
		a->setStatusTip(path);
		a->setToolTip(path);
		connect(a, &QAction::triggered, this, [this, path]() {
			emit fileRequested(path);
		});
	}
	this->recentMenu->addSeparator();
	QAction* clearAction = this->recentMenu->addAction(tr("Clear"));
	connect(clearAction, &QAction::triggered, this, [this]() {
		this->recentFiles.clear();
		this->rebuild();
	});
}
