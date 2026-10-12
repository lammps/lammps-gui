##########################################################################
# All source files of the lammps-gui executable plus the bundled
# lepton_mini support library.  New .cpp/.h file pairs in src/ must be
# added to the PROJECT_SOURCES list below; Qt's AUTOMOC picks them up
# automatically once listed.  Expects Qt6 to be found and the variables
# from Platform.cmake to be set.
##########################################################################

set(PROJECT_SOURCES
  ${CMAKE_SOURCE_DIR}/src/main.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpsgui.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpsgui.h
  ${CMAKE_SOURCE_DIR}/src/aboutdialog.cpp
  ${CMAKE_SOURCE_DIR}/src/aboutdialog.h
  ${CMAKE_SOURCE_DIR}/src/analysis.cpp
  ${CMAKE_SOURCE_DIR}/src/analysis.h
  ${CMAKE_SOURCE_DIR}/src/chartviewer.cpp
  ${CMAKE_SOURCE_DIR}/src/chartviewer.h
  ${CMAKE_SOURCE_DIR}/src/chartstyle.cpp
  ${CMAKE_SOURCE_DIR}/src/chartstyle.h
  ${CMAKE_SOURCE_DIR}/src/plotwidget.cpp
  ${CMAKE_SOURCE_DIR}/src/plotwidget.h
  ${CMAKE_SOURCE_DIR}/src/plotseries.h
  ${CMAKE_SOURCE_DIR}/src/plotaxismath.cpp
  ${CMAKE_SOURCE_DIR}/src/plotaxismath.h
  ${CMAKE_SOURCE_DIR}/src/codeeditor.cpp
  ${CMAKE_SOURCE_DIR}/src/codeeditor.h
  ${CMAKE_SOURCE_DIR}/src/commandwindow.cpp
  ${CMAKE_SOURCE_DIR}/src/commandwindow.h
  ${CMAKE_SOURCE_DIR}/src/colormaps.cpp
  ${CMAKE_SOURCE_DIR}/src/colormaps.h
  ${CMAKE_SOURCE_DIR}/src/constants.h
  ${CMAKE_SOURCE_DIR}/src/tutorials.cpp
  ${CMAKE_SOURCE_DIR}/src/tutorials.h
  ${CMAKE_SOURCE_DIR}/src/customfunc.cpp
  ${CMAKE_SOURCE_DIR}/src/customfunc.h
  ${CMAKE_SOURCE_DIR}/src/downloadprogress.cpp
  ${CMAKE_SOURCE_DIR}/src/downloadprogress.h
  ${CMAKE_SOURCE_DIR}/src/dumpimage.cpp
  ${CMAKE_SOURCE_DIR}/src/dumpimage.h
  ${CMAKE_SOURCE_DIR}/src/fileviewer.cpp
  ${CMAKE_SOURCE_DIR}/src/fileviewer.h
  ${CMAKE_SOURCE_DIR}/src/findandreplace.cpp
  ${CMAKE_SOURCE_DIR}/src/findandreplace.h
  ${CMAKE_SOURCE_DIR}/src/fitting.cpp
  ${CMAKE_SOURCE_DIR}/src/fitting.h
  ${CMAKE_SOURCE_DIR}/src/flagwarnings.cpp
  ${CMAKE_SOURCE_DIR}/src/flagwarnings.h
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.h
  ${CMAKE_SOURCE_DIR}/src/highlighter.cpp
  ${CMAKE_SOURCE_DIR}/src/highlighter.h
  ${CMAKE_SOURCE_DIR}/src/imagecache.cpp
  ${CMAKE_SOURCE_DIR}/src/imagecache.h
  ${CMAKE_SOURCE_DIR}/src/imageviewer.cpp
  ${CMAKE_SOURCE_DIR}/src/imageviewer.h
  ${CMAKE_SOURCE_DIR}/src/imageviewer_internal.h
  ${CMAKE_SOURCE_DIR}/src/imageviewersettings.cpp
  ${CMAKE_SOURCE_DIR}/src/inputvariables.cpp
  ${CMAKE_SOURCE_DIR}/src/inputvariables.h
  ${CMAKE_SOURCE_DIR}/src/lammpsrunner.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpsrunner.h
  ${CMAKE_SOURCE_DIR}/src/lammpssyntax.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpssyntax.h
  ${CMAKE_SOURCE_DIR}/src/syntaxcheck.cpp
  ${CMAKE_SOURCE_DIR}/src/syntaxcheck.h
  ${CMAKE_SOURCE_DIR}/src/lammpswrapper.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpswrapper.h
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.h
  ${CMAKE_SOURCE_DIR}/src/levmar.cpp
  ${CMAKE_SOURCE_DIR}/src/levmar.h
  ${CMAKE_SOURCE_DIR}/src/linenumberarea.h
  ${CMAKE_SOURCE_DIR}/src/logwindow.cpp
  ${CMAKE_SOURCE_DIR}/src/logwindow.h
  ${CMAKE_SOURCE_DIR}/src/movieimport.cpp
  ${CMAKE_SOURCE_DIR}/src/movieimport.h
  ${CMAKE_SOURCE_DIR}/src/plotblockdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotblockdata.h
  ${CMAKE_SOURCE_DIR}/src/plotdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdata.h
  ${CMAKE_SOURCE_DIR}/src/plotdata_internal.h
  ${CMAKE_SOURCE_DIR}/src/plotdatadialog.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdatadialog.h
  ${CMAKE_SOURCE_DIR}/src/preferences.cpp
  ${CMAKE_SOURCE_DIR}/src/preferences.h
  ${CMAKE_SOURCE_DIR}/src/qaddon.cpp
  ${CMAKE_SOURCE_DIR}/src/qaddon.h
  ${CMAKE_SOURCE_DIR}/src/rangebandslider.cpp
  ${CMAKE_SOURCE_DIR}/src/rangebandslider.h
  ${CMAKE_SOURCE_DIR}/thirdparty/rangeslider/rangeslider.cpp
  ${CMAKE_SOURCE_DIR}/thirdparty/rangeslider/rangeslider.h
  ${CMAKE_SOURCE_DIR}/src/setvariables.cpp
  ${CMAKE_SOURCE_DIR}/src/setvariables.h
  ${CMAKE_SOURCE_DIR}/src/shellaliases.cpp
  ${CMAKE_SOURCE_DIR}/src/shellaliases.h
  ${CMAKE_SOURCE_DIR}/src/shellprompt.cpp
  ${CMAKE_SOURCE_DIR}/src/shellprompt.h
  ${CMAKE_SOURCE_DIR}/src/slideshow.cpp
  ${CMAKE_SOURCE_DIR}/src/slideshow.h
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.h
  ${CMAKE_SOURCE_DIR}/src/tutorialwizard.cpp
  ${CMAKE_SOURCE_DIR}/src/tutorialwizard.h
  ${CMAKE_SOURCE_DIR}/src/urldownloader.cpp
  ${CMAKE_SOURCE_DIR}/src/urldownloader.h
  ${CMAKE_SOURCE_DIR}/src/windowlayout.cpp
  ${CMAKE_SOURCE_DIR}/src/windowlayout.h
  ${PLUGIN_LOADER_SRC}
  ${ICON_RC_FILE}
)
qt6_add_resources(PROJECT_SOURCES resources/lammpsgui.qrc)

# vendored, JIT-less subset of the Lepton expression parser (namespace
# LeptonMini) used for custom-function plotting and nonlinear fits.
add_library(lepton_mini STATIC
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/src/ExpressionProgram.cpp
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/src/ExpressionTreeNode.cpp
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/src/Operation.cpp
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/src/ParsedExpression.cpp
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/src/Parser.cpp
)
target_include_directories(lepton_mini PUBLIC
  ${CMAKE_SOURCE_DIR}/thirdparty/lepton_mini/include)
# on Windows: build/consume as a static library (no dllexport/dllimport)
target_compile_definitions(lepton_mini
  PRIVATE LEPTON_BUILDING_STATIC_LIBRARY
  PUBLIC LEPTON_USE_STATIC_LIBRARIES)
set_target_properties(lepton_mini PROPERTIES POSITION_INDEPENDENT_CODE ON)
