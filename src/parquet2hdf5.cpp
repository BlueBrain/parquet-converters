/**
 * Copyright (C) 2018 Blue Brain Project
 * All rights reserved. Do not distribute without further notice.
 *
 * @author Fernando Pereira <fernando.pereira@epfl.ch>
 *
 */
#include <stdexcept>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <mpi.h>

#include "CLI/CLI.hpp"

#include "circuit.h"
#include "progress.hpp"
#include "version.h"

using namespace neuron_parquet::circuit;

using neuron_parquet::Converter;
using utils::ProgressMonitor;

namespace fs = std::filesystem;


int mpi_size, mpi_rank;
MPI_Comm comm = MPI_COMM_WORLD;
MPI_Info info = MPI_INFO_NULL;

///
/// \brief convert_circuit_mpi: Converts parquet files to SYN2 using mpi
///
///
void convert_circuit_mpi(const std::vector<std::string>& filenames,
                         const std::string& metadata_path,
                         const std::string& sonata_path,
                         const std::string& population,
                         const bool create_index) {

    // Each reader and each writer in a separate MPI process
    int total_files = filenames.size();
    int my_n_files = total_files / mpi_size;
    int remaining = total_files % mpi_size;
    if( mpi_rank < remaining ) {
        my_n_files++;
    }

    int my_offset = total_files / mpi_size * mpi_rank;
    my_offset += (mpi_rank > remaining) ? remaining : mpi_rank;

    std::vector<std::string> input_names(filenames.begin() + my_offset, filenames.begin() + my_offset + my_n_files);
    if (mpi_rank < filenames.size()) {
        std::cout << std::setfill('.')
                  << "Process " << std::setw(4) << mpi_rank
                  << " is going to read files " << std::setw(8) << my_offset
                  << " to " << std::setw(8) << my_offset + my_n_files - 1 << std::endl;
    } else {
        std::cout << std::setfill('.')
                  << "Process " << std::setw(4) << mpi_rank
                  << " is not going to read files." << std::endl;
    }

    if (input_names.empty()) {
        // We need this to grab the schema of the input files. All ranks
        // need to have the schema to keep the state of the output HDF5
        // file in sync, otherwise the execution will hang when closing the
        // output HDF5 file.
        input_names.push_back(filenames.back());
    }

    MPI_Barrier(comm);

    if (mpi_rank == 0) {
        std::cout << "Writing to " << sonata_path << std::endl;
    }

    MPI_Barrier(comm);

    CircuitMultiReaderParquet reader(input_names, metadata_path);

    // Count the records and
    // 1. Sum
    // 2. Calculate offsets

    uint64_t record_count = mpi_rank < filenames.size() ? reader.record_count() : 0;
    uint64_t global_record_sum;
    MPI_Allreduce(&record_count, &global_record_sum, 1, MPI_UINT64_T, MPI_SUM, MPI_COMM_WORLD);

    uint32_t block_count = mpi_rank < filenames.size() ? reader.block_count() : 0;
    uint32_t global_block_sum;
    MPI_Allreduce(&block_count, &global_block_sum, 1, MPI_UINT32_T, MPI_SUM, MPI_COMM_WORLD);

    uint64_t *offsets=nullptr;
    if (mpi_rank == 0) {
        offsets = new uint64_t[mpi_size+1];
        offsets[0] = 0;
    }
    MPI_Gather(&record_count, 1, MPI_UINT64_T, offsets+1, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    if (mpi_rank == 0) {
        for(int i=1; i<mpi_size; i++) {
            offsets[i] += offsets[i-1];
        }
    }

    uint64_t offset;
    MPI_Scatter(offsets, 1, MPI_UINT64_T, &offset, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);

    if (mpi_rank < filenames.size()) {
        std::cout << std::setfill('.')
                  << "Process " << std::setw(4) << mpi_rank
                  << " is going to write " << std::setw(12) << reader.block_count()
                  << " block(s) with an offset of " << std::setw(12) << offset
                  << std::endl;
    } else {
        std::cout << std::setfill('.')
                  << "Process " << std::setw(4) << mpi_rank
                  << " is not going to write anything."
                  << std::endl;
    }

    SonataWriter writer(sonata_path, global_record_sum, {comm, info}, offset, population);

    //Create converter and progress monitor
    {
        Converter<CircuitData> converter(reader, writer);
        ProgressMonitor p(global_block_sum, mpi_rank==0);
        // Use progress of first process to estimate global progress
        if (mpi_rank == 0) {
            p.set_parallelism(mpi_size);
            converter.setProgressHandler(p, mpi_size);
        }
        if (mpi_rank < filenames.size()) {
            // See above: avoid converting data if we just opened the last
            // file to access the schema.
            converter.exportAll();
        }
    }

    MPI_Barrier(comm);

    if(mpi_rank == 0) {
        std::cout << std::endl
                  << "Data conversion complete. " << std::endl;
    }

    MPI_Barrier(comm);

    if (create_index) {
        if(mpi_rank == 0) {
            std::cout << "Creating indices..." << std::endl;
        }
        try {
            writer.write_indices(true);
        } catch (const std::exception& e) {
            std::cerr << "ERROR on rank " << mpi_rank << ": Failed to write indices: " << e.what() << std::endl;
            throw e;
        }
        MPI_Barrier(comm);
    }

    if(mpi_rank == 0) {
        std::cout << "Finished writing " << sonata_path << std::endl;
    }
}


int main(int argc, char* argv[]) {
    // Initialize MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &mpi_size);
    MPI_Comm_rank(comm, &mpi_rank);

    std::string output_filename;
    std::string output_population;
    std::string input_directory;
    bool create_index = true;

    // Every node makes his job in reading the args and
    // compute the sub array of files to process
    CLI::App app{"Convert Parquet synapse files into the SONATA format"};
    app.set_version_flag("-v,--version", neuron_parquet::VERSION);
    app.add_flag("--index,!--no-index", create_index, "Create a SONATA index");
    app.add_option("input_directory", input_directory, "Directory containing Parquet files to convert")
        ->check(CLI::ExistingDirectory)
        ->required();
    app.add_option("output_filename", output_filename, "Output filename to use")
        ->required();
    app.add_option("output_population", output_population, "Population to write")
        ->required();

    try {
        app.parse(argc, argv);
    } catch(const CLI::ParseError& e) {
        if (mpi_rank == 0) {
            app.exit(e);
        }
        MPI_Finalize();
        return 1;
    }

    std::string metadata_file = "";
    std::vector<std::string> input_files;
    {
        fs::path p(input_directory);

        auto meta = p / "_metadata";
        if (fs::is_regular_file(meta)) {
            metadata_file = meta.string();
        } else if (mpi_rank == 0) {
            std::cerr << "WARNING: Input directory '"
                      << input_directory
                      << "' did not contain a '_metadata' file"
                      << std::endl;
        }

        for (const auto& e: fs::directory_iterator(p)) {
            auto ep = e.path();
            if (fs::is_regular_file(ep) && ep.extension() == ".parquet") {
                input_files.push_back(ep.string());
            }
        }

        if (input_files.empty()) {
            std::cerr << "Imput directory '"
                      << input_directory
                      << "' did not contain any Parquet files"
                      << std::endl;
            MPI_Finalize();
            return 1;
        }
    }
    std::sort(input_files.begin(), input_files.end());

    if (mpi_rank == 0) {
        auto parent = fs::path(output_filename).parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent);
        }
    }
    MPI_Barrier(comm);

    convert_circuit_mpi(input_files, metadata_file, output_filename, output_population, create_index);

    MPI_Finalize();

    return 0;
}
