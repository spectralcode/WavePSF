# ApplicationController Guide

## General Concept

`ApplicationController` is the central application coordinator between the GUI and the domain layer. It keeps most widgets away from `PSFModule`, optimization internals, and coefficient storage details, while forwarding state changes back to the GUI through signals. The main current exception is `ImageSessionViewer`, which receives an `ImageSession*` for viewing/state synchronization.


## Current Architecture Pattern

```text
Widgets
  -> signals and a few direct MainWindow calls
  -> ApplicationController
  -> helper controllers / orchestrators
  -> domain objects
  -> ApplicationController signals
  -> widgets
```

- **GUI -> ApplicationController**: mostly signal-slot communication wired in `MainWindow`
- **GUI -> ApplicationController direct calls**: still used in a few places from `MainWindow` where emitting a custom signal would add little value
- **ApplicationController -> business/domain layer**: direct method calls
- **Business/domain layer -> GUI**: routed back through controller signals

## What ApplicationController Owns Today

Core domain-facing members:

- `ImageSession`
- `InputDataReader`
- `PSFModule`
- `CoefficientWorkspace`
- `PSFGridGenerator`

Workflow helpers:

- `DeconvolutionController`
- `OptimizationController`
- `InterpolationOrchestrator`
- `PSFFileController`

This is different from the older simplified description of "ImageSession, PSFModule, WavefrontParameterTable, OptimizationWorker, TableInterpolator". Those are no longer the direct ownership boundaries in the code.

## Responsibilities

1. Receive requests from the UI.
2. Keep frame/patch selection and coefficient storage in sync.
3. Forward state changes from `ImageSession` and `PSFModule`.
4. Start optimization and apply the results back into the session.
5. Trigger deconvolution workflows, including live mode and async batch/3D execution.
6. Coordinate PSF-file behavior, interpolation, and PSF-grid generation.
7. Broadcast current application state after all UI connections are established.

## What It Should Not Become

`ApplicationController` should not accumulate more heavy algorithmic code, long-running loops, or UI widgets. When a workflow becomes substantial, prefer extracting it into a focused helper like:

- `OptimizationController`
- `DeconvolutionController`
- `PSFFileController`
- `InterpolationOrchestrator`

That pattern is already paying off and should continue.

## Signal Forwarding Strategy

Use direct signal-to-signal forwarding when no transformation is needed:

```cpp
connect(imageSession, &ImageSession::frameChanged,
        this, &ApplicationController::frameChanged);
```

Use transformation slots when controller state must also be updated:

```cpp
connect(imageSession, &ImageSession::inputDataChanged,
        this, &ApplicationController::handleInputDataChanged);
```

Use explicit workflow methods when multiple modules must stay in sync, for example frame switching:

```cpp
void ApplicationController::setCurrentFrame(int frame)
{
    coefficientWorkspace->store();
    imageSession->setCurrentFrame(frame);
    coefficientWorkspace->loadForCurrentPatch();
    deconvolutionController->requestCurrentDeconvolution();
}
```

The real implementation contains extra guards for 3D mode and live-deconvolution suppression, but this is the important pattern.

## Deconvolution Pattern

The deconvolution workflow is split across three layers:

- `DeconvolutionController`: handles synchronous 2D patch execution, builds async 3D/batch requests, writes patch/volume outputs back into `ImageSession`, and forwards lifecycle/progress signals.
- `DeconvolutionWorkerController`: owns the worker thread and queued worker dispatch.
- `DeconvolutionWorker` / `BatchProcessor`: run batch and 3D compute work on the deconvolution worker thread.

`ApplicationController` requests deconvolution and forwards GUI-facing lifecycle signals.

`MainWindow` remains responsible for the progress dialog. The compute/controller layers only emit typed progress and cancellation signals.

## Adding a New Component

### Add a new workflow helper

1. Create a focused helper class with a narrow responsibility.
2. Instantiate it in `ApplicationController::initializeComponents()`.
3. Connect its outgoing signals to controller signals or controller slots.
4. Expose only the controller API that the GUI actually needs.

### Add a new widget

1. Keep the widget free of direct business-layer references.
2. Add signals for user intent.
3. Add slots for state updates.
4. Wire it in `MainWindow`.
5. If startup state matters, make sure `broadcastCurrentState()` or another initial sync path covers it.

## Practical Guidance

- If you are about to add another large block of logic to `ApplicationController`, pause and ask whether it belongs in an orchestrator/helper instead.
- If a change introduces direct GUI dependence into batch processing or compute code, consider whether that logic should move upward toward the GUI layer.
