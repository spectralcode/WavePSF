# 3D Volumetric Deconvolution
How to implement 3D deconvolution in WavePSF? Let's first check whats out there in the open-source and commercial space.


## Comparison with Open-Source 3D RL Implementations

| Feature | DeconvolutionLab2 | scikit-image | Flowdec (TensorFlow) | RedLionfish | YacuDecu |
|---|---|---|---|---|---|
| Language | Java (JTransforms) | Python (SciPy) | Python (TensorFlow) | Python (reikna/OpenCL) | CUDA C |
| FFT backend | JTransforms (CPU) | SciPy fftpack (CPU) | TensorFlow FFT (GPU) | reikna FFT (GPU) | cuFFT |
| Acceleration | Vector/Tikhonov-Miller | ? | ? | ? | ? |
| Early stopping | ? | ? | ? | ? | ? |
| TV regularization | Yes (option) | ? | ? | ? | ? |
| Padding strategy | Power of 2? | ? | Power of 2? | ? | ? |
| Memory management | Manual tiling | NumPy auto | TF auto | reikna auto | Manual |
| PSF handling | Normalize + circshift | Normalize only | Normalize + circshift | Normalize + circshift | Normalize only (bug: uses H not conj(H)???) |
| Source code | [RichardsonLucy.java](https://github.com/Biomedical-Imaging-Group/DeconvolutionLab2/blob/master/src/main/java/deconvolution/algorithm/RichardsonLucy.java) | [richardson_lucy()](https://github.com/scikit-image/scikit-image/blob/main/skimage/restoration/deconvolution.py) | [RichardsonLucyDeconvolver](https://github.com/hammerlab/flowdec/blob/master/python/flowdec/restoration.py) | [doRLDeconvolutionFromNP()](https://github.com/rdemaria/redlionfish/blob/master/redlionfish/rl_deconv.py) | [deconv()](https://github.com/krm15/yacudecu/blob/master/src/deconv.cu) |
| Repository  | [DeconvolutionLab2](https://github.com/Biomedical-Imaging-Group/DeconvolutionLab2) | [scikit-image](https://github.com/scikit-image/scikit-image) | [flowdec](https://github.com/hammerlab/flowdec) | [redlionfish](https://github.com/rdemaria/redlionfish) | [yacudecu](https://github.com/krm15/yacudecu) |
| Documentation |  [DeconvolutionLab2 site](http://bigwww.epfl.ch/deconvolution/deconvolutionlab2/) | [scikit-image docs](https://scikit-image.org/docs/stable/api/skimage.restoration.html#skimage.restoration.richardson_lucy) | [flowdec README](https://github.com/hammerlab/flowdec#readme) | [redlionfish README](https://github.com/rdemaria/redlionfish#readme) | [yacudecu README](https://github.com/krm15/yacudecu#readme) |



## Comparison with Commercial Software

| Feature  | [Huygens](https://svi.nl/Huygens-Deconvolution) (SVI) | [AutoQuant X3](https://mediacy.com/image-pro/autoquant-deconvolution/) (Media Cybernetics) | [Imaris ClearView](https://imaris.oxinst.com/products/clearview-gpu-deconvolution) (Oxford Instruments) | [NIS-Elements](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/deconv.settings.3d.html) (Nikon) |
|---|---|---|---|---|
| Algorithm | [CMLE](https://svi.nl/ClassicMaximumLikelihoodEstimation) (MLE-based, Poisson) | RL + Adaptive Blind | Enhanced RL + Inverse filter | RL + Landweber + Blind |
| Acceleration | Proprietary (QMLE fast variant) | Undisclosed |  "heavy-ball" (mentioned in the tutorial video) | None documented |
| Early stopping | Yes (quality criterion) | Yes (auto) | Undisclosed | Undisclosed |
| GPU acceleration | CUDA | CUDA | CUDA + AMD | Undisclosed |
| PSF options | Measured + theoretical + distilled | Measured + theoretical + blind | Measured + [Gibson-Lanni](https://imaris.oxinst.com/products/clearview-gpu-deconvolution) model | Measured + theoretical + blind + depth-dependent |
| Depth-dependent PSF | Yes | Yes (adaptive) | Yes (spherical aberration correction) | Yes |
| Auto noise estimation | Yes (SNR parameter) | Yes | Undisclosed | Yes |
| TV regularization | No | No | No | No |
| Batch processing || Yes | Yes | Yes | Yes |
| Documentation | [Deconvolution Algorithms](https://svi.nl/Deconvolution-Algorithms), [CMLE](https://svi.nl/ClassicMaximumLikelihoodEstimation) | [Product page](https://mediacy.com/image-pro/autoquant-deconvolution/), [Quick Start (PDF)](https://www.bitplane.com/download/manuals/AQX301/QuickStart.pdf) | [ClearView product page](https://imaris.oxinst.com/products/clearview-gpu-deconvolution), [Tutorial](https://imaris.oxinst.com/learning/view/article/deconvolution-software-microscopy-images-analysis-gpu) | [3D Deconvolution](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/deconv.settings.3d.html), [Choosing method](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/howto.deconv.choose.method.html) |
