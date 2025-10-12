CONFIG(debug, debug|release) {
	message("Building Debug version: qDebug() enabled")
} else:CONFIG(release, debug|release) {
	DEFINES += QT_NO_DEBUG_OUTPUT
	message("Building Release version: qDebug() disabled")
}