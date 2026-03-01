# PSF Menu

The PSF menu provides tools for saving, loading, and managing Point Spread Functions independently of the Zernike coefficient workflow.

## Load PSF from File

Loads a PSF image from disk and applies it to the **current patch**. Supported formats: TIFF (32-bit float preserved), PNG, JPG, BMP (converted to grayscale, normalized to [0, 1]).

- The loaded PSF is used for deconvolution instead of the computed Zernike PSF.
- The loaded PSF **persists across patch switches** — switching away and back restores it.
- Moving any Zernike slider or resetting coefficients **clears the loaded PSF** for that patch and returns to computed mode.

## Save PSF

Saves the current PSF (computed or loaded) as a 32-bit float TIFF to a user-chosen location.

## Auto Save PSF

When enabled, every PSF update (slider change, optimization result, etc.) automatically saves the PSF to the configured save folder. Files are named `frame_patch.tif` (e.g., `0_0.tif`, `5_3.tif`).

**Setup:**
1. Set the save folder via **Set PSF Save Folder...**
2. Enable **Auto Save PSF**


## Custom PSF Folder

Loads PSFs automatically from a folder when switching patches/frames, bypassing coefficient-based computation entirely. Each patch looks for a file named `frame_patch.*` in the folder (e.g., `0_0.tif`, `5_3.tif`). Any image format is accepted.

**Setup:**
1. Set the folder via **Set Custom PSF Folder...**
2. Enable **Use Custom PSF Folder**

If no matching file is found for a patch, WavePSF falls back to computing the PSF from stored Zernike coefficients.

