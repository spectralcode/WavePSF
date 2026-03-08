# Tutorial: PSF Estimation for a Single Patch

This guide walks through PSF estimation for a single patch using the tobacco leaf example dataset. Download `data_tobacco_leaf.zip` from the [example datasets](https://doi.org/10.25835/yu47lho4) and extract it. The dataset contains a purposely defocused hyperspectral image of a printed page of text alongside a digitially created ground truth image: 

- hsi_text_defocus_original.tif <- Hyperspectral dataset
- hsi_text_ground_truth_defocus.tif <- Ground truth image

---

## 1. Load the input data

Drag and drop the hyperspectral dataset onto the application window, or use **File → Open Image Data**.

## 2. Load the ground truth

Use **File → Open Ground Truth** and select the ground truth image.

To verify it loaded correctly: click any patch in the output image, then **hold X**-key, this temporarily replaces the display with the ground truth image. Release X-key to return to the normal view. Orient the output image the same way as the input.

## 3. Orient the image

The image may appear flipped or rotated. Click on the input image to give it keyboard focus, then press **H** and then **R**. You may want to rotate/flip the images differntly for different datasets::

- **R** — rotate 90°
- **V** — flip vertically
- **H** — flip horizontally


## 4. Configure coefficient ranges

Open **Extras → Settings**. Under the Zernike generator settings:

- Set **Global min coefficient** to `-3.0` and **Global max coefficient** to `3.0`
- In the per-coefficient override table, find **Noll index 4** (defocus) and set its maximum to `0`; defocus should only be negative to avoid ambiguity with positive defocus. 

Apply and close the dialog.

## 5. Select a patch and set a starting value for defocus

Click on a patch in the output image to select it. For example frame **250**, patch **17**.

In the **PSF Generator** section in the sidebar left, set the **Defocus** coefficient to `-2.0` to provide a reasonable starting value which reduces optimization time.


## 6. Configure the optimizer

Switch to the **Optimization** tab at the bottom and configure the optimization. This requires some testing and is different for different datasets, but here are some good starting values for the tobacco leaf dataset:

**Mode:** `Single Patch`

**Algorithm:** `Simulated Annealing`

| Simulated Annealing parameter | Value |
|---|---|
| Start Temperature | 0.4 |
| End Temperature | 0.0015 |
| Cooling Factor | 0.996 |
| Start Perturbance | 0.2 |
| End Perturbance | 0.02 |
| Iterations/Temperature | 1 |

**Initial Values → Source:** `Current stored`

**Metric:**
- Mode: `Reference Comparison`
- Type: `Normalized Cross Correlation`
- Multiplier: `-100`

**Coefficients to Optimize:** uncheck **Optimize All** and enter `2-8` in the IDs field.

## 7. Run the optimizer

Press **Start Optimization**. The optimizer runs simulated annealing and the metric plot updates live. When finished, the selected patch in the output image should appear noticeably sharper. It may be that the optimizer gets stuck in a local minimum, in which case you can try resetting the Zernike coefficients and simply run it again.

