#include "movegen.h"

#include "../board/zobrist.h"
#include "precomputed.h"

#include <sstream>
#include <stdexcept>
#include <algorithm>

#include <bit>

void movegen::initialize_from_uci_string(Board &board, std::string uci_string)
{
    std::istringstream iss(uci_string);
    std::string word;
    std::vector<std::string> words;

    while (iss >> word) {
        words.push_back(word);
    }

    // check for position command
    if (words[0] != "position") {
        throw new std::invalid_argument("Uci position string should begin with \"position\"");
    }

    // load starting position
    uint32 i;
    if (words[1] == "startpos") {
        board.initialize_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        i = 2;
    } else if (words[1] == "fen" && words.size() >= 8) {
        std::string fen = "";
        for (i = 2; i < 8; i++) {
            fen += " " + words[i];
        }
        board.initialize_from_fen(fen);
    } else {
        throw new std::invalid_argument("Uci position string does not contain valid startpos/fen info");
    }

    // check for moves argument
    if (words.size() >= i && words[i++] != "moves") {
        throw new std::invalid_argument("Uci position string contains invalid moves argument");
    }

    // input moves
    for (; i < words.size(); i++) {
        std::vector<Move> legal_moves = movegen::generate_legal_moves(board);
        Move move = Move::from_long_algebraic(board, words[i]);

        if (std::find(legal_moves.begin(), legal_moves.end(), move) == legal_moves.end()) {
            throw new std::invalid_argument("Uci position string contains invalid/illegal moves argument");
        }
        move.set_legal_flag();
        movegen::make_move(board, move, true);
    }
}

std::vector<Move> movegen::generate_legal_moves(Board &board) noexcept
{
    Move moves[225];
    uint32 end = 0;
    movegen::generate_pseudo_legal_moves(board, moves, end);

    std::vector<Move> legal_moves;

    for (uint32 i = 0; i < end; ++i) {
        if (movegen::is_legal(board, moves[i], true)) {
            legal_moves.push_back(moves[i]);
        }
    }

    return legal_moves;
}

