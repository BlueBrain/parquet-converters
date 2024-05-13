/**
 * Copyright (C) 2018 Blue Brain Project
 * All rights reserved. Do not distribute without further notice.
 *
 * @author Fernando Pereira <fernando.pereira@epfl.ch>
 *
 */
#include <assert.h>

#include <arrow/util/key_value_metadata.h>

#include "parquet_writer.h"
#include "version.h"

namespace neuron_parquet {
namespace touches {

using namespace parquet;


static std::shared_ptr<GroupNode> setupSchema(Version version) {
  schema::NodeVector fields;

  fields.push_back(schema::PrimitiveNode::Make(
      "synapse_id", Repetition::REQUIRED, Type::INT64, ConvertedType::INT_64));

  fields.push_back(schema::PrimitiveNode::Make(
      "source_node_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_32));

  fields.push_back(schema::PrimitiveNode::Make(
      "target_node_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_32));

  // POSITION OF THE SYNAPSE //
  fields.push_back(schema:: PrimitiveNode::Make(
       "efferent_section_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_16 ) );
  fields.push_back(schema:: PrimitiveNode::Make(
       "efferent_segment_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_16 ) );
  fields.push_back(schema:: PrimitiveNode::Make(
       "afferent_section_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_16 ) );
  fields.push_back(schema:: PrimitiveNode::Make(
       "afferent_segment_id", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_16 ) );

  fields.push_back(schema::PrimitiveNode::Make(
      "efferent_segment_offset", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));

  fields.push_back(schema::PrimitiveNode::Make(
      "afferent_segment_offset", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));

  fields.push_back(schema::PrimitiveNode::Make(
      "distance_soma", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));

  fields.push_back(schema::PrimitiveNode::Make(
      "branch_order", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_8));

  if (version >= V2) {
      fields.push_back(schema:: PrimitiveNode::Make(
           "efferent_section_pos", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE ) );
      fields.push_back(schema:: PrimitiveNode::Make(
           "afferent_section_pos", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE ) );

      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_surface_x", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_surface_y", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_surface_z", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_center_x", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_center_y", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_center_z", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));

      fields.push_back(schema::PrimitiveNode::Make(
          "spine_length", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));

      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_section_type", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_8));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_section_type", Repetition::REQUIRED, Type::INT32, ConvertedType::INT_8));
  }

  if (version >= V3) {
      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_center_x", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_center_y", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "efferent_center_z", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_surface_x", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_surface_y", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
      fields.push_back(schema::PrimitiveNode::Make(
          "afferent_surface_z", Repetition::REQUIRED, Type::FLOAT, ConvertedType::NONE));
  }

  // Create a GroupNode named 'schema' using the primitive nodes defined above
  // This GroupNode is the root node of the schema tree
  return std::static_pointer_cast<schema::GroupNode>(
      GroupNode::Make("touchSchema", Repetition::REQUIRED, fields));
}


TouchWriterParquet::TouchWriterParquet(const string filename, const Version v, const std::string& version_string)
    : version(v)
    , _buffer_offset(0)
{
    // Create a ParquetFileWriter instance
    PARQUET_ASSIGN_OR_THROW(
        out_file,
        ::arrow::io::FileOutputStream::Open(filename.c_str()));
    touchSchema = setupSchema(version);

    auto metadata = std::make_shared<::arrow::KeyValueMetadata>(
        std::unordered_map<std::string, std::string>{
            {"touchdetector_version", version_string},
            {"touch2parquet_version", neuron_parquet::VERSION}
        }
    );

    WriterProperties::Builder prop_builder;
    prop_builder.compression(Compression::SNAPPY);
    prop_builder.disable_dictionary();

    file_writer = ParquetFileWriter::Open(out_file, touchSchema, prop_builder.build(), metadata);

    // Allocate contiguous buffers for FULL output and TRANSPOSED block
    // Too big to put in stack
    _buffer.reset(new BUF_T<BUFFER_LEN>());
    _tbuffer.reset(new BUF_T<TRANSPOSE_LEN>());
}


TouchWriterParquet::~TouchWriterParquet() {
    if( _buffer_offset > 0 ) {
        // Flush remaining data
        _writeBuffer(_buffer_offset);
    }
    file_writer->Close();
    const auto status = out_file->Close();
    if (!status.ok()) {
        std::clog << status.ToString() << std::endl;
    }
}


void TouchWriterParquet::write(const IndexedTouch* data, uint32_t length) {

    //Split large Data in BUFFER_SIZE chunks
    while( length > 0 ) {
        uint32_t write_n = BUFFER_LEN - _buffer_offset;
        if( length < write_n ) {
            write_n = length;
        }

        //Write sub dataset
        _writeDataSet( data, write_n );
        data   += write_n;
        length -= write_n;
    }
}


// We need to chunk to avoid large buffers not fitting in cache
void TouchWriterParquet::_writeDataSet(const IndexedTouch* data, uint length) {
    uint n_chunks = length / TRANSPOSE_LEN;
    uint remaining = length % TRANSPOSE_LEN;

    for( uint i=0; i<n_chunks; i++) {
        _transpose_buffer_part(data, i*TRANSPOSE_LEN, TRANSPOSE_LEN);
    }
    _transpose_buffer_part(data, n_chunks*TRANSPOSE_LEN, remaining);

    assert(_buffer_offset+length <= BUFFER_LEN);

    if( _buffer_offset+length == BUFFER_LEN ) {
        // We only write full buffers
        // Remaining buffer data only on destruction
        _writeBuffer(BUFFER_LEN);
        _buffer_offset = 0;
    }
    else {
        _buffer_offset+=length;
    }
}


void TouchWriterParquet::_transpose_buffer_part(const IndexedTouch* data, uint offset, uint length) {
    // Here we are transposing using the TBuffer
    // Indexes at 0, so we also advance the local ptr to the offset
    data += offset;

    for( uint i=0; i<length; i++ ) {
        _tbuffer->synapse_id[i] = data[i].synapse_index;
        _tbuffer->pre_neuron_id[i] = data[i].getPreNeuronID();
        _tbuffer->post_neuron_id[i] = data[i].getPostNeuronID();
        _tbuffer->pre_offset[i] = data[i].pre_offset;
        _tbuffer->post_offset[i] = data[i].post_offset;
        _tbuffer->distance_soma[i] = data[i].distance_soma;
        _tbuffer->branch_order[i] = data[i].branch;
        _tbuffer->pre_section[i] = data[i].pre_synapse_ids[SECTION_ID];
        _tbuffer->pre_segment[i] = data[i].pre_synapse_ids[SEGMENT_ID];
        _tbuffer->post_section[i] = data[i].post_synapse_ids[SECTION_ID];
        _tbuffer->post_segment[i] = data[i].post_synapse_ids[SEGMENT_ID];

        if( _tbuffer->pre_section[i]>0x7fff ) {
            printf("Problematic pre_section %d of %d → %d\n",
                   _tbuffer->pre_section[i],
                   _tbuffer->pre_neuron_id[i],
                   _tbuffer->post_neuron_id[i]);
            throw runtime_error("Invalid pre_section. Please check endianess");
        }
        if( _tbuffer->pre_segment[i]>0x7fff )
            printf("Problematic pre_segment %d\n", _tbuffer->pre_segment[i]);
        if( _tbuffer->post_section[i]>0x7fff )
            printf("Problematic post_section %d\n", _tbuffer->post_section[i]);
        if( _tbuffer->post_segment[i]>0x7fff )
            printf("Problematic post_segment %d\n", _tbuffer->post_segment[i]);

        if (version >= V2) {
            _tbuffer->pre_section_fraction[i] = data[i].pre_section_fraction;
            _tbuffer->post_section_fraction[i] = data[i].post_section_fraction;

            _tbuffer->pre_position[0][i] = data[i].pre_position[0];
            _tbuffer->pre_position[1][i] = data[i].pre_position[1];
            _tbuffer->pre_position[2][i] = data[i].pre_position[2];
            _tbuffer->post_position[0][i] = data[i].post_position[0];
            _tbuffer->post_position[1][i] = data[i].post_position[1];
            _tbuffer->post_position[2][i] = data[i].post_position[2];
            _tbuffer->spine_length[i] = data[i].spine_length;
            _tbuffer->pre_branch_type[i] = ((data[i].branch_type >> BRANCH_SHIFT) & BRANCH_MASK) + BRANCH_OFFSET;
            _tbuffer->post_branch_type[i] = (data[i].branch_type & BRANCH_MASK) + BRANCH_OFFSET;
        }

        if (version >= V3) {
            _tbuffer->pre_position_center[0][i] = data[i].pre_position_center[0];
            _tbuffer->pre_position_center[1][i] = data[i].pre_position_center[1];
            _tbuffer->pre_position_center[2][i] = data[i].pre_position_center[2];
            _tbuffer->post_position_surface[0][i] = data[i].post_position_surface[0];
            _tbuffer->post_position_surface[1][i] = data[i].post_position_surface[1];
            _tbuffer->post_position_surface[2][i] = data[i].post_position_surface[2];
        }
    }

    // Append to main buffer
    uint buffer_offset = _buffer_offset + offset;
    std::copy( _tbuffer->synapse_id,     _tbuffer->synapse_id + length,   _buffer->synapse_id + buffer_offset );
    std::copy( _tbuffer->pre_neuron_id,  _tbuffer->pre_neuron_id+length,  _buffer->pre_neuron_id+buffer_offset );
    std::copy( _tbuffer->post_neuron_id, _tbuffer->post_neuron_id+length, _buffer->post_neuron_id+buffer_offset );
    std::copy( _tbuffer->pre_offset,     _tbuffer->pre_offset+length,     _buffer->pre_offset+buffer_offset );
    std::copy( _tbuffer->post_offset,    _tbuffer->post_offset+length,    _buffer->post_offset+buffer_offset );
    std::copy( _tbuffer->distance_soma,  _tbuffer->distance_soma+length,  _buffer->distance_soma+buffer_offset );
    std::copy( _tbuffer->branch_order,   _tbuffer->branch_order+length,   _buffer->branch_order+buffer_offset );
    std::copy( _tbuffer->pre_section,    _tbuffer->pre_section+length,    _buffer->pre_section+buffer_offset );
    std::copy( _tbuffer->pre_segment,    _tbuffer->pre_segment+length,    _buffer->pre_segment+buffer_offset );
    std::copy( _tbuffer->post_section,   _tbuffer->post_section+length,   _buffer->post_section+buffer_offset );
    std::copy( _tbuffer->post_segment,   _tbuffer->post_segment+length,   _buffer->post_segment+buffer_offset );

    if (version >= V2) {
        std::copy(_tbuffer->pre_section_fraction,
                  _tbuffer->pre_section_fraction + length,
                  _buffer->pre_section_fraction + buffer_offset);
        std::copy(_tbuffer->post_section_fraction,
                  _tbuffer->post_section_fraction + length,
                  _buffer->post_section_fraction + buffer_offset);
        for (int i = 0; i < 3; ++i) {
            std::copy(_tbuffer->pre_position[i],
                      _tbuffer->pre_position[i] + length,
                      _buffer->pre_position[i] + buffer_offset);
            std::copy(_tbuffer->post_position[i],
                      _tbuffer->post_position[i] + length,
                      _buffer->post_position[i] + buffer_offset);
        }
        std::copy(_tbuffer->spine_length,
                  _tbuffer->spine_length + length,
                  _buffer->spine_length + buffer_offset);
        std::copy(_tbuffer->pre_branch_type,
                  _tbuffer->pre_branch_type + length,
                  _buffer->pre_branch_type + buffer_offset);
        std::copy(_tbuffer->post_branch_type,
                  _tbuffer->post_branch_type + length,
                  _buffer->post_branch_type + buffer_offset);
    }

    if (version >= V3) {
        for (int i = 0; i < 3; ++i) {
            std::copy(_tbuffer->pre_position_center[i],
                      _tbuffer->pre_position_center[i] + length,
                      _buffer->pre_position_center[i] + buffer_offset);
            std::copy(_tbuffer->post_position_surface[i],
                      _tbuffer->post_position_surface[i] + length,
                      _buffer->post_position_surface[i] + buffer_offset);
        }
    }
}


///
/// Low-level function to write directly a IndexedTouch set to the currently open row group
///
void TouchWriterParquet::_writeBuffer(uint length) {
    RowGroupWriter* rg_writer = file_writer->AppendRowGroup();

    //pre_neuron / post_neuron [ids, section, segment]
    int64_writer = static_cast<Int64Writer*>(rg_writer->NextColumn());
    int64_writer->WriteBatch(length, nullptr, nullptr, _buffer->synapse_id);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_neuron_id);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_neuron_id);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_section);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_segment);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_section);
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_segment);

    //pre_offset
    float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
    float_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_offset);

