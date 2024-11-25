#include "feature_sets.h"

#include <algorithm>
#include <stdexcept>

#include "../../types.h"

BasicFeatureSetBatch::~BasicFeatureSetBatch()
{
    delete[] input;
    delete[] score;
}

BasicFeatureSetBatch *BasicFeatureSetBatch::get_batch(TrainingDataStream *stream, size_t size)
{
    if (!stream) {
        throw new std::runtime_error("Stream is null! Cannot get next batch!");
    }
    
    BasicFeatureSetBatch *batch = new BasicFeatureSetBatch();
    batch->size = size;
    batch->input = new float[size * 12 * 64];
    batch->score = new float[size];
    std::fill_n(batch->input, size * 12 * 64, 0.0f);

    for (size_t i = 0; i < size; i++) {
        const TrainingDataEntry *entry = stream->get_next_entry();
        if (!entry) {
            // No more data left
            return nullptr;
        }
        for (uint32 j = 0; j < 64; j++) {
            if (!entry->position.peices[j]) {
                continue;
            }
            uint32 peice = entry->position.peices[j];
            uint32 c = peice >> 4;
            uint32 peice_type = peice & 0b111;

            batch->input[i * 12 * 64 + c * 6 * 64 + (peice_type - 1) * 64 + j] = 1.0f;
        }
        batch->score[i] = static_cast<float>(entry->score);
    }
    return batch;
}