bool movegen::make_move(Board &board, Move &move, bool unsafe) noexcept
{
    if (!unsafe) {
        if (!is_pseudo_legal(board, move)) {
            return false;
        }
        move.unset_legal_flag();
    }
    
    // copy current metedata to next slot
    uint64 *metadata = &board.metadata[(board.halfmove_number + 1) % METADATA_LENGTH];
    *metadata = board.metadata[board.halfmove_number % METADATA_LENGTH];

    // increment halfmove number and zobrist hash for turn
    board.halfmove_number++;
    *metadata ^= ZOBRIST_TURN_KEY;

    // hm since pawn or cap += 1
    *metadata += 1ULL;

    // epsquare = 0
    *metadata &= ~(0b111111ULL << 6);

    uint32 start = move.start_square();
    uint32 target = move.target_square();

    uint64 start_mask = 1ULL << static_cast<uint64>(start);
    uint64 target_mask = 1ULL << static_cast<uint64>(target);

    uint32 moving_peice = board.peices[start];
    uint32 captured_peice = board.peices[target];

    move.store_captured_peice(captured_peice);

    uint32 c = moving_peice >> 4;
    uint32 color = (c + 1) << 3;

    uint32 friendly_back_rank = c * 56;
    uint32 enemy_back_rank = 56 - friendly_back_rank;

    // switch based on move type
    switch (move.data & (0b111 << 15)) {
        case 0: // regular moves

            // Update peices array
            board.peices[start] = 0;
            board.peices[target] = moving_peice;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][moving_peice & 0b111] ^= start_mask | target_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.all_peices ^= start_mask | target_mask;

            *metadata ^= ZOBRIST_PEICE_KEYS[c][moving_peice & 0b111][start];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][moving_peice & 0b111][target];

            // Update enemy peice bitboards and zobrist hash in case of capture
            if (captured_peice) {
                board.peices_of_color_and_type[1 - c][captured_peice & 0b111] ^= target_mask;
                board.peices_of_color[1 - c] ^= target_mask;
                board.all_peices |= target_mask;

                *metadata ^= ZOBRIST_PEICE_KEYS[1 - c][captured_peice & 0b111][target];

                // hm since pawn or cap = 0
                *metadata &= ~0b111111ULL;

            } else if ((moving_peice & 0b111) == Board::PAWN) {
                // hm since pawn or cap = 0
                *metadata &= ~0b111111ULL;
                
                // Check if move is a double jump
                if (target == start + 16 || start == target + 16) {
                    // set epsquare
                    *metadata += static_cast<uint64>(start + target) << 5;
                }
            }

            // update casling rights
            if (board.queenside_castling_rights_not_lost(c) && (start == friendly_back_rank + 4 || start == friendly_back_rank)) {
                // remove queenside castling rights
                *metadata &= ~(static_cast<uint64>(c + 1) << 14);
                *metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
            }
            if (board.kingside_castling_rights_not_lost(c) && (start == friendly_back_rank + 4 || start == friendly_back_rank + 7)) {
                // remove kingside castling rights
                *metadata &= ~(static_cast<uint64>(c + 1) << 12);
                *metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
            }
            if (board.queenside_castling_rights_not_lost(1 - c) && target == enemy_back_rank) {
                // remove queenside castling rights
                *metadata &= ~(static_cast<uint64>(2 - c) << 14);
                *metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[1 - c];
            }
            if (board.kingside_castling_rights_not_lost(1 - c) && target == enemy_back_rank + 7) {
                // remove kingside castling rights
                *metadata &= ~(static_cast<uint64>(2 - c) << 12);
                *metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[1 - c];
            }
            break;

        case (Move::PROMOTION_FLAG << 12): { // promotion

            uint32 promoted_to = move.promoted_to();

            // Update peices array
            board.peices[start] = 0;
            board.peices[target] = color + promoted_to;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::PAWN] ^= start_mask;
            board.peices_of_color_and_type[c][promoted_to] ^= target_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.all_peices ^= start_mask | target_mask;

            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::PAWN][start];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][promoted_to][target];

            // hm since pawn or cap = 0
            *metadata &= ~0b111111ULL;

            // Update enemy peice bitboards and zobrist hash in case of capture
            if (captured_peice) {
                board.peices_of_color_and_type[1 - c][captured_peice & 0b111] ^= target_mask;
                board.peices_of_color[1 - c] ^= target_mask;
                board.all_peices ^= target_mask;

                *metadata ^= ZOBRIST_PEICE_KEYS[1 - c][captured_peice & 0b111][target];
            }

            // update casling rights
            if (board.queenside_castling_rights_not_lost(1 - c) && target == enemy_back_rank) {
                // remove queenside castling rights
                *metadata &= ~(static_cast<uint64>(2 - c) << 14);
                *metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[1 - c];
            }
            if (board.kingside_castling_rights_not_lost(1 - c) && target == enemy_back_rank + 7) {
                // remove kingside castling rights
                *metadata &= ~(static_cast<uint64>(2 - c) << 12);
                *metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[1 - c];
            }
            break;
        }
        case (Move::EN_PASSANT_FLAG << 12): {

            uint32 capture_square = target - 8 + 16 * c;
            uint64 ep_mask = (1ULL << capture_square);

            // Update peices array
            board.peices[start] = 0;
            board.peices[target] = moving_peice;
            board.peices[capture_square] = 0;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::PAWN] ^= start_mask | target_mask;
            board.peices_of_color_and_type[1 - c][Board::PAWN] ^= ep_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.peices_of_color[1 - c] ^= ep_mask;
            board.all_peices ^= start_mask | target_mask | ep_mask;

            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::PAWN][start];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::PAWN][target];
            *metadata ^= ZOBRIST_PEICE_KEYS[1 - c][Board::PAWN][capture_square];

            // hm since pawn or cap = 0
            *metadata &= ~0b111111ULL;
            break;
        }
        case (Move::CASTLE_FLAG << 12): {

            // check if caslting is legal in current position
            if (!move.legal_flag_set() && !movegen::castling_move_is_legal(board, move, true)) {
                board.halfmove_number--;
                return false;
            }
            move.set_legal_flag();

            uint32 rook_start;
            uint32 rook_target;
            uint64 rook_move_mask;

            // check if kingside castling or queenside casling
            if (target > start) {
                // kingside castling
                rook_start = friendly_back_rank + 7;
                rook_target = friendly_back_rank + 5;

                // remove kingside castling rights
                *metadata &= ~(static_cast<uint64>(c + 1) << 12);
                *metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];

                if (board.queenside_castling_rights_not_lost(c)) {
                    // remove queenside castling rights
                    *metadata &= ~(static_cast<uint64>(c + 1) << 14);
                    *metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];
                }

            } else {
                // queenside castling
                rook_start = friendly_back_rank;
                rook_target = friendly_back_rank + 3;

                // remove queenside castling rights
                *metadata &= ~(static_cast<uint64>(c + 1) << 14);
                *metadata ^= ZOBRIST_QUEENSIDE_CASTLING_KEYS[c];

                if (board.kingside_castling_rights_not_lost(c)) {
                    // remove kingside castling rights
                    *metadata &= ~(static_cast<uint64>(c + 1) << 12);
                    *metadata ^= ZOBRIST_KINGSIDE_CASTLING_KEYS[c];
                }
            }
            rook_move_mask = (1ULL << static_cast<uint64>(rook_start)) | (1ULL << static_cast<uint64>(rook_target));

            // Update peices array
            board.peices[start] = 0;
            board.peices[target] = moving_peice;
            board.peices[rook_start] = 0;
            board.peices[rook_target] = color + Board::ROOK;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::KING] ^= start_mask | target_mask;
            board.peices_of_color_and_type[c][Board::ROOK] ^= rook_move_mask;
            board.peices_of_color[c] ^= start_mask | target_mask | rook_move_mask;
            board.all_peices ^= start_mask | target_mask | rook_move_mask;

            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::KING][start];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::KING][target];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::ROOK][rook_start];
            *metadata ^= ZOBRIST_PEICE_KEYS[c][Board::ROOK][rook_target];
            break;
        }
    }

    // verify that the move played was legal
    if (!move.legal_flag_set() && king_attacked(board, c)) {
        movegen::unmake_move(board, move);
        return false;
    }
    move.set_legal_flag();
    return true;
}

