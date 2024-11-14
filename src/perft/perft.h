#pragma once

#include "../standard_definitions.h"
#include "../board/board.h"

#include <string>
#include <chrono>
#include <atomic>

namespace perft
{
    extern Board board;

    extern std::atomic<bool> search_active;

    // runs a perft session
    // allows you to continue down lines until you see the error
    void perft_session(std::string fen, uint32 depth);

    // counts how many possible positions can be reached from te given depth away from the given position
    uint64 perft(std::string fen, uint32 depth);

    // counts how many possible positions can be reached from te given depth away from the given position
    void perft_accuracy_test();

    // returns the average nodes/second for a large number of test positions
    uint64 perft_speed_test();

    // recursive helper function for perft
    uint64 perft_h(uint32 depth);

    // function which sets the search_active flag to false after a set time
    void set_timeout(std::chrono::milliseconds duration);
}

