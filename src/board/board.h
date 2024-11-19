#pragma once

#include "../types.h"
#include <string>

#define METADATA_LENGTH 128

/*
NOTES ABOUT CHESS GAME REPRESENTATION

    - Fifty move rule is treated as an automatic draw
    - Threefold repitition is treated as an automatic draw

*/

struct Board;
struct Move;

/*
    struct for representing current game state
*/
struct Board {
    virtual ~Board() = default;

    // definitions for color bits in integer peice representations
    static constexpr uint32 WHITE = 0b01000;
    static constexpr uint32 BLACK = 0b10000;

    // definitions for peice types in integer peice representations
    static constexpr uint32 PAWN =    0b001;
    static constexpr uint32 KNIGHT =  0b010;
    static constexpr uint32 BISHOP =  0b011;
    static constexpr uint32 ROOK =    0b100;
    static constexpr uint32 QUEEN =   0b101;
    static constexpr uint32 KING =    0b110;

    // color and peice type at every square (index [0, 63] -> [a1, h8])
    // ex. white rook at b1 -> peices[2] == Colors::WHITE + Peices::ROOK
    uint32 peices[64];

    // metadata about the current and past positions of the board
    // | 48b zobrist | 4b castling bq/wq/bk/wk | 6b ep square | 6b hm since pawn/capt |
    uint64 metadata[METADATA_LENGTH];

    // total half moves since game start (half move is one player taking a turn)
    uint32 halfmove_number;

    // bitboards for all peices of a given color/type combination
    uint64 peices_of_color_and_type[2][7];

    // bitboards for all peices of a given color
    uint64 peices_of_color[2];

    // bitboard for all peices
    uint64 all_peices;

    // returns the position encoded in Forsyth–Edwards Notation (FEN)
    std::string as_fen() const noexcept;

    // returns the board as a string to be printed out in the terminal to view the board
    std::string as_pretty_string() const noexcept;

    // return true if the specified color still has kinside castling rights
    bool kingside_castling_rights_not_lost(uint32 c) const noexcept;

    // return true if the specified color still has queenside castling rights
    bool queenside_castling_right_not_lost(uint32 c) const noexcept;

    // returns the number of consecutive half moves played without capturing a peice or moving a pawn
    uint32 halfmoves_since_pawn_move_or_capture() const noexcept;

    // returns the square over which a pawn has just passed over in a double jump in the previous move (0 if not any)
    uint32 eligible_en_pasant_square() const noexcept;
};


/*
    struct for representing chess move
*/
struct Move {

    // all necesarry data associated with the move
    // | 9b value (signed) | 5b captured peice | 7b flags | 6b target square | 6b start square |
    int32 data;

    // FLAGS

    static constexpr uint32 PROMOTION_TO_VALUE =  0b0000111;
    static constexpr uint32 PROMOTION_FLAG =      0b0001000;
    static constexpr uint32 EN_PASSANT_FLAG =     0b0010000;
    static constexpr uint32 CASTLE_FLAG =         0b0100000;
    static constexpr uint32 LEGAL_FLAG =          0b1000000;


    Move();

    Move(uint32 start, uint32 target, uint32 flags);

    // returns the starting square of the move
    uint32 start_square() const noexcept;

    // returns the ending square of the move
    uint32 target_square() const noexcept;

    // returns the peice and color of the peice that was originally on the start square
    uint32 moving_peice(const Board &board) const noexcept;

    // returns the peice and color of the peice that is being captured (use this before move has been played)
    uint32 captured_peice(const Board &board) const noexcept;

    // ONLY CALL THIS AFTER MOVE HAS BEEN PLAYED returns peice and color of the peice that has been captured by the move
    void store_captured_peice(uint32 peice) noexcept;

    // ONLY CALL THIS AFTER MOVE HAS BEEN PLAYED returns peice and color of the peice that has been captured by the move, except in case of en passant
    uint32 get_stored_captured_peice() const noexcept;

    // returns true if the move consists of a pawn promoting to a new peice
    bool is_promotion() const noexcept;

    // if is promotion, returns the peice type the pawn promoted to (0 otherwise)
    uint32 promoted_to() const noexcept;
    
    // returns true if the move consists of a pawn capturing a peice via en passant
    bool is_en_passant() const noexcept;

    // returns true if the move consists of a king castling either king or queen side
    bool is_castling() const noexcept;

    // returns true if the move was guarenteed to be legal in the position which it was generated (used for optimization)
    bool legal_flag_set() const noexcept;

    // sets the "guarenteed legal" flag of the move
    void set_legal_flag() noexcept;

    std::string as_long_algebraic() const;

    // override equality operator with other move
    bool operator==(const Move &other) const noexcept;

    // defines a null move
    static const Move NULL_MOVE;
};

namespace board_helpers
{
    
    // returns index [0, 63] -> [a1, h8] of square on board given algebraic notation for position on chess board (ex e3, a1, c8)
    // throw std::invalid_argument if algebraic notation is not valid
    uint32 algebraic_notation_to_board_index(std::string algebraic);

    // returns algebraic notation for position on chess board (ex e3, a1, c8) given index [0, 63] -> [a1, h8] of square on board 
    // throw std::invalid_argument if index is out of range
    std::string board_index_to_algebraic_notation(uint32 board_index);

    // given the long algebraic notation of a move in the current position, returns the properly initialized Move
    // throw std::invalid_argument if algebraic notation is not valid
    Move long_algebraic_to_move(Board &board, std::string long_algebraic);
};