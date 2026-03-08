# ArrayFire Device Management

## Overview

ArrayFire (AF) maintains backend and device state **per thread**. When switching backends or devices, all cached `af::array` objects become invalid and must be released before the switch. The `AFDeviceManager` class centralizes device management and emits signals so each component can handle its own cleanup.

## Architecture

```
AFDeviceManager::setDevice(backendId, deviceId)
  │
  ├─ emit aboutToChangeDevice(backendId, deviceId)
  │    ├─ PSFModule::clearCachedArrays()         [self-connected]
  │    ├─ ImageSession::clearAFCaches()           [self-connected]
  │    └─ ApplicationController: externalPSFOverrides.clear()
  │
  ├─ af::setBackend(backendId)   ← actual AF switch (main thread)
  ├─ af::setDevice(deviceId)
  │
  └─ emit deviceChanged(backendId, deviceId)
       └─ ApplicationController: re-trigger PSF pipeline
```

## Threading Model

| Thread | AF usage | How it gets backend/device |
|--------|----------|--------------------------|
| Main thread | PSFModule, ImageSession, ImageDataAccessor, PSFCalculator, generators | `AFDeviceManager::setDevice()` calls `af::setBackend()`/`af::setDevice()` on this thread |
| OptimizationWorker thread | Local PSFCalculator, generator, Deconvolver | Receives `afBackend`/`afDeviceId` in `OptimizationConfig` struct, calls `af::setBackend()`/`af::setDevice()` at the start of each run |
| ImageRenderWorker thread | Does not use ArrayFire | N/A |

Main-thread components do **not** need to call `af::setBackend()`/`af::setDevice()` themselves — the manager does it for the entire thread. They only need to release their cached arrays before the switch.

Worker threads must set their own AF context because AF state is per-thread. The `OptimizationWorker` does this by reading from the config struct passed via queued signal connection.

## Signal Flow

### `aboutToChangeDevice(int backendId, int deviceId)`

Emitted **before** `af::setBackend()` is called. All listeners must release their `af::array` members during this signal. Since all connections are direct (same thread), all cleanup completes before the AF switch happens.

### `deviceChanged(int backendId, int deviceId)`

Emitted **after** the switch is complete. Used by `ApplicationController` to re-trigger the PSF pipeline, which lazily rebuilds all arrays on the new backend.

## Classes That Hold Persistent af::array Members

| Class | af::array members | Cleared by |
|-------|------------------|------------|
| PSFModule | `currentWavefront`, `currentPSF`, `externalPSF` | `clearCachedArrays()` |
| PSFCalculator | `cachedApertureMask` | PSFModule via `clearCachedArrays()` |
| ZernikeGenerator | `cachedBasisArrays` | PSFModule via `clearCachedArrays()` → `generator->invalidateCache()` |
| DeformableMirrorGenerator | `cachedInfluenceFunctions` | PSFModule via `clearCachedArrays()` → `generator->invalidateCache()` |
| ImageDataAccessor (x3) | `cachedFrame` | ImageSession via `clearAFCaches()` → `accessor->clearCache()` |
| ApplicationController | `externalPSFOverrides` | Direct `.clear()` in lambda |

## Adding a New AF-Using Module

If you add a new class that holds persistent `af::array` members on the main thread:

1. **Take `AFDeviceManager*` in the constructor:**
   ```cpp
   // mymodule.h
   class AFDeviceManager;

   class MyModule : public QObject {
       Q_OBJECT
   public:
       explicit MyModule(AFDeviceManager* afDeviceManager, QObject* parent = nullptr);

   public slots:
       void clearCachedArrays();

   private:
       af::array cachedResult;
   };
   ```

2. **Connect to `aboutToChangeDevice` in the constructor:**
   ```cpp
   // mymodule.cpp
   #include "utils/afdevicemanager.h"

   MyModule::MyModule(AFDeviceManager* afDeviceManager, QObject* parent)
       : QObject(parent)
   {
       connect(afDeviceManager, &AFDeviceManager::aboutToChangeDevice,
               this, &MyModule::clearCachedArrays);
   }

   void MyModule::clearCachedArrays()
   {
       this->cachedResult = af::array();  // release GPU memory
   }
   ```

3. **Pass `AFDeviceManager*` when creating the module** (e.g., in `ApplicationController::initializeComponents()`):
   ```cpp
   this->myModule = new MyModule(this->afDeviceManager, this);
   ```

No changes to `ApplicationController`, `AFDeviceManager`, or any other class are needed.

### Worker Threads

If your module runs on a **separate QThread** and uses ArrayFire:

- Do **not** connect to `aboutToChangeDevice` (the signal runs on the main thread, not yours)
- Instead, receive `afBackend`/`afDeviceId` in your work config struct
- Call `af::setBackend()` and `af::setDevice()` at the start of your worker method
- Create all AF objects locally within the worker method and clean up when done
- See `OptimizationWorker::runOptimization()` for the reference pattern

## Key Files

- `src/utils/afdevicemanager.h/.cpp` — centralized device management, signals, settings persistence
- `src/core/psf/psfmodule.cpp` — self-connects to `aboutToChangeDevice`
- `src/controller/imagesession.cpp` — self-connects to `aboutToChangeDevice`
- `src/controller/applicationcontroller.cpp` — connects for `externalPSFOverrides` + pipeline re-trigger
- `src/core/optimization/optimizationworker.cpp` — per-thread AF context setup
