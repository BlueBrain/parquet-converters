#include "circuit_defs.h"
#include "circuit_reader_parquet.h"
#include "circuit_writer_syn2.h"
#include "converter.h"
#include "progress.h"
#include <iostream>

using namespace neuron_parquet;
using namespace neuron_parquet::circuit;


void convert_circuit(const std::string & filename)  {
    CircuitReaderParquet reader(filename) ;
    CircuitWriterSYN2 writer(std::string("circuit_syn2"), reader.record_count());

    Converter<CircuitData> converter( reader, writer );

    ProgressMonitor p(reader.block_count());
    converter.setProgressHandler(p.getNewHandler());

    p.task_start();
    converter.exportAll();
    p.task_done();

    std::cout << "\nComplete." << std::endl;
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        std::cout << "Please provide a file to convert";
        return -1;
    }
    convert_circuit(std::string(argv[1]));
    return 0;
}
