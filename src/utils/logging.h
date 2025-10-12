#ifndef LOGGING_H
#define LOGGING_H

#include <QDebug>

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
	#define LOG_DEBUG() if (true) {} else qDebug()
	#define LOG_DEBUG_THIS() if (true) {} else qDebug()
	#define LOG_DEBUG_DETAILED() if (true) {} else qDebug()
#endif

#define LOG_INFO()		qInfo().noquote().nospace()
#define LOG_WARNING()	qWarning().noquote().nospace()
#define LOG_ERROR()		qCritical().noquote().nospace()

#endif // LOGGING_H
