#pragma once

#include <vector>
#include <map>
#include <string>

#include "../standard_definitions.h"
#include "../board/board.h"

/*
    struct for representing chess game in its entirety
*/
struct Game : public Board {
    virtual ~Game() = default;

    // Default constructor for game
    Game();

    // initializes the starting position into the game
    void initialize_starting_position() noexcept;

    // Loads the specified position (as Forsyth Edwards Notation) into the game
    // throws std::invalid_argument if the fen string is not valid (look at wikipedia article for reference)
    virtual void initialize_from_fen(std::string fen);

    // Loads the specified position from the uci position string
    // position [fen <fenstring> | startpos ]  moves <move1> .... <movei>
    // throws std::invalid_argument if the position string is not valid
    void from_uci_string(std::string uci_string);

    // returns true if it is white's turn
    bool white_to_move() const noexcept;

    // returns a vector of legal moves in the current position
    // if empty, than the game is over
    std::vector<Move> get_legal_moves() const noexcept;

    // returns true if the inputted move is legal in the current position
    bool is_legal(Move move) const noexcept;

    // if the inputted move is legal, play the move on the board
    // return false if unsuccesful (not a legal move)
    bool input_move(Move move) noexcept;
    bool input_move(std::string long_algebraic) noexcept;

    // returns true if the current player to move is in check
    bool in_check() const noexcept;

    // returns a string representation of the game in Portable Game Notation
    std::string as_pgn(std::map<std::string, std::string> headers = {});

    // list of all moves played so far
    std::vector<Move> game_moves;

    // list of all moves played so far (in algebraic notation)
    std::vector<std::string> game_moves_in_algebraic;

    // stores the fen which the game was initialed by
    std::string beginning_fen;
private:
    // helper fucntion to generate legal moves
    void generate_legal_moves() noexcept;

    // stores the current legal moves of the position
    std::vector<Move> current_legal_moves;
};