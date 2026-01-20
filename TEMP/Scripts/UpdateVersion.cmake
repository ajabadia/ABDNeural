
# Scripts/UpdateVersion.cmake

message(STATUS "Incrementing build version...")

if(NOT PROJECT_ROOT)
    set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
endif()

set(VERSION_FILE "${PROJECT_ROOT}/build_no.txt")
set(HEADER_FILE "${PROJECT_ROOT}/Source/Core/BuildVersion.h")

# Read current build number
if(EXISTS "${VERSION_FILE}")
    file(READ "${VERSION_FILE}" BUILD_VERSION)
    string(STRIP "${BUILD_VERSION}" BUILD_VERSION)
else()
    set(BUILD_VERSION "0")
endif()

# Increment
math(EXPR BUILD_VERSION "${BUILD_VERSION} + 1")

# Write back build number
file(WRITE "${VERSION_FILE}" "${BUILD_VERSION}")

# Get current date and time
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

# Generate header
file(WRITE "${HEADER_FILE}" "/* Auto-generated build version file */
#pragma once

#define CZ_BUILD_VERSION \"${BUILD_VERSION}\"
#define CZ_BUILD_TIMESTAMP \"${BUILD_TIMESTAMP}\"
")

message(STATUS "Build #${BUILD_VERSION} at ${BUILD_TIMESTAMP}")