void movegen::unmake_move(Board &board, Move &move) noexcept
{
    // update halfmove_number; restores old metadata
    board.halfmove_number--;

    uint32 start = move.start_square();
    uint32 target = move.target_square();

    uint64 start_mask = 1ULL << start;
    uint64 target_mask = 1ULL << target;

    uint32 moving_peice = board.peices[target];
    uint32 captured_peice = move.get_stored_captured_peice();

    uint32 c = moving_peice >> 4;
    uint32 color = (c + 1) << 3;

    // switch based on move type
    switch (move.data & (0b0111000 << 12)) {
        case 0: // regular moves

            // Update peices array
            board.peices[start] = moving_peice;
            board.peices[target] = captured_peice;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][moving_peice & 0b111] ^= start_mask | target_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.all_peices ^= start_mask | target_mask;

            // Update enemy peice bitboards and zobrist hash in case of capture
            if (captured_peice) {
                board.peices_of_color_and_type[1 - c][captured_peice & 0b111] ^= target_mask;
                board.peices_of_color[1 - c] ^= target_mask;
                board.all_peices ^= target_mask;
            }
            break;

        case (Move::PROMOTION_FLAG << 12): { // promotion

            uint32 promoted_to = move.promoted_to();

            // Update peices array
            board.peices[start] = color + Board::PAWN;
            board.peices[target] = captured_peice;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::PAWN] ^= start_mask;
            board.peices_of_color_and_type[c][promoted_to] ^= target_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.all_peices ^= start_mask | target_mask;

            // Update enemy peice bitboards and zobrist hash in case of capture
            if (captured_peice) {
                board.peices_of_color_and_type[1 - c][captured_peice & 0b111] ^= target_mask;
                board.peices_of_color[1 - c] ^= target_mask;
                board.all_peices ^= target_mask;
            }
            break;
        }
        case (Move::EN_PASSANT_FLAG << 12): {

            uint32 capture_square = target - 8 + 16 * c;
            uint64 ep_mask = (1ULL << capture_square);

            // Update peices array
            board.peices[start] = moving_peice;
            board.peices[target] = 0;
            board.peices[capture_square] = ((2 - c) << 3) + Board::PAWN;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::PAWN] ^= start_mask | target_mask;
            board.peices_of_color_and_type[1 - c][Board::PAWN] ^= ep_mask;
            board.peices_of_color[c] ^= start_mask | target_mask;
            board.peices_of_color[1 - c] ^= ep_mask;
            board.all_peices ^= start_mask | target_mask | ep_mask;
            break;
        }
        case (Move::CASTLE_FLAG << 12): {

            uint32 rank = start & 0b111000;
            uint32 rook_start;
            uint32 rook_target;
            uint32 rook_move_mask;

            // check if kingside castling or queenside casling
            if (target > start) {
                // kingside castling
                rook_start = rank + 7;
                rook_target = rank + 5;

            } else {
                // queenside castling
                rook_start = rank;
                rook_target = rank + 3;
            }
            rook_move_mask = (1ULL << rook_start) | (1ULL << rook_target);

            // Update peices array
            board.peices[start] = moving_peice;
            board.peices[target] = 0;
            board.peices[rook_start] = color + Board::ROOK;
            board.peices[rook_target] = 0;
            
            // Update friendly peice bitboards and zobrist hash
            board.peices_of_color_and_type[c][Board::KING] ^= start_mask | target_mask;
            board.peices_of_color_and_type[c][Board::ROOK] ^= rook_move_mask;
            board.peices_of_color[c] ^= start_mask | target_mask | rook_move_mask;
            board.all_peices ^= start_mask | target_mask | rook_move_mask;
            break;
        }
    }
}

