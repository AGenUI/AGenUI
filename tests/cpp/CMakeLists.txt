# =============================================================================
# AGenUI C++ Test Suite
# =============================================================================
#
# Standalone test project that builds AGenUI core sources from `core/src/`
# and runs them against gtest/gmock. Does NOT modify or rebuild the platform
# bridges under `platforms/`.
#
# Usage:
#   cmake -S tests/cpp -B tests/cpp/build
#   cmake --build tests/cpp/build -j
#   ctest --test-dir tests/cpp/build --output-on-failure
#
# Options:
#   AGENUI_TESTS_ENABLE_ASAN    Default ON   (mutex with TSAN)
#   AGENUI_TESTS_ENABLE_TSAN    Default OFF  (mutex with ASAN)
#   AGENUI_TESTS_ENABLE_COVERAGE Default OFF
#   AGENUI_TESTS_USE_LOCAL_YOGA Default OFF
#   AGENUI_TESTS_LOCAL_YOGA_DIR (only when USE_LOCAL_YOGA=ON)
# =============================================================================
cmake_minimum_required(VERSION 3.18)
project(agenui_cpp_tests CXX)

# -----------------------------------------------------------------------------
# Multi-config support (Xcode / Visual Studio).
#
# Single-config generators (Make, Ninja) pick the sanitizer at configure
# time via -DAGENUI_TESTS_ENABLE_ASAN / -DAGENUI_TESTS_ENABLE_TSAN.
#
# Multi-config generators (Xcode) define BOTH sanitizers as build
# configurations in the SAME project; the user toggles between them in
# Xcode (Edit Scheme → Run → Build Configuration).
#
#   Debug   → AddressSanitizer + UBSan   (default)
#   Tsan    → ThreadSanitizer
#   Release → no sanitizer
# -----------------------------------------------------------------------------
get_property(AGENUI_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(AGENUI_IS_MULTI_CONFIG)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Tsan;Release" CACHE STRING
        "Available build configurations" FORCE)
    # Mirror Debug-style flags into the Tsan config.
    foreach(_lang C CXX)
        set(CMAKE_${_lang}_FLAGS_TSAN
            "${CMAKE_${_lang}_FLAGS_DEBUG}"
            CACHE STRING "Flags used by the ${_lang} compiler during TSAN builds" FORCE)
    endforeach()
    foreach(_kind EXE SHARED MODULE)
        set(CMAKE_${_kind}_LINKER_FLAGS_TSAN
            "${CMAKE_${_kind}_LINKER_FLAGS_DEBUG}"
            CACHE STRING "Linker flags during TSAN builds" FORCE)
    endforeach()
    message(STATUS "Multi-config generator: configurations = ${CMAKE_CONFIGURATION_TYPES}")
endif()

