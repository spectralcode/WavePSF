# Batch Deconvolution

Deconvolves all patches of every frame in one go using stored Zernike coefficients.

## Workflow

1. Load input image data (**File > Open Image Data**)
2. Load a CSV parameter file (**File > Load Wavefront Coefficients**)
3. Run **Processing > Deconvolve All Frames** (Ctrl+Shift+D)

A progress dialog shows the current frame and patch. Press **Cancel** to stop early — patches processed so far are kept.

After completion the output viewer updates to show the deconvolved result. Save the output via **File > Save Output Data**.

## Notes

- The menu action is grayed out until a CSV parameter file has been loaded.
- Deconvolution uses the algorithm and settings configured in the Deconvolution tab (algorithm, iterations, etc.).
- The current Zernike slider values are restored after the batch finishes.
