cmake_minimum_required(VERSION 3.20)
project(frostfall)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Makefile generators on apple need this flag to compile mixed objective/c++
if(APPLE AND NOT XCODE)
	set(CMAKE_CXX_FLAGS "-ObjC++")
endif()

include(CMakeDependentOption)

option(FROST_CONFIG_DEBUG "Log debug messages (default: on in debug)" OFF)
option(FROST_BUILD_EXAMPLES "Build frost examples." ON)
option(FROST_BUILD_PLUGINS "Build frost default plugins." ON)
option(FROST_BUILD_TOOLS "Build frost tools." ON)
cmake_dependent_option(FROST_BUILD_TOOLS_BIN2C "Build frost binary to c converter." ON FROST_BUILD_TOOLS OFF)
# TODO: Honor FROST_LIBRARY_TYPE once the C++ code supports DLL export/import annotations.
set(FROST_LIBRARY_TYPE "STATIC" CACHE STRING "Linking type for library")
set_property(CACHE FROST_LIBRARY_TYPE PROPERTY STRINGS "STATIC" "SHARED")
# TODO: Declare installation options and provide exported targets and a CMake package config.

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>")

if(NOT FROST_DIR)
	set(FROST_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "Location of frost.")
elseif(NOT IS_ABSOLUTE "${FROST_DIR}")
	get_filename_component(FROST_DIR "${FROST_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

file(TO_CMAKE_PATH "${FROST_DIR}" FROST_DIR)

include(scripts/frost.cmake)

if(FROST_BUILD_EXAMPLES)
	include(scripts/examples.cmake)
endif()

if(FROST_BUILD_PLUGINS)
	include(scripts/plugins.cmake)
endif()

if(FROST_BUILD_TOOLS_BIN2C)
	include(scripts/bin2c.cmake)
endif()
