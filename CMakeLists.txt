cmake_minimum_required(VERSION 3.16)

set(PROJECT webrtc_agc2)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

project(${PROJECT} VERSION 0.1.0)

add_compile_options(-Wall -fPIC -Wno-deprecated -fexceptions -Wno-nullability-completeness)

if(EMSCRIPTEN)
    # WebAssembly: the runtime emulates POSIX, but not a specific desktop OS.
    add_definitions(-DWEBRTC_POSIX)
else()
    add_compile_options(-m64)

    if(APPLE)
        add_definitions(-DWEBRTC_MAC -DWEBRTC_POSIX)
    elseif(UNIX)
        add_definitions(-DWEBRTC_LINUX -DWEBRTC_POSIX)
    elseif(WIN32)
        add_definitions(-DWEBRTC_WIN)
    endif()
endif()

# Runtime dispatched SIMD implementations are compiled when the target
# architecture supports them.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    add_definitions(-DWEBRTC_HAS_NEON)
endif()

# Collect all source files
file(GLOB_RECURSE AGC2_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c
)

# Exclude SIMD files on non-x86 platforms
if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64"))
    list(FILTER AGC2_SRC EXCLUDE REGEX ".*_avx2\\.cc$")
    list(FILTER AGC2_SRC EXCLUDE REGEX ".*_sse2\\.cc$")
    list(FILTER AGC2_SRC EXCLUDE REGEX ".*_sse\\.cc$")
endif()

# Exclude NEON files on non-ARM platforms
if(NOT (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64"))
    list(FILTER AGC2_SRC EXCLUDE REGEX ".*_neon\\.cc$")
endif()

# Exclude Windows-only sources on other platforms
if(NOT WIN32)
    list(FILTER AGC2_SRC EXCLUDE REGEX ".*rtc_base/win32\\.cc$")
endif()

if(EMSCRIPTEN)
    add_library(${PROJECT} STATIC ${AGC2_SRC})
    # Upstream header-only templates that are not instantiated in this build.
    target_compile_options(${PROJECT} PRIVATE -Wno-unused-template)
else()
    add_library(${PROJECT} SHARED ${AGC2_SRC})
endif()

target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
)

set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)

if(APPLE AND NOT EMSCRIPTEN)
    target_link_libraries(${PROJECT} PRIVATE "-framework CoreFoundation")
endif()

# WebAssembly target (Emscripten): a self-contained ES module exposing the AGC2
# C API (`wasm/agc2_wasm.cc`) with the JavaScript wrapper `wasm/agc2.mjs`.
if(EMSCRIPTEN)
    add_executable(agc2_wasm
        ${CMAKE_CURRENT_SOURCE_DIR}/wasm/agc2_wasm.cc
    )
    target_link_libraries(agc2_wasm PRIVATE ${PROJECT})
    target_link_options(agc2_wasm PRIVATE
        "--no-entry"
        "-sMODULARIZE=1"
        "-sEXPORT_ES6=1"
        "-sEXPORT_NAME=createAgc2Module"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sSTACK_SIZE=1048576"
        "-sENVIRONMENT=web,node"
        "-sEXPORTED_RUNTIME_METHODS=['HEAPF32']"
        "-sEXPORTED_FUNCTIONS=['_malloc','_free','_agc2_create','_agc2_destroy','_agc2_frame_size','_agc2_set_fixed_gain_db','_agc2_set_capture_output_used','_agc2_process_frame','_agc2_recommended_input_volume']"
        "-O3"
    )
    set_target_properties(agc2_wasm PROPERTIES
        OUTPUT_NAME "agc2_wasm"
        SUFFIX ".mjs"
    )
endif()

option(BUILD_TESTS "Build the AGC2 test executable" OFF)

if(BUILD_TESTS)
    add_executable(agc2_test
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/agc2_test.cc
    )

    target_link_libraries(agc2_test PRIVATE ${PROJECT})

    target_include_directories(agc2_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/
    )
endif()