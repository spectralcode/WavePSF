# WavePSF - Changelog

## Version 1.1.0 (2026-03-08)

### Added

- Viewer control toolbar: Rotate, flip, toggle patch grid visibility, sync views, and show/hide axis indicator
- X/Y axis indicator: An overlay in the image viewers shows the current X and Y directions, updating correctly after rotations and flips. Can help identifying correct direction for interpolation.
- Recent files: `File > Recent Input Files` and `File > Recent Ground Truth Files` for quickly opening recently used datasets
- Arrow key navigation: Navigate between patches in the input and output viewers using the arrow keys
- ArrayFire device selection: Choose the compute backend (CPU, CUDA, OpenCL) and device directly from the settings dialog
- Interpolation plot controls: Zoom, pan, and reset the view in the interpolation plot. This now matches the optimization plot controls.
- The last selected tab (Deconvolution, Optimization, etc.) is now restored on restart
- Range fit in wavefront plot: double-click on color bar or right-click "Fit Range to Data" to adjust the color scale to the current value range

### Changed

- "Save/Load Parameters" renamed to "Save/Load Wavefront Coefficients"
- Changed various default values (deformable mirror coefficient range, SA optimization parameters, ...)
- When using the "From specific frame" option for initial optimization values, both patch number and frame number now need to be specified. 
- The phase scale factor applied to the wavefront during PSF calculation is no longer hardcoded and can now be configured in the settings dialog. Default value is 1.0 (previously 14.240). With this change, larger Zernike coefficients (and larger "perturbance" value for SA optimization) are required to produce the same PSF as with the previous version.
- Application settings file (wavepsf.ini) now uses human-readable format (no more binary blobs)

### Fixed

- Batch optimization now correctly tracks and writes output for each patch individually
- Patch grid updates correctly when loading a file with different dimensions
- Opening a new input file preserves the current frame selection and active patch position
- Aperture radius no longer gets corrupted when reopening the settings dialog with non-default deformable mirror settings, which previously caused black patches in the output viewer after using settings dialog
- Running optimization is now properly cancelled when closing the application
- First patch is no longer overwritten when loading new input data
- Pressing V or H when a viewer is focused now flips in the correct direction: V for vertical, H for horizontal.



## Version 1.0.0 (2026-03-05)
- First release