bool movegen::generate_pseudo_legal_moves(const Board &board, Move *stack, uint32 &idx, bool ignore_non_captures) noexcept
{
    
    uint32 c = board.halfmove_number % 2;

    // calculate checks and pins
    uint64 checkers, checking_squares, pinned_peices;
    movegen::calculate_checks_and_pins(board, c, checkers, checking_squares, pinned_peices);

    // generate king moves
    uint32 king_index = std::countr_zero(board.peices_of_color_and_type[c][Board::KING]);
    uint64 king_moves = KING_ATTACK_MASK[king_index] & ~(board.peices_of_color[c] | (checking_squares & ~checkers));
    while (king_moves) {
        uint32 target = std::countr_zero(king_moves); // get index of lsb
        king_moves &= king_moves - 1; // clear lsb
        
        stack[idx++] = Move(king_index, target, 0);
    }

    // when double check only king moves valid
    uint32 num_checks = std::popcount(checkers);
    if (num_checks > 1) {
        return true;
    }

    // info for pawn moves
    int32 epsquare = board.eligible_en_passant_square();
    int32 foward = 8 - 16 * c;

    // limited move gen when king in check
    if (num_checks) {

        int32 en_passant_pawn = epsquare - foward;
        bool en_passant_possible = (1ULL << en_passant_pawn) & checkers;

        // pawn moves
        uint64 not_pinned_pawns = board.peices_of_color_and_type[c][Board::PAWN] & ~pinned_peices; // pinned peice cannot defend check
        while (not_pinned_pawns) { // iterate over pawns
            uint32 pawn_index = std::countr_zero(not_pinned_pawns); // get index of lsb
            not_pinned_pawns &= not_pinned_pawns - 1; // clear lsb

            // pawn captures
            uint64 pawn_capture = PAWN_ATTACK_MASK[c][pawn_index] & checkers;
            if (pawn_capture) { // only one possible capture (capture checking peice)
                uint32 target = std::countr_zero(pawn_capture); // get index of lsb

                if (target % 56 < 8) { // pawn is promoting
                    stack[idx++] = Move(pawn_index, target, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::KNIGHT);
                    stack[idx++] = Move(pawn_index, target, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::BISHOP);
                    stack[idx++] = Move(pawn_index, target, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::ROOK);
                    stack[idx++] = Move(pawn_index, target, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::QUEEN);
                } else {
                    stack[idx++] = Move(pawn_index, target, Move::LEGAL_FLAG);
                }
            }
            // en passant capture
            if (en_passant_possible && ((1ULL << epsquare) & PAWN_ATTACK_MASK[c][pawn_index])) {
                stack[idx++] = Move(pawn_index, epsquare, Move::EN_PASSANT_FLAG);
            }

            // pawn non captures
            int32 ahead = static_cast<int32>(pawn_index) + foward;
            if (!board.peices[ahead]) {
                if ((1ULL << ahead) & checking_squares) {
                    if (ahead % 56 < 8) { // pawn is promoting
                        stack[idx++] = Move(pawn_index, ahead, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::KNIGHT);
                        stack[idx++] = Move(pawn_index, ahead, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::BISHOP);
                        stack[idx++] = Move(pawn_index, ahead, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::ROOK);
                        stack[idx++] = Move(pawn_index, ahead, Move::LEGAL_FLAG | Move::PROMOTION_FLAG | Board::QUEEN);
                    } else {
                        stack[idx++] = Move(pawn_index, ahead, Move::LEGAL_FLAG);
                    }
                }

                int32 double_ahead = ahead + foward;
                if ((pawn_index >> 3 == 1 + 5 * c) && !board.peices[double_ahead] && ((1ULL << double_ahead) & checking_squares)) {
                    stack[idx++] = Move(pawn_index, double_ahead, Move::LEGAL_FLAG);
                }
            }

        }

        // knight moves
        uint64 not_pinned_knights = board.peices_of_color_and_type[c][Board::KNIGHT] & ~pinned_peices; // pinned peice cannot defend check
        while (not_pinned_knights) { // iterate over knights
            uint32 knight_index = std::countr_zero(not_pinned_knights); // get index of lsb
            not_pinned_knights &= not_pinned_knights - 1; // clear lsb

            uint64 knight_moves = KNIGHT_ATTACK_MASK[knight_index] & checking_squares;
            while (knight_moves) { // iterate over moves
                uint32 target = std::countr_zero(knight_moves); // get index of lsb
                knight_moves &= knight_moves - 1; // clear lsb

                stack[idx++] = Move(knight_index, target, Move::LEGAL_FLAG);
            }
        }

        // bishop moves
        uint64 not_pinned_bishops = board.peices_of_color_and_type[c][Board::BISHOP] & ~pinned_peices; // pinned peice cannot defend check
        while (not_pinned_bishops) { // iterate over bishops
            uint32 bishop_index = std::countr_zero(not_pinned_bishops); // get index of lsb
            not_pinned_bishops &= not_pinned_bishops - 1; // clear lsb

            uint64 bishop_moves = movegen::bishop_attacks(board, bishop_index) & checking_squares;
            while (bishop_moves) { // iterate over moves
                uint32 target = std::countr_zero(bishop_moves); // get index of lsb
                bishop_moves &= bishop_moves - 1; // clear lsb
                
                stack[idx++] = Move(bishop_index, target, Move::LEGAL_FLAG);
            }
        }

        // rook moves
        uint64 not_pinned_rooks = board.peices_of_color_and_type[c][Board::ROOK] & ~pinned_peices; // pinned peice cannot defend check
        while (not_pinned_rooks) { // iterate over rooks
            uint32 rook_index = std::countr_zero(not_pinned_rooks); // get index of lsb
            not_pinned_rooks &= not_pinned_rooks - 1; // clear lsb

            uint64 rook_moves = movegen::rook_attacks(board, rook_index) & checking_squares;
            while (rook_moves) { // iterate over moves
                uint32 target = std::countr_zero(rook_moves); // get index of lsb
                rook_moves &= rook_moves - 1; // clear lsb
                
                stack[idx++] = Move(rook_index, target, Move::LEGAL_FLAG);
            }
        }

        // queen moves
        uint64 not_pinned_queens = board.peices_of_color_and_type[c][Board::QUEEN] & ~pinned_peices; // pinned peice cannot defend check
        while (not_pinned_queens) { // iterate over queens
            uint32 queen_index = std::countr_zero(not_pinned_queens); // get index of lsb
            not_pinned_queens &= not_pinned_queens - 1; // clear lsb

            uint64 queen_moves = movegen::queen_attacks(board, queen_index) & checking_squares;
            while (queen_moves) { // iterate over moves
                uint32 target = std::countr_zero(queen_moves); // get index of lsb
                queen_moves &= queen_moves - 1; // clear lsb
                
                stack[idx++] = Move(queen_index, target, Move::LEGAL_FLAG);
            }
        }
        return true;
    }

    // non check move generation

    if (ignore_non_captures) {
        movegen::generate_captures(board, stack, idx, pinned_peices);
        return false;
    }

    // generate castling moves
    uint32 back_rank = 56 * c;
    if (board.kingside_castling_rights_not_lost(c) && !(board.all_peices & (0b01100000ULL << back_rank))) {
        
        stack[idx++] = Move(back_rank + 4, back_rank + 6, Move::CASTLE_FLAG);
    }
    if (board.queenside_castling_rights_not_lost(c) && !(board.all_peices & (0b00001110ULL << back_rank))) {
        
        stack[idx++] = Move(back_rank + 4, back_rank + 2, Move::CASTLE_FLAG);
    }

    // pawn moves
    uint64 pawns = board.peices_of_color_and_type[c][Board::PAWN]; // pinned peice cannot defend check
    while (pawns) { // iterate over pawns
        uint32 pawn_index = std::countr_zero(pawns); // get index of lsb
        uint32 legal_flag = ((pawns & -pawns) & pinned_peices) ? 0 : Move::LEGAL_FLAG;
        pawns &= pawns - 1; // clear lsb

        // pawn captures
        uint64 pawn_captures = PAWN_ATTACK_MASK[c][pawn_index] & board.peices_of_color[1 - c];
        while (pawn_captures) {
            uint32 target = std::countr_zero(pawn_captures); // get index of lsb
            pawn_captures &= pawn_captures - 1; // clear lsb

            if (target % 56 < 8) { // pawn is promoting
                stack[idx++] = Move(pawn_index, target, legal_flag | Move::PROMOTION_FLAG | Board::KNIGHT);
                stack[idx++] = Move(pawn_index, target, legal_flag | Move::PROMOTION_FLAG | Board::BISHOP);
                stack[idx++] = Move(pawn_index, target, legal_flag | Move::PROMOTION_FLAG | Board::ROOK);
                stack[idx++] = Move(pawn_index, target, legal_flag | Move::PROMOTION_FLAG | Board::QUEEN);
            } else {
                stack[idx++] = Move(pawn_index, target, legal_flag);
            }
        }
        // en passant capture
        if (epsquare && ((1ULL << epsquare) & PAWN_ATTACK_MASK[c][pawn_index])) {
            stack[idx++] = Move(pawn_index, epsquare, Move::EN_PASSANT_FLAG);
        }

        // pawn non captures
        int32 ahead = static_cast<int32>(pawn_index) + foward;
        if (!board.peices[ahead]) {

            if (ahead % 56 < 8) { // pawn is promoting
                stack[idx++] = Move(pawn_index, ahead, legal_flag | Move::PROMOTION_FLAG | Board::KNIGHT);
                stack[idx++] = Move(pawn_index, ahead, legal_flag | Move::PROMOTION_FLAG | Board::BISHOP);
                stack[idx++] = Move(pawn_index, ahead, legal_flag | Move::PROMOTION_FLAG | Board::ROOK);
                stack[idx++] = Move(pawn_index, ahead, legal_flag | Move::PROMOTION_FLAG | Board::QUEEN);
            } else {
                stack[idx++] = Move(pawn_index, ahead, legal_flag);
            }

            int32 double_ahead = ahead + foward;
            if ((pawn_index >> 3 == 1 + 5 * c) && !board.peices[double_ahead]) {
                stack[idx++] = Move(pawn_index, double_ahead, legal_flag);
            }
        }
    }

    // knight moves
    uint64 knights = board.peices_of_color_and_type[c][Board::KNIGHT];
    while (knights) { // iterate over knights
        uint32 knight_index = std::countr_zero(knights); // get index of lsb
        uint32 legal_flag = ((knights & -knights) & pinned_peices) ? 0 : Move::LEGAL_FLAG;
        knights &= knights - 1; // clear lsb

        uint64 knight_moves = KNIGHT_ATTACK_MASK[knight_index] & ~board.peices_of_color[c];
        while (knight_moves) { // iterate over moves
            uint32 target = std::countr_zero(knight_moves); // get index of lsb
            knight_moves &= knight_moves - 1; // clear lsb

            stack[idx++] = Move(knight_index, target, legal_flag);
        }
    }

    // bishop moves
    uint64 bishops = board.peices_of_color_and_type[c][Board::BISHOP];
    while (bishops) { // iterate over bishops
        uint32 bishop_index = std::countr_zero(bishops); // get index of lsb
        uint32 legal_flag = ((bishops & -bishops) & pinned_peices) ? 0 : Move::LEGAL_FLAG;
        bishops &= bishops - 1; // clear lsb

        uint64 bishop_moves = movegen::bishop_attacks(board, bishop_index) & ~board.peices_of_color[c];
        while (bishop_moves) { // iterate over moves
            uint32 target = std::countr_zero(bishop_moves); // get index of lsb
            bishop_moves &= bishop_moves - 1; // clear lsb

            stack[idx++] = Move(bishop_index, target, legal_flag);
        }
    }

    // rook moves
    uint64 rooks = board.peices_of_color_and_type[c][Board::ROOK];
    while (rooks) { // iterate over rooks
        uint32 rook_index = std::countr_zero(rooks); // get index of lsb
        uint32 legal_flag = ((rooks & -rooks) & pinned_peices) ? 0 : Move::LEGAL_FLAG;
        rooks &= rooks - 1; // clear lsb

        uint64 rook_moves = movegen::rook_attacks(board, rook_index) & ~board.peices_of_color[c];
        while (rook_moves) { // iterate over moves
            uint32 target = std::countr_zero(rook_moves); // get index of lsb
            rook_moves &= rook_moves - 1; // clear lsb

            stack[idx++] = Move(rook_index, target, legal_flag);
        }
    }

    // queen moves
    uint64 queens = board.peices_of_color_and_type[c][Board::QUEEN];
    while (queens) { // iterate over queens
        uint32 queen_index = std::countr_zero(queens); // get index of lsb
        uint32 legal_flag = ((queens & -queens) & pinned_peices) ? 0 : Move::LEGAL_FLAG;
        queens &= queens - 1; // clear lsb

        uint64 queen_moves = movegen::queen_attacks(board, queen_index) & ~board.peices_of_color[c];
        while (queen_moves) { // iterate over moves
            uint32 target = std::countr_zero(queen_moves); // get index of lsb
            queen_moves &= queen_moves - 1; // clear lsb

            stack[idx++] = Move(queen_index, target, legal_flag);
        }
    }

    return false;
}

