#include "messagerouter.h"
#include <QMetaObject>
#include <QThread>
#include <QMessageBox>
#include <QCoreApplication>

MessageRouter* MessageRouter::self = nullptr;
QAtomicInt MessageRouter::installed = 0;

namespace {
	static QString timeString(const QDateTime& dt)
	{
		return dt.toString("HH:mm:ss");
	}
}

MessageRouter::MessageRouter(QObject* parent)
	: QObject(parent),
	  head(0),
	  count(0),
	  seq(0),
	  lastCriticalMs(0)
{
	this->ring.resize(MAX_MESSAGES);
}

MessageRouter::~MessageRouter() = default;

MessageRouter* MessageRouter::instance()
{
	static QMutex s_mutex;
	QMutexLocker lock(&s_mutex);
	if (!self)
		self = new MessageRouter();
	return self;
}

void MessageRouter::install()
{
	// Only install once
	if (installed.testAndSetOrdered(0, 1)) {
		qInstallMessageHandler(&MessageRouter::staticHandler);
	}
}

QVector<MessageEntry> MessageRouter::snapshot() const
{
	QMutexLocker lock(&this->mutex);
	QVector<MessageEntry> out;
	out.reserve(this->count);
	for (int i = 0; i < this->count; ++i) {
		const int idx = (this->head + i) % MAX_MESSAGES;
		out.push_back(this->ring[idx]);
	}
	return out;
}

void MessageRouter::staticHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
	MessageRouter::instance()->handle(type, ctx, msg);
}

void MessageRouter::handle(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
	Q_UNUSED(ctx);
	const QDateTime now = QDateTime::currentDateTime();

	// Build plain text for UI (no [LEVEL][CATEGORY] noise)
	QString text = msg;

	// Debug-only meta: thread id + file:line (visible to widget if desired)
	QString debugMeta;
#if !defined(NDEBUG)
	if (type == QtDebugMsg) {
		quintptr tid = reinterpret_cast<quintptr>(QThread::currentThreadId());
		const char* file = ctx.file ? ctx.file : "";
		int line = ctx.line;
		debugMeta = QString("{tid=0x%1, %2:%3}")
			.arg(QString::number(tid, 16))
			.arg(QString::fromUtf8(file))
			.arg(line);
	}
#endif

	// Write to ring buffer
	{
		QMutexLocker lock(&this->mutex);
		MessageEntry e;
		e.timestamp = now;
		e.type = type;
		e.text = text;
		e.debugMeta = debugMeta;

		this->ring[(this->head + this->count) % MAX_MESSAGES] = e;
		if (this->count < MAX_MESSAGES) {
			++this->count;
		} else {
			this->head = (this->head + 1) % MAX_MESSAGES;
		}
		this->seq++;
	}

	// Emit signal to the GUI
	MessageEntry emitted;
	emitted.timestamp = now;
	emitted.type = type;
	emitted.text = text;
	emitted.debugMeta = debugMeta;
	QMetaObject::invokeMethod(this, [this, emitted]() {
		emit messageArrived(emitted);
	}, Qt::QueuedConnection);

	// Dialog policy for Critical/Fatal (Error)
	if (type == QtCriticalMsg || type == QtFatalMsg) {
		const qint64 ms = now.toMSecsSinceEpoch();

		// 1s de-dup for identical text
		bool show = true;
		{
			QMutexLocker lock(&this->mutex);
			if (this->lastCriticalText == emitted.text && (ms - this->lastCriticalMs) < 1000) {
				show = false;
			} else {
				this->lastCriticalText = emitted.text;
				this->lastCriticalMs = ms;
			}
		}

		if (show) {
			// Clean dialog: only the message text (no timestamp/level)
			QMessageBox::critical(nullptr, QObject::tr("Error"), emitted.text);
		}

		if (type == QtFatalMsg) {
			// Unrecoverable path — terminate immediately (debug-time invariants).
			// Note: If you need a graceful shutdown, DO NOT log Fatal. Log Critical and quit explicitly.
			abort();
		}
	}
}
