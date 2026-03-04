msvc {
	QMAKE_CXXFLAGS += /wd4275  # suppress C4275 from ArrayFire headers (DLL interface base class)
}

win32 {
	AF_PATH = "C:/Program Files/ArrayFire/v3"
} else {
	AF_PATH = "/opt/arrayfire"
}

INCLUDEPATH += $${AF_PATH}/include
LIBS += -L$${AF_PATH}/lib -laf
