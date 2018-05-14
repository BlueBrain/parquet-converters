#include <cmath>
#include <boost/filesystem.hpp>
#include <stdio.h>
#include <mpi.h>
#include <unistd.h>
#include <string.h>
#include "touch_reader.h"
#include "touch_writer_parquet.h"
#include "converter.h"
#include "progress.h"

using namespace neuron_parquet;

// Stand-in until c++ std::filesystem is supported
namespace fs = boost::filesystem;

typedef Converter<Touch> TouchConverter;


int mpi_size, mpi_rank;
MPI_Comm comm = MPI_COMM_WORLD;
MPI_Info info = MPI_INFO_NULL;


enum class RunMode:int { QUIT_ERROR=-1, QUIT_OK, STANDARD, ENDIAN_SWAP };
struct Args {
    Args (RunMode runmode)
    : mode(runmode)
    {}
    RunMode mode;
    int n_opts = 0;
};


static const char *usage =
    "usage: touch2parquet[_endian] <touch_file1 touch_file2 ...>\n"
    "       touch2parquet [-h]\n";


Args process_args(int argc, char* argv[]) {
    if( argc < 2) {
        printf("%s", usage);
        return Args(RunMode::QUIT_ERROR);
    }

    Args args(RunMode::STANDARD);
    int cur_opt = 1;

    //Handle options
    for( ;cur_opt < argc && argv[cur_opt][0] == '-'; cur_opt++ ) {
        switch( argv[cur_opt][1] ) {
            case 'h':
                printf("%s", usage);
                return Args(RunMode::QUIT_OK);
        }
    }
    args.n_opts = cur_opt;


    for( int i=cur_opt ; i<argc ; i++ ) {
        if(access(argv[i] , F_OK) == -1) {
            fprintf(stderr, "File '%s' doesn't exist, or no permission\n", argv[i]);
            args.mode = RunMode::QUIT_ERROR;
            return args;
        }
    }

    //It will swap endians if we use the xxx_endian executable
    int len = strlen(argv[0]);
    if( len>6 && strcmp( &(argv[0][len-6]), "endian" ) == 0 ) {
        printf("[Info] Swapping endians\n");
        args.mode = RunMode::ENDIAN_SWAP;
    }

    return args;
}



int main( int argc, char* argv[] ) {
    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &mpi_size);
    MPI_Comm_rank(comm, &mpi_rank);

    Args args = process_args(argc, argv);
    if( args.mode < RunMode::STANDARD ) {
        return static_cast<int>(args.mode);
    }

    int first_file = args.n_opts;
    int number_of_files = argc - first_file;

    // Know the total number of buffers to be processed
    std::ifstream f1(argv[first_file], std::ios::binary | std::ios::ate);
    uint32_t blocks = TouchConverter::number_of_buffers(f1.tellg());
    f1.close();

    // Craete the progres monitor with an estimate on the number of blocks
    ProgressMonitor progress(number_of_files * blocks);
    if (mpi_rank == 0)
        progress.task_start(mpi_size);

    auto outfn = fs::path(argv[first_file]).stem().string() + "." + std::to_string(mpi_rank) + ".parquet";
    std::cout << "Process " << mpi_rank << " will write file " << outfn << std::endl;
    MPI_Barrier(comm);
    try {
        TouchWriterParquet tw(outfn);

        for (int i = args.n_opts; i < argc; i++) {
            if (mpi_rank == 0)
                printf("\r[Info] Converting %-86s\n", argv[i]);

            TouchReader tr(argv[i], args.mode == RunMode::ENDIAN_SWAP);
            auto work_unit = static_cast<long unsigned int>(std::ceil(tr.record_count() / double(mpi_size)));
            auto offset = work_unit * mpi_rank;
            work_unit = std::min(tr.record_count() - offset, work_unit);
            tr.seek((unsigned) offset);

            std::cout << "Process " << mpi_rank << " will process touches " << offset
                      << " to " << (offset + work_unit) << std::endl;

            TouchConverter converter(tr, tw);
            if (mpi_rank == 0) {
                // Progress handlers is just a function that triggers incrementing the progressbar
                auto f = [&progress](int) {
                    if(mpi_rank == 0) {
                        progress.updateProgress(mpi_size);
                    }
                };
                converter.setProgressHandler(f);
            }
            converter.exportN((unsigned) work_unit);
        }
    }
    catch (const std::exception& e){
        printf("\n[ERROR] Could not create output file.\n -> %s", e.what());
        MPI_Finalize();
        return 1;
    }

    if(mpi_rank == 0)
        progress.task_done(mpi_size);

    MPI_Barrier(comm);
    MPI_Finalize();

    printf("\nDone exporting\n");
    return 0;
}


