# the minimum version of CMake.
cmake_minimum_required(VERSION 3.14)
project(AGenUI)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

find_library(
    arkui-lib
    ace_ndk.z
    REQUIRED
)

add_definitions(-DHARMONY)

add_definitions(-DCONFIG_VERSION="2025-09-13")

# ---------------------------------------------------------------------------
# Developer options: use system-installed yoga / iksemel
# ---------------------------------------------------------------------------
option(AGENUI_USE_SYSTEM_YOGA "Use system-installed yoga instead of FetchContent" OFF)
option(AGENUI_USE_SYSTEM_IKSEMEL "Use system-installed iksemel instead of vendored source" OFF)
set(AGENUI_YOGA_SOURCE_DIR "" CACHE PATH "Local yoga source directory (skip download)")

# Resolve AGENUI_REPO_ROOT early (needed for core source path)
if(DEFINED OHOS_MODULE_DIR)
    get_filename_component(AGENUI_REPO_ROOT "${OHOS_MODULE_DIR}/../../.." ABSOLUTE)
else()
    get_filename_component(AGENUI_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../.." ABSOLUTE)
endif()

# ---------------------------------------------------------------------------
# yoga sources, in priority order:
#   1. Bundled prebuilt (declared by prebuilt/CMakeLists.txt, zero build cost).
#   2. System-installed yoga (-DAGENUI_USE_SYSTEM_YOGA=ON).
#   3. FetchContent from GitHub source (fallback).
# ---------------------------------------------------------------------------
add_subdirectory(prebuilt)

if(NOT TARGET yoga)
  if(AGENUI_USE_SYSTEM_YOGA)
    find_package(yoga REQUIRED)
  else()
    include(FetchContent)
    if(AGENUI_YOGA_SOURCE_DIR)
        FetchContent_Declare(yoga SOURCE_DIR "${AGENUI_YOGA_SOURCE_DIR}")
    else()
        FetchContent_Declare(yoga
            GIT_REPOSITORY https://github.com/facebook/yoga.git
            GIT_TAG        v2.0.0
            GIT_SHALLOW    TRUE
        )
    endif()
    FetchContent_Populate(yoga)

    # Build yoga as a shared library (separate from the core)
    file(GLOB_RECURSE YOGA_SOURCES "${yoga_SOURCE_DIR}/yoga/*.cpp")
    add_library(yoga SHARED ${YOGA_SOURCES})
    target_include_directories(yoga
        PUBLIC "${yoga_SOURCE_DIR}"
    )
    # OHOS cross-compile: disable -Werror and add include paths for internal headers
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
  endif()
endif()

# ---------------------------------------------------------------------------
# iksemel: build from vendored source (LGPL v2.1, shared library)
# Source: https://github.com/meduketto/iksemel (commit 03c0e84)
# ---------------------------------------------------------------------------
if(AGENUI_USE_SYSTEM_IKSEMEL)
    find_library(IKSEMEL_LIB iksemel REQUIRED)
    add_library(iksemel SHARED IMPORTED)
    set_target_properties(iksemel PROPERTIES IMPORTED_LOCATION ${IKSEMEL_LIB})
else()
    set(IKSEMEL_VENDORED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/third_party/ik")
    file(GLOB IKSEMEL_SOURCES "${IKSEMEL_VENDORED_DIR}/src/*.c")
    add_library(iksemel SHARED ${IKSEMEL_SOURCES})
    # config.h in ik/ root must be found BEFORE include/ (shadows autotools config)
    target_include_directories(iksemel BEFORE
        PRIVATE "${IKSEMEL_VENDORED_DIR}"
    )
    target_include_directories(iksemel
        PUBLIC "${IKSEMEL_VENDORED_DIR}/include"
    )
    target_compile_definitions(iksemel PRIVATE HAVE_CONFIG_H ASL_IKSEMEL_DLL)
    set_target_properties(iksemel PROPERTIES
        C_VISIBILITY_PRESET default
        POSITION_INDEPENDENT_CODE ON
    )
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/a2ui)

set(Harmony_AGenUI_DIR "${AGENUI_REPO_ROOT}/core")
include_directories(
    ${Harmony_AGenUI_DIR}/include
    ${Harmony_AGenUI_DIR}/src
    ${Harmony_AGenUI_DIR}/src/surface
    ${Harmony_AGenUI_DIR}/src/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/a2ui/third_party/ik/include
)

file(GLOB_RECURSE AGENUI_SOURCES
    "${Harmony_AGenUI_DIR}/src/*.cpp"
    "${Harmony_AGenUI_DIR}/src/surface/*.cpp"
    "${Harmony_AGenUI_DIR}/src/module/*.cpp"
    "${Harmony_AGenUI_DIR}/src/stream/*.cpp"
    "${Harmony_AGenUI_DIR}/src/function_call/*.cpp"
)

list(FILTER AGENUI_SOURCES EXCLUDE REGEX ".*/jni/.*")

file(GLOB A2UI_ROOT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/*.cpp"
)

file(GLOB A2UI_RENDER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/render/*.cpp"
)

file(GLOB A2UI_MEASURE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/measure/*.cpp"
)

file(GLOB_RECURSE A2UI_COMPONENTS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/render/components/*.cpp"
)

file(GLOB_RECURSE A2UI_PARSER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/render/parser/*.cpp"
)

file(GLOB_RECURSE A2UI_FACTORY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/render/factory/*.cpp"
)

file(GLOB_RECURSE A2UI_BRIDGE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/bridge/*.cpp"
)
list(REMOVE_DUPLICATES A2UI_BRIDGE_SOURCES)

file(GLOB_RECURSE A2UI_UTILS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/utils/*.cpp"
)

file(GLOB_RECURSE A2UI_HYBRID_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/render/hybrid/*.cpp"
)

file(GLOB_RECURSE A2UI_THIRD_PARTY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/third_party/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/a2ui/third_party/sax/sax.c"
)

set(A2UI_SOURCES
    ${A2UI_ROOT_SOURCES}
    ${A2UI_RENDER_SOURCES}
    ${A2UI_MEASURE_SOURCES}
    ${A2UI_COMPONENTS_SOURCES}
    ${A2UI_PARSER_SOURCES}
    ${A2UI_FACTORY_SOURCES}
    ${A2UI_BRIDGE_SOURCES}
    ${A2UI_UTILS_SOURCES}
    ${A2UI_HYBRID_SOURCES}
    ${A2UI_THIRD_PARTY_SOURCES}
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(a2ui-capi SHARED
    napi_init.cpp
    a2ui_api.h
    a2ui_hm_helper.h
    ${AGENUI_SOURCES}
    ${A2UI_SOURCES}
)

target_link_libraries(a2ui-capi PUBLIC
    libace_napi.z.so
    libhilog_ndk.z.so
    native_drawing
    ${arkui-lib}
    yoga
    iksemel
    libavplayer.so
    libnative_media_core.so
    pixelmap
)
