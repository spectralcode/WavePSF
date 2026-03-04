# TIFF Backend

WavePSF needs to load/save TIFF files. Initially, ArrayFire's built-in TIFF loading capabilities were used. However, ArrayFire only loads the first frame of multi-frame TIFFs. Qt's `QImageReader` can load multi-frame TIFFs but only supports 8-bit depth in Qt 5.12. Upgrading to Qt 5.13+ would add 16-bit support but still not support 32-bit or floating-point TIFFs. So I decided to use libtiff for TIFF file handling. 

To reduce external dependencies, a minimal subset of the libtiff source code is bundled directly in the project. 

There is a qmake `CONFIG` flag to switch between the libtiff backend and the ArrayFire backend for TIFF loading. The default is libtiff. This flag might be removed in the future if I can confirm that the libtiff backend runs on different machines without issues.

