# WavePSF Architecture

## Overview

WavePSF follows a layered MVC pattern. GUI widgets never call business logic directly — all communication goes through `ApplicationController` via Qt signals and slots.

```
┌──────────────────────────────────────────────────┐
│                  GUI Layer (Qt 5)                 │
│  MainWindow · ImageSessionViewer · PSFControlWidget│
│  MessageConsoleWidget · AboutDialog               │
├──────────────────────────────────────────────────┤
│             ApplicationController                 │
│  Lightweight coordinator — no algorithms          │
├──────────────────────────────────────────────────┤
│  ImageSession │ PSFModule │ OptimizationWorker    │
├──────────────────────────────────────────────────┤
│  ImageData / ImageDataAccessor                   │
│  IWavefrontGenerator · PSFCalculator · Deconvolver│
│  WavefrontParameterTable · TableInterpolator      │
└──────────────────────────────────────────────────┘
```

## Communication Pattern

```
GUI Widgets ←signal/slot→ ApplicationController ←direct calls→ Business Logic
```

- GUI → Controller: signals only, never direct calls
- Controller → Business: direct method calls
- Business → Controller → GUI: signal forwarding (mostly signal-to-signal)
- MainWindow wires all connections and owns the controller


## Key Components

### Data Layer
- **`ImageData`** — raw pixel buffer + metadata (width, height, frames, bit depth)
- **`ImageDataAccessor`** — patch grid management, extended patch extraction with borders, write-back and sync to output buffer
- **`WavefrontParameterTable`** — 3D coefficient store (frame × patch × coefficient), CSV save/load
- **`InputDataReader`** — loads ENVI HSI and TIFF files (libtiff backend by default)

### PSF Pipeline
- **`IWavefrontGenerator`** — interface; implemented by `ZernikeGenerator` and `DeformableMirrorGenerator`. Exposes `WavefrontParameter` descriptors so the GUI can build controls dynamically.
- **`PSFCalculator`** — converts a wavefront phase array to a PSF via FFT (ArrayFire)
- **`Deconvolver`** — applies one of 5 algorithms (Richardson-Lucy, Landweber, Tikhonov, Wiener, Convolution) to a patch
- **`PSFModule`** — orchestrates the above three; owned by `ApplicationController`

### Optimization
- **`IOptimizer`** — interface for optimization algorithms
- **`SimulatedAnnealingOptimizer`** — Metropolis acceptance, configurable temperature schedule and perturbation
- **`OptimizationWorker`** — runs `IOptimizer` on a `QThread`; emits progress signals back to GUI
- **`ImageMetricCalculator`** — 10 single-image metrics (variance, Laplacian, entropy, …) + 4 reference metrics (NCC, SSD, …)

### Settings & Logging
- **`SettingsFileManager`** — reads/writes `wavepsf.ini` via QSettings. Only `MainWindow` calls it directly; each widget exposes `getName()` / `getSettings()` / `setSettings()`.
- **`logging.h`** — `LOG_INFO()`, `LOG_WARNING()`, `LOG_ERROR()`, `LOG_DEBUG()`, `LOG_DEBUG_THIS()` macros routing to the Message Console. See [logging_usage_guide.md](logging_usage_guide.md).


## Threading

| Thread | Responsibility |
|---|---|
| Main (GUI) thread | All UI, event handling, ArrayFire PSF/deconvolution calls |
| OptimizationWorker thread | Long-running optimization loop only |

All ArrayFire compute happens on the main thread except during optimization. Qt signal-slot with `Qt::QueuedConnection` bridges the worker thread back to the GUI.


## Extension Points

- **New wavefront generator**: implement `IWavefrontGenerator`, register in `WavefrontGeneratorFactory`
- **New optimizer**: implement `IOptimizer`, add to algorithm selection in `OptimizationWidget`
- **New deconvolution algorithm**: extend `Deconvolver::DeconvolutionAlgo` enum and add a case in `deconvolve()`
- **New settings-aware widget**: implement `getName()` / `getSettings()` / `setSettings()`, register in `MainWindow::loadSettings()` / `saveSettings()`. See [settings_usage_guide.md](settings_usage_guide.md).