void movegen::generate_captures(const Board &board, Move *stack, uint32 &idx, uint64 pinned_peices) noexcept
{
    // TODO
}

bool movegen::is_legal(Board &board, Move &move, bool unsafe) noexcept
{
    if (!unsafe) {
        if (!is_pseudo_legal(board, move)) {
            return false;
        }
        move.unset_legal_flag();

    } else if (move.legal_flag_set()) {
        return true;
    }

    if (move.is_castling()) {
        if (movegen::king_attacked(board, board.halfmove_number % 2)) {
            return false;
        }

        return movegen::castling_move_is_legal(board, move, true);
    }

    if (movegen::make_move(board, move, true)) {
        movegen::unmake_move(board, move);
        move.set_legal_flag();
        return true;
    }
    return false;
}

bool movegen::castling_move_is_legal(const Board &board, Move &move, bool unsafe) noexcept
{
    int32 start = move.start_square();
    int32 target = move.target_square();
    uint32 c = start >> 5;

    if (!unsafe) {
        if (!is_pseudo_legal(board, move) || king_attacked(board, c)) {
            return false;
        }
        move.unset_legal_flag();

    } else if (move.legal_flag_set()) {
        return true;
    }

    // Check if anything is attacking squares on king's path
    if (start < target) {
        for (int32 s = start + 1; s <= target; s++) {
            if (attackers(board, s, 1 - c)) {
                return false;
            }
        }
    } else {
        for (int32 s = start - 1; s >= target; s--) {
            if (attackers(board, s, 1 - c)) {
                return false;
            }
        }
    }

    move.set_legal_flag();
    return true;
}

