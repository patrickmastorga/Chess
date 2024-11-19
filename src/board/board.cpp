#include "board.h"

#include <stdexcept>
#include <sstream>
#include <cctype>
#include <bit>

#include "zobrist.h"

// MOVE STRUCT HELPER METHODS DEFINITIONS

void Board::initialize_from_fen(std::string fen)
{
    // reset current members
    for (int32 i = 0; i < METADATA_LENGTH; i++) {
        metadata[i] = 0ULL;
    }
    for (int32 i = 0; i < 7; i++) {
        peices_of_color_and_type[0][i] = 0ULL;
        peices_of_color_and_type[1][i] = 0ULL;
    }
    peices_of_color[0] = 0ULL;
    peices_of_color[1] = 0ULL;
    all_peices = 0ULL;


    // extract data from fen string
    std::istringstream fen_string_stream(fen);
    std::string peice_placement_data, active_color, castling_rights, en_passant_target, halfmove_clock, fullmove_clock;

    // get peice placement data from fen string
    if (!std::getline(fen_string_stream, peice_placement_data, ' ')) {
        throw std::invalid_argument("Cannot get peice placement from FEN!");
    }
    
    // get active color data from fen string
    if (!std::getline(fen_string_stream, active_color, ' ')) {
        throw std::invalid_argument("Cannot get active color from FEN!");
    }
    
    // get castling availability data from fen string
    if (!std::getline(fen_string_stream, castling_rights, ' ')) {
        throw std::invalid_argument("Cannot get castling availability from FEN!");
    }

    // get en passant target data from fen string
    if (!std::getline(fen_string_stream, en_passant_target, ' ')) {
        throw std::invalid_argument("Cannot get en passant target from FEN!");
    }

    // get half move clock data from fen string
    if (!std::getline(fen_string_stream, halfmove_clock, ' ')) {
        halfmove_clock = "0";
    }
    
    // get full move number data from fen string
    if (!std::getline(fen_string_stream, fullmove_clock, ' ')) {
        fullmove_clock = "1";
    }


    // set halfmove number
    uint32 fullmove_number;
    try {
        fullmove_number = static_cast<uint32>(std::stoi(fullmove_clock));
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN full move number! ") + e.what());
    }

    // metadata for current position
    uint64 *_metadata;
    if (active_color == "w") {
        // white is to move
        halfmove_number = (fullmove_number - 1) * 2;
        _metadata = &metadata[halfmove_number % METADATA_LENGTH];
    }
    else if (active_color == "b") {
        // wlack is to move
        halfmove_number = (fullmove_number - 1) * 2 + 1;
        _metadata = &metadata[halfmove_number % METADATA_LENGTH];
        *_metadata ^= ZOBRIST_TURN_KEY;
    }
    else {
        // active color can only be "wb"
        throw std::invalid_argument("Unrecognised charecter in FEN active color");
    }

    // update the peices[] according to peice placement data
    int32 peice_index = 56;
    for (char peice_char : peice_placement_data) {
        if (std::isalpha(peice_char)) {
            // char contains data about peice color and type
            uint32 c = peice_char > 96 && peice_char < 123;
            uint32 color = (c + 1) << 3;

            // set appropriate members for peice
            switch (peice_char) {
            case 'P':
            case 'p':
                peices[peice_index] = color + Board::PAWN;
                peices_of_color_and_type[c][Board::PAWN] |= (1ULL << peice_index);
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::PAWN][peice_index++];
                break;
            case 'N':
            case 'n':
                peices[peice_index] = color + Board::KNIGHT;
                peices_of_color_and_type[c][Board::KNIGHT] |= (1ULL << peice_index);
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::KNIGHT][peice_index++];
                break;
            case 'B':
            case 'b':
                peices_of_color_and_type[c][Board::BISHOP] |= (1ULL << peice_index);
                peices[peice_index] = color + Board::BISHOP;
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::BISHOP][peice_index++];
                break;
            case 'R':
            case 'r':
                peices[peice_index] = color + Board::ROOK;
                peices_of_color_and_type[c][Board::ROOK] |= (1ULL << peice_index);
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::ROOK][peice_index++];
                break;
            case 'Q':
            case 'q':
                peices[peice_index] = color + Board::QUEEN;
                peices_of_color_and_type[c][Board::QUEEN] |= (1ULL << peice_index);
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::QUEEN][peice_index++];
                break;
            case 'K':
            case 'k':
                peices[peice_index] = color + Board::KING;
                peices_of_color_and_type[c][Board::KING] |= (1ULL << peice_index);
                *_metadata ^= ZOBRIST_PEICE_KEYS[c][Board::KING][peice_index++];
                break;
            default:
                throw std::invalid_argument("Unrecognised alpha char in FEN peice placement data!");
            }

        }
        else if (std::isdigit(peice_char)) {
            // char contains data about gaps between peices
            int32 gap = peice_char - '0';
            for (int32 i = 0; i < gap; ++i) {
                peices[peice_index++] = 0;
            }

        }
        else {
            if (peice_char != '/') {
                // only "123456789pnbrqkPNBRQK/" are allowed in peice placement data
                throw std::invalid_argument("Unrecognised char in FEN peice placement data!");
            }

            if (peice_index % 8 != 0) {
                // values between '/' should add up to 8
                throw std::invalid_argument("Arithmetic error in FEN peice placement data!");
            }

            // move peice index to next rank
            peice_index -= 16;
        }
    }

    // Initialize bitboards
    for (int32 i = 0; i < 7; i++) {
        peices_of_color[0] |= peices_of_color_and_type[0][i];
        peices_of_color[1] |= peices_of_color_and_type[1][i];
    }
    all_peices = peices_of_color[0] | peices_of_color[1];


    // set castling rights
    if (castling_rights != "-") {
        for (char castling_rights_char : castling_rights) {

            switch (castling_rights_char) {
            case 'K':
                *_metadata |= 0b0001ULL << 12;
                *_metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[0];
                break;
            case 'k':
                *_metadata |= 0b0010ULL << 12;
                *_metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[1];
                break;
            case 'Q':
                *_metadata |= 0b0100ULL << 12;
                *_metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[0];
                break;
            case 'q':
                *_metadata |= 0b1000ULL << 12;
                *_metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[1];
                break;
            default:
                throw std::invalid_argument("Unrecognised char in FEN castling availability data!");
            }
        }
    }


    // set eligible en passant square
    if (en_passant_target != "-") {
        try {
            uint64 eligible_en_passant_square = board_helpers::algebraic_notation_to_board_index(en_passant_target);
            *_metadata |= eligible_en_passant_square << 6;
        }
        catch (const std::invalid_argument& e) {
            throw std::invalid_argument(std::string("Invalid FEN en passant target! ") + e.what());
        }
    }


    // set halfmoves since pawn move or capture
    try {
        uint64 halmoves_since_pawn_move_or_capture = std::stoi(halfmove_clock);
        *_metadata |= halmoves_since_pawn_move_or_capture;
    }
    catch (const std::invalid_argument& e) {
        throw std::invalid_argument(std::string("Invalid FEN half move clock! ") + e.what());
    }
}

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
    if (queenside_castling_rights_not_lost(0)) {
        castlingAvailability += 'Q';
    }
    if (kingside_castling_rights_not_lost(1)) {
        castlingAvailability += 'k';
    }
    if (queenside_castling_rights_not_lost(1)) {
        castlingAvailability += 'q';
    }
    if (castlingAvailability.size() == 0) {
        fen += "- ";
    }
    else {
        fen += castlingAvailability + " ";
    }


    // en passant target
    if (eligible_en_passant_square()) {
        fen += board_helpers::board_index_to_algebraic_notation(eligible_en_passant_square()) + " ";
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
            uint32 peice_type = peice & 0b111;
            uint32 color = peice & ~0b111;
            if ((!peice_type && color) || (peice_type && !color) || peice_type == 7 || !(color == Board::WHITE || color == Board::BLACK || !color)) {
                pretty_string += "? | ";
            } else {
                pretty_string += pcs[peice_type] + static_cast<char>(32 * (peice >> 4));
                pretty_string += " | ";
            }
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

bool Board::queenside_castling_rights_not_lost(uint32 c) const noexcept
{
    return metadata[halfmove_number % METADATA_LENGTH] & (static_cast<uint64>(c + 1) << 14);
}

uint32 Board::halfmoves_since_pawn_move_or_capture() const noexcept
{
    return metadata[halfmove_number % METADATA_LENGTH] & ((1ULL << 6) - 1);
}

uint32 Board::eligible_en_passant_square() const noexcept
{
    return (metadata[halfmove_number % METADATA_LENGTH] >> 6) & ((1ULL << 6) - 1);
}


bool Board::is_draw_by_repitition(uint32 num_repititions) noexcept
{
    uint32 halfmoves_to_check = halfmoves_since_pawn_move_or_capture();
    
    if (halfmoves_to_check < 4 * num_repititions) {
        return false;
    }

    uint64 current_hash = metadata[halfmove_number % METADATA_LENGTH] & ~((1ULL << 16) - 1);

    uint32 num_repitions_found = 0;
    int32 start = static_cast<int32>(halfmove_number) - 4;
    int32 end = static_cast<int32>(halfmove_number) - halfmoves_to_check;
    for (int32 i = start; i >= end; i--) {
    
        if ((metadata[i % METADATA_LENGTH] & ~((1ULL << 16) - 1)) == current_hash) {
            num_repitions_found++;
            if (num_repitions_found == num_repititions) {
                return true;
            }
        }
    }

    return false;
}

bool Board::is_draw_by_fifty_move_rule() noexcept
{
    return halfmoves_since_pawn_move_or_capture() >= 50;
}

bool Board::is_draw_by_insufficient_material() noexcept
{
    if (
        peices_of_color_and_type[0][Board::PAWN] | peices_of_color_and_type[1][Board::PAWN]
      | peices_of_color_and_type[0][Board::ROOK] | peices_of_color_and_type[1][Board::ROOK]
      | peices_of_color_and_type[0][Board::QUEEN] | peices_of_color_and_type[1][Board::QUEEN]
    ) {
        return false;
    }
    if (std::popcount(peices_of_color_and_type[0][Board::KNIGHT]) + std::popcount(peices_of_color_and_type[0][Board::BISHOP]) > 1) {
        return false;
    }
    return std::popcount(peices_of_color_and_type[0][Board::KNIGHT]) + std::popcount(peices_of_color_and_type[0][Board::BISHOP]) <= 1;
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

void Move::unset_legal_flag() noexcept
{
    data &= ~(LEGAL_FLAG << 12);
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

Move Move::from_long_algebraic(Board &board, std::string long_algebraic)
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
    
    } else if ((board.peices[start] & 0b111) == Board::PAWN && target == board.eligible_en_passant_square()) {
        // en passant
        flags = Move::EN_PASSANT_FLAG;
    
    } else if ((board.peices[start] & 0b111) == Board::KING && (target == start + 2 || start == target + 2)) {
        // caslting
        flags = Move::CASTLE_FLAG;
    }

    return Move(start, target, flags);
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

const Move Move::NULL_MOVE = Move(0, 0, 0);