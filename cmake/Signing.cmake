# Signing.cmake - support for cryptographic signing of binaries
#
#   include(packaging/Signing.cmake)
#   ...
#   add_executable(myapp ...)
#   sign_target(myapp)                 # signs the exe right after linking
#
# Signing only activates for Windows cross builds and can be switched off
# with -DCODE_SIGNING=OFF (or SIGN_DISABLE=1 in the environment).

if((CMAKE_SYSTEM_NAME STREQUAL "Windows") AND CMAKE_CROSSCOMPILING)
  option(CODE_SIGNING "Authenticode-sign Windows binaries" ON)
else()
  option(CODE_SIGNING "Authenticode-sign Windows binaries" OFF)
endif()

set(SIGN_SCRIPT "${CMAKE_SOURCE_DIR}/packaging/sign.sh")

function(sign_target tgt)
    if(NOT WIN32 OR NOT CODE_SIGNING)
        return()
    endif()
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND "${SIGN_SCRIPT}" "$<TARGET_FILE:${tgt}>"
        COMMENT "Authenticode signing $<TARGET_FILE:${tgt}>"
        VERBATIM)
endfunction()

# Optional helper for signing loose files (bundled DLLs etc.) as part of a
# packaging target:
#   sign_files(sign_deps "${CMAKE_BINARY_DIR}/dist/foo.dll" ...)
function(sign_files custom_target_name)
    if(NOT WIN32 OR NOT CODE_SIGNING)
        add_custom_target(${custom_target_name})
        return()
    endif()
    add_custom_target(${custom_target_name}
        COMMAND "${SIGN_SCRIPT}" ${ARGN}
        COMMENT "Authenticode signing bundled files"
        VERBATIM)
endfunction()
