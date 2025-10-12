# WavePSF - Requirements Specification

**Wavefront-based PSF Optimization for Deconvolution of RGB and Hyperspectral Images**


## Overview
WavePSF is an image deconvolution application that estimates Point Spread Functions (PSFs) using Zernike polynomial–based wavefronts and optimization algorithms to improve spatial resolution across all frames, channels, or wavelengths of RGB, multispectral, or hyperspectral images. In addition to automatic optimization, WavePSF also allows users to manually generate PSFs by setting Zernike polynomial coefficients and applying them in space-variant deconvolution, where the image is divided into patches.

## Functional Requirements

### 1. Data Management
- **Input Support**
  - Load hyperspectral image data (common HSI formats)
  - Load standard images (JPG, PNG, TIFF, RAW)
  - Load ground truth image
  - Import Zernike coefficients from CSV files
  - Import pre-computed PSFs from image files
  - Load reference/ground truth images for optimization
  
- **Output Support**
  - Export deconvolved images (multiple formats)
  - Export PSF images (TIFF/PNG)
  - Export Zernike coefficients to CSV
  - Save/load complete session state (parameters, coefficients, GUI layout)
  - Export processing parameters to JSON/XML

### 2. Patch Processing System
- User-configurable patch grid (X × Y divisions)
- Adjustable patch size (width × height)
- Configurable border extension (default: 10 pixels)
- Individual patch selection via mouse click
- Multi-patch selection (Shift/Ctrl+click)
- Copy/paste Zernike coefficients between patches via context menu

### 3. PSF Generation and Manipulation
- **Manual Mode**
  - Real-time Zernike coefficient adjustment via sliders
  - Numerical input for precise coefficient values
  - Coefficient range: Noll indices 2-8 (tip, tilt, defocus, astigmatisms, comas)
  - Instant PSF preview upon coefficient changes
  - Reset coefficients to default values
  
- **Display**
  - 2D PSF intensity visualization
  - 3D wavefront surface plot
  - Real-time metric calculation and plotting
  - User-selectable metrics (NCC, MSE, SSIM)

### 4. Optimization System
- **Algorithms**
  - Simulated Annealing (primary)
  - Support for additional algorithms (future)
  
- **Configuration**
  - Select optimization metric (with or without ground truth image)
  - Set temperature parameters (start, end, cooling rate)
  - Configure perturbation factor
  - Define iterations per temperature step
  - Select which Zernike coefficients to optimize
  
- **Batch Processing**
  - Optimize selected patches per frame
  - Specify frame numbers for multi-frame optimization
  - Use optimized coefficients as starting point for next patch/frame

### 5. Deconvolution Processing
- **Algorithms**
  - Richardson-Lucy (with iteration control)
  - Wiener (with NSR parameter)
  - Tikhonov (with regularization parameter)
  - Landweber (with step size and iterations)
  
- **Features**
  - Real-time single-patch preview
  - Full dataset processing
  - Patch border extension handling
  - GPU acceleration for performance

### 6. Interpolation System
- Polynomial interpolation of Zernike coefficients
- User-configurable polynomial order (1st-5th)
- Spatial interpolation (across patches)
- Spectral interpolation (across frames/wavelengths)
- Combined spatio-spectral interpolation

### 7. User Interface
- **Main Display Areas**
  - Input image viewer with patch grid overlay
  - Deconvolved image viewer (synchronized navigation)
  - PSF visualization panel
  - Wavefront display (2D map using qcustomplot)
  - Metric plots (optimization progress, manual mode)
  
- **Control Panels**
  - Zernike coefficient sliders (dynamically generated)
  - Optimization control (start/stop/pause)
  - Deconvolution algorithm selection
  - Patch/frame navigation controls
  
- **Status Information**
  - Current patch coordinates
  - Current metric values
  - Processing progress
  - Temperature (during optimization)
  - Iteration counter

## Non-Functional Requirements

### Performance
- Real-time PSF preview: <100ms response time
- Real-time deconvolution preview for single patches
- GPU acceleration via ArrayFire library
- Efficient memory management for large datasets

### Technical Stack
- **Language**: C++
- **GUI Framework**: Qt 5
- **GPU Computing**: ArrayFire
- **FFT Library**: FFTW or ArrayFire 
- **Matrix Operations**: Eigen (only if no simpler option is available for polynomial fitting to interpolate Zernike coefficients)
- **Plotting**: QCustomPlot

### Usability
- Save/restore complete workspace state
- Undo/redo for coefficient changes
- Synchronized zoom/pan between image viewers
- Tooltips for all controls
- Keyboard shortcuts for common operations

## Priorities
1. **Core**: PSF generation from Zernike coefficients
2. **Core**: Real-time deconvolution with Richardson-Lucy
3. **Core**: Manual coefficient adjustment with live preview
4. **High**: Simulated Annealing optimization
5. **High**: Patch-wise processing system
6. **Medium**: Interpolation system
7. **Medium**: Session save/load
8. **Low**: Additional deconvolution algorithms
9. **Low**: Additional optimization algorithms