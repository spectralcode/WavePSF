# WavePSF - Software Architecture Document

## 1. System Overview

### 1.1 Purpose
WavePSF is a desktop application for wavefront-based PSF optimization and deconvolution of RGB and hyperspectral images. It implements the method described in the research paper, enabling researchers and domain experts to improve spatial resolution across all frames and wavelengths through computed wavefront optimization.

### 1.2 Architecture Style
- **Layered Architecture** with Model-View-Controller (MVC) pattern
- **Component-based design** for modularity and maintainability
- **Plugin architecture** for optimization algorithms and deconvolution methods
- **Event-driven GUI** with worker threads for computational tasks

### 1.3 High-Level Architecture
```
┌─────────────────────────────────────────────────────┐
│                   GUI Layer (Qt 5)                   │
│  - Main Window                                       │
│  - Control Panels                                    │
│  - Image Viewers                                     │
│  - Real-time Plots (QCustomPlot)                    │
├─────────────────────────────────────────────────────┤
│              Application Controller                  │
│  - Workflow Management                               │
│  - Event Handling                                    │
│  - Thread Coordination                               │
├─────────────────────────────────────────────────────┤
│   PSF Engine  │  Optimization Engine │  Deconvolution Engine   │
├─────────────────────────────────────────────────────┤
│           Data Management & I/O Layer               │
│  - HSI/Image Loading                                │
│  - Patch Management                                 │
│  - Session Persistence                              │
└─────────────────────────────────────────────────────┘
```

## 2. Core Components

### 2.1 Data Management Layer

#### Responsibilities
- Load/save hyperspectral data (HDR/IMG format pairs)
- Load/save standard images (JPG, PNG, TIFF, RAW)
- Import/export Zernike coefficients (CSV)
- Import/export PSFs (TIFF/PNG)
- Manage patch grid division with configurable borders
- Handle metadata preservation
- Session state persistence (JSON/XML)

#### Key Classes
todo

### 2.2 PSF Engine

#### Responsibilities
- Generate wavefronts from Zernike coefficients
- Convert wavefronts to PSFs using FFT
- Manage coefficient constraints (e.g., defocus sign restriction)
- Handle spatial and spectral interpolation
- Real-time PSF updates for GUI interaction

#### Key Classes
```cpp
class Wavefront {
	// Represents computed wavefront
private:
	af::array phase;  // 2D phase array
	std::vector<double> zernikeCoeffs;
	double wavelength;
	
public:
	void updateFromCoefficients(const std::vector<double>& coeffs);
	af::array computePSF() const;
};

class ZernikeGenerator {
	// Generates Zernike polynomials (Noll indexing)
	// No maximum limit on Noll indices
	af::array generatePolynomial(int nollIndex, int size);
	af::array combinePolynomials(const std::vector<double>& coeffs);
};

class PSFCalculator {
	// Converts wavefront to PSF
private:
	af::array pupilFunction;
	FFTWrapper fft;
	
public:
	af::array wavefrontToPSF(const Wavefront& wavefront);
};

class InterpolationEngine {
	// 3rd-order polynomial interpolation
	// Spatial and spectral dimensions
	std::vector<double> interpolateCoefficients(
		const std::map<Position, std::vector<double>>& keyPoints,
		const Position& targetPos
	);
};
```

### 2.3 Optimization Engine

#### Responsibilities
- Implement optimization algorithms
- Calculate quality metrics
- Manage optimization parameters and constraints
- Support both ground-truth and no-reference metrics

#### Key Classes
```cpp
class IOptimizer {
	// Abstract interface for optimization algorithms
public:
	virtual OptimizationResult optimize(
		const Patch& input,
		const Patch* groundTruth,  // Optional
		const OptimizationParams& params
	) = 0;
};

class SimulatedAnnealing : public IOptimizer {
	// Primary optimization algorithm
private:
	double temperature;
	double coolingRate;
	double perturbationFactor;
	std::vector<CoefficientConstraint> constraints;
};

class ImageMetricCalculator {
	// Quality metrics implementation
public:
enum ImageMetric{
	NORMALIZED_SUM_SQUARED_DIFFERENCES = 0,
	NORMALIZED_CROSS_CORRELATION = 1,
	SUM_OF_HAMMING_DISTANCE_BINARY = 2,
	SUM_DIFFERENCES =3
  ...
};
  void setMetricsetImageMetric(ImageMetric metric);
  void calculate(af::array image, af::array reference);
  //somehow find way to include metrics that do not require reference image
};

class CoefficientConstraint {
	// Manages constraints on Zernike coefficients
	// e.g., defocus must be negative/positive only
	int nollIndex;
	double minValue;
	double maxValue;
	bool enforceSign;
};
```

