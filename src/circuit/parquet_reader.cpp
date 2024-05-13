/**
 * Copyright (C) 2018 Blue Brain Project
 * All rights reserved. Do not distribute without further notice.
 *
 * @author Fernando Pereira <fernando.pereira@epfl.ch>
 *
 */
#include <stdexcept>
#include "parquet_reader.h"

namespace {

std::unique_ptr<parquet::ParquetFileReader> create_reader(const std::string& filename) {
    auto props = parquet::default_reader_properties();
    // TODO Find out which buffer size may reduce FS reads
    // props.enable_buffered_stream();
    // props.set_buffer_size(16 * 1024 * 1024);
    return parquet::ParquetFileReader::OpenFile(filename, false, props);
}

}

namespace neuron_parquet {
namespace circuit {

CircuitReaderParquet::CircuitReaderParquet(const std::string & filename)
  :
    filename_(filename),
    reader_(create_reader(filename)),
    parquet_metadata_(reader_->metadata()),
    column_count_(parquet_metadata_->num_columns()),
    rowgroup_count_(parquet_metadata_->num_row_groups()),
    record_count_(parquet_metadata_->num_rows())
{
}

void CircuitReaderParquet::close() {
    if(data_reader_) {
        data_reader_.reset(nullptr);
    }
    if(reader_) {
        reader_.reset(nullptr);
    }
}


void CircuitReaderParquet::init_data_reader() {
    if(! reader_) {
        reader_ = parquet::ParquetFileReader::OpenFile(filename_, false);
    }
    // NOTE that reader is unique. We must give it up to the other reader
    const auto status = parquet::arrow::FileReader::Make(arrow::default_memory_pool(), std::move(reader_), &data_reader_);
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
    cur_row_group_ = 0;
}


uint32_t CircuitReaderParquet::fillBuffer(CircuitData* buf, uint32_t length) {
    // We are using parquet::arrow::reader to recreate the data
    // parquet::reader is a low-level reader not handling automatically repetition levels, etc...
    (void) length;  // length is not used since the client always gets the full block

    if(!data_reader_) {
        init_data_reader();
    }
    if( cur_row_group_ >= rowgroup_count_) {
        return 0;
    }

    const auto status = data_reader_->ReadRowGroup(cur_row_group_++, &(buf->row_group));
    if (!status.ok()) {
        throw std::runtime_error(status.ToString());
    }
    return (uint32_t) buf->row_group->num_rows();
}


///////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////

CircuitMultiReaderParquet::CircuitMultiReaderParquet(const std::vector<std::string>& filenames, const std::string& metadata_filename)
 :
   rowgroup_count_(0),
   record_count_(0),
   cur_file_(0)
{
    if (filenames.empty()) {
        throw std::runtime_error("need at least one file to read");
    }

    if (metadata_filename.empty()) {
        metadata_reader_.reset(new CircuitReaderParquet(filenames[0]));
    } else {
        metadata_reader_.reset(new CircuitReaderParquet(metadata_filename));
    }

    circuit_readers_.reserve(filenames.size());
    rowgroup_offsets_.reserve(filenames.size());
    rowgroup_offsets_.push_back(0);

    for(auto name : filenames) {
        std::shared_ptr<CircuitReaderParquet> reader(new CircuitReaderParquet(name));
        circuit_readers_.push_back(reader);
        rowgroup_count_ += reader->rowgroup_count_;
        record_count_ += reader->record_count_;
        // Offsets
        rowgroup_offsets_.push_back(rowgroup_count_);

        //Free file handler
        reader->close();
    }


}

uint32_t CircuitMultiReaderParquet::fillBuffer(CircuitData *buf, uint length) {
    uint32_t n = circuit_readers_[cur_file_]->fillBuffer(buf, length);
    if(n <= 0) {
        circuit_readers_[cur_file_]->close();
        cur_file_++;
        if(cur_file_ >= circuit_readers_.size()) {
            // EOF
            return 0;
        }
        n = circuit_readers_[cur_file_]->fillBuffer(buf, length);
    }
    return n;
}


const CircuitData::Schema* CircuitMultiReaderParquet::schema() const {
    return metadata_reader_->schema();
}


const std::shared_ptr<const CircuitData::Metadata> CircuitMultiReaderParquet::metadata() const {
    return metadata_reader_->metadata();
}


void CircuitMultiReaderParquet::seek(uint64_t pos) {
    if (block_count() == 0 && pos == 0) {
        // We have no data to read, seek should be no-op
        return;
    }

    if( pos >= rowgroup_count_ ){
        throw std::runtime_error("Cant seek over file length");
    }

    for(unsigned int i=0; i<rowgroup_offsets_.size()
                         && rowgroup_offsets_[i]<=pos  ; i++) {
        cur_file_ = i;
    }

    uint32_t offset = pos - rowgroup_offsets_[cur_file_];
    circuit_readers_[cur_file_]->seek(offset);
}



}} // ns nrn_parquet::circuit
