QT += core gui widgets printsupport

CONFIG += c++11
TARGET = WavePSF
VERSION = 1.2.0
DEFINES += APP_NAME=\\\"$$TARGET\\\"
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

include($$PWD/pri/logging.pri) #disable debug messages on release build

include($$PWD/pri/openmp.pri) #enable OpenMP

include($$PWD/pri/arrayfire.pri)

include($$PWD/pri/libtiff.pri)


SOURCEDIR = $$shell_path($$PWD/src)
QCUSTOMPLOTDIR = $$shell_path($$PWD/thirdparty/qcustomplot)

INCLUDEPATH +=  \
	$$QCUSTOMPLOTDIR \
	$$SOURCEDIR

SOURCES += \
	$$QCUSTOMPLOTDIR/qcustomplot.cpp \
	$$SOURCEDIR/main.cpp \
	$$SOURCEDIR/gui/mainwindow.cpp \
	$$SOURCEDIR/gui/viewertoolbar.cpp \
	$$SOURCEDIR/gui/aboutdialog.cpp \
	$$SOURCEDIR/gui/shortcutsdialog.cpp \
	$$SOURCEDIR/gui/recentfilesmenu.cpp \
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
	$$SOURCEDIR/gui/imagesessionviewer/datacrosssectionwidget.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/rangeslider.cpp \
	$$SOURCEDIR/gui/imagesessionviewer/displaycontrolbar.cpp \
	$$SOURCEDIR/utils/supportedfilechecker.cpp \
	$$SOURCEDIR/gui/messageconsole/messagerouter.cpp \
	$$SOURCEDIR/gui/messageconsole/messageconsolewidget.cpp \
	$$SOURCEDIR/core/psf/zernikegenerator.cpp \
	$$SOURCEDIR/core/psf/deformablemirror/deformablemirrorgenerator.cpp \
	$$SOURCEDIR/core/psf/wavefrontgeneratorfactory.cpp \
	$$SOURCEDIR/core/psf/psfcalculator.cpp \
	$$SOURCEDIR/core/psf/richardswolfcalculator.cpp \
	$$SOURCEDIR/core/psf/apertureutils.cpp \
	$$SOURCEDIR/core/psf/deconvolver.cpp \
	$$SOURCEDIR/core/psf/composedpsfgenerator.cpp \
	$$SOURCEDIR/core/psf/psfgeneratorfactory.cpp \
	$$SOURCEDIR/core/psf/psfmodule.cpp \
	$$SOURCEDIR/gui/psfcontrol/coefficienteditorwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/wavefrontplotwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/psfpreviewwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/psf3dpreviewwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/sliceviewerwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/rwsettingswidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/psfgenerationwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/processingcontrolwidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/deconvolutionsettingswidget.cpp \
	$$SOURCEDIR/gui/psfcontrol/settingsdialog.cpp \
	$$SOURCEDIR/core/psf/psfsettings.cpp \
	$$SOURCEDIR/data/wavefrontparametertable.cpp \
	$$SOURCEDIR/core/optimization/imagemetriccalculator.cpp \
	$$SOURCEDIR/core/optimization/optimizationworker.cpp \
	$$SOURCEDIR/core/optimization/simulatedannealingoptimizer.cpp \
	$$SOURCEDIR/core/optimization/cmaesoptimizer.cpp \
	$$SOURCEDIR/core/optimization/differentialevolutionoptimizer.cpp \
	$$SOURCEDIR/core/optimization/neldermeadoptimizer.cpp \
	$$SOURCEDIR/core/optimization/optimizerfactory.cpp \
	$$SOURCEDIR/gui/psfcontrol/optimizationwidget.cpp \
	$$SOURCEDIR/core/interpolation/tableinterpolator.cpp \
	$$SOURCEDIR/core/interpolation/interpolationorchestrator.cpp \
	$$SOURCEDIR/core/optimization/optimizationjobbuilder.cpp \
	$$SOURCEDIR/core/psf/psffilemanager.cpp \
	$$SOURCEDIR/core/psf/filepsfgenerator.cpp \
	$$SOURCEDIR/core/processing/batchprocessor.cpp \
	$$SOURCEDIR/core/processing/volumetricdeconvolver.cpp \
	$$SOURCEDIR/core/processing/volumetricprocessor.cpp \
	$$SOURCEDIR/gui/psfcontrol/interpolationwidget.cpp \
	$$SOURCEDIR/gui/verticalscrollarea.cpp \
	$$SOURCEDIR/gui/plotutils.cpp \
	$$SOURCEDIR/utils/afdevicemanager.cpp \
	$$SOURCEDIR/core/psf/psfgridgenerator.cpp \
	$$SOURCEDIR/gui/psfgridview/psfgridwidget.cpp \
	$$SOURCEDIR/gui/lut.cpp

