#ifndef RECENTFILESMENU_H
#define RECENTFILESMENU_H

#include <QObject>
#include <QMenu>
#include <QStringList>
#include <QVariantMap>

// Manages a "recent files" submenu: owns the QMenu, the ordered file list,
// deduplication, trimming to a max size, and menu rebuilding.
// Emits fileRequested(path) when the user clicks an entry.
// Follows the getName/getSettings/setSettings convention of other components.
class RecentFilesMenu : public QObject
{
	Q_OBJECT

public:
	explicit RecentFilesMenu(const QString& title, const QString& settingsGroup,
	                         QWidget* parent, int maxCount = 10);

	QMenu* menu() const;

	// Settings
	QString    getName()    const;
	QVariantMap getSettings() const;
	void        setSettings(const QVariantMap& settings);

public slots:
	void addFile(const QString& path);

signals:
	void fileRequested(const QString& path);

private:
	void rebuild();

	QMenu* recentMenu;
	QStringList recentFiles;
	QString settingsName;
	int maxFiles;
};

#endif // RECENTFILESMENU_H
