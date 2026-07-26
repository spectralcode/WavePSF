# Logging Usage Guide

General info:
- Include "utils/logging.h" wherever you want to use log messages
- All log messages appear in the **Message Console**. A future headless frontend can use Qt's default message handler to print logs to the terminal.
- Logging is **diagnostic only — it never creates UI**. To make a failure visible to the user, return an `OperationResult`, set the run result's `RunStatus`, or emit an error signal into the controller funnel. The full contract is in [Error Handling](coding_conventions.md#error-handling).
- **Debug** messages are included **only in Debug builds** and **do not need `tr()`** (they're for developers).


## How to use / Examples

```cpp
#include "utils/logging.h"

// Diagnostics
LOG_INFO()		<< "Loaded" << n << "items";
LOG_WARNING()	<< QString("Skipping patch (%1,%2): empty result").arg(x).arg(y);
LOG_ERROR()		<< "Device probe failed:" << e.what();	// severe diagnostic; no dialog

// Debug (developer-only, no tr)
LOG_DEBUG()		<< "init complete";				// free/static OK
LOG_DEBUG_THIS()	<< "processing frame";		// inside QObject method

// Reporting a failure to the USER: return a result, don't just log
OperationResult saveSomething(const QString& filePath)
{
	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		return {false, QObject::tr("Could not write file: %1").arg(file.errorString())};
	}
	// ...
	return {true, QString()};
}
```

Notes:
- `LOG_ERROR()` does **not** open a dialog. It remains valid for severe diagnostics and for failures without a return path — but when an `OperationResult` or run result is available, put the message there; the frontend decides how to log and present it. Using a log call as the only error report where a result exists is the anti-pattern.
- Streamed values are separated by spaces: `<< "with" << 3 << "jobs"` prints `with 3 jobs`. Use `QString("…").arg(…)` when spaces around punctuation would look wrong.
- `LOG_DEBUG_THIS()` prints `[DEBUG] [MyClass] …`; `LOG_DEBUG()` prints `[DEBUG] …`.
- Don't add timestamps/levels in messages — the UI handles that.
- Never log inside inner loops (per-pixel, per-evaluation, per-iteration); log one summary per job or run.


## Translation
- Wrap **user-visible** strings — result messages, and log lines users are meant to read — in `tr("…")` with `%1`, `%2`, … placeholders.
- Leave **debug** strings in plain English (no `tr()`).
