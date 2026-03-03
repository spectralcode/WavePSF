#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QDialog>
#include <QList>

class QTabWidget;
class QVBoxLayout;
class QTextBrowser;
class QListWidget;

struct ComponentInfo {
	QString name;
	QString url;
	QString licensePath;
};

class AboutDialog : public QDialog
{
	Q_OBJECT
public:
	explicit AboutDialog(QWidget *parent = nullptr);

private:
	QVBoxLayout* createLogoLayout();
	void setupTabs(QTabWidget* tabWidget);
	void addAboutTab(QTabWidget* tabWidget);
	void addLicenseTab(QTabWidget* tabWidget);
	void addThirdPartyTab(QTabWidget* tabWidget);

	QList<ComponentInfo> getThirdPartyComponents();
	QString loadLicenseText(const QString& path);
	void updateComponentDetails(QTextBrowser* browser, const ComponentInfo& component);
};

#endif // ABOUTDIALOG_H
