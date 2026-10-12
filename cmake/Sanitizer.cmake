##########################################################################
# Optional instrumentation of the application with a code sanitizer,
# selected through the ENABLE_SANITIZER cache variable.  Only supported
# by GNU and Clang compilers; ignored with a warning when the lammps-gui
# target is not built (BUILD_DOC_ONLY=ON).
##########################################################################

# helper function
function(validate_option name values)
    string(TOLOWER ${${name}} needle_lower)
    string(TOUPPER ${${name}} needle_upper)
    list(FIND ${values} ${needle_lower} IDX_LOWER)
    list(FIND ${values} ${needle_upper} IDX_UPPER)
    if(${IDX_LOWER} LESS 0 AND ${IDX_UPPER} LESS 0)
        set(POSSIBLE_VALUE_LIST "")
        foreach(VALUE IN LISTS ${values})
            string(APPEND POSSIBLE_VALUE_LIST "- ${VALUE}\n")
        endforeach()
        message(FATAL_ERROR "\n########################################################################\n"
                            "Invalid value '${${name}}' for option ${name}\n"
                            "\n"
                            "Possible values are:\n"
                            "${POSSIBLE_VALUE_LIST}"
                            "########################################################################")
    endif()
endfunction(validate_option)

set(ENABLE_SANITIZER "none" CACHE STRING "Select a code sanitizer option (none (default), address, hwaddress, leak, thread, undefined)")
set(ENABLE_SANITIZER_VALUES none address hwaddress leak thread undefined)
set_property(CACHE ENABLE_SANITIZER PROPERTY STRINGS ${ENABLE_SANITIZER_VALUES})
validate_option(ENABLE_SANITIZER ENABLE_SANITIZER_VALUES)
string(TOLOWER ${ENABLE_SANITIZER} ENABLE_SANITIZER)
if(NOT ENABLE_SANITIZER STREQUAL "none")
  # with BUILD_DOC_ONLY there is no application target to instrument
  if(NOT TARGET lammps-gui)
    message(WARNING "ENABLE_SANITIZER option has no effect when not building the application. Ignoring.")
    set(ENABLE_SANITIZER "none")
  elseif((${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU") OR (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang"))
    target_compile_options(lammps-gui PUBLIC -fsanitize=${ENABLE_SANITIZER})
    target_link_options(lammps-gui PUBLIC -fsanitize=${ENABLE_SANITIZER})
  else()
    message(WARNING "ENABLE_SANITIZER option not supported by ${CMAKE_CXX_COMPILER_ID} compilers. Ignoring.")
    set(ENABLE_SANITIZER "none")
  endif()
endif()
