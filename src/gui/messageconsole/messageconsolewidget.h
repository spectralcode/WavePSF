#ifndef MESSAGECONSOLEWIDGET_H
#define MESSAGECONSOLEWIDGET_H

#include <QWidget>
#include <QVector>
#include "messagerouter.h"

class QPlainTextEdit;
class QAction;
class QSettings;

/*
	MessageConsoleWidget
	- Simple console for info/warning/error/debug messages.
	- Persists its own settings via saveSettings/loadSettings (group/keys defined in .cpp).
*/

class MessageConsoleWidget : public QWidget
{
	Q_OBJECT
public:
	explicit MessageConsoleWidget(QWidget* parent = nullptr);

	QString getName() const;
	QVariantMap getSettings() const;
	void setSettings(const QVariantMap& m);

	void loadSnapshot(const QVector<MessageEntry>& entries);

protected:
	void changeEvent(QEvent* ev) override;

private slots:
	void onMessage(const MessageEntry& e);

private:
	// Internal helpers
	void appendOrPrepend(const MessageEntry& e);
	void rebuildAll();						// re-render when "Newest at Top" toggles or palette/style changes
	void setupContextMenu();
	QString styledText(const MessageEntry& e) const;

private:
	// UI
	QPlainTextEdit* output;
	QAction* actionClear;
	QAction* actionCopyAll;
	QAction* actionNewestTop;
	QAction* actionAutoscroll;

	// Settings (initialized in .cpp constructor)
	bool newestAtTop;
	bool autoscroll;

	// Cached data for rebuilds
	QVector<MessageEntry> cache;
};

#endif // MESSAGECONSOLEWIDGET_H
