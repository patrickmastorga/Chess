#include "data_stream.h"

#include <cstring>
#include <stdexcept>
#include <bit>
#include <string>
#include <iostream>
#include <iomanip>

#include "../../movegen/movegen.h"
#include "../../movegen/zobrist.h"

#define ENTRY_SKIPPED 32002

BinpackTrainingDataStream::BinpackTrainingDataStream(std::filesystem::path path, float drop, size_t worker_id, size_t num_workers, size_t buffer_size) :
    drop(drop),
    worker_id(worker_id),
    num_workers(num_workers),
    file(path, std::ios::binary),
    buffer_size(buffer_size),
    block_num(0),
    entry_num(0),
    block_size(0),
    byte_index(0),
    bits_remaining(8),
    plies_remaining(0)
{
    if (!file.is_open()) {
        throw new std::runtime_error("Could not open file!");
    }

    buffer = new uint8[buffer_size];

    advance_blocks(worker_id + 1);
}

BinpackTrainingDataStream::~BinpackTrainingDataStream()
{
    delete[] buffer;
}

bool BinpackTrainingDataStream::get_next_entry()
{
    do {
        entry_num++;
        if (plies_remaining) {
            // read next move in the movetext
            read_movetext_entry();

        } else if (!read_stem()) {
            // read next new position
            return false;
        }
    } while (entry.score == ENTRY_SKIPPED);

    return true;
}

size_t BinpackTrainingDataStream::read_block_header()
{
    uint8 header[8];

    // read next block header
    if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) {
        throw new std::runtime_error("Unexpected end of file");
    }
    
    // validate header
    if (header[0] != 'B' || header[1] != 'I' || header[2] != 'N' || header[3] != 'P')
    {
        throw new std::runtime_error("Invalid binpack file or chunk.");
    }

    uint32 next_block_size = 
        header[4]
        | (header[5] << 8)
        | (header[6] << 16)
        | (header[7] << 24);
    return next_block_size;
}

bool BinpackTrainingDataStream::advance_blocks(size_t num_blocks)
{
    block_num += num_blocks;
    std::cout << "Block num: " << std::right << std::setw(6) << block_num << " Entry num: " << std::setw(16) << entry_num << std::endl;
    // Advance past blocks not belonging to this worker
    for (size_t i = 0; i < num_blocks - 1; i++) {
        if (file.peek() == std::ifstream::traits_type::eof()) {
            // no more blocks remaining
            return false;
        }
        // read next block header
        size_t next_block_size = read_block_header();
        // advance past block
        file.seekg(next_block_size, std::ios::cur);
    }

    // load next block into buffer
    if (file.peek() == std::ifstream::traits_type::eof()) {
        // no more blocks remaining
        return false;
    }
    // read next block header
    size_t next_block_size = read_block_header();
    if (next_block_size > buffer_size) {
        throw new std::runtime_error("Set buffer_size to something big enough to hold the entire block!");
    }
    // load block into buffer
    block_size = next_block_size;
    if (!file.read(reinterpret_cast<char*>(buffer), block_size)) {
        throw new std::runtime_error("Unexpected end of file");
    }
    byte_index = 0;
    bits_remaining = 8;
    plies_remaining = 0;

    return true;
}

bool BinpackTrainingDataStream::data_available()
{
    return byte_index < block_size;
}

