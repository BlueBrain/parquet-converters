configure_file(version.h.in version.h @ONLY)

set(TOUCH_SRCS
    "touches/touch_reader.cpp"
    "touches/parquet_writer.cpp")
set(CIRCUIT_SRCS
    "circuit/parquet_reader.cpp"
    "circuit/sonata_writer.cpp"
    "circuit/sonata_file.cpp"
    "index/index.cpp")

add_library(TouchParquet STATIC ${TOUCH_SRCS})
target_include_directories(TouchParquet PUBLIC
                           $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src>
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(TouchParquet
                      arrow_shared
                      parquet_shared
                      range-v3)
target_compile_options(TouchParquet PRIVATE -Werror=unused-result)

add_library(CircuitParquet STATIC ${CIRCUIT_SRCS})
target_include_directories(CircuitParquet PUBLIC
                      $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/src>
                      $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(CircuitParquet
                      ${HDF5_LIBRARIES}
                      arrow_shared
                      parquet_shared
                      nlohmann_json::nlohmann_json
                      HighFive
                      range-v3)
target_compile_options(CircuitParquet PRIVATE -Werror=unused-result)

add_executable(touch2parquet touch2parquet.cpp)
target_link_libraries(touch2parquet
                      TouchParquet
                      CLI11::CLI11)

add_executable(parquet2hdf5 parquet2hdf5.cpp)
target_link_libraries(parquet2hdf5
                      CircuitParquet
                      CLI11::CLI11)

install(TARGETS parquet2hdf5 touch2parquet DESTINATION bin)
