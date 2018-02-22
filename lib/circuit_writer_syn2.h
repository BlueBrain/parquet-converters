#ifndef CIRCUITWRITERSYN2_H
#define CIRCUITWRITERSYN2_H


#include "generic_writer.h"
#include "circuit_defs.h"
#include "zeromem_queue.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mpi.h>
#include <hdf5.h>


namespace neuron_parquet {
namespace circuit {


typedef ZeroMemQ<arrow::Column> ZeroMemQ_Column;

struct h5_ids {
    hid_t file, ds, dspace, plist;
};


///
/// \brief The CircuitWriterSYN2 which writes one hdf5 file per column.
///        Each file is written by a separate thread, so the main thread only
///        dispatches data and is immediately freed to fetch more data.
///
class CircuitWriterSYN2 : public Writer<CircuitData>
{
public:
    CircuitWriterSYN2(const std::string & destination_dir, uint64_t n_records);

    virtual void write(const CircuitData* data, uint length) override;

    void set_output_block_position(int part_id, uint64_t offset, uint64_t part_length);

//    /// If not initialized called automatically on first write. If mpio is to be enables call use_mpio beforehand
//    void create_files_and_handles(const shared_ptr<Table> data);

    void use_mpio(MPI_Comm comm=MPI_COMM_WORLD, MPI_Info info=MPI_INFO_NULL);

    ~CircuitWriterSYN2() {
        close_files();
    }

private:
    // The writer can write to several files using different threads
    // and therefore release the main thread to go and fetch more data

    h5_ids init_h5file(const std::string & filename, std::shared_ptr<arrow::Column> column);

    void create_thread_process_data(h5_ids h5_output, ZeroMemQ_Column & queue);

    ZeroMemQ_Column & get_create_handler_for_column(const std::shared_ptr<arrow::Column> col);

    void close_files();


    std::vector<std::unique_ptr<ZeroMemQ_Column>> column_writer_queues_;
    std::unordered_map<std::string, int> col_name_to_idx_;

    std::vector<std::thread> threads_;
    std::vector<h5_ids> files_;

    const std::string destination_dir_;
    const uint64_t total_records_;
    //These entries are mostly useful in parallel writing
    uint64_t output_part_length_;
    uint64_t output_file_offset_ = 0;
    int output_part_id_ = 0;

    bool use_mpio_ = false;
    struct {
        MPI_Comm comm;
        MPI_Info info;
    } mpi_;

};


// Thread functions

inline void write_data(h5_ids h5_ds, uint64_t& r_offset, const std::shared_ptr<const arrow::Column>& r_col_data);
inline hid_t parquet_types_to_h5(arrow::Type::type t);


} }  // NS EOF
#endif // CIRCUITWRITERSYN2_H
