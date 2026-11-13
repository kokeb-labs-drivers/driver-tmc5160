#===============================================================================
# TMC5160 driver: build settings
#===============================================================================
# Read by the root CMakeLists.txt in both of its modes (plain CMake and the
# ESP-IDF component): target name, version, include directories, sources and
# ESP-IDF requirements. This file is the single source
# of the version; tests/CMakeLists.txt checks every other copy against it.
#===============================================================================

include_guard(GLOBAL)

set(TMC5160_TARGET_NAME "tmc5160")

#===============================================================================
# Version (single source of truth)
#===============================================================================
set(TMC5160_VERSION_MAJOR 0)
set(TMC5160_VERSION_MINOR 1)
set(TMC5160_VERSION_PATCH 0)
set(TMC5160_VERSION "${TMC5160_VERSION_MAJOR}.${TMC5160_VERSION_MINOR}.${TMC5160_VERSION_PATCH}")

#===============================================================================
# Generated version header (build tree, never the source tree)
#===============================================================================
set(TMC5160_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/../inc/tmc5160_version.h.in")
set(TMC5160_VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/tmc5160_generated")
set(TMC5160_VERSION_HEADER "${TMC5160_VERSION_HEADER_DIR}/tmc5160_version.h")

# The directory must exist before ESP-IDF's idf_component_register() checks the include paths.
file(MAKE_DIRECTORY "${TMC5160_VERSION_HEADER_DIR}")
configure_file("${TMC5160_VERSION_TEMPLATE}" "${TMC5160_VERSION_HEADER}" @ONLY)

#===============================================================================
# Public include directories, sources, ESP-IDF requirements
#===============================================================================
set(TMC5160_PUBLIC_INCLUDE_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../inc"
    "${TMC5160_VERSION_HEADER_DIR}"
)

# Header-only: the template implementation is src/tmc5160.ipp, included by inc/tmc5160.hpp.
set(TMC5160_SOURCE_FILES)

set(TMC5160_IDF_REQUIRES)
