#pragma once

#include "../../board/board.h"
#include "../../standard_definitions.h"

#include <filesystem>
#include <vector>
#include <fstream>

/*
    Contains the data included in every training data entry
*/
struct TrainingDataEntry
{
    Board position;
    int16 score;
    int16 result;
};

/*
    Class for streaming training data from a binpack file
*/
class BinpackTrainingDataStream
{
public:
    // path: reates a stream from the provided .binpack file at provided path
    // drop: the probability that a training data entry is skipped
    // buffer_size: the amount of space allocated to buffer (should be bigger than the biggest block in the .binpack file)
    // worker_id & num_workers: ensures that if multiple threads are streaming data, no duplicate data is received
    BinpackTrainingDataStream(std::filesystem::path path, float drop, size_t buffer_size, size_t worker_id, size_t num_workers);

    // attempts to fetch the next entry in the file
    // fills the "entry" variable with the next entry
    // returns false if no more entries are available
    bool get_next_entry();

    // where the current entry is stored (read only)
    TrainingDataEntry entry;

private:
    std::ifstream file;
    float drop;
    size_t buffer_size;
    size_t worker_id;
    size_t num_workers;
    std::vector<char> buffer;

    uint16 plies_remaining;
};