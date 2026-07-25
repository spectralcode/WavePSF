#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>
#include <QThread>

// Debug: enabled unless QT_NO_DEBUG_OUTPUT is defined by the build
#if !defined(QT_NO_DEBUG_OUTPUT)
// Plain debug (works in any context)
	#define LOG_DEBUG() qDebug() << "[DEBUG] "
// Debug with class tag (only inside QObject instance methods!)
	#define LOG_DEBUG_THIS() qDebug() << "[DEBUG] [" << this->metaObject()->className() << "] "
// Detailed debug: includes thread id + file:line (works anywhere)
	#define LOG_DEBUG_DETAILED() \
		qDebug().noquote().nospace() << "[DEBUG] [tid=0x" \
		<< QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16) \
		<< ", " << __FILE__ ":" QT_STRINGIFY(__LINE__) << "] "
#else
	#define LOG_DEBUG() while (false) qDebug()
	#define LOG_DEBUG_THIS() while (false) qDebug()
	#define LOG_DEBUG_DETAILED() while (false) qDebug()
#endif

#define LOG_INFO()		qInfo().noquote()
#define LOG_WARNING()	qWarning().noquote()
#define LOG_ERROR()		qCritical().noquote()

#endif // LOGGING_H
