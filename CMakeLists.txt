cmake_minimum_required(VERSION 3.15)
project(neuron_parquet VERSION 0.8.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Look for libraries in 'lib64' as well as in 'lib'
set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ON)

# Determine version from git if possible
find_package(Git)
execute_process(
  COMMAND ${GIT_EXECUTABLE} describe --tags
  OUTPUT_VARIABLE NEURONPARQUET_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE ECODE
  ERROR_QUIET)

if(ECODE)
  message("Failed to determine version from git, using ${CMAKE_PROJECT_VERSION}")
  set(NEURONPARQUET_VERSION ${CMAKE_PROJECT_VERSION})
endif()

# Find required packages
find_package(MPI REQUIRED)
find_package(Arrow REQUIRED)

# Add MPI include directories and definitions
include_directories(SYSTEM ${MPI_INCLUDE_PATH})
add_definitions(${MPI_CXX_COMPILE_FLAGS})
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MPI_CXX_LINK_FLAGS}")
add_definitions(-DOMPI_SKIP_MPICXX)
get_filename_component(MY_SEARCH_DIR ${Arrow_CONFIG} DIRECTORY)
find_package(Parquet REQUIRED HINTS ${MY_SEARCH_DIR})
find_package(HDF5 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Range-v3 REQUIRED)
find_package(HighFive)
find_package(CLI11)
find_package(Catch2)
find_package(Python 3.8 COMPONENTS Interpreter Development.Module REQUIRED)

if(NOT HighFive_FOUND)
  set(HDF5_IS_PARALLEL ON CACHE BOOL "Enable parallel HDF5" FORCE)
  add_subdirectory(${CMAKE_SOURCE_DIR}/deps/highfive)
endif()

if(NOT CLI11_FOUND)
  add_subdirectory(${CMAKE_SOURCE_DIR}/deps/cli11)
endif()

if(NOT ${Catch2_FOUND})
  add_subdirectory(deps/catch2 EXCLUDE_FROM_ALL)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/deps/catch2/extras)
endif()

execute_process(
        COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE nanobind_ROOT)
find_package(nanobind CONFIG REQUIRED)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

add_library(index_writer STATIC
    src/index/index.cpp
)

set_target_properties(index_writer PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(index_writer PRIVATE HighFive HDF5::HDF5)

nanobind_add_module(index_writer_py src/index/index_bindings.cpp)

target_include_directories(index_writer_py PRIVATE ${HighFive_INCLUDE_DIRS})
target_link_libraries(index_writer_py PRIVATE index_writer HighFive)

# Install the Python module
#install(TARGETS index_writer_py DESTINATION .)
