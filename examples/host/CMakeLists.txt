# =============================================================================
# TMC5160 driver: host examples (any PC, no hardware)
# =============================================================================
# The driver against a simulated TMC5160 (the chip model the host tests use, tests/mock_tmc5160.hpp) through the
# host ports in this folder. One program per file:
#
#   cmake -S examples/host -B build-example
#   cmake --build build-example && ./build-example/tmc5160_host_quickstart
#
# The host tests also build this folder and run every example (CTest tmc5160_host_<name>), so they always compile
# against the current headers.
# =============================================================================
cmake_minimum_required(VERSION 3.16)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(tmc5160_host_examples LANGUAGES CXX)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../.. tmc5160)
endif()

# ISO C++20, also when this folder is built on its own (handbook KB-CPP20-001).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TMC5160_HOST_EXAMPLES quickstart ramp_profile stealthchop stallguard homing_switch homing_stall encoder
    uart_chain spi_chain direct_mode diag_interrupt)
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(TMC5160_HOST_EXAMPLES ${TMC5160_HOST_EXAMPLES} PARENT_SCOPE) # the host tests run each one
endif()
foreach(name ${TMC5160_HOST_EXAMPLES})
  add_executable(tmc5160_host_${name} ${name}.cpp)
  target_link_libraries(tmc5160_host_${name} PRIVATE tmc5160::tmc5160)
  target_include_directories(tmc5160_host_${name} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/../../tests)
  target_compile_features(tmc5160_host_${name} PRIVATE cxx_std_20)
  target_compile_options(tmc5160_host_${name} PRIVATE -fno-exceptions -fno-rtti -Wall -Wextra -Wpedantic
    -pedantic-errors)
endforeach()
