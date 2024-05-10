find_program(
  mpi_launcher
  NAMES srun mpirun ${MPIEXEC_EXECUTABLE}
  HINTS ${MPI_HOME} $ENV{MPI_HOME}
  PATHS ${_MPI_PREFIX_PATH}
  PATH_SUFFIXES bin
  DOC "Executable for running MPI programs.")

add_test(NAME parquet2hdf5_mpi
         COMMAND ${mpi_launcher} -n 4 $<TARGET_FILE:parquet2hdf5>
                 ${CMAKE_CURRENT_SOURCE_DIR}/parquets edges_n4.h5 All)

add_test(NAME touches_conversion_v1
         COMMAND $<TARGET_FILE:touch2parquet>
                 ${CMAKE_CURRENT_SOURCE_DIR}/touches_v1/touchesData.0)

add_test(NAME parquet_conversion_v1
         COMMAND ${mpi_launcher} -n 1 $<TARGET_FILE:parquet2hdf5> . edges_v1.h5
                 All)

add_test(NAME touches_conversion_v2
         COMMAND $<TARGET_FILE:touch2parquet>
                 ${CMAKE_CURRENT_SOURCE_DIR}/touches_v2/touchesData.0)

add_test(NAME parquet_conversion_v2
         COMMAND ${mpi_launcher} -n 1 $<TARGET_FILE:parquet2hdf5> . edges_v2.h5
                 All)

add_test(NAME touches_conversion_v3
         COMMAND $<TARGET_FILE:touch2parquet>
                 ${CMAKE_CURRENT_SOURCE_DIR}/touches_v3/touchesData.0)

set_tests_properties(touches_conversion_v1 PROPERTIES FIXTURES_SETUP touches_v1)
set_tests_properties(parquet_conversion_v1 PROPERTIES FIXTURES_REQUIRED
                                                      touches_v1)
set_tests_properties(touches_conversion_v2 PROPERTIES FIXTURES_SETUP touches_v2)
set_tests_properties(parquet_conversion_v2 PROPERTIES FIXTURES_REQUIRED
                                                      touches_v2)

set_tests_properties(touches_conversion_v1 parquet_conversion_v1
                     PROPERTIES RUN_SERIAL TRUE)
set_tests_properties(touches_conversion_v2 parquet_conversion_v2
                     PROPERTIES RUN_SERIAL TRUE)

add_executable(test_indexing test_indexing.cpp
                             ${${PROJECT_NAME}_SOURCE_DIR}/src/index/index.cpp)
target_link_libraries(test_indexing Catch2::Catch2WithMain HighFive MPI::MPI_C)
target_include_directories(
  test_indexing PRIVATE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src>)

include(CTest)
include(Catch)
catch_discover_tests(test_indexing)