bool BinpackTrainingDataStream::read_stem()
{
    // Advance to next whole byte
    if (bits_remaining < 8) {
        bits_remaining = 8;
        byte_index++;
    }

    // get next block if needed
    if (!data_available() && !advance_blocks(num_workers)) {
        return false;
    }

    // read position into entry.position based on format:
    // https://github.com/Sopel97/nnue_data_compress/blob/master/src/chess/Position.h#L1166
    // reset current members
    for (int32 i = 0; i < 64; i++) {
        entry.position.peices[i] = 0;
    }
    for (int32 i = 0; i < METADATA_LENGTH; i++) {
        entry.position.metadata[i] = 0ULL;
    }
    for (int32 i = 0; i < 7; i++) {
        entry.position.peices_of_color_and_type[0][i] = 0ULL;
        entry.position.peices_of_color_and_type[1][i] = 0ULL;
    }
    entry.position.peices_of_color[0] = 0ULL;
    entry.position.peices_of_color[1] = 0ULL;
    entry.position.all_peices = 0ULL;

    uint64 metadata = 0;
    bool black_to_move = false;

    // read the occupied bitboard from the buffer
    uint64 occupied =
        static_cast<uint64>(buffer[byte_index])     << 56 |
        static_cast<uint64>(buffer[byte_index + 1]) << 48 |
        static_cast<uint64>(buffer[byte_index + 2]) << 40 |
        static_cast<uint64>(buffer[byte_index + 3]) << 32 |
        static_cast<uint64>(buffer[byte_index + 4]) << 24 |
        static_cast<uint64>(buffer[byte_index + 5]) << 16 |
        static_cast<uint64>(buffer[byte_index + 6]) << 8  |
        static_cast<uint64>(buffer[byte_index + 7]);
    byte_index += 8;

    // iterate over each index in the occupied bitboard
    for (uint32 i = 0; occupied; i++) {
        uint32 index = std::countr_zero(occupied); // get index of lsb
        occupied &= occupied - 1; // clear lsb

        // get nibble from buffer
        uint8 nibble;
        if (i % 2 == 0) {
            nibble = buffer[byte_index + (i / 2)] & 0x0F;
        } else {
            nibble = (buffer[byte_index + (i / 2)] & 0xF0) >> 4;
        }

        // non special values
        if (nibble < 12) {
            uint32 c = nibble % 2;
            uint32 color = (c + 1) << 3;
            uint32 peice_type = (nibble / 2) + 1;

            entry.position.peices[index] = color + peice_type;
            entry.position.peices_of_color_and_type[c][peice_type] |= (1ULL << index);
            metadata ^= ZOBRIST_PEICE_KEYS[c][peice_type][index];
            continue;
        }

        // special values
        switch (nibble) {
        case 12: {
            // pawn with epsquare behind it
            uint32 c = (index >> 5) & 1;
            uint32 color = (c + 1) << 3;
            uint32 epsquare = index - 8 + 16 * c;
            metadata |= epsquare << 6;

            entry.position.peices[index] = color + Board::PAWN;
            entry.position.peices_of_color_and_type[c][Board::PAWN] |= (1ULL << index);
            metadata ^= ZOBRIST_PEICE_KEYS[c][Board::PAWN][index];
            break;
        }
        case 13:
        case 14: {
            // white/black rook with castling rights
            uint32 c = (nibble - 1) % 2;
            uint32 color = (c + 1) << 3;

            if (index % 8 == 0) {
                // queenside rights
                metadata |= 0b0100ULL << (12 + c);
                metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
            } else {
                // kingside rights
                metadata |= 0b0001ULL << (12 + c);
                metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
            }

            entry.position.peices[index] = color + Board::ROOK;
            entry.position.peices_of_color_and_type[c][Board::ROOK] |= (1ULL << index);
            metadata ^= ZOBRIST_PEICE_KEYS[c][Board::ROOK][index];
            break;
        }
        case 15: {
            // black king and black is side to move
            black_to_move = true;
            metadata ^= ZOBRIST_TURN_KEY;

            entry.position.peices[index] = Board::BLACK + Board::KING;
            entry.position.peices_of_color_and_type[1][Board::KING] |= (1ULL << index);
            metadata ^= ZOBRIST_PEICE_KEYS[1][Board::KING][index];
            break;
        }
        default:
            throw std::runtime_error("Unrecognised nibble!");
        }
    }
    byte_index += 16;

    // Initialize bitboards
    for (int32 i = 0; i < 7; i++) {
        entry.position.peices_of_color[0] |= entry.position.peices_of_color_and_type[0][i];
        entry.position.peices_of_color[1] |= entry.position.peices_of_color_and_type[1][i];
    }
    entry.position.all_peices = entry.position.peices_of_color[0] | entry.position.peices_of_color[1];

    // read move into entry.move based on format:
    // https://github.com/Sopel97/nnue_data_compress/blob/master/src/chess/Chess.h#L1044
    // clear move data
    entry.move.data = 0;

    uint16 compressed_move = 
        static_cast<uint16>(buffer[byte_index]) << 8 |
        static_cast<uint16>(buffer[byte_index + 1]);
    byte_index += 2;

    uint32 start_square = (compressed_move >> 8) & 0b111111;
    uint32 target_square = (compressed_move >> 2) & 0b111111;

    // switch on move type
    switch (compressed_move >> 14) {
    case 1: {
        // promotion
        entry.move.data |= Move::PROMOTION_FLAG << 12;
        uint32 promotion_value = (compressed_move & 0b11) + Board::KNIGHT;
        entry.move.data |= promotion_value << 12;
        break;
    }
    case 2: 
        // castling
        entry.move.data |= Move::CASTLE_FLAG << 12;
        // fix discrepancy in how start/from for castling moves are stored
        if (target_square < start_square) {
            target_square = start_square - 2;
        } else {
            target_square = start_square + 2;
        }
        break;
    case 3: 
        // en passant
        entry.move.data |= Move::EN_PASSANT_FLAG << 12;
        break;
    }

    // fill move from/to
    entry.move.data |= start_square;
    entry.move.data |= target_square << 6;

    // get score from buffer
    entry.score = unsigned_to_signed(
        static_cast<uint16>(buffer[byte_index]) << 8 |
        static_cast<uint16>(buffer[byte_index + 1])
    );
    byte_index += 2;

    // get ply/result from buffer
    uint16 ply_and_result = 
        static_cast<uint16>(buffer[byte_index]) << 8 |
        static_cast<uint16>(buffer[byte_index + 1]);
    byte_index += 2;
    
    // bottom 14 bits are ply
    uint16 ply = ply_and_result & 0x3FFF;
    if (black_to_move && ply % 2 == 0) {
        ply++;
    }
    entry.position.halfmove_number = ply;

    // top 2 bits are result
    entry.result = unsigned_to_signed(ply_and_result >> 14);

    // get 50 move counter from buffer
    uint16 fiftymove_counter = 
        static_cast<uint16>(buffer[byte_index]) << 8 |
        static_cast<uint16>(buffer[byte_index + 1]);
    byte_index += 2;

    // set metadata
    metadata |= fiftymove_counter;
    entry.position.metadata[ply % METADATA_LENGTH] = metadata;

    // check if move is legal in current position
    if (!movegen::is_legal(entry.position, entry.move)) {
        std::string fen = entry.position.as_fen();
        throw new std::runtime_error("Read move is not legal in the current position!");
    }

    // read number of plies in the move text
    plies_remaining = 
        static_cast<size_t>(buffer[byte_index]) << 8 |
        static_cast<size_t>(buffer[byte_index + 1]);
    byte_index += 2;

    return true;
}

