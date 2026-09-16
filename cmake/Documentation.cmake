##########################################################################
# Documentation build: Doxygen API extraction plus the Sphinx targets
# (html, spelling, linkcheck, latex, pdf, doc) run from a python virtual
# environment that is set up on demand.  Only active when BUILD_DOC or
# BUILD_DOC_ONLY is enabled.
##########################################################################

if(BUILD_DOC OR BUILD_DOC_ONLY)
  # Find Doxygen for API documentation
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_CONFIG_FILE_IN ${CMAKE_SOURCE_DIR}/doc/Doxyfile.in)
    set(DOXYGEN_CONFIG_FILE ${CMAKE_BINARY_DIR}/doc/Doxyfile)
    set(DOXYGEN_OUTPUT_DIR ${CMAKE_BINARY_DIR}/doc/doxygen)

    # Configure Doxyfile
    configure_file(${DOXYGEN_CONFIG_FILE_IN} ${DOXYGEN_CONFIG_FILE} @ONLY)

    # Create custom target for Doxygen
    add_custom_target(doxygen
      COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM
    )
  else()
    message(WARNING "Doxygen not found. API documentation will not be generated.")
  endif()

  # Current Sphinx versions require at least Python 3.8
  # use default (or custom) Python executable, if version is sufficient
  if(Python_VERSION VERSION_GREATER_EQUAL 3.8)
    set(Python3_EXECUTABLE ${Python_EXECUTABLE})
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  if(Python3_VERSION VERSION_LESS 3.8)
    message(FATAL_ERROR "Python 3.8 and up is required to build the LAMMPS-GUI HTML documentation")
  endif()
  if (CMAKE_CROSSCOMPILING)
    # must force host python when cross-compiling
    set(VIRTUALENV python3 -m venv)
  else()
    set(VIRTUALENV ${Python3_EXECUTABLE} -m venv)
  endif()

  set(SPHINX_DIR ${CMAKE_SOURCE_DIR}/doc)
  set(SPHINX_CONFIG_FILE_TEMPLATE ${SPHINX_DIR}/conf.py.in)
  file(GLOB DOC_SOURCES CONFIGURE_DEPENDS ${SPHINX_DIR}/[0-9a-z]*.rst)

  # configuration and static files are copied to binary dir to avoid collisions with parallel builds
  set(DOC_BUILD_DIR ${CMAKE_BINARY_DIR}/doc)
  set(DOC_BUILD_STATIC_DIR ${DOC_BUILD_DIR}/_static)
  set(DOC_BUILD_CONFIG_FILE ${DOC_BUILD_DIR}/conf.py)

  file(MAKE_DIRECTORY ${DOC_BUILD_DIR})
  configure_file(${SPHINX_CONFIG_FILE_TEMPLATE} ${DOC_BUILD_CONFIG_FILE})
  file(COPY ${SPHINX_DIR}/_static DESTINATION ${DOC_BUILD_DIR}/)
  file(COPY ${SPHINX_DIR}/_templates DESTINATION ${DOC_BUILD_DIR}/)
  file(COPY ${SPHINX_DIR}/LAMMPSLexer.py DESTINATION ${DOC_BUILD_DIR}/)
  file(COPY ${CMAKE_SOURCE_DIR}/resources/icons/lammps-gui-banner.png DESTINATION ${DOC_BUILD_DIR})

  # configure virtual environment for sphinx-build and related
  set(DOCENV_BINARY_DIR ${CMAKE_BINARY_DIR}/docenv/bin)
  set(DOCENV_REQUIREMENTS_FILE ${SPHINX_DIR}/requirements.txt)
  set(DOCENV_DEPS ${DOCENV_REQUIREMENTS_FILE} ${SPHINX_DIR}/_templates/page.html)
  add_custom_target(create-docenv
    COMMAND ${VIRTUALENV} docenv --clear
    COMMAND ${DOCENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install --upgrade pip
    COMMAND ${DOCENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install -r ${DOCENV_REQUIREMENTS_FILE} --upgrade
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )
  # custom target to update the python virtual environment
  add_custom_target(
    upgrade
    DEPENDS create-docenv ${DOCENV_REQUIREMENTS_FILE}
    COMMAND ${DOCENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install --upgrade pip
    COMMAND ${DOCENV_BINARY_DIR}/pip $ENV{PIP_OPTIONS} install -r ${DOCENV_REQUIREMENTS_FILE} --upgrade
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  if(NOT TARGET Sphinx::sphinx-build)
    add_executable(Sphinx::sphinx-build IMPORTED GLOBAL)
    set_target_properties(Sphinx::sphinx-build PROPERTIES IMPORTED_LOCATION "${DOCENV_BINARY_DIR}/sphinx-build")
  endif()

  add_custom_target(
    html
    DEPENDS create-docenv ${DOC_SOURCES} ${DOCENV_DEPS} ${SPHINX_CONFIG_FILE_TEMPLATE} ${SPHINX_DIR}/_static/style.css
    COMMAND Sphinx::sphinx-build ${SPHINX_EXTRA_OPTS} -b html -c ${DOC_BUILD_DIR} -d ${DOC_BUILD_DIR}/doctrees ${CMAKE_SOURCE_DIR}/doc ${DOC_BUILD_DIR}/html ${DOC_SOURCES}
  )
  # remove virtual environment on "make clean"
  set_target_properties(html
    PROPERTIES ADDITIONAL_CLEAN_FILES "${CMAKE_BINARY_DIR}/docenv")

  # Make html target depend on doxygen if available
  if(DOXYGEN_FOUND)
    add_dependencies(html doxygen)
  endif()

  add_custom_target(
    spelling
    DEPENDS create-docenv ${DOC_SOURCES} ${DOCENV_DEPS} ${SPHINX_CONFIG_FILE_TEMPLATE} ${SPHINX_DIR}/_static/style.css
    COMMAND Sphinx::sphinx-build ${SPHINX_EXTRA_OPTS} -b spelling -c ${DOC_BUILD_DIR} -d ${DOC_BUILD_DIR}/doctrees ${CMAKE_SOURCE_DIR}/doc ${DOC_BUILD_DIR}/spelling ${DOC_SOURCES}
  )

  add_custom_target(
    linkcheck
    DEPENDS create-docenv ${DOC_SOURCES} ${DOCENV_DEPS} ${SPHINX_CONFIG_FILE_TEMPLATE} ${SPHINX_DIR}/_static/style.css
    COMMAND Sphinx::sphinx-build ${SPHINX_EXTRA_OPTS} -b linkcheck -c ${DOC_BUILD_DIR} -d ${DOC_BUILD_DIR}/doctrees ${CMAKE_SOURCE_DIR}/doc ${DOC_BUILD_DIR}/linkcheck ${DOC_SOURCES}
  )

  add_custom_target(
    latex
    DEPENDS create-docenv ${DOC_SOURCES} ${DOCENV_DEPS} ${SPHINX_CONFIG_FILE_TEMPLATE} ${SPHINX_DIR}/_static/style.css
    COMMAND Sphinx::sphinx-build ${SPHINX_EXTRA_OPTS} -b latex -c ${DOC_BUILD_DIR} -d ${DOC_BUILD_DIR}/doctrees ${CMAKE_SOURCE_DIR}/doc ${DOC_BUILD_DIR}/latex ${DOC_SOURCES}
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/idxlayout.sty ${DOC_BUILD_DIR}/latex/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/doc/ellipse.sty ${DOC_BUILD_DIR}/latex/
  )
  # Make latex target depend on doxygen if available
  if(DOXYGEN_FOUND)
    add_dependencies(latex doxygen)
  endif()

  # need latexmk and pdflatex for translating LaTeX to PDF
  find_program(PDFLATEX_COMMAND pdflatex DOC "Path to pdflatex command")
  find_program(LATEXMK_COMMAND latexmk DOC "Path to latex command")
  if (PDFLATEX_COMMAND AND LATEXMK_COMMAND)
    add_custom_target(
      pdf
      DEPENDS latex
      COMMAND ${LATEXMK_COMMAND} -pdf -dvi- -ps- lammps-gui.tex
      COMMAND ${CMAKE_COMMAND} -E rename lammps-gui.pdf ../../lammps-gui-v${PROJECT_VERSION}.pdf
      WORKING_DIRECTORY ${DOC_BUILD_DIR}/latex
    )
  else()
    add_custom_target(
      pdf
      COMMAND ${CMAKE_COMMAND} -E echo "Need to have pdflatex and latexmk installed to create PDF of docs"
    )
  endif()

  add_custom_target(
    doc ALL
    DEPENDS create-docenv html
    SOURCES ${CMAKE_SOURCE_DIR}/doc/requirements.txt ${DOC_SOURCES}
  )

  install(DIRECTORY ${DOC_BUILD_DIR}/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
