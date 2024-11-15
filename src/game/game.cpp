#include "game.h"

#include <sstream>
#include <chrono>
#ifdef _WIN32
    #include <ctime>
#endif
#include <iomanip>

#include "../movegen/movegen.h"

Game::Game()
{
    initialize_starting_position();
}

void Game::initialize_starting_position() noexcept
{
    initialize_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Game::initialize_from_fen(std::string fen)
{
    // initialize board members
    movegen::initialize_from_fen(*this, fen);

    // reset game members
    game_moves.clear();
    game_moves_in_algebraic.clear();
    beginning_fen = fen;
    generate_legal_moves();
}

void Game::from_uci_string(std::string uci_string)
{
    // initialize board members
    movegen::initialize_from_uci_string(*this, uci_string);

    // reset game members
    game_moves.clear();
    game_moves_in_algebraic.clear();
    beginning_fen = as_fen();
    generate_legal_moves();
}

bool Game::white_to_move() const noexcept
{
    return halfmove_number % 2 == 0;
}

std::vector<Move> Game::get_legal_moves() const noexcept
{
    return current_legal_moves;
}

bool Game::is_legal(Move move) const noexcept
{
    for (const Move &legal_move : current_legal_moves) {
        if (move == legal_move) {
            return true;
        }
    }
    return false;
}

bool Game::input_move(Move move) noexcept
{
    if (!is_legal(move)) {
        return false;
    }

    game_moves.push_back(move);

    // algebraic notation
    // castling case
    if (move.is_castling()) {
        game_moves_in_algebraic.push_back((move.target_square() < move.start_square()) ? "O-O-O" : "O-O");
        movegen::make_move(*this, move);
        generate_legal_moves();
        return true;
    }

    std::string algebraic = "";

    if ((move.moving_peice(*this) & 0b111) == Board::PAWN) {
        // pawn moves dont require peice indicator unless capture
        if (peices[move.target_square()] || move.is_en_passant()) {
            algebraic += board_helpers::board_index_to_algebraic_notation(move.start_square())[0];
        }

    } else {
        // add peice indicator
        char peice_indentifiers[] = { '\0', '\0', 'N', 'B', 'R', 'Q', 'K' };
        algebraic += peice_indentifiers[move.moving_peice(*this) & 0b111];

        // add additional peice indicator in case of disambiguation
        std::vector<uint32> possible_start_squares;
        for (const Move& other_move : current_legal_moves) {
            if (other_move.moving_peice(*this) == move.moving_peice(*this)
                && other_move.target_square() == move.target_square()
                && other_move.start_square() != move.start_square()) {

                possible_start_squares.push_back(other_move.start_square());
            }
        }
        if (!possible_start_squares.empty()) {
            uint32 file = move.start_square() & 0b111;
            uint32 rank = move.start_square() >> 3;

            std::string start_square_algebraic = board_helpers::board_index_to_algebraic_notation(move.start_square());

            // Only include rank or file if possible
            if (std::find_if(possible_start_squares.begin(), possible_start_squares.end(), [&](uint32 other_square) {return (other_square & 0b111) == file;}) == possible_start_squares.end()) {
                algebraic += start_square_algebraic[0];
            }
            else if (std::find_if(possible_start_squares.begin(), possible_start_squares.end(), [&](uint32 other_square) {return (other_square >> 3) == rank;}) == possible_start_squares.end()) {
                algebraic += start_square_algebraic[1];
            }
            else {
                algebraic += start_square_algebraic;
            }
        }
    }

    // add capture indicator
    if (peices[move.target_square()] || move.is_en_passant()) {
        algebraic += 'x';
    }

    // add target square
    algebraic += board_helpers::board_index_to_algebraic_notation(move.target_square());

    // add promotion
    if (move.is_promotion()) {
        char algebraicPeiceIndentifiers[] = { '\0', '\0', 'N', 'B', 'R', 'Q' };
        algebraic += '=';
        algebraic += algebraicPeiceIndentifiers[move.promoted_to()];
    }

    // make move before checking if in check
    movegen::make_move(*this, move);
    generate_legal_moves();

    // add check/mate indicator
    if (in_check()) {
        algebraic += current_legal_moves.empty() ? '#' : '+';
    }

    game_moves_in_algebraic.push_back(algebraic);
    return true;
}

bool Game::input_move(std::string long_algebraic) noexcept
{
    Move move;
    try {
        move = board_helpers::long_algebraic_to_move(*this, long_algebraic);
    } catch (std::invalid_argument &e) {
        return false;
    }
    return input_move(move);
}

bool Game::in_check() const noexcept
{
    return movegen::king_attacked(*this, halfmove_number % 2);
}

std::string Game::as_pgn(std::map<std::string, std::string> headers)
{
    std::string pgn;

    // Add default headers
    // Event
    if (headers.find("Event") == headers.end()) {
        pgn += "[Event \"??\"]\n";
    }
    else {
        pgn += "[Event \"" + headers["Event"] + "\"]\n";
        headers.erase("Event");
    }

    // Date
    if (headers.find("Date") == headers.end()) {
        // Get the current time
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        // Convert to a tm structure
        std::tm tm;
#ifdef _WIN32
        // Use localtime_s on Windows (thread-safe version)
        localtime_s(&tm, &time);
#else
        // Use localtime on other platforms (not thread-safe)
        std::tm* tmptr = std::localtime(&time);
        if (tmptr) {
            tm = *tmptr;  // Copy the contents to tm
        }
#endif
        // Format date as "YYYY.MM.DD"
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y.%m.%d");

        pgn += "[Date \"" + oss.str() + "\"]\n";
    }
    else {
        pgn += "[Date \"" + headers["Date"] + "\"]\n";
        headers.erase("Date");
    }

    // White
    if (headers.find("White") == headers.end()) {
        pgn += "[White \"??\"]\n";
    }
    else {
        pgn += "[White \"" + headers["White"] + "\"]\n";
        headers.erase("White");
    }

    // Black
    if (headers.find("Black") == headers.end()) {
        pgn += "[Black \"??\"]\n";
    }
    else {
        pgn += "[Black \"" + headers["Black"] + "\"]\n";
        headers.erase("Black");
    }

    // Termination
    if (headers.find("Termination") == headers.end()) {
        std::string termination = current_legal_moves.empty() ? "Normal" : "Forfeit";
        pgn += "[Termination \"" + termination + "\"]\n";
    }
    else {
        pgn += "[Termination \"" + headers["Termination"] + "\"]\n";
        headers.erase("Termination");
    }

    // Result
    std::string result;
    if (headers.find("Result") == headers.end()) {
        if (current_legal_moves.empty() && !in_check()) {
            result = "1/2-1/2";
        } else {
            result = white_to_move() ? "0-1" : "1-0";
        }
        pgn += "[Result \"" + result + "\"]\n";
    }
    else {
        result = headers["Result"];
        pgn += "[Result \"" + result + "\"]\n";
        headers.erase("Result");
    }

    // Add headers if position originated from FEN
    if (beginning_fen != "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") {
        // SetUp
        pgn += "[SetUp \"1\"]\n";

        // FEN
        pgn += "[FEN \"" + beginning_fen + "\"]\n";
    } else {
        // SetUp
        pgn += "[SetUp \"0\"]\n";
    }
    headers.erase("SetUp");
    headers.erase("FEN");

    // other headers
    for (const auto& header : headers) {
        pgn += "[" + header.first + " \"" + header.second + "\"]\n";
    }

    pgn += "\n";

    int i = 0;
    for (const std::string& move : game_moves_in_algebraic) {
        if (i % 2 == 0) {
            // Add move number before White's move
            pgn += std::to_string(i / 2 + 1) + ". ";
        }
        ++i;
        pgn += move + " ";
    }
    pgn += result + "\n\n";

    return pgn;
}

void Game::generate_legal_moves() noexcept
{
    // empty vector if draw
    if (movegen::is_draw_by_fifty_move_rule(*this) || movegen:: is_draw_by_repitition(*this) || movegen::is_draw_by_insufficient_material(*this)) {
        current_legal_moves = std::vector<Move>();
    } else {
        current_legal_moves = movegen::generate_legal_moves(*this);
    }
}