void BinpackTrainingDataStream::read_movetext_entry()
{
    // input move on position
    //if (block_num == 215 && entry_num > 85097480) {
    //    std::string fen = entry.position.as_fen();
    //    std::cout << fen << " move " << entry.move.as_long_algebraic() << " score " << entry.score << std::endl;
    //    int j = 0;
    //}
    plies_remaining--;
    movegen::make_move(entry.position, entry.move);
    entry.move = read_vle_move();
    entry.score = -entry.score + unsigned_to_signed(read_vle_int());
    entry.result = -entry.result;
}

uint16 BinpackTrainingDataStream::read_vle_int()
{
    uint16 mask = 0b1111;
    uint16 value = 0;
    for(size_t offset = 0;; offset += 4)
    {
        uint16 block = static_cast<uint16>(read_bits(5));
        value |= ((block & mask) << offset);

        // continue of extension bit is set 
        if (!(block >> 4)) break;
    }
    return value;
}

Move BinpackTrainingDataStream::read_vle_move()
{
    uint32 start_square, target_square, flags;
    uint8 peice_id, move_id;
    uint64 friendly_peices = entry.position.peices_of_color[entry.position.halfmove_number % 2];
    
    // get move start square
    uint32 num_peices = std::popcount(friendly_peices);
    peice_id = read_bits(std::bit_width(num_peices - 1));
    start_square = index_of_nth_set_bit(friendly_peices, peice_id);

    uint64 possible_destinations = movegen::pseudo_moves(entry.position, start_square);
    uint32 num_moves;
    uint32 peice_type = entry.position.peices[start_square] & 0b111;

    // switch on peice type
    switch (peice_type) {
    case Board::PAWN: {
        constexpr uint64 promoting_squares = (0xFFULL << 56) | 0xFFULL;
        if (possible_destinations & promoting_squares) {
            // move is a promotion
            num_moves = 4 * std::popcount(possible_destinations);
            move_id = read_bits(std::bit_width(num_moves - 1));

            target_square = index_of_nth_set_bit(possible_destinations, move_id / 4);
            flags = Move::PROMOTION_FLAG | (move_id % 4 + Board::KNIGHT);
            break;
        }
        // move is not a promotion
        uint32 ep_square = entry.position.eligible_en_pasant_square();

        // possible fix to weird quirk
        if (ep_square && (1ULL << ep_square) & possible_destinations) {
            // ignore ep move if it is illegal
            Move ep_move(start_square, ep_square, Move::EN_PASSANT_FLAG);
            if (!movegen::is_legal(entry.position, ep_move, true)) {
                possible_destinations &= ~(1ULL << ep_square);
            }
        }

        num_moves = std::popcount(possible_destinations);
        move_id = read_bits(std::bit_width(num_moves - 1));

        target_square = index_of_nth_set_bit(possible_destinations, move_id);
        flags = target_square == ep_square ? Move::EN_PASSANT_FLAG : 0;
        break;
    }
    case Board::KING: {
        uint32 c = entry.position.peices[start_square] >> 4;
        uint32 num_castlings = entry.position.kingside_castling_rights_not_lost(c) + entry.position.queenside_castling_right_not_lost(c);
        num_moves = std::popcount(possible_destinations);
        move_id = read_bits(std::bit_width(num_moves + num_castlings - 1));

        if (move_id >= num_moves) {
            // move is castling
            flags = Move::CASTLE_FLAG;
            move_id -= num_moves;
            if (move_id || !entry.position.queenside_castling_right_not_lost(c)) {
                // short castling
                target_square = start_square + 2;
            } else {
                // long castling
                target_square = start_square - 2;
            }
            break;
        }
        // move is not castling
        target_square = index_of_nth_set_bit(possible_destinations, move_id);
        flags = 0;
        break;
    }
    case Board::KNIGHT:
    case Board::BISHOP:
    case Board::ROOK:
    case Board::QUEEN:
        num_moves = std::popcount(possible_destinations);
        move_id = read_bits(std::bit_width(num_moves - 1));

        target_square = index_of_nth_set_bit(possible_destinations, move_id);
        flags = 0;
        break;
    default:
        throw new std::runtime_error("No peice at move start square!");
    }

    Move move(start_square, target_square, flags);
    if (!movegen::is_legal(entry.position, move)) {
        std::string fen = entry.position.as_fen();
        throw new std::runtime_error("Generated move is not legal in the current position!");
    }
    return move;
}

