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

## Performance Tips

1. **Process patches in batches** when possible
2. **Keep the same frame cached** when processing multiple patches

## Error Handling

The accessor provides safe defaults:
- Invalid coordinates return empty arrays
- Out-of-bounds patches are clamped safely  
- Border extension respects image boundaries
- All operations are validated before execution