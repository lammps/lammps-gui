##########################################################################
# Unit tests (GoogleTest via CTest) and code coverage reporting.
# Testing is only supported on native Linux builds.  Expects the
# lammps-gui target to exist (the coverage flags are applied to it).
##########################################################################

if(ENABLE_TESTING AND (NOT CMAKE_CROSSCOMPILING) AND (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  message(STATUS "Testing is enabled")
  include(CTest)
  add_subdirectory(test)
  # Compiler specific features for testing
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    option(ENABLE_COVERAGE "Enable collecting code coverage data" OFF)
    if(ENABLE_COVERAGE)
      target_compile_options(lammps-gui PUBLIC --coverage)
      target_link_options(lammps-gui PUBLIC --coverage)

      find_program(GCOVR_BINARY gcovr)
      include(FindPackageHandleStandardArgs)
      find_package_handle_standard_args(GCOVR DEFAULT_MSG GCOVR_BINARY)
      if(GCOVR_FOUND)
        message(STATUS "Code coverage reporting with gcovr is enabled")
        get_filename_component(ABSOLUTE_SOURCE_DIR ${CMAKE_SOURCE_DIR} ABSOLUTE)

        set(COVERAGE_HTML_DIR ${CMAKE_BINARY_DIR}/coverage_html)
        add_custom_target(coverage_html_folder
          COMMAND ${CMAKE_COMMAND} -E make_directory ${COVERAGE_HTML_DIR})

        add_custom_target(
          coverage
          COMMAND ${GCOVR_BINARY} -s  --html --html-nested --html-self-contained -r ${ABSOLUTE_SOURCE_DIR} --object-directory=${CMAKE_BINARY_DIR} -o ${COVERAGE_HTML_DIR}/index.html
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMENT "Generating HTML coverage report..."
        )
        add_dependencies(coverage coverage_html_folder)

        add_custom_target(clean_coverage
          ${CMAKE_COMMAND} -E remove_directory ${COVERAGE_HTML_DIR}
          COMMAND ${CMAKE_COMMAND} -E remove -f */*.gcda */*/*.gcda */*/*/*.gcda
          */*/*/*/*.gcda */*/*/*/*/*.gcda */*/*/*/*/*/*.gcda
          */*/*/*/*/*/*/*.gcda */*/*/*/*/*/*/*/*.gcda
          */*/*/*/*/*/*/*/*/*.gcda */*/*/*/*/*/*/*/*/*/*.gcda
          WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
          COMMENT "Deleting coverage report and coverage data files..."
        )
      endif()
    endif()
  endif()
else()
  message(STATUS "Testing is disabled")
endif()
