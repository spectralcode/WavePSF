# libtiff.pri — bundled libtiff backend
# Active by default. To use the ArrayFire backend instead, uncomment the next line:
#CONFIG+=tiff_backend_arrayfire

!contains(CONFIG, tiff_backend_arrayfire) {
	DEFINES += WAVEPSF_LIBTIFF_BACKEND

	LIBTIFF_DIR = $$shell_path($$PWD/../thirdparty/libtiff)
	INCLUDEPATH += $${LIBTIFF_DIR}

	SOURCES += \
		$${LIBTIFF_DIR}/tif_aux.c \
		$${LIBTIFF_DIR}/tif_close.c \
		$${LIBTIFF_DIR}/tif_codec.c \
		$${LIBTIFF_DIR}/tif_compress.c \
		$${LIBTIFF_DIR}/tif_dir.c \
		$${LIBTIFF_DIR}/tif_dirinfo.c \
		$${LIBTIFF_DIR}/tif_dirread.c \
		$${LIBTIFF_DIR}/tif_dirwrite.c \
		$${LIBTIFF_DIR}/tif_error.c \
		$${LIBTIFF_DIR}/tif_extension.c \
		$${LIBTIFF_DIR}/tif_flush.c \
		$${LIBTIFF_DIR}/tif_hash_set.c \
		$${LIBTIFF_DIR}/tif_open.c \
		$${LIBTIFF_DIR}/tif_predict.c \
		$${LIBTIFF_DIR}/tif_read.c \
		$${LIBTIFF_DIR}/tif_strip.c \
		$${LIBTIFF_DIR}/tif_swab.c \
		$${LIBTIFF_DIR}/tif_tile.c \
		$${LIBTIFF_DIR}/tif_version.c \
		$${LIBTIFF_DIR}/tif_warning.c \
		$${LIBTIFF_DIR}/tif_write.c \
		$${LIBTIFF_DIR}/tif_lzw.c \
		$${LIBTIFF_DIR}/tif_packbits.c \
		$${LIBTIFF_DIR}/tif_thunder.c \
		$${LIBTIFF_DIR}/tif_next.c \
		$${LIBTIFF_DIR}/tif_dumpmode.c

	win32 {
		SOURCES += $${LIBTIFF_DIR}/tif_win32.c
	} else {
		SOURCES += $${LIBTIFF_DIR}/tif_unix.c
		# Hide bundled libtiff symbols from the dynamic linker so they cannot
		# interpose symbols from a system libtiff.so loaded by other libraries
		# (e.g. ArrayFire's FreeImage dependency). Internal linking is unaffected.
		QMAKE_CFLAGS += -fvisibility=hidden
	}
}
