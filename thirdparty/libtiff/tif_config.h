/*
 * tif_config.h — hand-configured for WavePSF's bundled libtiff.
 * Targets: Windows (MSVC x64) and Linux (GCC/Clang x64).
 */
#include "tiffconf.h"

#define PACKAGE           "libtiff"
#define PACKAGE_NAME      "LibTIFF Software"
#define PACKAGE_BUGREPORT "tiff@lists.maptools.org"
#define PACKAGE_TARNAME   "tiff"
#define PACKAGE_URL       ""

#define TIFF_MAX_DIR_COUNT 1048576
#define STRIP_SIZE_DEFAULT 8192
#define SIZEOF_SIZE_T      8

#ifdef _WIN32
#  define USE_WIN32_FILEIO 1
#  define HAVE_FCNTL_H     1
#  define HAVE_IO_H        1
#  define HAVE_SETMODE     1
#  define HAVE_ASSERT_H    1
#else
#  define HAVE_ASSERT_H    1
#  define HAVE_FCNTL_H     1
#  define HAVE_UNISTD_H    1
#  define HAVE_SYS_TYPES_H 1
#  define HAVE_MMAP        1
#  define HAVE_FSEEKO      1
#  define HAVE_STRINGS_H   1
#endif

/* little-endian — WORDS_BIGENDIAN intentionally not defined */

#ifndef __MINGW32__
#  define TIFF_SIZE_FORMAT "zu"
#endif
#if SIZEOF_SIZE_T == 8
#  define TIFF_SSIZE_FORMAT PRId64
#  ifdef __MINGW32__
#    define TIFF_SIZE_FORMAT PRIu64
#  endif
#else
#  define TIFF_SSIZE_FORMAT PRId32
#  ifdef __MINGW32__
#    define TIFF_SIZE_FORMAT PRIu32
#  endif
#endif