# -----------------------------------------------------------------------------
# 1. Locate repo root and core source tree
# -----------------------------------------------------------------------------
get_filename_component(AGENUI_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(AGENUI_CORE_ROOT "${AGENUI_REPO_ROOT}/core")

if(NOT EXISTS "${AGENUI_CORE_ROOT}/include/agenui_engine.h")
    message(FATAL_ERROR "AGenUI core not found at ${AGENUI_CORE_ROOT}")
endif()

message(STATUS "AGenUI repo root: ${AGENUI_REPO_ROOT}")

# -----------------------------------------------------------------------------
# 2. C++ standard and global build settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# -----------------------------------------------------------------------------
# 3. Options
# -----------------------------------------------------------------------------
option(AGENUI_TESTS_ENABLE_ASAN "Enable AddressSanitizer + UBSan" ON)
option(AGENUI_TESTS_ENABLE_TSAN "Enable ThreadSanitizer (mutually exclusive with ASan)" OFF)
option(AGENUI_TESTS_ENABLE_COVERAGE "Generate coverage" OFF)
option(AGENUI_TESTS_USE_LOCAL_YOGA "Use local yoga source" OFF)
set(AGENUI_TESTS_LOCAL_YOGA_DIR "" CACHE PATH "Path to local yoga source")

if(AGENUI_TESTS_ENABLE_ASAN AND AGENUI_TESTS_ENABLE_TSAN)
    message(FATAL_ERROR "ASAN and TSAN cannot be enabled together")
endif()

# -----------------------------------------------------------------------------
# 4. Dependencies
#
# Strategy (avoids cross-generator cache conflicts):
#   * gtest: prefer find_package; fall back to FetchContent into
#            ${CMAKE_BINARY_DIR}/_deps (per-build, gitignored under build/)
#   * yoga:  no FetchContent. We only need the *sources*, so a plain
#            git clone into a shared cache path. No CMake subbuild dir
#            means no generator-cache conflict between Make / Xcode / Ninja.
#
# Cache locations:
#   yoga source: $AGENUI_TESTS_DEPS_DIR/yoga-src
#                or ${HOME}/.cache/agenui-tests-deps/yoga-src by default
#                or override -DAGENUI_TESTS_LOCAL_YOGA_DIR=/path/to/yoga
#   gtest:       built per-build under ${CMAKE_BINARY_DIR}/_deps/
#                (or use system gtest via find_package).
# -----------------------------------------------------------------------------

include(FetchContent)

# ------------------- gtest: prefer system install, fallback to FetchContent ----
option(AGENUI_TESTS_FORCE_FETCH_GTEST "Skip system gtest, always FetchContent" OFF)

set(_gtest_target_ok FALSE)
if(NOT AGENUI_TESTS_FORCE_FETCH_GTEST)
    find_package(GTest QUIET CONFIG)
    if(NOT GTest_FOUND)
        find_package(GTest QUIET)  # find-module flavor (older CMake)
    endif()
    if(GTest_FOUND AND TARGET GTest::gtest AND TARGET GTest::gmock)
        set(_gtest_target_ok TRUE)
        message(STATUS "Using system gtest: ${GTest_DIR}")
    endif()
endif()

if(NOT _gtest_target_ok)
    message(STATUS "System gtest not found; fetching v1.14.0 into ${CMAKE_BINARY_DIR}/_deps")
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# ------------------- yoga: shared SOURCE cache, no FetchContent subbuild ------
# Resolve the cache directory:
#   1. -DAGENUI_TESTS_LOCAL_YOGA_DIR=/path  (explicit)
#   2. $AGENUI_YOGA_SOURCE_DIR              (env, shared with platforms/)
#   3. $AGENUI_TESTS_DEPS_DIR/yoga-src      (env override)
#   4. ${HOME}/.cache/agenui-tests-deps/yoga-src   (default)
if(NOT AGENUI_TESTS_LOCAL_YOGA_DIR)
    if(DEFINED ENV{AGENUI_YOGA_SOURCE_DIR})
        set(AGENUI_TESTS_LOCAL_YOGA_DIR "$ENV{AGENUI_YOGA_SOURCE_DIR}")
    elseif(DEFINED ENV{AGENUI_TESTS_DEPS_DIR})
        set(AGENUI_TESTS_LOCAL_YOGA_DIR "$ENV{AGENUI_TESTS_DEPS_DIR}/yoga-src")
    elseif(DEFINED ENV{HOME})
        set(AGENUI_TESTS_LOCAL_YOGA_DIR "$ENV{HOME}/.cache/agenui-tests-deps/yoga-src")
    else()
        set(AGENUI_TESTS_LOCAL_YOGA_DIR "${CMAKE_BINARY_DIR}/_yoga-src")
    endif()
endif()

# Clone if the cache is empty.
if(NOT EXISTS "${AGENUI_TESTS_LOCAL_YOGA_DIR}/yoga/Yoga.h")
    message(STATUS "yoga not found at ${AGENUI_TESTS_LOCAL_YOGA_DIR}; cloning v2.0.0")
    get_filename_component(_yoga_parent "${AGENUI_TESTS_LOCAL_YOGA_DIR}" DIRECTORY)
    file(MAKE_DIRECTORY "${_yoga_parent}")
    execute_process(
        COMMAND git clone --depth 1 --branch v2.0.0
                https://github.com/facebook/yoga.git
                "${AGENUI_TESTS_LOCAL_YOGA_DIR}"
        RESULT_VARIABLE _yoga_clone_rc
    )
    if(NOT _yoga_clone_rc EQUAL 0)
        message(FATAL_ERROR
            "Failed to clone yoga (rc=${_yoga_clone_rc}). Either:\n"
            " * pre-clone manually:\n"
            "     git clone --depth 1 --branch v2.0.0 https://github.com/facebook/yoga.git \\\n"
            "         ${AGENUI_TESTS_LOCAL_YOGA_DIR}\n"
            " * or point at an existing checkout:\n"
            "     -DAGENUI_TESTS_LOCAL_YOGA_DIR=/path/to/yoga")
    endif()
endif()

message(STATUS "Using yoga source: ${AGENUI_TESTS_LOCAL_YOGA_DIR}")
set(yoga_SOURCE_DIR "${AGENUI_TESTS_LOCAL_YOGA_DIR}")

file(GLOB_RECURSE YOGA_SOURCES "${yoga_SOURCE_DIR}/yoga/*.cpp")
add_library(yogacore STATIC ${YOGA_SOURCES})
target_include_directories(yogacore PUBLIC "${yoga_SOURCE_DIR}")
target_compile_options(yogacore PRIVATE
    -fexceptions -frtti -fno-omit-frame-pointer
    -Wno-error -Wno-everything
)

# -----------------------------------------------------------------------------
# 5. Build agenui_core_for_test (static lib that mirrors core/src)
# -----------------------------------------------------------------------------
file(GLOB_RECURSE AGENUI_CORE_SOURCES
    "${AGENUI_CORE_ROOT}/src/*.cpp"
)
# Exclude platform-specific sources
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "/jni/")
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "agenui_platform_invoker_.*")
# Exclude harmony-only third_party
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "/third_party/.*Html\\.cpp")
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "/third_party/sax/.*")
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "/third_party/ik/.*")
list(FILTER AGENUI_CORE_SOURCES EXCLUDE REGEX "/third_party/key_define\\.cpp")

