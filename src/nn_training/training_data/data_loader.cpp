#include "data_loader.h"

#include <iostream>

template <typename BatchT>
DataLoader<BatchT>::DataLoader(std::filesystem::path &path, size_t batch_size, float drop, size_t num_workers) : 
    path(path),
    batch_size(batch_size),
    drop(drop),
    num_workers(num_workers),
    current_worker(0),
    stop(false)
{
    // initialize worker infos
    worker_infos.resize(num_workers);

    // create worker threads
    for (size_t worker_id = 0; worker_id < num_workers; worker_id++) {
        workers.emplace_back(&DataLoader::worker_fn, this, worker_id);
    }
}

template <typename BatchT>
DataLoader<BatchT>::~DataLoader()
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
        cond_var.notify_all();
    }
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

template <typename BatchT>
BatchT *DataLoader<BatchT>::get_next_batch() noexcept
{
    std::unique_lock<std::mutex> lock(mutex);

    size_t beginning_worker = current_worker;

    do {
        // wait for the current worker to be ready with its batch
        cond_var.wait(lock, [this] {
            return worker_infos[current_worker].ready || worker_infos[current_worker].finished || stop;
        });

        // handle possible conditions
        if (stop) {
            return nullptr;
        }
        if (worker_infos[current_worker].finished) {
            current_worker = (current_worker + 1) % num_workers;
            continue;
        }
        if (!worker_infos[current_worker].ready) {
            continue;
        }

        // get the batch from the current worker
        BatchT *batch = worker_infos[current_worker].batch;
        worker_infos[current_worker].ready = false;

        // notify the worker to continue
        cond_var.notify_all();

        // move to the next worker in a round-robin fashion
        current_worker = (current_worker + 1) % num_workers;
        return batch;

    } while (current_worker != beginning_worker);

    return nullptr;
}

template <typename BatchT>
void DataLoader<BatchT>::worker_fn(size_t worker_id) noexcept
{
    std::unique_ptr<TrainingDataStream> stream = TrainingDataStream::create_stream(path, drop, worker_id, num_workers);

    while (true) {
        BatchT *batch = get_batch<BatchT>(stream.get(), batch_size);
        {
            std::unique_lock<std::mutex> lock(mutex);

            // If no more data, mark the worker as finished
            if (!batch) {
                worker_infos[worker_id].finished = true;
                cond_var.notify_all();
                break;
            }

            // Store the batch in the worker's status
            worker_infos[worker_id].batch = batch;
            worker_infos[worker_id].ready = true;

            // Notify the parent that the batch is ready
            cond_var.notify_all();

            // Wait until the parent retrieves the batch
            cond_var.wait(lock, [this, worker_id] {
                return !worker_infos[worker_id].ready || stop;
            });

            if (stop) break;
        }
    }
}

DataLoader<BasicFeatureSetBatch> *create_basic_data_loader(const char *path, size_t batch_size, float drop, size_t num_workers) noexcept
{
    std::filesystem::path fpath(path);
    return new DataLoader<BasicFeatureSetBatch>(fpath, batch_size, drop, num_workers);
}

void destroy_basic_data_loader(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept
{
    delete data_loader;
}

BasicFeatureSetBatch *get_basic_batch(DataLoader<BasicFeatureSetBatch> *data_loader) noexcept
{
    return data_loader->get_next_batch();
}

void destroy_basic_batch(BasicFeatureSetBatch *batch) noexcept
{
    delete batch;
}
