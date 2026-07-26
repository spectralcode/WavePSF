# Build/run out-of-source from tests/build-tests (Qt's bin on PATH):
#   qmake ..\tests.pro -spec win32-msvc CONFIG+=debug && nmake check
TEMPLATE = subdirs
SUBDIRS += test_differentialevolution.pro
