#ifndef OPERATIONRESULT_H
#define OPERATIONRESULT_H

#include <QString>

// Result of a fallible synchronous operation (file I/O, validation).
// ok == true means success; message is empty on success and holds a
// user-presentable reason on failure.
struct OperationResult
{
	bool ok;
	QString message;
};

// final status of an asynchronous run (deconvolution, optimization).
enum class RunStatus {
	COMPLETED = 0,
	PARTIAL,
	CANCELLED,
	FAILED
};

#endif // OPERATIONRESULT_H
