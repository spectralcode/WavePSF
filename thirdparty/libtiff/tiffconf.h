/*
 * tiffconf.h — hand-configured for WavePSF's bundled libtiff.
 * Supports: uncompressed, LZW, PackBits, ThunderScan, NeXT.
 * No external library dependencies required.
 * Targets: x86-64 little-endian (Windows MSVC, Linux GCC/Clang).
 */
#ifndef _TIFFCONF_
#define _TIFFCONF_

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

#define TIFF_INT8_T    int8_t
#define TIFF_INT16_T   int16_t
#define TIFF_INT32_T   int32_t
#define TIFF_INT64_T   int64_t
#define TIFF_UINT8_T   uint8_t
#define TIFF_UINT16_T  uint16_t
#define TIFF_UINT32_T  uint32_t
#define TIFF_UINT64_T  uint64_t
#define TIFF_SSIZE_T   ptrdiff_t

#define HAVE_IEEEFP 1
#define HOST_FILLORDER FILLORDER_LSB2MSB
#define HOST_BIGENDIAN 0

#define LZW_SUPPORT      1
#define PACKBITS_SUPPORT 1
#define THUNDER_SUPPORT  1
#define NEXT_SUPPORT     1

#define SUBIFD_SUPPORT 1
#define STRIPCHOP_DEFAULT TIFF_STRIPCHOP
#define DEFAULT_EXTRASAMPLE_AS_ALPHA 1
#define MDI_SUPPORT 1

/* backward compatibility */
#define COLORIMETRY_SUPPORT
#define YCBCR_SUPPORT
#define CMYK_SUPPORT
#define ICC_SUPPORT
#define PHOTOSHOP_SUPPORT
#define IPTC_SUPPORT

#endif /* _TIFFCONF_ */
