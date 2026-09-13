# CMakeLists.txt for LAMMPS-GUI unit tests
# (only entered via add_subdirectory() from the top-level CMakeLists.txt)

# Fetch GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.18.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# testing is already enabled via include(CTest) in the top-level CMakeLists.txt
include(GoogleTest)

# Find Qt6 (Qt5 is no longer supported)
find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

# check that the Xvfb runner and a supported screenshooter are present
find_program(XVFB_RUNNER
  NAMES xvfb-run
  REQUIRED
  DOC "Path to Xvfb runner script")

find_program(SHOOTER_BINARY
  NAMES magick import xfce4-screenshooter gnome-screenshot
  REQUIRED
  DOC "Path to a supported screenshooter program")
cmake_path(GET SHOOTER_BINARY FILENAME SHOOTER)
message(STATUS "Using ${SHOOTER} as screen shooter")

# configure virtual environment for tests using PyAutoGUI and Xvfb
find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
set(TESTENV_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/testenv/bin/")
add_custom_command(
  OUTPUT ${TESTENV_BINARY_DIR}
  COMMAND ${Python3_EXECUTABLE} -m venv "${CMAKE_CURRENT_BINARY_DIR}/testenv"
  COMMAND ${TESTENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install --upgrade pip
  COMMAND ${TESTENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install -r ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt --upgrade
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/shooter.${SHOOTER} ${TESTENV_BINARY_DIR}/shooter
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/requirements.txt
)
add_custom_target(setup_tests ALL
  ${CMAKE_COMMAND} -E echo "Setting up virtual environment for testing"
  DEPENDS ${TESTENV_BINARY_DIR}
  SOURCES requirements.txt shooter.magick shooter.import shooter.xfce4-screenshooter shooter.gnome-screenshot
)
set_target_properties(setup_tests PROPERTIES ADDITIONAL_CLEAN_FILES ${CMAKE_CURRENT_BINARY_DIR}/testenv)

# General command line tests on the LAMMPS-GUI executable
add_test(NAME CommandLine.GetVersion
  COMMAND $<TARGET_FILE:lammps-gui> --platform offscreen -v)
set_tests_properties(CommandLine.GetVersion PROPERTIES
  ENVIRONMENT "OMP_NUM_THREADS=1"
  PASS_REGULAR_EXPRESSION "LAMMPS-GUI \\(QT6\\) ${PROJECT_VERSION}")

add_test(NAME CommandLine.HasPlugin
  COMMAND $<TARGET_FILE:lammps-gui> --platform offscreen -h)
if (LAMMPS_GUI_USE_PLUGIN)
  set_tests_properties(CommandLine.HasPlugin PROPERTIES
    ENVIRONMENT "OMP_NUM_THREADS=1"
    PASS_REGULAR_EXPRESSION "Options:.?  -p, --pluginpath <path> +Set path to LAMMPS")
else()
  set_tests_properties(CommandLine.HasPlugin PROPERTIES
    ENVIRONMENT "OMP_NUM_THREADS=1"
    PASS_REGULAR_EXPRESSION "Options:.?  -h, --help   +Displays help on commandline options.")
endif()

# Test executable for helpers
add_executable(test_helpers
  test_helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
)

# Test executable for the widget-scoped shortcut helpers
add_executable(test_shortcuts
  test_shortcuts.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
)

target_include_directories(test_shortcuts PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_shortcuts PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_shortcuts)

# Test executable for the docked-layout presentation policy
add_executable(test_windowlayout
  test_windowlayout.cpp
  ${CMAKE_SOURCE_DIR}/src/windowlayout.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
)

set_target_properties(test_windowlayout PROPERTIES AUTOMOC ON)
target_include_directories(test_windowlayout PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_windowlayout PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_windowlayout)

# Test executable for the completing shell prompt
add_executable(test_shellprompt
  test_shellprompt.cpp
  ${CMAKE_SOURCE_DIR}/src/shellprompt.cpp
)

set_target_properties(test_shellprompt PROPERTIES AUTOMOC ON)
target_include_directories(test_shellprompt PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_shellprompt PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_shellprompt)

target_include_directories(test_helpers PRIVATE
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_helpers PRIVATE GTest::gtest_main Qt6::Widgets)

# Discover tests for test executables
gtest_discover_tests(test_helpers)

# Test executable for StdCapture
add_executable(test_stdcapture
  test_stdcapture.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
)

target_include_directories(test_stdcapture PRIVATE  ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_stdcapture PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_stdcapture)

# Test executable for the LAMMPS input syntax engine (Qt Core only)
add_executable(test_lammpssyntax
  test_lammpssyntax.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpssyntax.cpp
)

target_include_directories(test_lammpssyntax PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_lammpssyntax PRIVATE
  COMMAND_SPECS_TABLE="${CMAKE_SOURCE_DIR}/resources/command_specs.table")
target_link_libraries(test_lammpssyntax PRIVATE GTest::gtest_main Qt6::Core)

gtest_discover_tests(test_lammpssyntax)

# Test executable for the pre-run input syntax checker (Qt Core only)
add_executable(test_syntaxcheck
  test_syntaxcheck.cpp
  ${CMAKE_SOURCE_DIR}/src/syntaxcheck.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpssyntax.cpp
)

target_include_directories(test_syntaxcheck PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_syntaxcheck PRIVATE
  COMMAND_SPECS_TABLE="${CMAKE_SOURCE_DIR}/resources/command_specs.table")
target_link_libraries(test_syntaxcheck PRIVATE GTest::gtest_main Qt6::Core)

gtest_discover_tests(test_syntaxcheck)

# Test executable for the input script syntax highlighter (headless
# QTextDocument-based, needs the offscreen platform via QApplication)
add_executable(test_highlighter
  test_highlighter.cpp
  ${CMAKE_SOURCE_DIR}/src/highlighter.cpp
  ${CMAKE_SOURCE_DIR}/src/lammpssyntax.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
)

target_include_directories(test_highlighter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_highlighter PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_highlighter)

# Test executable for the index-variable parse and merge helpers (Qt Core only)
add_executable(test_inputvariables
  test_inputvariables.cpp
  ${CMAKE_SOURCE_DIR}/src/inputvariables.cpp
)

target_include_directories(test_inputvariables PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_inputvariables PRIVATE GTest::gtest_main Qt6::Core)

gtest_discover_tests(test_inputvariables)

# Test executable for FlagWarnings
add_executable(test_flagwarnings
  test_flagwarnings.cpp
  ${CMAKE_SOURCE_DIR}/src/flagwarnings.cpp
)

target_include_directories(test_flagwarnings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_flagwarnings PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_flagwarnings)

# Test executable for the dump-image command builder
add_executable(test_dumpimage
  test_dumpimage.cpp
  ${CMAKE_SOURCE_DIR}/src/dumpimage.cpp
  ${CMAKE_SOURCE_DIR}/src/colormaps.cpp
)

target_include_directories(test_dumpimage PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_dumpimage PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_dumpimage)

# Test executable for the movie probing and frame selection helpers
add_executable(test_movieimport
  test_movieimport.cpp
  ${CMAKE_SOURCE_DIR}/src/movieimport.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/qaddon.cpp
)

target_include_directories(test_movieimport PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_movieimport PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_movieimport)

# Test executable for the converted image cache
add_executable(test_imagecache
  test_imagecache.cpp
  ${CMAKE_SOURCE_DIR}/src/imagecache.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
)

target_include_directories(test_imagecache PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_imagecache PRIVATE GTest::gtest_main Qt6::Widgets)

gtest_discover_tests(test_imagecache)

# Test executable for the least-squares / linear-algebra toolkit (Qt-free)
add_executable(test_leastsquares
  test_leastsquares.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
)

target_include_directories(test_leastsquares PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_leastsquares PRIVATE GTest::gtest_main)

gtest_discover_tests(test_leastsquares)

# Test executable for the PlotData model and file parsers
add_executable(test_plotdata
  test_plotdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdata.cpp
)

target_include_directories(test_plotdata PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plotdata PRIVATE GTest::gtest_main Qt6::Core)

gtest_discover_tests(test_plotdata)

# Test executable for the block-structured fix ave/* file parsers
add_executable(test_plotblockdata
  test_plotblockdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotblockdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdata.cpp
)

target_include_directories(test_plotblockdata PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plotblockdata PRIVATE GTest::gtest_main Qt6::Core)

gtest_discover_tests(test_plotblockdata)

# Test executable for the vendored LeptonMini expression parser (Qt-free)
add_executable(test_lepton
  test_lepton.cpp
)

target_link_libraries(test_lepton PRIVATE GTest::gtest_main lepton_mini)

gtest_discover_tests(test_lepton)

# Test executable for the Levenberg-Marquardt solver (Qt-free; uses leastsquares)
add_executable(test_levmar
  test_levmar.cpp
  ${CMAKE_SOURCE_DIR}/src/levmar.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
)

target_include_directories(test_levmar PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_levmar PRIVATE GTest::gtest_main)

gtest_discover_tests(test_levmar)

# Test executable for the custom-function evaluator and nonlinear fit
# (LeptonMini + the Levenberg-Marquardt / leastsquares core + Qt Core)
add_executable(test_customfunc
  test_customfunc.cpp
  ${CMAKE_SOURCE_DIR}/src/customfunc.cpp
  ${CMAKE_SOURCE_DIR}/src/levmar.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
)

target_include_directories(test_customfunc PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_customfunc PRIVATE GTest::gtest_main Qt6::Core lepton_mini)

gtest_discover_tests(test_customfunc)

# Test executable for the column-picker dialog's derived-column expressions
# and live column renames (offscreen QApplication)
add_executable(test_plotdatadialog
  test_plotdatadialog.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdatadialog.cpp
  ${CMAKE_SOURCE_DIR}/src/plotdata.cpp
  ${CMAKE_SOURCE_DIR}/src/plotblockdata.cpp
  ${CMAKE_SOURCE_DIR}/src/customfunc.cpp
  ${CMAKE_SOURCE_DIR}/src/levmar.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
  ${CMAKE_SOURCE_DIR}/src/helpers.cpp
  ${CMAKE_SOURCE_DIR}/src/stdcapture.cpp
)

set_target_properties(test_plotdatadialog PROPERTIES AUTOMOC ON)
target_include_directories(test_plotdatadialog PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plotdatadialog PRIVATE GTest::gtest_main Qt6::Widgets lepton_mini)

gtest_discover_tests(test_plotdatadialog)

# Test executable for the post-processing analyses (Qt-free)
add_executable(test_analysis
  test_analysis.cpp
  ${CMAKE_SOURCE_DIR}/src/analysis.cpp
)

target_include_directories(test_analysis PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_analysis PRIVATE GTest::gtest_main)

gtest_discover_tests(test_analysis)

# Test executable for the axis-layout helpers (Qt-free)
add_executable(test_plotaxismath
  test_plotaxismath.cpp
  ${CMAKE_SOURCE_DIR}/src/plotaxismath.cpp
)

target_include_directories(test_plotaxismath PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_plotaxismath PRIVATE GTest::gtest_main)

gtest_discover_tests(test_plotaxismath)

# Test executable for the curve fits (Qt-free; depends on the leastsquares core)
add_executable(test_fitting
  test_fitting.cpp
  ${CMAKE_SOURCE_DIR}/src/fitting.cpp
  ${CMAKE_SOURCE_DIR}/src/leastsquares.cpp
)

target_include_directories(test_fitting PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fitting PRIVATE GTest::gtest_main)

gtest_discover_tests(test_fitting)

# only run framebuffer tests without sanitizers
if(ENABLE_SANITIZER STREQUAL "none")
########################################################################
# Tests that check the GUI environment and the GUI
# IMPORTANT NOTE: we must use a different display number for the -n flag
# for each XVFB_RUNNER invocation, to allow parallel execution of tests.

# set up environment for python testing
set(PYTHON_TEST_ENVIRONMENT PYTHONUNBUFFERED=1)
list(APPEND PYTHON_TEST_ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
list(APPEND PYTHON_TEST_ENVIRONMENT "OMP_NUM_THREADS=1")
list(APPEND PYTHON_TEST_ENVIRONMENT "LAMMPS_GUI=$<TARGET_FILE:lammps-gui>")
list(APPEND PYTHON_TEST_ENVIRONMENT "PATH=${TESTENV_BINARY_DIR}:$ENV{PATH}")

# test screen shooter wrapper script
add_test(NAME Framebuffer.CreateScreenshot
  COMMAND ${XVFB_RUNNER} -n 11 -s "-screen 0 1024x768x24" -w 1
  ${TESTENV_BINARY_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/test_shooter.py
)

# test if PyAutoGUI is usable
add_test(NAME Framebuffer.CheckSize
  COMMAND ${XVFB_RUNNER} -n 12 -s "-screen 0 1024x768x24" -w 1
  ${TESTENV_BINARY_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/test_xvfbsize.py
)
set_tests_properties(Framebuffer.CreateScreenshot  Framebuffer.CheckSize
                     PROPERTIES ENVIRONMENT "${PYTHON_TEST_ENVIRONMENT}")

# test basic LAMMPS-GUI editor tasks
add_test(NAME Framebuffer.GUIEditorChecks
  COMMAND ${XVFB_RUNNER} -n 13 -s "-screen 0 1024x768x24" -w 1
  ${TESTENV_BINARY_DIR}/python ${CMAKE_CURRENT_SOURCE_DIR}/test_gui_edit.py
)
set_tests_properties(Framebuffer.GUIEditorChecks
                     PROPERTIES ENVIRONMENT "${PYTHON_TEST_ENVIRONMENT}")
endif()
