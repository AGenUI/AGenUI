# ---------------------------------------------------------------------------
# Bundled prebuilt native dependencies.
#
# This directory hosts prebuilt artifacts of native libraries so that the build
# can avoid downloading and compiling them every time. When a library's
# subdirectory is present, an IMPORTED target with the same name is declared
# automatically; otherwise nothing happens here and the parent CMakeLists falls
# back to other sources (system package, FetchContent, ...).
#
# Layout convention (per library):
#   prebuilt/<lib>/include/...        public headers root
#   prebuilt/<lib>/libs/lib<lib>.so   shared library (OHOS arm64-v8a only)
#
# Currently supported libraries:
#   - yoga (https://github.com/facebook/yoga, v2.0.0)
#
# To add a new prebuilt library, copy the yoga block below and adapt the names.
# ---------------------------------------------------------------------------

# ----- yoga ---------------------------------------------------------------
set(_YOGA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/yoga")
set(_YOGA_LIB "${_YOGA_DIR}/libs/libyoga.so")
set(_YOGA_INCLUDE "${_YOGA_DIR}/include")

if(EXISTS "${_YOGA_LIB}")
    if(NOT IS_DIRECTORY "${_YOGA_INCLUDE}")
        message(FATAL_ERROR
            "AGenUI: prebuilt yoga lib found at ${_YOGA_LIB} "
            "but include dir is missing at ${_YOGA_INCLUDE}. "
            "Make sure prebuilt/yoga/ contains both include/ and libs/libyoga.so.")
    endif()
    message(STATUS "AGenUI: using bundled prebuilt yoga at ${_YOGA_DIR}")
    add_library(yoga SHARED IMPORTED GLOBAL)
    set_target_properties(yoga PROPERTIES
        IMPORTED_LOCATION "${_YOGA_LIB}"
        INTERFACE_INCLUDE_DIRECTORIES "${_YOGA_INCLUDE}"
    )
endif()