message(STATUS "AGenUI core source files: ${CMAKE_LIST_LENGTH}")
list(LENGTH AGENUI_CORE_SOURCES _agenui_src_count)
message(STATUS "AGenUI core source count: ${_agenui_src_count}")

add_library(agenui_core_for_test STATIC ${AGENUI_CORE_SOURCES})

target_include_directories(agenui_core_for_test PUBLIC
    ${AGENUI_CORE_ROOT}/include
    ${AGENUI_CORE_ROOT}/src
    ${AGENUI_CORE_ROOT}/src/third_party
    ${AGENUI_CORE_ROOT}/src/third_party/nlohmann
)

target_link_libraries(agenui_core_for_test PUBLIC yogacore)

# Android NDK targets need to link `log` for __android_log_print, which is
# referenced from agenui_core's default logger and from yoga.
if(ANDROID OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(agenui_core_for_test PUBLIC log)
    target_link_libraries(yogacore             PUBLIC log)
endif()

target_compile_options(agenui_core_for_test PUBLIC
    -fexceptions -frtti
    -fno-omit-frame-pointer
)
target_compile_options(agenui_core_for_test PRIVATE
    -Wno-error
    -Wno-everything
)

# -----------------------------------------------------------------------------
# 6. Sanitizer / coverage helper
# -----------------------------------------------------------------------------
function(agenui_apply_sanitizer target)
    if(AGENUI_IS_MULTI_CONFIG)
        # Per-configuration: Debug=ASan, Tsan=TSan, Release=clean
        target_compile_options(${target} PUBLIC
            "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
            "$<$<CONFIG:Debug>:-fno-omit-frame-pointer>"
            "$<$<CONFIG:Tsan>:-fsanitize=thread>"
            "$<$<CONFIG:Tsan>:-fno-omit-frame-pointer>"
        )
        target_link_options(${target} PUBLIC
            "$<$<CONFIG:Debug>:-fsanitize=address,undefined>"
            "$<$<CONFIG:Tsan>:-fsanitize=thread>"
        )
    else()
        if(AGENUI_TESTS_ENABLE_ASAN)
            target_compile_options(${target} PUBLIC -fsanitize=address,undefined)
            target_link_options(${target}    PUBLIC -fsanitize=address,undefined)
        elseif(AGENUI_TESTS_ENABLE_TSAN)
            target_compile_options(${target} PUBLIC -fsanitize=thread)
            target_link_options(${target}    PUBLIC -fsanitize=thread)
        endif()
    endif()
    if(AGENUI_TESTS_ENABLE_COVERAGE)
        target_compile_options(${target} PUBLIC --coverage -O0 -g)
        target_link_options(${target}    PUBLIC --coverage)
    endif()
endfunction()

agenui_apply_sanitizer(agenui_core_for_test)
agenui_apply_sanitizer(yogacore)

# -----------------------------------------------------------------------------
# 7. Fixtures path (compile-time macro)
# -----------------------------------------------------------------------------
set(TESTS_FIXTURE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# Copy standard catalog from test/ into fixtures/catalog (build-time copy, no
# committed duplicate file).
set(_SRC_CATALOG  "${AGENUI_REPO_ROOT}/test/a2ui_standard_catalog.json")
set(_DST_CATALOG  "${TESTS_FIXTURE_DIR}/catalog/standard_catalog.json")
if(EXISTS "${_SRC_CATALOG}")
    configure_file("${_SRC_CATALOG}" "${_DST_CATALOG}" COPYONLY)
    message(STATUS "Catalog copied: ${_SRC_CATALOG} -> ${_DST_CATALOG}")
endif()

# -----------------------------------------------------------------------------
# 8. Test executables
# -----------------------------------------------------------------------------
enable_testing()

set(AGENUI_TESTS_INCLUDE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/support"
    "${AGENUI_CORE_ROOT}/include"
    "${AGENUI_CORE_ROOT}/src"
    "${AGENUI_CORE_ROOT}/src/third_party"
    "${AGENUI_CORE_ROOT}/src/third_party/nlohmann"
)

set(AGENUI_TESTS_COMPILE_DEFS
    "TESTS_FIXTURE_DIR=\"${TESTS_FIXTURE_DIR}\""
)

# ---- 8.1 Unit + Integration + Stress + Sanitizer/ASan ----
file(GLOB_RECURSE _INTEGRATION_SOURCES integration/*.cpp)
file(GLOB_RECURSE _UNIT_SOURCES        unit/*.cpp)
file(GLOB_RECURSE _STRESS_SOURCES      stress/*.cpp)
file(GLOB         _ASAN_SOURCES        sanitizer/memory_safety_test.cpp)

add_executable(agenui_unit_tests
    main.cpp
    ${_INTEGRATION_SOURCES}
    ${_UNIT_SOURCES}
    ${_STRESS_SOURCES}
    ${_ASAN_SOURCES}
)
target_include_directories(agenui_unit_tests PRIVATE ${AGENUI_TESTS_INCLUDE_DIRS})
target_compile_definitions(agenui_unit_tests PRIVATE ${AGENUI_TESTS_COMPILE_DEFS})
target_link_libraries(agenui_unit_tests PRIVATE
    agenui_core_for_test
    GTest::gtest
    GTest::gmock
)
target_compile_options(agenui_unit_tests PRIVATE -Wno-error)
agenui_apply_sanitizer(agenui_unit_tests)

# ---- 8.2 Concurrency ----
# `sanitizer/thread_sanitizer_test.cpp` is also a regular concurrency test;
# we include it here so it's always part of the project tree (visible in
# Xcode / CLion etc.) regardless of which sanitizer is enabled.
file(GLOB_RECURSE _CONCURRENCY_SOURCES concurrency/*.cpp)
file(GLOB         _SANITIZER_THREAD    sanitizer/thread_sanitizer_test.cpp)

if(_CONCURRENCY_SOURCES OR _SANITIZER_THREAD)
    add_executable(agenui_concurrency_tests
        main.cpp
        ${_CONCURRENCY_SOURCES}
        ${_SANITIZER_THREAD}
    )
    target_include_directories(agenui_concurrency_tests PRIVATE ${AGENUI_TESTS_INCLUDE_DIRS})
    target_compile_definitions(agenui_concurrency_tests PRIVATE ${AGENUI_TESTS_COMPILE_DEFS})
    target_link_libraries(agenui_concurrency_tests PRIVATE
        agenui_core_for_test
        GTest::gtest
        GTest::gmock
    )
    target_compile_options(agenui_concurrency_tests PRIVATE -Wno-error)
    agenui_apply_sanitizer(agenui_concurrency_tests)
endif()

# ---- 8.3 Stream Tests (standalone target) ----
# REMOVED: Internal stream unit tests (ProtocolStreamExtractor,
# MarkdownStreamPlugin, TextStreamPlugin, CompositeStreamPlugin)
# have been deleted to enforce black-box testing. Streaming behavior
# is fully covered by the integration tests (in integration/ and
# unit/stream/integration/) which exercise the public ISurfaceManager
# API (beginTextStream / receiveTextChunk / endTextStream).

# ---- 8.4 TSan dedicated executable ----
# Identical sources to agenui_concurrency_tests but built specifically to
# carry the TSan-only tests. In multi-config (Xcode), this target is
# always built and is most useful under the `Tsan` configuration. In
# single-config (Make/Ninja), it's only built when -DAGENUI_TESTS_ENABLE_TSAN=ON.
if((AGENUI_IS_MULTI_CONFIG OR AGENUI_TESTS_ENABLE_TSAN) AND (_CONCURRENCY_SOURCES OR _SANITIZER_THREAD))
    add_executable(agenui_tsan_tests
        main.cpp
        ${_CONCURRENCY_SOURCES}
        ${_SANITIZER_THREAD}
    )
    target_include_directories(agenui_tsan_tests PRIVATE ${AGENUI_TESTS_INCLUDE_DIRS})
    target_compile_definitions(agenui_tsan_tests PRIVATE ${AGENUI_TESTS_COMPILE_DEFS})
    target_link_libraries(agenui_tsan_tests PRIVATE
        agenui_core_for_test
        GTest::gtest
        GTest::gmock
    )
    target_compile_options(agenui_tsan_tests PRIVATE -Wno-error)
    agenui_apply_sanitizer(agenui_tsan_tests)
endif()

# -----------------------------------------------------------------------------
# 8.5 Apple ad-hoc codesign for Xcode generator (macOS Ventura+ arm64
#     refuses to launch unsigned binaries; the Ninja/Make generators sign
#     automatically via the linker but the Xcode generator does not).
# -----------------------------------------------------------------------------
if(CMAKE_GENERATOR STREQUAL "Xcode" AND APPLE)
    foreach(_target agenui_unit_tests agenui_concurrency_tests agenui_tsan_tests)
        if(TARGET ${_target})
            add_custom_command(TARGET ${_target} POST_BUILD
                COMMAND codesign --force --sign - "$<TARGET_FILE:${_target}>"
                COMMENT "Ad-hoc signing ${_target}"
                VERBATIM)
        endif()
    endforeach()
endif()

# -----------------------------------------------------------------------------
# 9. ctest registration (skip for cross-compile builds where the host
#    cannot execute the target binary, e.g. Android NDK).
# -----------------------------------------------------------------------------
if(NOT CMAKE_CROSSCOMPILING AND NOT ANDROID)
    include(GoogleTest)
    gtest_discover_tests(agenui_unit_tests
        PROPERTIES TIMEOUT 60
        DISCOVERY_TIMEOUT 60
    )
    if(TARGET agenui_concurrency_tests)
        gtest_discover_tests(agenui_concurrency_tests
            PROPERTIES TIMEOUT 120
            DISCOVERY_TIMEOUT 120
        )
    endif()
    if(TARGET agenui_tsan_tests)
        gtest_discover_tests(agenui_tsan_tests
            PROPERTIES TIMEOUT 120
            DISCOVERY_TIMEOUT 120
        )
    endif()
else()
    message(STATUS "Cross-compile detected; skipping gtest_discover_tests")
endif()

message(STATUS "")
message(STATUS "==== AGenUI test build summary ====")
message(STATUS "  ASAN:     ${AGENUI_TESTS_ENABLE_ASAN}")
message(STATUS "  TSAN:     ${AGENUI_TESTS_ENABLE_TSAN}")
message(STATUS "  COVERAGE: ${AGENUI_TESTS_ENABLE_COVERAGE}")
message(STATUS "  CORE_SRC: ${_agenui_src_count} files")
message(STATUS "===================================")
message(STATUS "")