### 2.4 Deconvolution Engine

#### Responsibilities
- Implement multiple deconvolution algorithms
- Handle patch-wise processing with border management
- Provide real-time preview capability
- Manage algorithm-specific parameters

#### Key Classes
```cpp
class Deconvolver {
	// Abstract interface for deconvolution
public:
	enum DeconvolutionAlgo {
		RICHARDSON_LUCY,
		LANDWEBER,
		TIKHONOV,
		WIENER,
		CONVOLUTION,
	};
	void setAlgorithm(DeconvolutionAlgo algo);
af::array deconvolve(const af::array& blurredInput, const af::array& psfKernel);
};


class PatchProcessor {
	// Handles patch-wise deconvolution
	af::array processPatch(
		const af::array& patch,
		const af::array& psf,
		Deconvolver* deconvolver,
		int borderExtension
	);
};
```

### 2.5 Application Controller

#### Responsibilities
- Orchestrate workflow between components
- Handle thread management for long operations
- Coordinate GUI updates
- Manage application state

#### Key Classes
```cpp
class ApplicationController {
private:
	std::unique_ptr<PSFEngine> psfEngine;
	std::unique_ptr<OptimizationEngine> optimizationEngine;
	std::unique_ptr<DeconvolutionEngine> deconvolutionEngine;
	std::unique_ptr<DataManager> dataManager;
	
public:
	void processDataset();
	void optimizePatch(int x, int y, int frame);
	void updatePreview();
};

class ComputeConfig {
	// ArrayFire backend configuration
public:
	static void initialize() {
		// Try GPU backends first, fall back to CPU
		try {
			af::setBackend(AF_BACKEND_CUDA);
		} catch(...) {
			try {
				af::setBackend(AF_BACKEND_OPENCL);
			} catch(...) {
				af::setBackend(AF_BACKEND_CPU);
			}
		}
	}
	
	static af::Backend getCurrentBackend() {
		return af::getActiveBackend();
	}
};
```

### 2.6 GUI Components

#### Main Window
- Dual image viewers (input/deconvolved) with synchronized navigation
- Patch grid overlay visualization
- Status bar with current operation info

#### Control Panels
- **Zernike Control Panel**
  - Dynamic slider generation based on selected Noll indices
  - Slider and QDoubleSpinbox input fields for precise values
  - Reset buttons for coefficient groups
  
- **Optimization Control Panel**
  - Algorithm selection
  - Parameter configuration
  - Start/stop/pause controls
  - Progress visualization
  
- **Deconvolution Control Panel**
  - Algorithm selection dropdown
  - Algorithm-specific parameter controls
  - Preview toggle

#### Visualization Components
- **PSF Viewer**: 2D intensity display
- **Wavefront Plot**: 2D phase map (QCustomPlot)
- **Metric Plot**: Real-time optimization progress (QCustomPlot)

## 3. Data Flow

### 3.1 Manual PSF Adjustment Flow
```
1. User adjusts Zernike coefficient slider
2. GUI emits coefficientChanged signal
3. PSFEngine updates wavefront
4. PSF recalculated via FFT
5. DeconvolutionEngine processes current patch
6. Preview and metric plot updated in real-time
```

### 3.2 Optimization Workflow
```
1. User selects patches (corners + edges + center)
2. Sets optimization parameters
3. For each selected patch:
   a. Initialize with manual/previous coefficients
   b. Optimization loop:
      - Generate PSF from coefficients
      - Deconvolve patch
      - Calculate metric (NCC/MSE/SSIM)
      - Adjust coefficients (Simulated Annealing)
   c. Store optimized coefficients
4. Interpolate coefficients for remaining patches
5. Generate full PSF set
6. Apply to complete dataset
```