    //post_offset
    float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
    float_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_offset);

    //distance_soma
    float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
    float_writer->WriteBatch(length, nullptr, nullptr, _buffer->distance_soma);

    //branch_order
    int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
    int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->branch_order);

    if (version >= V2) {
        float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
        float_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_section_fraction);
        float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
        float_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_section_fraction);

        for (int i = 0; i < 3; ++i) {
            float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
            float_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_position[i]);
        }

        for (int i = 0; i < 3; ++i) {
            float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
            float_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_position[i]);
        }

        float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
        float_writer->WriteBatch(length, nullptr, nullptr, _buffer->spine_length);

        int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
        int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_branch_type);

        int32_writer = static_cast<Int32Writer*>(rg_writer->NextColumn());
        int32_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_branch_type);
    }

    if (version >= V3) {
        for (int i = 0; i < 3; ++i) {
            float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
            float_writer->WriteBatch(length, nullptr, nullptr, _buffer->pre_position_center[i]);
        }

        for (int i = 0; i < 3; ++i) {
            float_writer = static_cast<FloatWriter*>(rg_writer->NextColumn());
            float_writer->WriteBatch(length, nullptr, nullptr, _buffer->post_position_surface[i]);
        }
    }
}


}  // namespace touches
}  // namespace neuron_parquet
