#include "perft.h"

#include "../movegen/movegen.h"

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <thread>
#include <filesystem>
#include <fstream>
#include <sstream>

#define RED_TEXT "\033[31m"
#define GREEN_TEXT "\033[32m"
#define RESET_TEXT "\033[0m"

Board perft::board;

std::atomic<bool> perft::search_active(false);

void perft::perft_session(std::string fen, uint32 depth)
{
    std::cout << std::left << std::dec << std::setfill(' ');
    std::cout << "Running perft session for src/board/move_gen.cpp:\n - Starting FEN: " << fen << "\n - Starting depth: " << depth << std::endl;

    perft::search_active = true;
    movegen::initialize_from_fen(perft::board, fen);

    while (depth > 0) {
        
        std::cout << board.as_pretty_string();

        std::cout << "Begin depth: " << depth << std::endl;

        std::vector<Move> legal_moves = movegen::generate_legal_moves(board);
        
        uint64 total = 0;
        for (uint32 i = 0; i < legal_moves.size(); i++) {
            std::cout << std::setw(4) << i << std::setw(5) << legal_moves[i].as_long_algebraic() << ": ";
            std::cout.flush();

            movegen::make_move(perft::board, legal_moves[i]);
            uint64 nodes = perft_h(depth - 1);
            total += nodes;
            std::cout << nodes << std::endl;
            movegen::unmake_move(perft::board, legal_moves[i]);
        }

        std::cout << "Done!\n - Total: " << total << std::endl;

        if (depth == 1) {
            break;
        }

        uint32 input;
        while (true) {
            std::cout << "Enter next move: ";
            std::cout.flush();
            std::cin >> input;

            // Check if the input is valid
            if (std::cin.fail() || input >= legal_moves.size()) {
                std::cin.clear(); // Clear the error flag
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard invalid input
                continue;
            } 
            break;
        }

        movegen::make_move(perft::board, legal_moves[input]);
        depth--;
    }
    perft::search_active = false;
}

uint64 perft::perft(std::string fen, uint32 depth)
{   
    std::cout << "Running perft test for src/board/move_gen.cpp:\n - Starting FEN: " << std::dec << fen << "\n - Depth: " << depth << "\nMoves:" << std::endl;

    perft::search_active = true;
    movegen::initialize_from_fen(perft::board, fen);
    std::vector<Move> legal_moves = movegen::generate_legal_moves(board);

    uint64 total = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (Move &move : legal_moves) {
        std::cout << " - " << move.as_long_algebraic() << ": ";
        std::cout.flush();

        movegen::make_move(perft::board, move);
        uint64 nodes = perft_h(depth - 1);
        total += nodes;
        std::cout << nodes << std::endl;
        movegen::unmake_move(perft::board, move);
    }

    perft::search_active = false;

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Done!\n - Total: " << total << "\n - Time: " << duration.count() << "millis" << std::endl;
    return total;
}

void perft::perft_accuracy_test()
{
    std::cout << std::left << std::dec << std::setfill(' ');
    std::cout << "Running perft accuracy test for src/board/move_gen.cpp:" << std::endl;

    perft::search_active = true;

    // Get the full path of the test suite
    std::filesystem::path source_path = __FILE__;
    std::filesystem::path source_directory = source_path.parent_path();
    std::filesystem::path file_path = source_directory / "accuracy_test_suite.csv";

    // Open file
    std::ifstream suite(file_path);
    if (!suite.good()) {
        throw new std::runtime_error("Problem opening accuracy_test_suite.csv!");
    }

    // Iterate over lines of csv
    std::string test_string;
    while (std::getline(suite, test_string)) {
        // Parse line of csv
        std::istringstream test(test_string);
        std::string fen;
        if (!std::getline(test, fen, ',')) {
            throw new std::runtime_error("Problem reading FEN from accuracy_test_suite.csv!");
        }
        int64 correct_nodes[6];
        for (int i = 0; i < 6; i++) {
            std::string nodes_string;
            if (!std::getline(test, nodes_string, ',')) {
                throw new std::runtime_error("Problem reading nodes from accuracy_test_suite.csv!");
            }
            try {
                correct_nodes[i] = std::stoi(nodes_string);
            }
            catch (const std::invalid_argument& e) {
                throw std::invalid_argument(std::string("Problem reading number from accuracy_test_suite.csv! ") + e.what());
            }
        }

        uint32 depth = 6;
        while (depth) {
            if (correct_nodes[depth - 1] < 0) {
                depth--;
                continue;
            }

            std::cout << "depth " << depth << " " << std::setw(86) << fen;
            std::cout.flush();

            movegen::initialize_from_fen(perft::board, fen);
            int64 nodes = perft_h(depth);

            if (nodes == correct_nodes[depth - 1]) {
                std::cout << GREEN_TEXT << nodes << RESET_TEXT << std::endl;
                break;
            
            } else {
                std::cout << RED_TEXT << nodes << RESET_TEXT << std::endl;
            }
            depth--;
        }
    }
    std::cout << "DONE" << std::endl;
    perft::search_active = false;
}

uint64 perft::perft_speed_test() {
    std::cout << std::left << std::dec << std::setfill(' ');
    std::cout << "Running perft speed test for src/board/move_gen.cpp:" << std::endl;

    // Get the full path of the test suite
    std::filesystem::path source_path = __FILE__;
    std::filesystem::path source_directory = source_path.parent_path();
    std::filesystem::path file_path = source_directory / "random_positions.txt";

    // Open file
    std::ifstream random_fens(file_path);
    if (!random_fens.good()) {
        throw new std::runtime_error("Problem opening accuracy_test_suite.csv!");
    }

    uint64 total_nodes = 0;

    // Iterate over lines of txt
    std::string fen;
    uint32 i = 0;
    while (std::getline(random_fens, fen)) {
        try {
            movegen::initialize_from_fen(perft::board, fen);
        } catch (const std::invalid_argument &e) {
            throw new std::runtime_error(std::string("Problem reading fen from random_positions.txt! FEN: ") + fen + std::string(" ") + e.what());
        }

        std::cout << "(" << std::setw(3) << ++i << "/200) fen " << std::setw(86) << fen;
        std::cout.flush();

        uint64 nodes = 0;

        perft::search_active = true;
        std::thread timeout(set_timeout, std::chrono::milliseconds(200));

        for (uint32 depth = 1; depth < 100; depth++) {
            nodes += perft_h(depth);
        }
        timeout.join();

        std::cout << 5 * nodes << "n/s" << std::endl;
        total_nodes += nodes;
    }

    uint64 average_speed = 5 * total_nodes / i;
    std::cout << "DONE!\nAVERAGE SPEED: " << average_speed << "n/s" << std::endl;
    return average_speed;
}

uint64 perft::perft_h(uint32 depth)
{
    if (depth == 0) {
        return 1;
    }

    if (!perft::search_active) {
        return 0;
    }

    Move moves[225];
    uint32 end = 0;
    movegen::generate_pseudo_legal_moves(perft::board, moves, end);

    uint64 nodes = 0;

    for (uint32 i = 0; i < end; i++) {
        if (movegen::make_move(perft::board, moves[i])) {
            nodes += perft_h(depth - 1);
            movegen::unmake_move(perft::board, moves[i]);
        }
    }

    return nodes;
}

void perft::set_timeout(std::chrono::milliseconds duration)
{
    std::this_thread::sleep_for(duration);
    perft::search_active = false;
}