uint8 BinpackTrainingDataStream::read_bits(size_t num_bits)
{
    if (num_bits == 0) return 0;

    if (bits_remaining == 8 && !data_available()) {
        throw new std::runtime_error("Not enough bits left!");
    }

    // get most significant bits from current byte in buffer
    uint8 byte = buffer[byte_index] << (8 - bits_remaining);
    uint8 bits = (byte) >> (8 - num_bits);

    // if needed, get less significant bits from the next byte in the buffer
    if (num_bits > bits_remaining) {
        if (!data_available()) {
            throw new std::runtime_error("Not enough bits left!");
        }
        
        size_t spill_count = num_bits - bits_remaining;
        bits |= buffer[byte_index + 1] >> (8 - spill_count);

        bits_remaining += 8;
        byte_index++;
    }

    bits_remaining -= num_bits;

    if (bits_remaining == 0) {
        byte_index++;
        bits_remaining = 8;
    }
    return bits;
}

int16 BinpackTrainingDataStream::unsigned_to_signed(uint16 val)
{
    int16 sval;
    // move sign bit to msb
    val = (val << 15) | (val >> 1);
    // flip bits if negative
    if (val & 0x8000)
    {
        val ^= 0x7FFF;
    }
    std::memcpy(&sval, &val, 2);
    return sval;
}

uint32 BinpackTrainingDataStream::index_of_nth_set_bit(uint64 val, size_t n)
{
    // clear lsb n times
    for (; n; n--) {
        val &= val - 1;
    }
    if (!val) {
        throw new std::runtime_error("There must be at least n + 1 bits set!");
    }
    return std::countr_zero(val);
}
