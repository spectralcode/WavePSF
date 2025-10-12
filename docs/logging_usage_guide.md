# Logging Usage Guide

General info:
- Include "utils/logging.h" wherever you want to use log messages
- All log messages appear in the **Message Console**.
- **Error** messages additionally show a **modal dialog** that requires user interaction (user needs to click on OK).
- **Debug** messages are included **only in Debug builds** and **do not need `tr()`** (they’re for developers).


## How to use / Examples

```cpp
#include "utils/logging.h"

// User-facing (translate)
LOG_INFO()		<< tr("Loaded %1 items").arg(n);
LOG_WARNING()	<< tr("Expected %1, got %2").arg(exp).arg(act);
LOG_ERROR()		<< tr("Failed to save: %1").arg(err);

// Debug (developer-only, no tr)
LOG_DEBUG()		<< "init complete";				// free/static OK
LOG_DEBUG_THIS()	<< "processing frame";		// inside QObject method

// Quick patterns
LOG_ERROR()		<< tr("Open %1 failed: %2").arg(path).arg(file.errorString());
LOG_WARNING()	<< tr("Clamped to [%1, %2]").arg(min).arg(max);
LOG_INFO()		<< tr("Saved: %1").arg(outPath);
```

Notes:
- `LOG_DEBUG_THIS()` prints `[DEBUG] [MyClass] …`; `LOG_DEBUG()` prints `[DEBUG] …`.
- Outputs are unquoted; don’t add timestamps/levels in messages—the UI handles that.


## Translation
- Wrap **user-visible** strings in `tr("…")` with `%1`, `%2`, … placeholders.
- Leave **debug** strings in plain English (no `tr()`).
