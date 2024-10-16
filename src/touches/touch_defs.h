/**
 * Copyright (C) 2018 Blue Brain Project
 * All rights reserved. Do not distribute without further notice.
 *
 * @author Fernando Pereira <fernando.pereira@epfl.ch>
 *
 */
#pragma once

#include <algorithm>
#include <iostream>

namespace neuron_parquet {
namespace touches {

enum Location { NEURON_ID, SECTION_ID, SEGMENT_ID };
enum Version { V1, V2, V3 };

namespace v1 {
    struct Touch {
        using Schema = void;
        using Metadata = void;

        int pre_synapse_ids[3];
        int post_synapse_ids[3];
        int branch;
        float distance_soma;
        float pre_offset, post_offset;
    };
}

namespace v2 {
    struct Touch : public v1::Touch {
        float pre_section_fraction;
        float post_section_fraction;
        float pre_position[3];
        float post_position[3];
        float spine_length = -1;
        unsigned char branch_type = 255;

        Touch() = default;
        Touch(const Touch&) = default;
        Touch(const v1::Touch& t)
            : v1::Touch(t)
        {};
    };
}

namespace v3 {
    struct Touch : public v2::Touch {
        float pre_position_center[3];
        float post_position_surface[3];

        Touch() = default;
        Touch(const Touch&) = default;
        Touch(const v1::Touch& t)
            : v2::Touch(t)
        {};
        Touch(const v2::Touch& t)
            : v2::Touch(t)
        {};
    };
}

struct IndexedTouch : public v3::Touch {
    int getPreNeuronID() const {
        return pre_synapse_ids[NEURON_ID];
    }
    int getPostNeuronID() const {
        return post_synapse_ids[NEURON_ID];
    }

    IndexedTouch() = default;
    IndexedTouch(const IndexedTouch&) = default;

    IndexedTouch(const v1::Touch& t, long index)
        : v3::Touch(t)
        , synapse_index(index)
    {};

    IndexedTouch(const v2::Touch& t, long index)
        : v3::Touch(t)
        , synapse_index(index)
    {};

    IndexedTouch(const v3::Touch& t, long index)
        : v3::Touch(t)
        , synapse_index(index)
    {};

    long synapse_index;
};

}  // namespace touches
}  // namespace neuron_parquet
