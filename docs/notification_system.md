# Notification System

A simple signal-based system for user notifications with automatic severity routing.

## Severity Levels

| Severity | Display | Duration | Use Case |
|----------|---------|----------|----------|
| `Status` | Status bar (persistent) | Until cleared | Long operations |
| `Info` | Status bar | 5 seconds | Success messages |
| `Warning` | Status bar | 5 seconds | Non-critical issues |
| `Error` | Modal dialog | Until dismissed | Failed operations |
| `Critical` | Modal dialog | Until dismissed | Serious errors |

## Usage

### Basic Macros
```cpp
NOTIFY_STATUS("Loading file...");     // Persistent status
NOTIFY_INFO("File saved");            // Temporary info
NOTIFY_WARNING("Using defaults");     // Temporary warning  
NOTIFY_ERROR("File not found");       // Error dialog
NOTIFY_CRITICAL("Out of memory");     // Critical dialog
CLEAR_STATUS();                       // Remove persistent status
```

### Complex Messages
```cpp
QString msg = tr("Error: %1 at line %2").arg(error).arg(line);
emit userNotification(NotificationMessage(NotificationSeverity::Error, msg));
```

### Long Operations
```cpp
NOTIFY_STATUS("Processing image...");
// ... do work ...
CLEAR_STATUS();
NOTIFY_INFO("Processing completed");
```

## Integration

1. **Add notifications to any QObject class**:
   ```cpp
   #include "utils/notificationtypes.h"
   
   class MyClass : public QObject {
       Q_OBJECT
       
	   public:
	   //.. your public and private members, slots etc ..

	   signals:
       ADD_NOTIFICATIONS  // Adds userNotification signal
       
       // Can now use NOTIFY_* macros anywhere
   };
   ```

2. **Connect signals** in ApplicationController:
   ```cpp
   connect(myClass, &MyClass::userNotification, 
           this, &ApplicationController::userNotification);
   ```

3. **Setup display** in MainWindow:
   ```cpp
   notificationDisplay = new NotificationDisplay(this, statusBar(), this);
   connect(applicationController, &ApplicationController::userNotification,
           notificationDisplay, &NotificationDisplay::displayNotification);
   ```

## Translation Support

When using the macros messages are automatically wrapped with `tr()` for future translation:
```cpp
NOTIFY_ERROR("File not found");  // Translatable
```

## Comparison to logging.h
### Logging System (Developer-focused)
- Audience: Developers, debugging, troubleshooting
- Output: Log files, console, debug output
- Content: Technical details, execution traces, internal state

### Notification System (User-focused)
- Audience: End users during normal operation
- Output: UI elements (status bar, dialogs)
- Content: User-friendly messages about application state

