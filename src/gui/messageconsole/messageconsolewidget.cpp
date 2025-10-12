#include "messageconsolewidget.h"

#include <QVBoxLayout>
#include <QScrollBar>
#include <QMenu>
#include <QTextCursor>
#include <QPlainTextEdit>
#include <QPalette>
#include <QSettings>
#include <algorithm>


namespace {
	// Settings group + keys (local to this file)
	const char* SETTINGS_GROUP      = "message_console_widget";
	const char* NEWEST_AT_TOP_KEY   = "newest_at_top";
	const char* AUTOSCROLL_KEY      = "autoscroll";
}

namespace  {
	static QString timeString(const QDateTime& dt)
	{
		return dt.toString("HH:mm:ss");
	}

	// Colors for message levels
	static inline QColor errorTargetColor()   { return QColor(235, 35, 35); }
	static inline QColor warningTargetColor() { return QColor(195, 85, 5); }
	static inline QColor debugTargetColor()   { return QColor(90, 90, 90); }

	// Blend two colors with weight in [0.0, 1.0]; 1.0 means full 'b'
	static inline QColor blend(const QColor& a, const QColor& b, double weightB)
	{
		const double wb = std::min(1.0, std::max(0.0, weightB));
		const double wa = 1.0 - wb;
		return QColor(
			static_cast<int>(a.red()   * wa + b.red()   * wb),
			static_cast<int>(a.green() * wa + b.green() * wb),
			static_cast<int>(a.blue()  * wa + b.blue()  * wb)
		);
	}
}

MessageConsoleWidget::MessageConsoleWidget(QWidget* parent)
	: QWidget(parent),
	  output(new QPlainTextEdit(this)),
	  actionClear(new QAction(tr("Clear"), this)),
	  actionCopyAll(new QAction(tr("Copy All"), this)),
	  actionNewestTop(new QAction(tr("Newest at Top"), this)),
	  actionAutoscroll(new QAction(tr("Auto-scroll to Newest"), this)),
	  newestAtTop(false),
	  autoscroll(true)
{
	this->output->setReadOnly(true);

	// Use normal application font (no monospace override)

	this->setupContextMenu();

	// Layout
	QVBoxLayout* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(this->output);
	this->setLayout(layout);

	// Connect to router
	connect(MessageRouter::instance(), &MessageRouter::messageArrived,
			this, &MessageConsoleWidget::onMessage, Qt::QueuedConnection);

	// Load initial snapshot
	this->loadSnapshot(MessageRouter::instance()->snapshot());
}

QString MessageConsoleWidget::getName() const
{
	return QLatin1String(SETTINGS_GROUP);
}

QVariantMap MessageConsoleWidget::getSettings() const
{
	QVariantMap m;
	m.insert(QLatin1String(NEWEST_AT_TOP_KEY), this->newestAtTop);
	m.insert(QLatin1String(AUTOSCROLL_KEY), this->autoscroll);
	return m;
}

void MessageConsoleWidget::setSettings(const QVariantMap& m)
{
	this->newestAtTop = m.value(QLatin1String("newest_at_top"), false).toBool();
	this->autoscroll  = m.value(QLatin1String("autoscroll"), true).toBool();

	// Sync UI + apply
	if (this->actionNewestTop)	this->actionNewestTop->setChecked(this->newestAtTop);
	if (this->actionAutoscroll)	this->actionAutoscroll->setChecked(this->autoscroll);

	this->rebuildAll();
}

void MessageConsoleWidget::setupContextMenu()
{
	this->actionNewestTop->setCheckable(true);
	this->actionAutoscroll->setCheckable(true);
	this->actionNewestTop->setChecked(this->newestAtTop);
	this->actionAutoscroll->setChecked(this->autoscroll);

	connect(this->actionClear, &QAction::triggered, this, [this]() {
		this->cache.clear();
		this->output->clear();
	});
	connect(this->actionCopyAll, &QAction::triggered, this, [this]() {
		this->output->selectAll();
		this->output->copy();
		this->output->moveCursor(QTextCursor::End);
	});
	connect(this->actionNewestTop, &QAction::toggled, this, [this](bool on) {
		this->newestAtTop = on;
		this->rebuildAll(); // reorder the view immediately
	});
	connect(this->actionAutoscroll, &QAction::toggled, this, [this](bool on) {
		this->autoscroll = on;
	});

	this->output->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this->output, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
		QMenu m(this);
		m.addAction(this->actionClear);
		m.addAction(this->actionCopyAll);
		m.addSeparator();
		m.addAction(this->actionNewestTop);
		m.addAction(this->actionAutoscroll);
		m.exec(this->output->mapToGlobal(pos));
	});
}

