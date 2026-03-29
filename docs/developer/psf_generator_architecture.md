# PSF Generator Architecture

## Overview

PSF generation uses a two-layer composition. An `IWavefrontGenerator` produces phase arrays from user-controlled coefficients, and an `IPSFPropagator` converts those phase arrays into PSF intensities. `ComposedPSFGenerator` wires them together behind the unified `IPSFGenerator` interface. `PSFGeneratorFactory` instantiates generators by type name.

```
IPSFGenerator  (top-level interface — "give me a PSF")
    │
    └─ ComposedPSFGenerator  (composition)
         ├─ IWavefrontGenerator  ("phase from coefficients")
         │    ├─ ZernikeGenerator
         │    └─ DeformableMirrorGenerator
         │
         └─ IPSFPropagator  ("phase → PSF intensity")
              ├─ PSFCalculator  (2D scalar FFT)
              └─ RichardsWolfCalculator  (3D vectorial)
```


## Interfaces

| Interface | Role | Key Methods |
|---|---|---|
| `IPSFGenerator` | Unified PSF generation | `generatePSF(gridSize)`, `typeName()`, `serializeSettings()`, `deserializeSettings()`, `getParameterDescriptors()`, `is3D()` |
| `IWavefrontGenerator` | Phase array from coefficients | `generateWavefront(gridSize)`, `setCoefficient(id, value)`, `getParameterDescriptors()` |
| `IPSFPropagator` | Phase → PSF conversion | `computePSF(wavefront, gridSize)`, `is3D()`, `getSettingsDescriptors()` |

`IPSFGenerator` also exposes `getSettingsDescriptors()` for auto-generated settings UI and `applyInlineSettings()` for runtime parameter updates from inline widgets.


## ComposedPSFGenerator

Takes ownership of one `IWavefrontGenerator` and one `IPSFPropagator`. Its `generatePSF()` calls `generator->generateWavefront()` then `propagator->computePSF()`. Coefficient methods delegate to the generator; capability queries (`is3D()`, `getApertureGeometry()`) delegate to the propagator.

Settings are stored in a composed format:
```json
{
  "generator_settings": { "noll_index_spec": "2-22", "global_min": -3.0, ... },
  "propagator_settings": { "phase_scale": 1.0, "wavelength_nm": 500, ... }
}
```

`serializeSettings()` merges both sub-maps; `deserializeSettings()` splits and forwards each to the respective component.


## PSFGeneratorFactory

Creates fully configured `IPSFGenerator` instances by type name.

| Type Name | Wavefront Generator | Propagator | 3D |
|---|---|---|---|
| `"Zernike"` | `ZernikeGenerator` | `PSFCalculator` | No |
| `"Deformable Mirror"` | `DeformableMirrorGenerator` | `PSFCalculator` | No |
| `"3D PSF Microscopy"` | `ZernikeGenerator` | `RichardsWolfCalculator` | Yes |

Source: `src/core/psf/psfgeneratorfactory.cpp`


## How to Add a New PSF Generator

1. **Decide what's new**: a wavefront generator, a propagator, or a new combination of existing ones.

2. **Implement the interface(s)**:
   - New wavefront source → implement `IWavefrontGenerator` (see `ZernikeGenerator` for reference)
   - New propagation method → implement `IPSFPropagator` (see `PSFCalculator` for 2D or `RichardsWolfCalculator` for 3D)

3. **Register in `PSFGeneratorFactory`** (`src/core/psf/psfgeneratorfactory.cpp`):
   - Add a case in `create()` that constructs a `ComposedPSFGenerator` with your generator + propagator
   - Add the type name string to `availableTypeNames()`

4. **Add to the build** — list new `.h`/`.cpp` files in `wavepsf.pro` under `HEADERS` / `SOURCES`.

5. **Settings UI** — if your propagator or generator exposes `getSettingsDescriptors()`, the settings dialog will auto-generate spinboxes/combos for those settings. No manual UI code needed for basic numeric parameters.
