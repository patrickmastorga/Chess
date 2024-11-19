#pragma once

#include "../../board/board.h"
#include "../../types.h"

#include <filesystem>
#include <vector>
#include <fstream>

/*
    Contains the data included in every training data entry
*/
struct TrainingDataEntry
{
    // the position for which to evaluate
    Board position;
    // the next move in the continuation
    Move move;
    // the score in centipawns
    int16 score;
    // the result of the game from which the move is from
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
    BinpackTrainingDataStream(std::filesystem::path path, float drop, size_t worker_id, size_t num_workers, size_t buffer_size=1050000);
    ~BinpackTrainingDataStream();

    // attempts to fetch the next entry in the file
    // fills the "entry" variable with the next entry
    // returns false if no more entries are available
    bool get_next_entry();

    // where the current entry is stored (read only)
    TrainingDataEntry entry;

private:
    float drop;
    size_t worker_id;
    size_t num_workers;
    
    std::ifstream file;
    size_t buffer_size;
    uint8 *buffer;

    size_t block_num;
    size_t entry_num;

    size_t block_size;
    size_t byte_index;
    size_t bits_remaining;

    size_t plies_remaining;

    // reads the header of the next block and returns the size of the next block
    size_t read_block_header();

    // loads the next block into memory
    // returns false if no more blocks remaining
    bool advance_blocks(size_t num_blocks=1);

    // returns true of there is data still remaining on the buffer
    bool data_available();

    // reinitializes the entry with a new position
    bool read_stem();

    // updates the current entry based on the next move/score in the movetext
    void read_movetext_entry();

    // reads the variable length encoded move from buffer
    // https://github.com/Sopel97/chess_pos_db/blob/master/docs/bcgn/variable_length.md
    Move read_vle_move();

    // reads the next 4 bit block size variable length encoding number from buffer
    uint16 read_vle_int();

    // gets the next bits in the buffer 
    uint8 read_bits(size_t num_bits);

    // tranfsormed the unsigned binpack format to appropriate signed integer
    int16 unsigned_to_signed(uint16 val);

    // returns the index of the nth set bit (starting from lsb) (0 indexed)
    // requires that at least n + 1 bits are set
    uint32 index_of_nth_set_bit(uint64 val, size_t n);
};