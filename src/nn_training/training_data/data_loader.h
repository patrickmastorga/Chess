#pragma once

#include <filesystem>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

#include "../training_data/data_stream.h"
#include "../training_data/feature_sets.h"

template <typename BatchT>
class DataLoader
{
public:
    DataLoader(std::filesystem::path &path, size_t batch_size, float drop, size_t num_workers);
    ~DataLoader();

    BatchT *get_next_batch() noexcept;

private:
    struct WorkerInfo {
        BatchT *batch;
        bool ready = false;
        bool finished = false;
    };

    std::filesystem::path path;
    size_t batch_size;
    float drop;
    size_t num_workers;
    size_t current_worker;
    std::vector<std::thread> workers;
    std::vector<WorkerInfo> worker_infos;
    std::mutex mutex;
    std::condition_variable cond_var;
    bool stop;

    void worker_fn(size_t worker_id) noexcept;
};

extern "C" {
    
    // returns the appropriate data stream for path type
    // returns nullptr if unsuccessful
    _declspec(dllexport) DataLoader<BasicFeatureSetBatch> *create_basic_data_loader(const char *path, size_t batch_size, float drop, size_t num_workers) noexcept;

    // destroys the data stream
    _declspec(dllexport) void destroy_basic_data_loader(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept;

    // constructs a basic batch with data from the training data stream
    _declspec(dllexport) BasicFeatureSetBatch *get_basic_batch(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept;

    // destroys the batch
    _declspec(dllexport) void destroy_basic_batch(BasicFeatureSetBatch *batch) noexcept;
}