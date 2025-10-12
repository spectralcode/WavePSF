QT += core gui widgets printsupport

CONFIG += c++11
TARGET = WavePSF

include($$PWD/pri/logging.pri) #disable debug messages on release build

include($$PWD/pri/openmp.pri) #enable OpenMP

include($$PWD/pri/arrayfire.pri)


SOURCEDIR = $$shell_path($$PWD/src)
QCUSTOMPLOTDIR = $$shell_path($$PWD/thirdparty/qcustomplot)

INCLUDEPATH +=  \
	$$QCUSTOMPLOTDIR \
	$$SOURCEDIR

SOURCES += \
	$$QCUSTOMPLOTDIR/qcustomplot.cpp \
	$$SOURCEDIR/main.cpp \
	$$SOURCEDIR/gui/mainwindow.cpp \
	$$SOURCEDIR/utils/settingsfilemanager.cpp \
	$$SOURCEDIR/gui/stylemanager.cpp \
	$$SOURCEDIR/data/imagedata.cpp \
	$$SOURCEDIR/data/inputdatareader.cpp \
	$$SOURCEDIR/data/imagedataaccessor.cpp \
	$$SOURCEDIR/controller/imagesession.cpp \
	$$SOURCEDIR/controller/applicationcontroller.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/graphicsview.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/rectitem.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/rectitemgroup.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/imagedataviewer.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/imagesessionviewer.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/imagerenderworker.cpp \
	$$SOURCEDIR/utils/supportedfilechecker.cpp \
	$$SOURCEDIR/gui/messageconsole/messagerouter.cpp \
	$$SOURCEDIR/gui/messageconsole/messageconsolewidget.cpp

HEADERS += \
	$$QCUSTOMPLOTDIR/qcustomplot.h \
	$$SOURCEDIR/gui/mainwindow.h \
	$$SOURCEDIR/utils/settingsfilemanager.h \
	$$SOURCEDIR/gui/stylemanager.h \
	$$SOURCEDIR/data/imagedata.h \
	$$SOURCEDIR/data/inputdatareader.h \
	$$SOURCEDIR/data/imagedataaccessor.h \
	$$SOURCEDIR/utils/logging.h \
	$$SOURCEDIR/controller/imagesession.h \
	$$SOURCEDIR/controller/applicationcontroller.h \
	$$SOURCEDIR/gui/imagesessionviewer/graphicsview.h \
	$$SOURCEDIR/gui/imagesessionviewer/rectitem.h \
	$$SOURCEDIR/gui/imagesessionviewer/rectitemgroup.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagedataviewer.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagesessionviewer.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagerenderworker.h \
	$$SOURCEDIR/utils/supportedfilechecker.h \
	$$SOURCEDIR/gui/messageconsole/messagerouter.h \
	$$SOURCEDIR/gui/messageconsole/messageconsoledock.h \
	$$SOURCEDIR/gui/messageconsole/messageconsolewidget.h

FORMS += \
	$$SOURCEDIR/gui/mainwindow.ui

RESOURCES += \
	resources.qrc


