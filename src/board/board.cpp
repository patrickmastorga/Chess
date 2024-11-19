#include "board.h"

#include <stdexcept>

// MOVE STRUCT HELPER METHODS DEFINITIONS
std::string Board::as_fen() const noexcept
{
    std::string fen = "";

    // peice placement data
    char pcs[7] = { '\0', 'P', 'N', 'B', 'R', 'Q', 'K' };
    int32 gap = 0;
    for (int32 i = 56; i >= 0; i -= 8) {
        for (int32 j = 0; j < 8; j++) {
            // iterate over every square on board
            if (!peices[i + j]) {
                ++gap;
                continue;
            }
            // add gap charecter if needed
            if (gap) {
                fen += '0' + static_cast<char>(gap);
                gap = 0;
            }
            // add peice charecter
            uint32 peice = peices[i + j];
            fen += pcs[peice & 0b111] + static_cast<char>(32 * (peice >> 4));
        }
        // add gap charecter if needed
        if (gap) {
            fen += '0' + static_cast<char>(gap);
            gap = 0;
        }
        // add rank seperator
        if (i != 0) {
            fen += '/';
        }
    }


    // player to move
    fen += (halfmove_number % 2) ? " b " : " w ";


    // castling availiability
    std::string castlingAvailability = "";
    if (kingside_castling_rights_not_lost(0)) {
        castlingAvailability += 'K';
    }
    if (queenside_castling_right_not_lost(0)) {
        castlingAvailability += 'Q';
    }
    if (kingside_castling_rights_not_lost(1)) {
        castlingAvailability += 'k';
    }
    if (queenside_castling_right_not_lost(1)) {
        castlingAvailability += 'q';
    }
    if (castlingAvailability.size() == 0) {
        fen += "- ";
    }
    else {
        fen += castlingAvailability + " ";
    }


    // en passant target
    if (eligible_en_pasant_square()) {
        fen += board_helpers::board_index_to_algebraic_notation(eligible_en_pasant_square()) + " ";
    }
    else {
        fen += "- ";
    }


    // halfmoves since pawn move or capture
    fen += std::to_string(halfmoves_since_pawn_move_or_capture());
    fen += ' ';


    // total moves
    fen += std::to_string(halfmove_number / 2 + 1);

    return fen;
}

std::string Board::as_pretty_string() const noexcept
{
    std::string pretty_string = "\n +---+---+---+---+---+---+---+---+";

    // board printnout
    char pcs[7] = { ' ', 'P', 'N', 'B', 'R', 'Q', 'K' };
    for (int32 rank = 7; rank >= 0; rank -= 1) {
        pretty_string += "\n | ";

        for (int32 file = 0; file < 8; ++file) {
            // iterate over every square on board and add peice charecter
            uint32 peice = peices[rank * 8 + file];
            pretty_string += pcs[peice & 0b111] + static_cast<char>(32 * (peice >> 4));
            pretty_string += " | ";
        }

        pretty_string += '1' + static_cast<char>(rank);
        pretty_string += "\n +---+---+---+---+---+---+---+---+";
    }

    pretty_string += "\n   a   b   c   d   e   f   g   h\n\nFen: ";
    pretty_string += as_fen();
    pretty_string += "\n";

    return pretty_string;
}

bool Board::kingside_castling_rights_not_lost(uint32 c) const noexcept
{
    return metadata[halfmove_number % METADATA_LENGTH] & (static_cast<uint64>(c + 1) << 12);
}

bool Board::queenside_castling_right_not_lost(uint32 c) const noexcept
{
    return metadata[halfmove_number % METADATA_LENGTH] & (static_cast<uint64>(c + 1) << 14);
}

uint32 Board::halfmoves_since_pawn_move_or_capture() const noexcept
{
    return metadata[halfmove_number % METADATA_LENGTH] & ((1ULL << 6) - 1);
}

uint32 Board::eligible_en_pasant_square() const noexcept
{
    return (metadata[halfmove_number % METADATA_LENGTH] >> 6) & ((1ULL << 6) - 1);
}

// MOVE STRUCT HELPER METHODS DEFINITIONS

Move::Move() : data(0) {}

Move::Move(uint32 start, uint32 target, uint32 flags)
{
    data = start | (target << 6) | (flags << 12);
}

