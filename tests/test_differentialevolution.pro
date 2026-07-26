QT = core
TEMPLATE = app
TARGET = test_differentialevolution
CONFIG += console c++11 testcase
CONFIG -= app_bundle debug_and_release

# Launch the built exe via .\ so `nmake check` finds it even when Windows
# does not resolve bare executable names from the current directory.
TEST_TARGET_DIR = .

INCLUDEPATH += ../src

SOURCES += test_differentialevolution.cpp \
	../src/core/optimization/differentialevolutionoptimizer.cpp
