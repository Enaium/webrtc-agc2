cmake_minimum_required(VERSION 3.16)

set(PROJECT webrtc_agc2)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

project(${PROJECT} VERSION 0.1.0)

add_compile_options(-Wall -fPIC -Wno-deprecated -m64 -fexceptions -Wno-nullability-completeness)

if(APPLE)
    add_definitions(-DWEBRTC_MAC -DWEBRTC_POSIX)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DWEBRTC_LINUX -DWEBRTC_POSIX)
elseif(WIN32)
    add_definitions(-DWEBRTC_WIN)
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

add_library(${PROJECT} SHARED ${AGC2_SRC})

target_include_directories(${PROJECT} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
)

set_target_properties(${PROJECT} PROPERTIES LINKER_LANGUAGE CXX)

if(APPLE)
    target_link_libraries(${PROJECT} PRIVATE "-framework CoreFoundation")
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