void MessageConsoleWidget::loadSnapshot(const QVector<MessageEntry>& entries)
{
	// Replace cache and rebuild view according to current order
	this->cache = entries;
	this->rebuildAll();
}

void MessageConsoleWidget::changeEvent(QEvent* ev)
{
	switch (ev->type()) {
		case QEvent::PaletteChange:
		case QEvent::ApplicationPaletteChange:
		case QEvent::StyleChange:
		case QEvent::ApplicationFontChange:
			// Re-emit all lines so colors/fonts reflect the new theme
			this->rebuildAll();
			break;
		default:
			break;
	}
	QWidget::changeEvent(ev);
}

QString MessageConsoleWidget::styledText(const MessageEntry& e) const
{
	// Build the visible line: "HH:mm:ss  <text>", with prefixes for Warning/Error.
	QString txt = e.text;

	if (e.type == QtWarningMsg) {
		txt = QStringLiteral("Warning: ") + txt;
	} else if (e.type == QtCriticalMsg || e.type == QtFatalMsg) {
		txt = QStringLiteral("Error: ") + txt;
	}

	return QStringLiteral("%1  %2").arg(timeString(e.timestamp), txt);
}

void MessageConsoleWidget::appendOrPrepend(const MessageEntry& e)
{
	// Choose color based on palette (no hardcoded hex)
	const QPalette pal = this->output->palette();
	QColor col = pal.color(QPalette::Text); // default (Info)

	if (e.type == QtWarningMsg) {
		col = blend(col, warningTargetColor(), 0.90);
	} else if (e.type == QtCriticalMsg || e.type == QtFatalMsg) {
		col = blend(col, errorTargetColor(), 0.90);
	} else if (e.type == QtDebugMsg) {
		col = blend(col, debugTargetColor(), 0.90);
	}

	const QString line = this->styledText(e);

	QTextCharFormat fmt;
	fmt.setForeground(col);

	QTextDocument* doc = this->output->document();
	const bool empty = doc->isEmpty();

	QTextCursor cur(doc);
	if (this->newestAtTop) {
		// Insert at start; add a separator *after* only if there was content
		cur.movePosition(QTextCursor::Start);
		cur.insertText(line, fmt);
		if (!empty) cur.insertText("\n");
	} else {
		// Insert at end; add a separator *before* only if there is content
		cur.movePosition(QTextCursor::End);
		if (!empty) cur.insertText("\n");
		cur.insertText(line, fmt);
	}

	// Autoscroll if enabled
	if (this->autoscroll) {
		if (this->newestAtTop) {
			this->output->verticalScrollBar()->setValue(this->output->verticalScrollBar()->minimum());
		} else {
			this->output->verticalScrollBar()->setValue(this->output->verticalScrollBar()->maximum());
		}
	}
}

void MessageConsoleWidget::onMessage(const MessageEntry& e)
{
	// Keep a cache for rebuilds when toggling ordering or palette changes
	this->cache.push_back(e);

	// If we exceed number of max messages, remove all messages
	// todo: replace this with an efficient way to only drop the oldest message
	const int cap = MessageRouter::maxMessages();
	if (this->cache.size() > cap) {
		this->cache.clear();
		this->cache.push_back(e);
		this->rebuildAll();
		return;
	}

	// Insert in the live view respecting the current order setting
	this->appendOrPrepend(e);
}

void MessageConsoleWidget::rebuildAll()
{
	this->output->clear();

	// Always iterate oldest -> newest; insertion side decides final order
	for (int i = 0; i < this->cache.size(); ++i)
		this->appendOrPrepend(this->cache.at(i));

	// Final autoscroll position after a full rebuild
	if (this->autoscroll) {
		if (this->newestAtTop) {
			this->output->verticalScrollBar()->setValue(this->output->verticalScrollBar()->minimum());
		} else {
			this->output->verticalScrollBar()->setValue(this->output->verticalScrollBar()->maximum());
		}
	}
}
