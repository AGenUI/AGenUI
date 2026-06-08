# CMakeLists.txt for AGenUI standalone Android AAR build
cmake_minimum_required(VERSION 3.18.1)

project("amap_AGenUI")

# ---------------------------------------------------------------------------
# Native debug symbols (opt-in)
# ---------------------------------------------------------------------------
# When AGENUI_EMIT_DEBUG_SYMBOLS=ON (and CMAKE_BUILD_TYPE=Release):
#   - DWARF symbol info is split out of each non-IMPORTED SHARED target into a
#     companion file named lib<target>.so.debug, sitting next to the .so in the
#     CMake output directory (build/intermediates/cxx/Release/<hash>/obj/<abi>/).
#   - The original .so is then stripped and tagged with a .gnu_debuglink ELF
#     section pointing at lib<target>.so.debug, so addr2line/llvm-symbolizer/
#     ndk-stack can find the symbol file automatically by name.
#   - The .so.debug suffix follows the GNU/LLVM convention (GDB doc: "The
#     default file extension for objcopy is .debug"). It is NOT related to
#     Android's "debug" buildType.
# Defaults to OFF so open-source consumers see no change; the amap wrapper
# scripts flip it on via -DAGENUI_EMIT_DEBUG_SYMBOLS=ON for release builds
# whose symbols are uploaded alongside the AAR.
option(AGENUI_EMIT_DEBUG_SYMBOLS
       "Emit lib*.so.debug companion files alongside stripped .so for release builds" OFF)

function(agenui_split_debug_symbols target)
    if(NOT AGENUI_EMIT_DEBUG_SYMBOLS)
        return()
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        return()
    endif()
    if(NOT CMAKE_OBJCOPY OR NOT CMAKE_STRIP)
        message(WARNING "agenui_split_debug_symbols(${target}): CMAKE_OBJCOPY or CMAKE_STRIP unavailable; skip.")
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --only-keep-debug
                $<TARGET_FILE:${target}> $<TARGET_FILE:${target}>.debug
        COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:${target}>
        COMMAND ${CMAKE_OBJCOPY} --add-gnu-debuglink=$<TARGET_FILE_NAME:${target}>.debug
                $<TARGET_FILE:${target}>
        WORKING_DIRECTORY $<TARGET_FILE_DIR:${target}>
        COMMENT "[symbols] split: $<TARGET_FILE_NAME:${target}> -> $<TARGET_FILE_NAME:${target}>.debug"
        VERBATIM)
endfunction()

# ---------------------------------------------------------------------------
# yoga: prebuilt binary OR build from source via FetchContent
# ---------------------------------------------------------------------------
# Option: YOGA_PREBUILT_DIR - path to a directory containing prebuilt yoga artifacts
# Expected structure:
#   {YOGA_PREBUILT_DIR}/
#   ├── include/yoga/   (header files)
#   └── libs/libyoga.so (shared library)
#
# If YOGA_PREBUILT_DIR is set and valid, use prebuilt; otherwise fetch from GitHub.
# ---------------------------------------------------------------------------

if(DEFINED YOGA_PREBUILT_DIR AND EXISTS "${YOGA_PREBUILT_DIR}/libs/libyoga.so")
    message(STATUS "Using prebuilt yoga from: ${YOGA_PREBUILT_DIR}")

    # Import prebuilt libyoga.so as a shared library
    add_library(yoga SHARED IMPORTED)
    set_target_properties(yoga PROPERTIES
        IMPORTED_LOCATION "${YOGA_PREBUILT_DIR}/libs/libyoga.so"
    )
    # Make headers available
    target_include_directories(yoga INTERFACE
        "${YOGA_PREBUILT_DIR}/include"
    )
    # Set yoga_SOURCE_DIR for downstream include_directories compatibility
    set(yoga_SOURCE_DIR "${YOGA_PREBUILT_DIR}/include")

else()
    if(DEFINED YOGA_PREBUILT_DIR)
        message(WARNING "YOGA_PREBUILT_DIR is set but libyoga.so not found at: ${YOGA_PREBUILT_DIR}/libs/libyoga.so")
        message(WARNING "Falling back to FetchContent from GitHub...")
    endif()

    message(STATUS "Building yoga from source (GitHub FetchContent)")
    include(FetchContent)
    FetchContent_Declare(yoga
        GIT_REPOSITORY https://github.com/facebook/yoga.git
        GIT_TAG        v2.0.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_Populate(yoga)

    file(GLOB_RECURSE YOGA_SOURCES "${yoga_SOURCE_DIR}/yoga/*.cpp")
    add_library(yoga SHARED ${YOGA_SOURCES})
    target_include_directories(yoga
        PUBLIC "${yoga_SOURCE_DIR}"
    )
    target_compile_options(yoga PRIVATE
        -fno-omit-frame-pointer
        -fexceptions
        -fno-rtti
        -Wno-error
    )
    target_include_directories(yoga PRIVATE
        "${yoga_SOURCE_DIR}/include"
        "${yoga_SOURCE_DIR}/include/yoga"
    )
    set_target_properties(yoga PROPERTIES
        CXX_VISIBILITY_PRESET default
        POSITION_INDEPENDENT_CODE ON
    )

    target_link_libraries(yoga PRIVATE log)
endif()

add_definitions(-DPLATFORM_ANDROID)
add_definitions(-D_GNU_SOURCE)
add_definitions(-DMT_MAIN=15)
add_definitions(-DMT_SUB=0)
add_definitions(-DCONFIG_VERSION="2025-09-13")

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_WARNING_FLAGS "-Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-parameter")