uint32 Move::start_square() const noexcept
{
    return data & 0b111111;
}

uint32 Move::target_square() const noexcept
{
    return (data >> 6) & 0b111111;
}

uint32 Move::moving_peice(const Board &board) const noexcept
{
    return board.peices[start_square()];
}

uint32 Move::captured_peice(const Board &board) const noexcept
{
    if (is_en_passant()) {
        return board.peices[start_square() + (target_square() - start_square()) % 8];
    } else {
        return board.peices[target_square()];
    }
}

void Move::store_captured_peice(uint32 peice) noexcept
{
    data |= peice << 19;
}

uint32 Move::get_stored_captured_peice() const noexcept
{
    return (data >> 19) & 0b11111;
}

bool Move::is_promotion() const noexcept
{
    return data & (PROMOTION_FLAG << 12);
}

uint32 Move::promoted_to() const noexcept
{
    return (data >> 12) & 0b111;
}

bool Move::is_en_passant() const noexcept
{
    return data & (EN_PASSANT_FLAG << 12);
}

bool Move::is_castling() const noexcept
{
    return data & (CASTLE_FLAG << 12);
}

bool Move::legal_flag_set() const noexcept
{
    return data & (LEGAL_FLAG << 12);
}

void Move::set_legal_flag() noexcept
{
    data |= (LEGAL_FLAG << 12);
}

std::string Move::as_long_algebraic() const
{
    std::string algebraic;

    algebraic += board_helpers::board_index_to_algebraic_notation(start_square());
    algebraic += board_helpers::board_index_to_algebraic_notation(target_square());

    char pcs[7] = { ' ', ' ', 'n', 'b', 'r', 'q', ' ' };
    if (is_promotion()) {
        algebraic += pcs[promoted_to()];
    }

    return algebraic;
}

bool Move::operator==(const Move& other) const noexcept
{
    return (this->data & ((1ULL << 18) - 1)) == (other.data & ((1ULL << 18) - 1));
}


// BOARD HELPER METHOD DEFINITIONS

uint32 board_helpers::algebraic_notation_to_board_index(std::string algebraic)
{
    if (algebraic.size() != 2) {
        throw std::invalid_argument("Algebraic notation should only be two letters long!");
    }

    int file = algebraic[0] - 'a';
    int rank = algebraic[1] - '1';

    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        throw std::invalid_argument("Algebraic notation should be in the form [a-h][1-8]!");
    }

    return rank * 8 + file;
}

std::string board_helpers::board_index_to_algebraic_notation(uint32 board_index)
{
    if (board_index < 0 || board_index > 63) {
        throw std::invalid_argument("Algebraic notation should only be two letters long!");
    }

    char file = 'a' + static_cast<char>(board_index % 8);
    char rank = '1' + static_cast<char>(board_index / 8);

    std::string algebraic;
    algebraic = file;
    algebraic += rank;

    return algebraic;
}

Move board_helpers::long_algebraic_to_move(Board &board, std::string long_algebraic)
{
    uint32 flags = 0;
    uint32 start = board_helpers::algebraic_notation_to_board_index(long_algebraic.substr(0, 2));
    uint32 target = board_helpers::algebraic_notation_to_board_index(long_algebraic.substr(2, 2));
    
    if (long_algebraic.size() == 5) {
        // promotion
        switch (long_algebraic.at(4)) {
            case 'n':
                flags = Move::PROMOTION_FLAG | Board::KNIGHT;
                break;
            case 'b':
                flags = Move::PROMOTION_FLAG | Board::BISHOP;
                break;
            case 'r':
                flags = Move::PROMOTION_FLAG | Board::ROOK;
                break;
            case 'q':
                flags = Move::PROMOTION_FLAG | Board::QUEEN;
                break;
            default:
                throw new std::invalid_argument("Invalid charecter in move notation!");
        }
    
    } else if ((board.peices[start] & 0b111) == Board::PAWN && target == board.eligible_en_pasant_square()) {
        // en passant
        flags = Move::EN_PASSANT_FLAG;
    
    } else if ((board.peices[start] & 0b111) == Board::KING && (target == start + 2 || start == target + 2)) {
        // caslting
        flags = Move::CASTLE_FLAG;
    }

    return Move(start, target, flags);
}

const Move Move::NULL_MOVE = Move(0, 0, 0);