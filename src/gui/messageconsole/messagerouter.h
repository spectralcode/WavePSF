#ifndef MESSAGEROUTER_H
#define MESSAGEROUTER_H

#include <QObject>
#include <QDateTime>
#include <QMutex>
#include <QVector>
#include <QAtomicInt>
#include <QMessageLogContext>

/*
	MessageRouter installs a global Qt message handler and forwards messages to the UI.
	- Thread-safe ring buffer (MAX_MESSAGES).
	- Emits a Qt signal for each new message (QueuedConnection).
	- Pops a dialog for Critical/Fatal messages (Error).
*/

struct MessageEntry
{
	QDateTime	timestamp;	// when it was received
	QtMsgType	type;		// QtDebugMsg / QtInfoMsg / QtWarningMsg / QtCriticalMsg / QtFatalMsg
	QString		text;		// plain message text (no brackets, no level/category)
	QString		debugMeta;	// only for Debug lines in Debug builds: "{tid=0x..., file:line}"
};
Q_DECLARE_METATYPE(MessageEntry)

class MessageRouter : public QObject
{
	Q_OBJECT
public:
	static MessageRouter* instance();

	// Install qInstallMessageHandler once
	void install();

	// Return current buffer contents oldest -> newest
	QVector<MessageEntry> snapshot() const;

	static int maxMessages() {return MAX_MESSAGES;}

signals:
	void messageArrived(const MessageEntry& entry);

private:
	explicit MessageRouter(QObject* parent = nullptr);
	~MessageRouter() override;

	static void staticHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);

	void handle(QtMsgType type, const QMessageLogContext& ctx, const QString& msg);

private:
	static MessageRouter* self;
	static QAtomicInt installed;

private:
	static const int MAX_MESSAGES = 4096;

	// Ring buffer state
	mutable QMutex mutex;
	QVector<MessageEntry> ring;
	int head;
	int count;
	quint64 seq;

	// Dialog de-dup (ms since epoch)
	QString lastCriticalText;
	qint64 lastCriticalMs;
};

#endif // MESSAGEROUTER_H