bool movegen::is_pseudo_legal(const Board &board, Move &move) noexcept
{
    if (move.moving_peice(board) >> 4 != board.halfmove_number % 2) {
        // wrong color
        return false;
    }
    uint32 start = move.start_square();
    uint32 target = move.target_square();
    uint32 peice_type = move.moving_peice(board) & 0b111;
    uint32 c = move.moving_peice(board) >> 4;

    // check if a move with a flag set is violating anything
    switch (move.data & (0b111 << 15)) {
    case Move::CASTLE_FLAG << 12: {
        // check is move labeled castling is legal castling move
        uint32 back_rank = 56 * c;
        if (peice_type != Board::KING || (start & 0b111000) != back_rank) {
            return false;
        }

        if (start + 2 == target) {
            // kingside castling
            return board.kingside_castling_rights_not_lost(c) && !(board.all_peices & (0b01100000ULL << back_rank));
        }
        if (start == target + 2) {
            // queenside castling
            return board.queenside_castling_rights_not_lost(c) && !(board.all_peices & (0b00001110ULL << back_rank));
        }
        return false;
    }
    case Move::PROMOTION_FLAG << 12:
        if (peice_type != Board::PAWN || target % 56 >= 8) {
            return false;
        }
        break;
    case Move::EN_PASSANT_FLAG << 12:
        if (peice_type != Board::PAWN || target != board.eligible_en_passant_square()) {
            return false;
        }
        break;
    }

    // general case
    uint64 psuedo_legal_targets = pseudo_moves(board, start);
    return psuedo_legal_targets & (1ULL << target);
}

