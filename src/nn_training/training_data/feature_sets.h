#pragma once

#include "data_stream.h"

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