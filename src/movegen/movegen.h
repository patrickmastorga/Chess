#pragma once

#include <vector>

#include "../types.h"
#include "../board/board.h"

/*
    optimized (but unsafe) helper methods for interacting with a board struct to generate/play moves and verify if a given move is legal
*/
namespace movegen {

    // populate all fields to match given position in Forsyth–Edwards Notation (FEN)
    // throws std::invalid_argument if the fen string is not valid (look at wikipedia article for reference)
    void initialize_from_fen(Board &board, std::string fen);

    // Loads the specified position from the uci position string
    // position [fen <fenstring> | startpos ]  moves <move1> .... <movei>
    // throws std::invalid_argument if the position string is not valid
    void initialize_from_uci_string(Board &board, std::string uci_string);

    // returns a vector of legal moves for the given board
    std::vector<Move> generate_legal_moves(Board &board) noexcept;
    
    // generates pseudo-legal moves for the given board
    // populates the stack starting from the given index
    // doesnt generate all pseudo legal moves, omits moves that are guarenteed to be illegal
    // returns true if the king was in check
    bool generate_pseudo_legal_moves(const Board &board, Move *stack, uint32 &idx, bool ignore_non_captures = false) noexcept;

    // generates pseudo-legal captures for the given board
    // populates the stack starting from the given index
    // assumes the king is not in check
    // pinned_peices is bitboard of pinned peices
    // used by generate_pseudo_legal_moves when generate_only_captures_or_forced is true
    void generate_captures(const Board &board, Move *stack, uint32 &idx, uint64 pinned_peices) noexcept;

    // populates the given fields based on the inputted board for the perspective of the given color's king
    // checkers <- bitboard of peices attacking king
    // checking_squares <- bitboard including checkers and rays between king and sliding peice checkers
    // pinned_peices <- bitboard of peices pinned to the king (both friendly and enemy)
    void calculate_checks_and_pins(const Board &board, uint32 c, uint64 &checkers, uint64 &checking_squares, uint64 &pinned_peices) noexcept;

    // return true if inputted move is legal in the given board
    // if unsafe is set to true, relies on the legal flag of move and relies that the move is pseudo legal
    bool is_legal(Board &board, Move &move, bool unsafe = false) noexcept;

    // return true if inputted move is pseudo legal in the given board
    bool is_pseudo_legal(Board &board, Move &move) noexcept;

    // return true if inputted pseudo legal castling move is legal
    // assumes castling rights are not lost and king is not in check
    // relies on the legal flag of move (unsafe)
    bool castling_move_is_legal(const Board &board, Move &move) noexcept;

    // return true if the king belonging to the inputted color is currently being attacked (0 for white and 1 for black)
    bool king_attacked(const Board &board, uint32 c) noexcept;

    // return bitboard of pseudo legal destination squares of peice on given index
    uint64 pseudo_moves(const Board &board, uint32 index) noexcept;

    // return bitboard of pseudo legal destination squares of pawn on given index (including en_passant)
    uint64 pawn_pseudo_moves(const Board &board, uint32 index, uint32 c) noexcept;

    // return bitboard of knight attacks from a given index
    uint64 knight_attacks(uint32 index) noexcept;

    // return bitboard of bishop attacks from a given index
    uint64 bishop_attacks(const Board &board, uint32 index) noexcept;

    // return bitboard of rook attacks from a given index
    uint64 rook_attacks(const Board &board, uint32 index) noexcept;

    // return bitboard of queen attacks from a given index
    uint64 queen_attacks(const Board &board, uint32 index) noexcept;

    // return bitboard of knight attacks from a given index
    uint64 king_attacks(uint32 index) noexcept;

    // return a bitboard of attackers of a given color (0 for white and 1 for black) on the given square (index [0, 63] -> [a1, h8])
    uint64 attackers(const Board &board, uint32 index, uint32 c) noexcept;

    // update the given board based on the inputted move (must be pseudo legal) (unsafe)
    // returns true if move was legal and process completed
    // relies on the legal flag of move (unsafe)
    bool make_move(Board &board, Move &move) noexcept;

    // update the given board to reverse the inputted move (must have just been move previously played) (unsafe)
    void unmake_move(Board &board, Move &move) noexcept;

    // returns true if any repitition has occured (twofold repitition)
    bool any_repitition_occured(const Board &board) noexcept;

    // returns true if the game is over by threefold repititions
    bool is_draw_by_repitition(const Board &board) noexcept;

    // returns true if the game is over by the fifty move rule
    bool is_draw_by_fifty_move_rule(const Board &board)  noexcept;

    // returns true if the game is over by insufficient material
    bool is_draw_by_insufficient_material(const Board &board)  noexcept;
}