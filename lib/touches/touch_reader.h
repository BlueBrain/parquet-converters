#ifndef LOADER_H
#define LOADER_H

#include <fstream>
#include <istream>
#include <functional>
#include <memory>
#include <vector>
#include <numeric>
#include <mutex>
#include <string.h>

#include "../generic_reader.h"
#include "touch_defs.h"



class TouchReader : public Reader<Touch>{
public:
    TouchReader( const char *filename, bool different_endian=false, bool buffered=false );
    ~TouchReader();


    Touch & begin();
    Touch & end();
    Touch & getNext();
    Touch & getItem( uint32_t index );

    virtual uint32_t block_count() const override {
        uint64_t n_blocks = record_count_ / BUFFER_LEN;
        if(record_count_ % BUFFER_LEN > 0) {
            n_blocks ++;
        }
        return n_blocks;
    }

    virtual uint64_t record_count() const override {
        return record_count_;
    }

    virtual void seek(uint64_t pos) override;

    virtual uint32_t fillBuffer(Touch* buf, uint32_t length) override;

    virtual bool is_chunked() const override {
        return false;
    }

    static const uint32_t BUFFER_LEN = 256;

private:
    void _fillBuffer();
    void _load_into( Touch* buf, uint32_t length );

    // File
    std::ifstream touchFile_;
    uint64_t record_count_;
    uint64_t offset_;
    bool endian_swap_;
    bool buffered_;

    // Internal Buffer: to be used for iteration
    uint32_t it_buf_index_;  // Offset relative to buffer
    uint32_t buffer_record_count_;
    std::unique_ptr<Touch[]> buffer_;
};



#endif // LOADER_H