HEADERS += \
	$$QCUSTOMPLOTDIR/qcustomplot.h \
	$$SOURCEDIR/gui/mainwindow.h \
	$$SOURCEDIR/gui/viewertoolbar.h \
	$$SOURCEDIR/gui/aboutdialog.h \
	$$SOURCEDIR/gui/shortcutsdialog.h \
	$$SOURCEDIR/gui/recentfilesmenu.h \
	$$SOURCEDIR/utils/settingsfilemanager.h \
	$$SOURCEDIR/gui/stylemanager.h \
	$$SOURCEDIR/data/imagedata.h \
	$$SOURCEDIR/data/inputdatareader.h \
	$$SOURCEDIR/data/imagedataaccessor.h \
	$$SOURCEDIR/data/patchlayout.h \
	$$SOURCEDIR/utils/logging.h \
	$$SOURCEDIR/controller/imagesession.h \
	$$SOURCEDIR/controller/applicationcontroller.h \
	$$SOURCEDIR/gui/imagesessionviewer/graphicsview.h \
	$$SOURCEDIR/gui/imagesessionviewer/rectitem.h \
	$$SOURCEDIR/gui/imagesessionviewer/rectitemgroup.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagedataviewer.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagesessionviewer.h \
	$$SOURCEDIR/gui/imagesessionviewer/imagerenderworker.h \
	$$SOURCEDIR/gui/imagesessionviewer/datacrosssectionwidget.h \
	$$SOURCEDIR/gui/imagesessionviewer/rangeslider.h \
	$$SOURCEDIR/gui/imagesessionviewer/displaycontrolbar.h \
	$$SOURCEDIR/utils/supportedfilechecker.h \
	$$SOURCEDIR/gui/messageconsole/messagerouter.h \
	$$SOURCEDIR/gui/messageconsole/messageconsoledock.h \
	$$SOURCEDIR/gui/messageconsole/messageconsolewidget.h \
	$$SOURCEDIR/core/psf/wavefrontparameter.h \
	$$SOURCEDIR/core/psf/iwavefrontgenerator.h \
	$$SOURCEDIR/core/psf/zernikegenerator.h \
	$$SOURCEDIR/core/psf/deformablemirror/deformablemirrorgenerator.h \
	$$SOURCEDIR/core/psf/wavefrontgeneratorfactory.h \
	$$SOURCEDIR/core/psf/psfcalculator.h \
	$$SOURCEDIR/core/psf/richardswolfcalculator.h \
	$$SOURCEDIR/core/psf/apertureutils.h \
	$$SOURCEDIR/core/psf/deconvolver.h \
	$$SOURCEDIR/core/psf/ipsfpropagator.h \
	$$SOURCEDIR/core/psf/ipsfgenerator.h \
	$$SOURCEDIR/core/psf/composedpsfgenerator.h \
	$$SOURCEDIR/core/psf/psfgeneratorfactory.h \
	$$SOURCEDIR/core/psf/psfmodule.h \
	$$SOURCEDIR/gui/psfcontrol/coefficienteditorwidget.h \
	$$SOURCEDIR/gui/psfcontrol/wavefrontplotwidget.h \
	$$SOURCEDIR/gui/psfcontrol/psfpreviewwidget.h \
	$$SOURCEDIR/gui/psfcontrol/psf3dpreviewwidget.h \
	$$SOURCEDIR/gui/psfcontrol/sliceviewerwidget.h \
	$$SOURCEDIR/gui/psfcontrol/rwsettingswidget.h \
	$$SOURCEDIR/gui/psfcontrol/psfgenerationwidget.h \
	$$SOURCEDIR/gui/psfcontrol/processingcontrolwidget.h \
	$$SOURCEDIR/gui/psfcontrol/deconvolutionsettingswidget.h \
	$$SOURCEDIR/gui/psfcontrol/settingsdialog.h \
	$$SOURCEDIR/core/psf/psfsettings.h \
	$$SOURCEDIR/data/wavefrontparametertable.h \
	$$SOURCEDIR/core/optimization/ioptimizer.h \
	$$SOURCEDIR/core/optimization/imagemetriccalculator.h \
	$$SOURCEDIR/core/optimization/optimizationworker.h \
	$$SOURCEDIR/core/optimization/simulatedannealingoptimizer.h \
	$$SOURCEDIR/core/optimization/cmaesoptimizer.h \
	$$SOURCEDIR/core/optimization/differentialevolutionoptimizer.h \
	$$SOURCEDIR/core/optimization/neldermeadoptimizer.h \
	$$SOURCEDIR/core/optimization/optimizerfactory.h \
	$$SOURCEDIR/gui/psfcontrol/optimizationwidget.h \
	$$SOURCEDIR/core/interpolation/tableinterpolator.h \
	$$SOURCEDIR/core/interpolation/interpolationorchestrator.h \
	$$SOURCEDIR/core/optimization/optimizationjobbuilder.h \
	$$SOURCEDIR/core/psf/psffilemanager.h \
	$$SOURCEDIR/core/psf/filepsfgenerator.h \
	$$SOURCEDIR/core/processing/batchprocessor.h \
	$$SOURCEDIR/core/processing/volumetricdeconvolver.h \
	$$SOURCEDIR/core/processing/volumetricprocessor.h \
	$$SOURCEDIR/gui/psfcontrol/interpolationwidget.h \
	$$SOURCEDIR/gui/qcppaletteobserver.h \
	$$SOURCEDIR/gui/verticalscrollarea.h \
	$$SOURCEDIR/gui/plotutils.h \
	$$SOURCEDIR/utils/afdevicemanager.h \
	$$SOURCEDIR/core/psf/psfgridgenerator.h \
	$$SOURCEDIR/gui/psfgridview/psfgriddock.h \
	$$SOURCEDIR/gui/psfgridview/psfgridwidget.h \
	$$SOURCEDIR/gui/lut.h \
	$$SOURCEDIR/gui/imagesessionviewer/displaysettings.h

FORMS += \
	$$SOURCEDIR/gui/mainwindow.ui

RESOURCES += \
	resources.qrc

RC_ICONS = icon/wavepsf.ico


