QT = core
TEMPLATE = app
TARGET = test_wienerkernel
CONFIG += console c++11 testcase
CONFIG -= app_bundle debug_and_release

# Launch the built exe via .\ so `nmake check` finds it even when Windows
# does not resolve bare executable names from the current directory.
TEST_TARGET_DIR = .

# Isolate object files per test: the test projects compile overlapping
# sources into the same build directory and would otherwise share .obj
# files across Makefiles (a race under parallel builds, and stale reuse
# if per-test compiler settings ever diverge).
OBJECTS_DIR = obj/$${TARGET}
MOC_DIR = obj/$${TARGET}

INCLUDEPATH += ../src

include(../pri/arrayfire.pri)

HEADERS += ../src/core/psf/deconvolver.h \
	../src/core/processing/volumetricdeconvolver.h

SOURCES += test_wienerkernel.cpp \
	../src/core/psf/deconvolver.cpp \
	../src/core/processing/volumetricdeconvolver.cpp
