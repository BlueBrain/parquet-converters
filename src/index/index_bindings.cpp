#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <highfive/H5File.hpp>
#include <mpi.h>
#include "index.h"
#include <filesystem>

namespace nb = nanobind;

void write_index(const std::string& filename, const std::string& group_path, uint64_t sourceNodeCount, uint64_t targetNodeCount) {
    try {
        // Check if the file exists
        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("File does not exist: " + filename);
        }

        // Open the file in read-write mode with MPI-IO
        HighFive::FileAccessProps fapl;
        fapl.add(HighFive::MPIOFileAccess(MPI_COMM_WORLD, MPI_INFO_NULL));
        HighFive::File file(filename, HighFive::File::ReadWrite, fapl);
        
        if (!file.exist(group_path)) {
            throw std::runtime_error("Group '" + std::string(group_path) + "' not found in file");
        }

        HighFive::Group group = file.getGroup(group_path);
        
        if (!group.exist("source_node_id") || !group.exist("target_node_id")) {
            throw std::runtime_error("Required datasets 'source_node_id' or 'target_node_id' not found in group '" + std::string(group_path) + "'");
        }
        
        indexing::write(group, sourceNodeCount, targetNodeCount);
    } catch (const HighFive::Exception& e) {
        throw std::runtime_error("HighFive error in write_index: " + std::string(e.what()));
    } catch (const std::exception& e) {
        throw std::runtime_error("Error in write_index: " + std::string(e.what()));
    } catch (...) {
        throw std::runtime_error("Unknown error in write_index");
    }
}

NB_MODULE(index_writer_py, m) {
    m.def("write", &write_index, "Write index to HDF5 file using parallel I/O",
          nb::arg("filename"), nb::arg("groupPath"), nb::arg("sourceNodeCount"), nb::arg("targetNodeCount"));
}
