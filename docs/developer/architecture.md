# WavePSF Architecture

## Overview

WavePSF is organized as a layered Qt desktop application with a strong separation between GUI widgets and the computational core.

Most widget actions flow through `ApplicationController`, while `MainWindow` is responsible for wiring widgets to controller slots and controller signals back to the widgets. One important current exception is that `ImageSessionViewer` receives an `ImageSession*` as a read-model style session reference.

```text
+-------------------------------------------------------------+
| GUI Layer (Qt 5)                                            |
| MainWindow · ImageSessionViewer · PSFGenerationWidget       |
| ProcessingControlWidget · PSFGridWidget · Message Console   |
+-------------------------------------------------------------+
| Coordination Layer                                          |
| ApplicationController                                       |
|   |- CoefficientWorkspace                                   |
|   |- OptimizationController -> OptimizationWorker (QThread) |
|   |- DeconvolutionController -> DeconvolutionWorker         |
|   |- PSFFileController                                      |
|   `- InterpolationOrchestrator                              |
+-------------------------------------------------------------+
| Domain / Compute Layer                                      |
| ImageSession · PSFModule · WavefrontParameterTable          |
| IPSFGenerator · IWavefrontGenerator · IPSFPropagator        |
| Deconvolver · ImageMetricCalculator · Optimizers            |
| BatchProcessor · PatchDeconvolutionProcessor                |
| VolumeDeconvolutionProcessor · VolumetricProcessor          |
| PSFGridGenerator                                            |
+-------------------------------------------------------------+
| Data / I/O Layer                                            |
| ImageData · ImageDataAccessor · InputDataReader             |
+-------------------------------------------------------------+
```

## Communication Pattern

```text
Widget signals / selected direct UI calls
    -> MainWindow wiring
    -> ApplicationController
    -> domain objects / orchestrators
    -> controller signals
    -> widgets
```

- Most GUI-to-controller communication uses Qt signals/slots wired in `MainWindow`.
- A few UI actions still call controller methods directly from `MainWindow` for convenience, for example applying settings during startup and handling global copy/paste/delete shortcuts.
- Controller-to-domain communication is mostly direct method calls.
- Domain-to-GUI communication is routed back through `ApplicationController` signals.
- `MainWindow` wires the connections, but it does **not** own the controller instance. The controller is created in [`src/main.cpp`](/c:/Users/Miro/Documents/GitHub/WavePSF/src/main.cpp).

## Key Components

### GUI Layer

- **`MainWindow`**: creates top-level widgets, menus, docks, and signal wiring.
- **`ImageSessionViewer`**: input/output/ground-truth viewing and patch interaction.
- **`PSFGenerationWidget`**: generator selection, coefficients, inline generator settings.
- **`ProcessingControlWidget`**: deconvolution, optimization, interpolation, and patch-grid controls.
- **`PSFGridWidget`**: overview of PSFs across the patch grid.

### Coordination Layer

- **`ApplicationController`**: central coordinator. 
- **`CoefficientWorkspace`**: owns the active `WavefrontParameterTable`, clipboard/undo behavior, and per-generator table caching.
- **`OptimizationController`**: manages the optimization worker thread and throttled live preview updates.
- **`DeconvolutionController`**: handles synchronous 2D patch execution, builds async 3D/batch requests, owns the deconvolution worker controller, writes outputs back to the session, and forwards progress.
- **`PSFFileController`**: save/load and auto-save behavior for PSF files and file-based PSF mode metadata.
- **`InterpolationOrchestrator`**: interpolation operations on coefficient tables.

### Domain / Compute Layer

- **`ImageSession`**: current input/output/ground-truth dataset, current frame, current patch, and patch-grid configuration.
- **`PSFModule`**: current generator instance, current PSF cache, deconvolver settings, and PSF regeneration.
- **`IPSFGenerator` / `ComposedPSFGenerator`**: top-level PSF generation abstraction and composed implementation.
- **`IWavefrontGenerator`**: phase/wavefront generation from coefficients.
- **`IPSFPropagator`**: wavefront-to-PSF propagation.
- **`Deconvolver`**: 2D and 3D deconvolution algorithms.
- **`ImageMetricCalculator`** and **optimizers**: optimization objective evaluation and parameter search.
- **`BatchProcessor`**: GUI-free batch loop with cancellation and typed progress/output signals.
- **`PatchDeconvolutionProcessor`**: shared per-patch 2D execution helper.
- **`VolumeDeconvolutionProcessor`**: shared per-volume 3D execution helper.
- **`VolumetricProcessor`**: worker-safe subvolume and PSF-volume assembly helpers for 3D processing.
- **`PSFGridGenerator`**: builds PSF overview grids for the UI.

### Data / I/O Layer

- **`ImageData`**: raw frame storage plus metadata.
- **`ImageDataAccessor`**: cached frame access, patch extraction with border extension, and patch/frame write-back.
- **`InputDataReader`**: ENVI, TIFF, standard image, and folder-stack loading.
- **`WavefrontParameterTable`**: coefficient storage across frame and patch dimensions.

## Threading

| Thread | Responsibility |
|---|---|
| Main (GUI) thread | UI, PSF preview generation, normal 2D patch deconvolution, batch request building, most file I/O orchestration |
| Deconvolution worker thread | Batch 2D, batch 3D, and single-patch 3D deconvolution with its own ArrayFire backend/device context |
| Optimization worker thread | Optimization loop with its own ArrayFire backend/device context |

Important current behavior:

- Optimization and long-running deconvolution workflows now run on dedicated worker threads.
- Manual 2D patch deconvolution still runs synchronously on the GUI thread.
- `MainWindow` owns the `QProgressDialog` for long-running deconvolution. Compute-side code no longer creates progress dialogs or calls `QApplication::processEvents()`.
- ArrayFire state is thread-local, so both optimization and deconvolution workers explicitly restore backend/device selection before running.

## Settings and Ownership Notes

- `SettingsFileManager` is used by `MainWindow`, but it is also passed to `AFDeviceManager` and `StyleManager` during application startup.
- `ApplicationController` is instantiated in `main.cpp` and passed into `MainWindow`.
- Most objects use Qt parent ownership. Some components also replace sub-objects manually, such as `PSFModule` replacing the active generator when switching generator type.

## Extension Points

- **New PSF generator**: implement `IWavefrontGenerator` and/or `IPSFPropagator`, register the combination in `PSFGeneratorFactory`.
- **New optimizer**: implement `IOptimizer`, register it in `OptimizerFactory`, and expose it in the optimization UI.
- **New deconvolution algorithm**: extend `Deconvolver` and the deconvolution settings UI.
- **New controller-managed workflow**: prefer adding a focused orchestrator/controller helper instead of growing `ApplicationController` further.
- **New settings-aware widget**: implement `getName()` / `getSettings()` / `setSettings()` and register it in `MainWindow::loadSettings()` / `saveSettings()`.
