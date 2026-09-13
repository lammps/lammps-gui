cmake_minimum_required(VERSION 3.20)

project(lammps-gui
  VERSION 3.1.0
  LANGUAGES CXX
  DESCRIPTION "The LAMMPS Graphical User Interface"
  HOMEPAGE_URL "https://lammps-gui.lammps.org/")

include(GNUInstallDirs)

##################################################################################
# configuration settings:
# prefer compiling plugin version only if standalone compile
option(LAMMPS_GUI_USE_PLUGIN "Load LAMMPS library dynamically at runtime" ON)
# build html documentation by default
option(BUILD_DOC "Build LAMMPS-GUI HTML documentation" ON)
option(BUILD_DOC_ONLY "Build only the documentation, skip the application" OFF)
# testing support is off by default
option(ENABLE_TESTING "Build unit tests" OFF)
##################################################################################

# add automatic Qt infrastructure rules
set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# same as LAMMPS' default
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT BUILD_DOC_ONLY)
  # checks
  if (NOT LAMMPS_SOURCE_DIR AND NOT LAMMPS_GUI_USE_PLUGIN)
    message(FATAL_ERROR "Must provide -D LAMMPS_SOURCE_DIR=<path to LAMMPS sources> when not compiling with -D LAMMPS_GUI_USE_PLUGIN=on")
  endif()
  set(LAMMPS_HEADER_DIR ${LAMMPS_SOURCE_DIR})
  if (NOT LAMMPS_GUI_USE_PLUGIN AND NOT LAMMPS_LIBRARY)
    message(FATAL_ERROR "Must provide -D LAMMPS_LIBRARY=<path to LAMMPS library file> when not compiling with -D LAMMPS_GUI_USE_PLUGIN=on")
  endif()

  # install prefix, compiler and platform quirks, plugin loader, bundle resources
  include(${CMAKE_SOURCE_DIR}/cmake/Platform.cmake)
  # support for signing executables
  include(${CMAKE_SOURCE_DIR}/cmake/Signing.cmake)

  # charts are rendered by the self-contained native QPainter backend; only the
  # LGPL Qt Widgets are needed (no Qt Charts / Graphs / Quick modules)
  # Qt Svg provides the SVG icon-engine plugin used to render the .svg icons;
  # declaring it here ensures the plugin is found and bundled by the deployment
  # tools (macdeployqt / windeployqt / flatpak)
  find_package(Qt6 6.2 REQUIRED COMPONENTS Gui Widgets Network Svg)
  message(STATUS "Using Qt version ${Qt6_VERSION}")

  # the PROJECT_SOURCES file list, embedded resources, and the lepton_mini library
  include(${CMAKE_SOURCE_DIR}/cmake/Sources.cmake)

  qt_add_executable(lammps-gui
    MANUAL_FINALIZATION
    ${MACOSX_ICON_FILE}
    ${PROJECT_SOURCES}
  )

  # when compiling for macOS, request compilation of an app-bundle
  if(APPLE)
    string(TIMESTAMP CURRENT_YEAR "%Y")
    set_target_properties(lammps-gui PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${MACOSX_PLIST_FILE}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
      MACOSX_BUNDLE_ICON_FILE lammps-gui.icns
      MACOSX_BUNDLE_COPYRIGHT "(c) 2023 - ${CURRENT_YEAR}, Axel Kohlmeyer"
      MACOSX_BUNDLE TRUE
    )
    # purge app bundle directory during configure
    file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/lammps-gui.app)
  endif()

  # when compiling for Windows, call the WinMain entry point for a pure GUI application
  if(WIN32)
    set_target_properties(lammps-gui PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()

  install(TARGETS lammps-gui DESTINATION ${CMAKE_INSTALL_BINDIR})

  qt_finalize_executable(lammps-gui)
  sign_target(lammps-gui)

  # compilation settings
  if(LAMMPS_GUI_USE_PLUGIN)
    target_compile_definitions(lammps-gui PRIVATE LAMMPS_GUI_USE_PLUGIN)
    target_include_directories(lammps-gui PRIVATE ${LAMMPS_PLUGINLIB_DIR})
  else()
    target_include_directories(lammps-gui PRIVATE ${LAMMPS_HEADER_DIR})
    target_link_libraries(lammps-gui PRIVATE ${LAMMPS_LIBRARY})
  endif()
  target_include_directories(lammps-gui PRIVATE ${CMAKE_SOURCE_DIR}/src)
  target_include_directories(lammps-gui PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/rangeslider)
  target_link_libraries(lammps-gui PRIVATE lepton_mini)
  target_compile_definitions(lammps-gui PRIVATE LAMMPS_GUI_VERSION="${PROJECT_VERSION}")
  target_link_libraries(lammps-gui PRIVATE Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Svg)

  # unit tests and code coverage, only enabled on Linux
  include(${CMAKE_SOURCE_DIR}/cmake/Testing.cmake)
endif()

# optional instrumentation with a code sanitizer
include(${CMAKE_SOURCE_DIR}/cmake/Sanitizer.cmake)

# documentation build with Doxygen and Sphinx
include(${CMAKE_SOURCE_DIR}/cmake/Documentation.cmake)

# packaging targets (flatpak, dmg, Windows installer, tgz, source tarball)
include(${CMAKE_SOURCE_DIR}/cmake/Packaging.cmake)
