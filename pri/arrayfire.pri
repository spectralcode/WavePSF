AF_PATH = "C:\Program Files\ArrayFire\v3" #todo: is there a better way to set the array fire path? todo: add linux support
INCLUDEPATH += $${AF_PATH}/include
LIBS += -L$${AF_PATH}/lib -laf
QMAKE_CXXFLAGS += /wd4275  # suppress C4275 from ArrayFire headers (DLL interface base class)
