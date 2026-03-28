# Volumetric Deconvolution

How to implement 3D deconvolution in WavePSF? Let's check whats out there in the open-source and commercial space.

## Open-Source 3D Deconvolution Implementations

| Name | Backend | Links |
|---|---|---|
| **DeconvolutionLab2** | Java FFT backend (framework-configurable) | [docs](https://bigwww.epfl.ch/deconvolution/deconvolutionlab2/)<br>[RL source](https://github.com/Biomedical-Imaging-Group/DeconvolutionLab2/blob/master/src/main/java/deconvolution/algorithm/RichardsonLucy.java)<br>[RLTV source](https://github.com/Biomedical-Imaging-Group/DeconvolutionLab2/blob/master/src/main/java/deconvolution/algorithm/RichardsonLucyTV.java) |
| **scikit-image** | `scipy.signal.convolve` | [docs](https://scikit-image.org/docs/stable/api/skimage.restoration.html#skimage.restoration.richardson_lucy)<br>[source](https://github.com/scikit-image/scikit-image/blob/main/skimage/restoration/deconvolution.py) |
| **Flowdec (TensorFlow)** | TensorFlow FFT | [README](https://github.com/hammerlab/flowdec#readme)<br>[source](https://github.com/hammerlab/flowdec/blob/master/python/flowdec/restoration.py) |
| **RedLionfish** | CPU: SciPy FFT<br>GPU: Reikna/OpenCL FFT | [README](https://github.com/rosalindfranklininstitute/RedLionfish#readme)<br>[CPU source](https://github.com/rosalindfranklininstitute/RedLionfish/blob/main/RedLionfishDeconv/RLDeconv3DScipy.py)<br>[GPU source](https://github.com/rosalindfranklininstitute/RedLionfish/blob/main/RedLionfishDeconv/RLDeconv3DReiknaOCL.py) |
| **YacuDecu** | cuFFT | [README](https://github.com/bobpepin/YacuDecu#readme)<br>[CUDA source](https://github.com/bobpepin/YacuDecu/blob/master/deconv.cu) |
| **cudaDecon / pycudadecon** | CUDA/C++ core (`cudaDecon`)<br>Python wrapper (`pycudadecon`) | [cudaDecon README](https://github.com/scopetools/cudadecon#readme)<br>[cudaDecon source](https://github.com/scopetools/cudadecon/tree/main/src)<br>[pycudadecon docs](https://www.talleylambert.com/pycudadecon/)<br>[pycudadecon source](https://github.com/tlambert03/pycudadecon/tree/main/src/pycudadecon) |
| **pyDeCon** | Python RL library; useful public mirror of MATLAB-style `deconvlucy` / `corelucy` | [README](https://github.com/david-hoffman/pyDeCon#readme)<br>[deconvlucy.m mirror](https://github.com/david-hoffman/pyDeCon/blob/master/notebooks/deconvlucy.m)<br>[corelucy.m mirror](https://github.com/david-hoffman/pyDeCon/blob/master/notebooks/corelucy.m) |
| **DeconvLR** | CUDA/C++ (accelerated RL + TV regularization) | [README](https://github.com/y3nr1ng/DeconvLR#readme)<br>[source tree](https://github.com/y3nr1ng/DeconvLR/tree/main/src)<br>[include tree](https://github.com/y3nr1ng/DeconvLR/tree/main/include) |
| **ThreeDeconv.jl** | Julia (convex 3D deconvolution, **not RL**) | [README](https://github.com/computational-imaging/ThreeDeconv.jl#readme)<br>[source tree](https://github.com/computational-imaging/ThreeDeconv.jl/tree/master/src) |



## Commercial Software

| Name | Links |
|---|---|
| **Huygens** | [Product](https://svi.nl/Huygens-Deconvolution)<br>[Algorithms](https://svi.nl/Deconvolution-Algorithms)<br>[CMLE](https://svi.nl/ClassicMaximumLikelihoodEstimation)<br>[GPU](https://svi.nl/HuygensGPU) |
| **AutoQuant Deconvolution** | [Product](https://mediacy.com/image-pro/autoquant-deconvolution/)<br>[White paper](https://mediacy.com/wp-content/uploads/2023/03/AutoQuant-Deconvolution-White-Paper.pdf)<br>[Quick Start (PDF)](https://www.bitplane.com/download/manuals/AQX301/QuickStart.pdf) |
| **Imaris ClearView** | [Product](https://imaris.oxinst.com/products/clearview-gpu-deconvolution)<br>[Tutorial / article](https://www.oxinst.com/learning/view/article/clearview-gpu-deconvolution-put-those-photons-back-where-they-came-from)<br>[Packages](https://imaris.oxinst.com/packages) |
| **NIS-Elements** | [3D Deconvolution](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/deconv.settings.3d.html)<br>[Choosing method](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/howto.deconv.choose.method.html)<br>[PSF handling](https://www.nisoftware.net/NikonSaleApplication/Help/Docs-AR/eng_ar/howto.deconv.psf.html) |
| **Microvolution** | [Overview](https://www.biovision-technologies.com/microvolution.html)<br>[ImageJ manual (PDF)](https://imb.uq.edu.au/files/31107/Microvolution%20manual%20-%20IJ.pdf) |
| **ZEISS Deconvolution Toolkit** | [Product](https://www.zeiss.com/microscopy/en/products/software/zeiss-zen/deconvolution-toolkit.html)<br>[Practical guide (PDF)](https://pages.zeiss.com/rs/896-XMS-794/images/ZEISS-Microscopy_A-Practical-Guide-of-Deconvolution.pdf) |