void movegen::calculate_checks_and_pins(const Board &board, uint32 c, uint64 &checkers, uint64 &checking_squares, uint64 &pinned_peices) noexcept
{
    uint32 e = 1 - c;
    uint32 king_index = std::countr_zero(board.peices_of_color_and_type[c][Board::KING]);

    checkers = 0;
    checking_squares = 0;
    pinned_peices = 0;
    uint64 potential_checker;
    uint64 potential_pinner;

    // pawn checks
    checkers |= PAWN_ATTACK_MASK[c][king_index] & board.peices_of_color_and_type[e][Board::PAWN];

    // knight checks
    checkers |= KNIGHT_ATTACK_MASK[king_index] & board.peices_of_color_and_type[e][Board::KNIGHT];

    // king checks
    checkers |= KING_ATTACK_MASK[king_index] & board.peices_of_color_and_type[e][Board::KING];

    checking_squares = checkers;
    
    // sliding peice checks and pins
    uint64 diagonal_attackers = board.peices_of_color_and_type[e][Board::BISHOP] | board.peices_of_color_and_type[e][Board::QUEEN];
    uint64 straight_attackers = board.peices_of_color_and_type[e][Board::ROOK] | board.peices_of_color_and_type[e][Board::QUEEN];


    if (DIAGONAL_RAYS_MASK[king_index] & diagonal_attackers) {
        // north west checks
        potential_pinner = NW_RAY_MASK[king_index] & board.all_peices;
        potential_checker = potential_pinner & -potential_pinner; // isolate lsb
        if (potential_checker & diagonal_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= NW_RAY_MASK[king_index] ^ NW_RAY_MASK[checking_index];
        }
        // north west pins
        potential_pinner &= potential_pinner - 1; // clear lsb
        potential_pinner &= -potential_pinner; // isolate lsb
        if (potential_pinner & diagonal_attackers) {
            pinned_peices |= potential_checker;
        }

        // north east checks
        potential_pinner = NE_RAY_MASK[king_index] & board.all_peices;
        potential_checker = potential_pinner & -potential_pinner; // isolate lsb
        if (potential_checker & diagonal_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= NE_RAY_MASK[king_index] ^ NE_RAY_MASK[checking_index];
        }
        // north east pins
        potential_pinner &= potential_pinner - 1; // clear lsb
        potential_pinner &= -potential_pinner; // isolate lsb
        if (potential_pinner & diagonal_attackers) {
            pinned_peices |= potential_checker;
        }

        // south east checks
        potential_pinner = SE_RAY_MASK[king_index] & board.all_peices;
        potential_checker =  std::bit_floor(potential_pinner); // isolate lsb
        if (potential_checker & diagonal_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= SE_RAY_MASK[king_index] ^ SE_RAY_MASK[checking_index];
        }
        // south east pins
        potential_pinner &= ~potential_checker; // clear lsb
        potential_pinner = std::bit_floor(potential_pinner); // isolate lsb
        if (potential_pinner & diagonal_attackers) {
            pinned_peices |= potential_checker;
        }

        // south west checks
        potential_pinner = SW_RAY_MASK[king_index] & board.all_peices;
        potential_checker =  std::bit_floor(potential_pinner); // isolate lsb
        if (potential_checker & diagonal_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= SW_RAY_MASK[king_index] ^ SW_RAY_MASK[checking_index];
        }
        // south west pins
        potential_pinner &= ~potential_checker; // clear lsb
        potential_pinner = std::bit_floor(potential_pinner); // isolate lsb
        if (potential_pinner & diagonal_attackers) {
            pinned_peices |= potential_checker;
        }
    }

    if (STRAIGHT_RAYS_MASK[king_index] & straight_attackers) {
        // north checks
        potential_pinner = N_RAY_MASK[king_index] & board.all_peices;
        potential_checker = potential_pinner & -potential_pinner; // isolate lsb
        if (potential_checker & straight_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= N_RAY_MASK[king_index] ^ N_RAY_MASK[checking_index];
        }
        // north pins
        potential_pinner &= potential_pinner - 1; // clear lsb
        potential_pinner &= -potential_pinner; // isolate lsb
        if (potential_pinner & straight_attackers) {
            pinned_peices |= potential_checker;
        }

        // east checks
        potential_pinner = E_RAY_MASK[king_index] & board.all_peices;
        potential_checker = potential_pinner & -potential_pinner; // isolate lsb
        if (potential_checker & straight_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= E_RAY_MASK[king_index] ^ E_RAY_MASK[checking_index];
        }
        // east pins
        potential_pinner &= potential_pinner - 1; // clear lsb
        potential_pinner &= -potential_pinner; // isolate lsb
        if (potential_pinner & straight_attackers) {
            pinned_peices |= potential_checker;
        }

        // south checks
        potential_pinner = S_RAY_MASK[king_index] & board.all_peices;
        potential_checker =  std::bit_floor(potential_pinner); // isolate lsb
        if (potential_checker & straight_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= S_RAY_MASK[king_index] ^ S_RAY_MASK[checking_index];
        }
        // south pins
        potential_pinner &= ~potential_checker; // clear lsb
        potential_pinner = std::bit_floor(potential_pinner); // isolate lsb
        if (potential_pinner & straight_attackers) {
            pinned_peices |= potential_checker;
        }

        // west checks
        potential_pinner = W_RAY_MASK[king_index] & board.all_peices;
        potential_checker =  std::bit_floor(potential_pinner); // isolate lsb
        if (potential_checker & straight_attackers) {
            checkers |= potential_checker;
            uint32 checking_index = std::countr_zero(potential_checker);
            checking_squares |= W_RAY_MASK[king_index] ^ W_RAY_MASK[checking_index];
        }
        // west pins
        potential_pinner &= ~potential_checker; // clear lsb
        potential_pinner = std::bit_floor(potential_pinner); // isolate lsb
        if (potential_pinner & straight_attackers) {
            pinned_peices |= potential_checker;
        }
    }
}

bool movegen::king_attacked(const Board &board, uint32 c) noexcept
{
    uint32 king_index = std::countr_zero(board.peices_of_color_and_type[c][Board::KING]);
    return movegen::attackers(board, king_index, 1 - c);
}

uint64 movegen::pseudo_moves(const Board &board, uint32 index) noexcept
{
    uint32 peice_type = board.peices[index] & 0b111;
    uint32 c = board.peices[index] >> 4;
    uint64 friendly_peices = board.peices_of_color[c];

    switch (peice_type) {
    case Board::PAWN:
        return pawn_pseudo_moves(board, index, c);    
    case Board::KNIGHT:
        return knight_attacks(index) & ~friendly_peices;
    case Board::BISHOP:
        return bishop_attacks(board, index) & ~friendly_peices;
    case Board::ROOK:
        return rook_attacks(board, index) & ~friendly_peices;
    case Board::QUEEN:
        return queen_attacks(board, index) & ~friendly_peices;
    case Board::KING:
        return king_attacks(index) & ~friendly_peices;
    default:
        return 0;
    }
}

uint64 movegen::pawn_pseudo_moves(const Board &board, uint32 index, uint32 c) noexcept
{
    uint32 ep_square = board.eligible_en_passant_square();
    uint64 ep_mask = ep_square ? (1ULL << ep_square) : 0ULL;

    // pawn captures
    uint64 pseudo_moves = PAWN_ATTACK_MASK[c][index] & (board.peices_of_color[1 - c] | ep_mask);

    // pawn non captures
    int32 foward = 8 - 16 * c;
    int32 ahead = static_cast<int32>(index) + foward;
    if (!board.peices[ahead]) {
        pseudo_moves |= (1ULL << ahead);

        int32 double_ahead = ahead + foward;
        if ((index >> 3 == 1 + 5 * c) && !board.peices[double_ahead]) {
            pseudo_moves |= (1ULL << double_ahead);
        }
    }
    return pseudo_moves;
}

