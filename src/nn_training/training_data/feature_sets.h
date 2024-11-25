#pragma once

#include <iostream>

#include "data_stream.h"

// returns a pointer to the next bacth created from data from the stream
// returns nullptr if unsuccessful
template <typename BatchT>
BatchT *get_batch(TrainingDataStream *stream, size_t size) noexcept
{
    try {
        return BatchT::get_batch(stream, size);
    } catch (std::exception &e) {
        std::cout << "ERROR: while getting batch: " << e.what() << std::endl;
        return nullptr;
    }
}

struct BasicFeatureSetBatch;

/*
    Represents a batch of training daya with binary (peice*color, rank, file)
*/
struct BasicFeatureSetBatch
{
    ~BasicFeatureSetBatch();
    
    // the number of trainnig data entries in this batch
    size_t size;
    
    // arrays with first dimension size
    float *input;
    float *score;

    static BasicFeatureSetBatch *get_batch(TrainingDataStream *stream, size_t size);
};