# Common C flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_WARNING_FLAGS}")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -funsigned-char")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fshort-wchar")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")

# Common C++ flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fshort-wchar")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_definitions(-DNDEBUG)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fPIC -g")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -fPIC -g")
    # Link-time optimization
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -flto")
    # Linker: remove unused code sections
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    # NOTE: -Wl,--strip-all was removed so DWARF info reaches the post-build step
    # that splits symbols into lib*.so.debug (see agenui_split_debug_symbols below).
    # AGP runs its own strip on the .so before packaging, so the release AAR size
    # is unaffected; .so.debug stays at the cxx intermediate output directory.
else()
    add_definitions(-DDEBUG)
    add_definitions(-D_DEBUG)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -fPIC -g")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -fPIC -g")
endif()

# ============ Path to C++ core (monorepo) ============
# AGENUI_REPO_ROOT is injected by platforms/android/build.gradle via -DAGENUI_REPO_ROOT=...
# It always points to the agenui/ repo root, avoiding .. traversal inside CMakeLists.
if(NOT DEFINED AGENUI_REPO_ROOT)
    message(FATAL_ERROR "AGENUI_REPO_ROOT is not defined. It must be passed via build.gradle externalNativeBuild arguments.")
endif()
set(AGENUI_CPP_ROOT "${AGENUI_REPO_ROOT}/core")

include_directories(
    ${AGENUI_CPP_ROOT}/include
    ${AGENUI_CPP_ROOT}/src
    ${AGENUI_CPP_ROOT}/src/jni
    ${AGENUI_CPP_ROOT}/src/third_party
    ${AGENUI_CPP_ROOT}/src/third_party/nlohmann
    ${yoga_SOURCE_DIR}
)

# src/ root level cpp files (agenui_log.cpp, agenui_errorcode_define.cpp, etc.)
file(GLOB AGENUI_ROOT_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/*.cpp"
)

# src/jni (JNI files for Android)
file(GLOB JNI_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/jni/*.cpp"
)

# src/module
file(GLOB MODULE_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/module/*.cpp"
)

# src/stream
file(GLOB STREAM_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/stream/*.cpp"
)

# src/function_call
file(GLOB_RECURSE FUNCTIONCALL_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/function_call/*.cpp"
)

# src/surface (all subdirectories recursively)
file(GLOB_RECURSE SURFACE_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/surface/*.cpp"
)

# src/style_parser
file(GLOB STYLE_PARSER_SOURCES CONFIGURE_DEPENDS
    "${AGENUI_CPP_ROOT}/src/style_parser/*.cpp"
)

# Combine all sources
set(AGENUI_SOURCES
    ${AGENUI_ROOT_SOURCES}
    ${JNI_SOURCES}
    ${MODULE_SOURCES}
    ${STREAM_SOURCES}
    ${FUNCTIONCALL_SOURCES}
    ${SURFACE_SOURCES}
    ${STYLE_PARSER_SOURCES}
)

# ============ Exclude platform-specific and non-Android files ============
# Exclude Android platform invoker (JNI layer handles this instead)
list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*agenui_platform_invoker_android\\.cpp$")
# Exclude third_party/Html.cpp (Harmony only, not for Android/iOS)
list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*/third_party/Html.cpp")
# Exclude sax/ (Harmony only, not for Android/iOS)
list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*/third_party/sax/.*")
# Exclude ik/ (not in Android all_srcs_path)
list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*/third_party/ik/.*")
list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*/third_party/key_define.cpp")

# ============ Shared library ============
add_library(${CMAKE_PROJECT_NAME} SHARED
    ${AGENUI_SOURCES}
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    z
    android
    log
    yoga
)

# Note: -Wl,--no-wchar-size-warning is not supported by lld (Android NDK linker), omitted
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
    -Wl,-z,max-page-size=16384
)

# Emit lib<name>.so.debug companion files when AGENUI_EMIT_DEBUG_SYMBOLS=ON.
# See the comment block near the top of this file for behavior details.
agenui_split_debug_symbols(${CMAKE_PROJECT_NAME})

# yoga only needs the same treatment when it is built from source (FetchContent
# path); the prebuilt path imports it as IMPORTED and has no .so we can rewrite.
if(TARGET yoga)
    get_target_property(_agenui_yoga_imported yoga IMPORTED)
    if(NOT _agenui_yoga_imported)
        agenui_split_debug_symbols(yoga)
    endif()
endif()
