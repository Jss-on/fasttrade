cmake_minimum_required(VERSION 3.16)
project(fasttrade VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_TESTS "Build test suite" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" OFF)
option(BUILD_EXAMPLES "Build examples" ON)

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create fasttrade library
set(FASTTRADE_SOURCES
    src/core/order_book.cpp
    src/core/trading_core.cpp
    src/core/limit_order.cpp
    src/core/clock.cpp
    src/utils/decimal.cpp
)

set(FASTTRADE_HEADERS
    include/fasttrade/core/order_book.hpp
    include/fasttrade/core/trading_core.hpp
    include/fasttrade/core/limit_order.hpp
    include/fasttrade/core/clock.hpp
    include/fasttrade/utils/decimal.hpp
    include/fasttrade/fasttrade.hpp
)

# Main library
add_library(fasttrade SHARED ${FASTTRADE_SOURCES})
target_include_directories(fasttrade PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(fasttrade Threads::Threads)

# Compiler flags for optimization
target_compile_options(fasttrade PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
    $<$<CONFIG:Debug>:-g -O0>
)

# # Python bindings
# if(BUILD_PYTHON_BINDINGS)
#     find_package(pybind11 QUIET)
#     if(pybind11_FOUND)
#         pybind11_add_module(pyfasttrade src/python/bindings.cpp)
#         target_link_libraries(pyfasttrade PRIVATE fasttrade)
#         target_compile_definitions(pyfasttrade PRIVATE VERSION_INFO=${PROJECT_VERSION})
#     endif()
# endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
install(TARGETS fasttrade
    EXPORT fasttradeTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/fasttrade DESTINATION include)