### 3.3 Batch Processing Flow (Future)
```
1. Load multiple datasets
2. Apply saved PSF profiles or optimization settings
3. Process in queue with progress reporting
4. Export results with metadata preservation
```

## 4. Threading Model

### 4.1 Thread Architecture
- **Main GUI Thread**: UI updates, event handling
- **Optimization Worker Thread**: Long-running optimization tasks
- **Preview Worker Thread**: Real-time deconvolution preview. todo: rethink if this thread is really required
- **I/O Thread**: File operations (loading/saving large datasets)

### 4.2 Thread Safety
- ArrayFire operations are thread-safe per context
- Use Qt's signal-slot mechanism for thread communication
- Mutex protection for shared coefficient data

## 5. Performance Considerations

### 5.1 Target Performance
- Single patch PSF generation: microseconds to milliseconds
- Full dataset processing: minutes to hours acceptable
- Real-time preview update: <100ms
- Typical dataset: 1024×1024×500 (300-600 MB)

### 5.2 Optimization Strategies
- **Memory Management**
  - Pre-allocate patch buffers
  - Reuse FFT plans
  - Keep frequently used PSFs in GPU memory
  
- **Computation Optimization**
  - Parallel patch processing where possible
  - Lazy PSF generation (compute only when needed)
  - Cache interpolated coefficients
  
- **GPU Utilization**
  - Batch operations when possible
  - Minimize CPU-GPU transfers
  - Use ArrayFire's automatic memory management

## 6. Configuration System

### 6.1 User Preferences
```yaml
# Default configuration
wavelength:
  min: 400  # nm
  max: 700  # nm
  
gpu:
  enabled: true
  preferred_backend: "CUDA"  # CUDA/OpenCL/CPU
  
optimization:
  default_algorithm: "SimulatedAnnealing"
  default_metric: "NCC"
  
ui:
  theme: "default"
  autosave_session: true
```

### 6.2 System Configuration
- Memory limits for patch processing
- Thread pool sizes
- Cache sizes
- Default file paths

## 7. Extension Points

### 7.1 Planned Extensions
- Additional optimization algorithms 
- New quality metrics 
- Additional deconvolution methods 
- Batch processing system
- Alternative wavefront models, like generating wavefronts based on a deformable mirror simulation (future consideration)

### 7.2 Plugin Architecture
- Optimization algorithms as plugins
- Metrics as plugins
- File format loaders as plugins
//todo rethink if plugin architecture is needed of if it makes everything too complex. clarify what "plugins" are

## 8. Build System

### 8.1 Dependencies
- **Required**
  - Qt 5.x
  - ArrayFire 3.8.x
  - FFTW 3.3.x
  - QCustomPlot 2.x
  
- **Optional**
  - Eigen 3.4.x (for polynomial fitting if ArrayFire insufficient)


## 9. Development Phases

### Phase 1: Core Functionality
1. Basic data I/O (HSI, images)
2. PSF generation from Zernike coefficients (with option to extend it later to use a deformable mirror simulation to generate wavefronts instead of zernike polynomials)
3. Deconvolver
4. Manual coefficient adjustment GUI

### Phase 2: Optimization
1. Simulated Annealing implementation
2. Patch-wise processing
3. Real-time preview
4. Basic interpolation

### Phase 3: Extended Features
1. Additional deconvolution algorithms
2. Multiple quality metrics
3. Session management
4. Advanced interpolation

### Phase 4: Polish & Performance
1. GPU optimization
2. UI refinements
3. Batch processing
4. Documentation

## 10. Testing Strategy

### 10.1 Unit Testing
- PSF generation accuracy
- Deconvolution correctness
- Metric calculations
- Interpolation accuracy

### 10.2 Integration Testing
- Full optimization workflow
- I/O with various formats
- GPU/CPU backend switching
- Session save/load

### 10.3 Performance Testing
- Benchmark against paper's results
- Memory usage profiling
- GPU utilization analysis
- Real-time preview latency
todo: rethink what actual performance metrics are interesting. probably it is intersting to know how long it takes to process a full hsi data set.  