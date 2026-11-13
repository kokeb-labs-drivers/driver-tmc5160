#===============================================================================
# TMC5160 driver: root CMake
#===============================================================================
# One file, two build systems:
#
# - ESP-IDF (ESP_PLATFORM): this folder is an ESP-IDF component. Point
#   EXTRA_COMPONENT_DIRS at it, check it out under components/, or add it as a
#   component-manager git dependency. menuconfig reads _config/Kconfig.
# - Any other CMake build (host, STM32, RP2040, nRF, ...): one INTERFACE target,
#   tmc5160, consumed as tmc5160::tmc5160 both through add_subdirectory() and
#   through find_package(tmc5160) after install.
#
#   cmake -S . -B build -DTMC5160_BUILD_TESTS=ON
#   cmake --build build && ctest --test-dir build --output-on-failure
#===============================================================================
cmake_minimum_required(VERSION 3.16)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/tmc5160_build_settings.cmake)

#===============================================================================
# ESP-IDF component
#===============================================================================
if(ESP_PLATFORM)
    idf_component_register(
        SRCS ${TMC5160_SOURCE_FILES}
        INCLUDE_DIRS ${TMC5160_PUBLIC_INCLUDE_DIRS}
        REQUIRES ${TMC5160_IDF_REQUIRES}
        KCONFIG ${CMAKE_CURRENT_LIST_DIR}/_config/Kconfig
    )
    if(TMC5160_SOURCE_FILES)
        target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
    else()
        target_compile_features(${COMPONENT_LIB} INTERFACE cxx_std_20)
    endif()
    return()
endif()

project(tmc5160_driver
    VERSION ${TMC5160_VERSION}
    DESCRIPTION "C++20 driver for the Trinamic TMC5160 and TMC5160A stepper motor controller and driver with SPI and UART interfaces"
    LANGUAGES CXX)

include(GNUInstallDirs)

# ISO C++20 for every target this project builds (tests, fuzzers, examples), never the GNU dialect
# that CMake picks by default (handbook KB-CPP-001, KB-CPP20-001). CXX_EXTENSIONS does not travel
# through an INTERFACE target, so each folder that builds a target sets it as well.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The installed layout keeps the source tree's inc/ + src/ pair, because tmc5160.hpp includes
# "../src/tmc5160.ipp" relative to itself:
#   <prefix>/include/tmc5160/inc/*.hpp, tmc5160_version.h
#   <prefix>/include/tmc5160/src/tmc5160.ipp
set(TMC5160_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/tmc5160/inc")

#===============================================================================
# Target
#===============================================================================
add_library(${TMC5160_TARGET_NAME} INTERFACE)
target_include_directories(${TMC5160_TARGET_NAME}
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/inc>
        $<BUILD_INTERFACE:${TMC5160_VERSION_HEADER_DIR}>
        $<INSTALL_INTERFACE:${TMC5160_INSTALL_INCLUDEDIR}>
)
target_compile_features(${TMC5160_TARGET_NAME} INTERFACE cxx_std_20)

# tmc5160::tmc5160 everywhere: the alias for add_subdirectory(), EXPORT_NAME for the installed package.
add_library(tmc5160::tmc5160 ALIAS ${TMC5160_TARGET_NAME})
set_target_properties(${TMC5160_TARGET_NAME} PROPERTIES EXPORT_NAME tmc5160)

option(TMC5160_ENABLE_WARNINGS "Add -Wall -Wextra -Wpedantic to every target that links tmc5160::tmc5160" OFF)
if(TMC5160_ENABLE_WARNINGS)
    target_compile_options(${TMC5160_TARGET_NAME} INTERFACE -Wall -Wextra -Wpedantic)
endif()

#===============================================================================
# Host tests, documentation snippets, install check (tests/)
#===============================================================================
option(TMC5160_BUILD_TESTS "Build the host tests in tests/" OFF)
option(TMC5160_BUILD_FUZZERS "Also build the libFuzzer targets in tests/fuzz/ (Clang only)" OFF)
if(TMC5160_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

#===============================================================================
# Install and package export
#===============================================================================
include(CMakePackageConfigHelpers)

install(TARGETS ${TMC5160_TARGET_NAME} EXPORT tmc5160Targets
    INCLUDES DESTINATION ${TMC5160_INSTALL_INCLUDEDIR})

install(DIRECTORY inc/ DESTINATION ${TMC5160_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")
install(FILES "${TMC5160_VERSION_HEADER}" DESTINATION ${TMC5160_INSTALL_INCLUDEDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tmc5160/src
    FILES_MATCHING PATTERN "*.ipp")

install(EXPORT tmc5160Targets
    FILE tmc5160Targets.cmake
    NAMESPACE tmc5160::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tmc5160)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/tmc5160Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tmc5160Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tmc5160)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/tmc5160ConfigVersion.cmake"
    VERSION ${TMC5160_VERSION}
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/tmc5160Config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/tmc5160ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/tmc5160)
