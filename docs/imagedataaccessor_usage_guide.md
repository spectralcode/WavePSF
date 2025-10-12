# ImageDataAccessor Usage Guide

## Overview
`ImageDataAccessor` provides efficient CPU↔GPU data access for patch-based image processing workflows. It caches frames on GPU and handles border extension automatically.

## Basic Setup

```cpp
#include "imagedataaccessor.h"

// Create accessor
ImageDataAccessor* accessor = new ImageDataAccessor(imageData, false); // false = writable

// Configure patch grid
accessor->configurePatchGrid(4, 4, 10); // 4x4 grid with 10px borders
```

## Core Workflows

### 1. Simple Patch Processing
```cpp
// Get patch with borders
ImagePatch patch = accessor->getExtendedPatch(patchX, patchY, frameNr);

// Process with deconvolution
af::array deconvolved = richardsonLucy(patch.data, psf);

// Write back (automatically crops borders)
accessor->writePatchResult(patch, deconvolved);
```

### 2. High-Performance Optimization
```cpp
// Switch to manual sync for speed
accessor->setSyncMode(ImageDataAccessor::MANUAL);

ImagePatch patch = accessor->getExtendedPatch(x, y, frame);

// Thousands of iterations on GPU
for (int i = 0; i < 5000; ++i) {
    af::array optimized = optimizeStep(patch.data, coeffs[i]);
    // No CPU transfer during loop
}

// Single CPU sync when done
accessor->writePatchResult(patch, finalResult);
accessor->forceSyncToCPU();
```

### 3. Live GUI Updates
```cpp
// Real-time updates for preview
accessor->setSyncMode(ImageDataAccessor::IMMEDIATE);

// Every write automatically updates CPU for display
ImagePatch patch = accessor->getExtendedPatch(x, y, frame);
af::array result = quickDeconvolve(patch.data, psf);
accessor->writePatchResult(patch, result); // GUI updates immediately
```

## Sync Modes

| Mode | Use Case | CPU↔GPU Transfers |
|------|----------|-------------------|
| `IMMEDIATE` | Live preview, GUI interaction | After every write |
| `MANUAL` | Optimization, batch processing | Only when requested |
| `DISABLED` | Maximum performance | Never automatic |

## Border Handling

The `ImagePatch` automatically tracks which borders were extended:

```cpp
ImagePatch patch = accessor->getExtendedPatch(2, 2, 0); // corner patch

// Check what borders were added
if (patch.borders.leftExtended) { /* left border was added */ }
if (patch.borders.rightExtended) { /* right border was added */ }

// Core area within extended patch (relative coordinates)
QRect core = patch.coreArea;

// Position in full image (absolute coordinates)  
QRect position = patch.imagePosition;
```

## Legacy Methods (Core Area Only)

```cpp
// Get core patch without borders
af::array corePatch = accessor->getPatch(x, y, frame);

// Write core patch back
accessor->writePatch(x, y, frame, processedCore);
```

## Performance Tips

1. **Use appropriate sync mode** for your workflow
2. **Process patches in batches** when possible
3. **Call `forceSyncToCPU()`** only when GUI needs updates
4. **Keep the same frame cached** when processing multiple patches

## Error Handling

The accessor provides safe defaults:
- Invalid coordinates return empty arrays
- Out-of-bounds patches are clamped safely  
- Border extension respects image boundaries
- All operations are validated before execution