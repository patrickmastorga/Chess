#include <iostream>
#include <filesystem>

#include "../training_data/data_stream.h"
#include "../training_data/feature_sets.h"

extern "C" {
    
    _declspec(dllexport) void test() {
        std::cout << "Hello World!" << std::endl;
    }

    // returns the appropriate data stream for path type
    // returns nullptr if unsuccessful
    _declspec(dllexport) TrainingDataStream *create_stream(const char *path, float drop, size_t worker_id, size_t num_workers)
    {
        std::filesystem::path fpath(path);
        if (fpath.extension() == ".binpack") {
            return new BinpackTrainingDataStream(fpath, drop, worker_id, num_workers);
        } else {
            return nullptr;
        }
    }

    // destroys the data stream
    _declspec(dllexport) void destroy_stream(TrainingDataStream *stream)
    {
        delete stream;
    }

    // constructs a basic batch with data from the training data stream
    _declspec(dllexport) BasicFeatureSetBatch *get_basic_batch(TrainingDataStream *stream, size_t size)
    {
        return BasicFeatureSetBatch::get_batch(stream, size);
    }

    // destroys the batch
    _declspec(dllexport) void destroy_basic_batch(BasicFeatureSetBatch *batch)
    {
        delete batch;
    }

}