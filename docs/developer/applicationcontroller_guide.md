# ApplicationController Guide

## General Concept

ApplicationController serves as a coordinator between GUI widgets and business logic. It maintains strict separation: GUI never directly calls business methods, and business logic never directly updates GUI.

## Architecture Pattern

```
GUI Widgets ←signal/slot→ ApplicationController ←direct calls→ Business Logic
```

- **GUI → ApplicationController**: Signal-only communication
- **ApplicationController → Business**: Direct method calls
- **Business → ApplicationController → GUI**: Signal forwarding

## Key Responsibilities

1. **Owns business modules**: ImageSession, PSFModule, WavefrontParameterTable, OptimizationWorker, TableInterpolator
2. **Translates requests**: Receives GUI signals, calls business methods
3. **Forwards notifications**: Business signals → GUI signals (mostly direct signal-to-signal)
4. **Coordinates initialization**: Broadcasts current state to GUI after connections

## Signal Forwarding Strategy

**Direct forwarding** (signal-to-signal):
```cpp
connect(imageSession, &ImageSession::frameChanged,
        this, &ApplicationController::frameChanged);
```

**Transformation slots** (only when needed):
```cpp
connect(imageSession, &ImageSession::inputDataChanged,
        this, &ApplicationController::handleInputDataChanged);
```

## Adding New Components

### 1. Add Business Module to ApplicationController

**Header (.h):**
```cpp
private:
	PSFModule* psfModule;  // Add new business module

signals:
	void psfParametersChanged(const QVector<double>& coefficients);  // Add relevant signals
```

**Implementation (.cpp):**
```cpp
// In initializeComponents()
this->psfModule = new PSFModule(this);

// In connectSessionSignals() or new connectPSFModuleSignals()
connect(this->psfModule, &PSFModule::psfUpdated,
        this, &ApplicationController::psfUpdated);
```

### 2. Create GUI Widget

Create widget without any business logic references:
```cpp
class PSFWidget : public QWidget {
signals:
	void coefficientChangeRequested(int nollIndex, double value);
	void generatePSFRequested();

public slots:
	void updateCoefficients(const QVector<double>& coefficients);
};
```

### 3. Connect in MainWindow

Add connection method:
```cpp
void MainWindow::connectPSFWidget() {
	// Widget → Controller
	connect(psfWidget, &PSFWidget::coefficientChangeRequested,
	        applicationController, &ApplicationController::setPSFCoefficient);
	        
	// Controller → Widget  
	connect(applicationController, &ApplicationController::psfParametersChanged,
	        psfWidget, &PSFWidget::updateCoefficients);
}
```

Call in MainWindow constructor after `broadcastCurrentState()`.

## Key Principles

- **No direct references** between GUI and business logic
- **ApplicationController stays lightweight** - no complex algorithms
- **Business modules access each other directly** when needed
- **MainWindow coordinates all connections**
- **Push-based state management** - widgets cache received data