uint64 movegen::knight_attacks(uint32 index) noexcept
{
    return KNIGHT_ATTACK_MASK[index];
}

uint64 movegen::bishop_attacks(const Board &board, uint32 index) noexcept
{
    uint64 attacks = 0;
    uint64 blockers;
    uint32 blocker_index;

    // north west attacks
    blockers = NW_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(blockers); // index of lsb
    attacks |= NW_RAY_MASK[index] ^ NW_RAY_MASK[blocker_index];

    // north east attacks
    blockers = NE_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(blockers); // index of lsb
    attacks |= NE_RAY_MASK[index] ^ NE_RAY_MASK[blocker_index];

    // south east attacks
    blockers = SE_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(std::bit_floor(blockers)); // index of msb
    attacks |= SE_RAY_MASK[index] ^ SE_RAY_MASK[blocker_index];

    // south west attacks
    blockers = SW_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(std::bit_floor(blockers)); // index of msb
    attacks |= SW_RAY_MASK[index] ^ SW_RAY_MASK[blocker_index];

    return attacks;
}

uint64 movegen::rook_attacks(const Board &board, uint32 index) noexcept
{
    uint64 attacks = 0;
    uint64 blockers;
    uint32 blocker_index;

    // north attacks
    blockers = N_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(blockers); // index of lsb
    attacks |= N_RAY_MASK[index] ^ N_RAY_MASK[blocker_index];

    // east attacks
    blockers = E_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(blockers); // index of lsb
    attacks |= E_RAY_MASK[index] ^ E_RAY_MASK[blocker_index];

    // south attacks
    blockers = S_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(std::bit_floor(blockers)); // index of msb
    attacks |= S_RAY_MASK[index] ^ S_RAY_MASK[blocker_index];

    // west attacks
    blockers = W_RAY_MASK[index] & board.all_peices;
    blocker_index = std::countr_zero(std::bit_floor(blockers)); // index of msb
    attacks |= W_RAY_MASK[index] ^ W_RAY_MASK[blocker_index];

    return attacks;
}

uint64 movegen::queen_attacks(const Board &board, uint32 index) noexcept
{
    return bishop_attacks(board, index) | rook_attacks(board, index);
}

uint64 movegen::king_attacks(uint32 index) noexcept
{
    return KING_ATTACK_MASK[index];
}

uint64 movegen::attackers(const Board &board, uint32 index, uint32 c) noexcept
{
    uint64 attackers = 0;
    
    // pawn attacks
    attackers |= PAWN_ATTACK_MASK[1 - c][index] & board.peices_of_color_and_type[c][Board::PAWN];

    // knight attacks
    attackers |= KNIGHT_ATTACK_MASK[index] & board.peices_of_color_and_type[c][Board::KNIGHT];

    // king attacks
    attackers |= KING_ATTACK_MASK[index] & board.peices_of_color_and_type[c][Board::KING];
    
    // sliding peice attacks
    uint64 diagonal_attackers = board.peices_of_color_and_type[c][Board::BISHOP] | board.peices_of_color_and_type[c][Board::QUEEN];
    uint64 straight_attackers = board.peices_of_color_and_type[c][Board::ROOK] | board.peices_of_color_and_type[c][Board::QUEEN];
    uint64 potential_attacker;

    // attacks from north west
    potential_attacker = NW_RAY_MASK[index] & board.all_peices;
    potential_attacker &= -potential_attacker; // isolate lsb
    attackers |= potential_attacker & diagonal_attackers;

    // attacks from north east
    potential_attacker = NE_RAY_MASK[index] & board.all_peices;
    potential_attacker &= -potential_attacker; // isolate lsb
    attackers |= potential_attacker & diagonal_attackers;

    // attacks from south east
    potential_attacker = SE_RAY_MASK[index] & board.all_peices;
    potential_attacker = std::bit_floor(potential_attacker); // isolate msb
    attackers |= potential_attacker & diagonal_attackers;

    // attacks from south west
    potential_attacker = SW_RAY_MASK[index] & board.all_peices;
    potential_attacker = std::bit_floor(potential_attacker); // isolate msb
    attackers |= potential_attacker & diagonal_attackers;

    // attacks from north
    potential_attacker = N_RAY_MASK[index] & board.all_peices;
    potential_attacker &= -potential_attacker; // isolate lsb
    attackers |= potential_attacker & straight_attackers;

    // attacks from east
    potential_attacker = E_RAY_MASK[index] & board.all_peices;
    potential_attacker &= -potential_attacker; // isolate lsb
    attackers |= potential_attacker & straight_attackers;

    // attacks from south
    potential_attacker = S_RAY_MASK[index] & board.all_peices;
    potential_attacker = std::bit_floor(potential_attacker); // isolate msb
    attackers |= potential_attacker & straight_attackers;

    // attacks from west
    potential_attacker = W_RAY_MASK[index] & board.all_peices;
    potential_attacker = std::bit_floor(potential_attacker); // isolate msb
    attackers |= potential_attacker & straight_attackers;

    return attackers;
}
