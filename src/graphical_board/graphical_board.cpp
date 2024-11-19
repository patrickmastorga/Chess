#include "graphical_board.h"

#include <stdexcept>
#include <filesystem>

#define LIGHT_SQUARE_COLOR sf::Color(0xf0, 0xd9, 0xb5)
#define DARK_SQUARE_COLOR sf::Color(0xb5, 0x88, 0x63)

#define LIGHT_SELECTED_SQUARE sf::Color(0xdc, 0xc3, 0x4b)
#define DARK_SELECTED_SQUARE LIGHT_SELECTED_SQUARE

#define LIGHT_AVAILABLE_TARGET LIGHT_SQUARE_COLOR * sf::Color(210, 210, 200)
#define DARK_AVAILABLE_TARGET DARK_SQUARE_COLOR * sf::Color(200, 200, 200)

#define LIGHT_PREVIOUS_MOVE sf::Color(0xA0, 0xD0, 0xE0) * sf::Color(200, 200, 200)
#define DARK_PREVIOUS_MOVE LIGHT_PREVIOUS_MOVE

#define IS_LIGHT_SQUARE(x) (((x) + (x) / 8) % 2 == 0)

#define COLOR_TO_MOVE(hm) (((((hm) % 2) + 1) << 3))

GraphicalBoard::GraphicalBoard(sf::Vector2f position) noexcept : position(position), white_on_bottom(true), selected_square(-1)
{
    // get the full path of the folder containing the peice textures
    std::filesystem::path source_path = __FILE__;
    std::filesystem::path source_directory = source_path.parent_path();
    std::filesystem::path peice_texture_folder = source_directory / "assets/120px";

    // load white textures
    peice_textures[9].loadFromFile(peice_texture_folder.string() + "/white_pawn.png");
    peice_textures[10].loadFromFile(peice_texture_folder.string() + "/white_knight.png");
    peice_textures[11].loadFromFile(peice_texture_folder.string() + "/white_bishop.png");
    peice_textures[12].loadFromFile(peice_texture_folder.string() + "/white_rook.png");
    peice_textures[13].loadFromFile(peice_texture_folder.string() + "/white_queen.png");
    peice_textures[14].loadFromFile(peice_texture_folder.string() + "/white_king.png");

    // load black textures
    peice_textures[17].loadFromFile(peice_texture_folder.string() + "/black_pawn.png");
    peice_textures[18].loadFromFile(peice_texture_folder.string() + "/black_knight.png");
    peice_textures[19].loadFromFile(peice_texture_folder.string() + "/black_bishop.png");
    peice_textures[20].loadFromFile(peice_texture_folder.string() + "/black_rook.png");
    peice_textures[21].loadFromFile(peice_texture_folder.string() + "/black_queen.png");
    peice_textures[22].loadFromFile(peice_texture_folder.string() + "/black_king.png");
}

void GraphicalBoard::initialize_from_fen(std::string fen)
{
    reset_selection();
    Game::initialize_from_fen(fen);
}

void GraphicalBoard::from_uci_string(std::string uci_string)
{
    reset_selection();
    Game::from_uci_string(uci_string);
}

bool GraphicalBoard::attempt_selection(sf::Vector2f board_position) noexcept
{
    reset_selection();
    if (!WITHIN_BOARD_BOUNDS(board_position)) {
        return false;
    }
    uint32 index = board_position_to_index(board_position);

    if (!(peices[index] & COLOR_TO_MOVE(halfmove_number))) {
        return false;
    }

    selected_square = index;

    for (const Move &move : get_legal_moves()) {
        if (move.start_square() == index) {
            selected_moves.push_back(move);
        }
    }
    return true;
}

int32 GraphicalBoard::get_selected_square() const noexcept
{
    return selected_square;
}

void GraphicalBoard::reset_selection() noexcept
{
    selected_moves.clear();
    selected_square = -1;
}

bool GraphicalBoard::attempt_move(sf::Vector2f board_position) noexcept
{
    if (selected_square <= 0 || !WITHIN_BOARD_BOUNDS(board_position)) {
        reset_selection();
        return false;
    }
    uint32 index = board_position_to_index(board_position);

    if (index == static_cast<uint32>(selected_square)) {
        return false;
    }

    for (const Move &move : selected_moves) {
        if (move.target_square() == index) {
            input_move(move);
            reset_selection();
            return true;
        }
    }
    
    reset_selection();
    return false;
}

void GraphicalBoard::draw_hovering_peice(sf::RenderTarget &target, sf::Vector2f mouse_position)
{
    if (selected_square < 0) {
        return;
    }

    // clear selected square
    sf::RectangleShape square(sf::Vector2f(120, 120));
    square.setPosition(position + board_index_to_position(selected_square));
    square.setFillColor(IS_LIGHT_SQUARE(selected_square) ? LIGHT_SELECTED_SQUARE : DARK_SELECTED_SQUARE);
    target.draw(square);

    // translucent peice
    sf::Sprite peice(peice_textures.at(peices[selected_square]));
    sf::Color sprite_color = peice.getColor();
    sprite_color.a = 128;
    peice.setColor(sprite_color);
    peice.setPosition(board_index_to_position(selected_square));
    target.draw(peice);

    // hovering peice
    sprite_color.a = 255;
    peice.setColor(sprite_color);
    peice.setPosition(mouse_position - sf::Vector2f(60, 60));
    target.draw(peice);
}

void GraphicalBoard::flip() noexcept
{
    white_on_bottom = !white_on_bottom;
}

uint32 GraphicalBoard::board_position_to_index(sf::Vector2f board_position) const
{
    float x = board_position.x;
    float y = board_position.y;
    if (x < 0.0f || x >= 960.0f || y < 0.0f || y >= 960.0f) {
        throw new std::invalid_argument("position out of bounds!");
    }
    uint32 square_x = static_cast<uint32>(x / 120.0f);
    uint32 square_y = static_cast<uint32>(y / 120.0f);
    uint32 rank, file;
    if (white_on_bottom) {
        rank = 7 - square_y;
        file = square_x;

    } else {
        rank = square_y;
        file = 7 - square_x;
    }
    return 8 * rank + file;
}

sf::Vector2f GraphicalBoard::board_index_to_position(uint32 index) const
{
    if (index > 63) {
        throw new std::invalid_argument("index out of range!");
    }
    uint32 rank = index / 8;
    uint32 file = index % 8;
    float x, y;
    if (white_on_bottom) {
        x = 120.0f * file;
        y = 120.0f * (7 - rank);
    
    } else {
        x = 120.0f * (7 - file);
        y = 120.0f * rank;
    }
    return sf::Vector2f(x, y);
}

void GraphicalBoard::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
    // Draw checkerboard
    sf::RectangleShape square(sf::Vector2f(120, 120));
    for (uint32 i = 0; i < 8; ++i) {
        for (uint32 j = 0; j < 8; ++j) {
            square.setPosition(position + sf::Vector2f(120.0f * i, 120.0f * j));
            square.setFillColor((i + j) % 2 == 0 ? LIGHT_SQUARE_COLOR : DARK_SQUARE_COLOR);
            target.draw(square);
        }
    }

    // Draw previous move highlights
    if (!game_moves.empty()) {
        Move previous_move = game_moves.back();

        square.setPosition(position + board_index_to_position(previous_move.start_square()));
        square.setFillColor(IS_LIGHT_SQUARE(previous_move.start_square()) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
        target.draw(square);

        square.setPosition(position + board_index_to_position(previous_move.target_square()));
        square.setFillColor(IS_LIGHT_SQUARE(previous_move.target_square()) ? LIGHT_PREVIOUS_MOVE : DARK_PREVIOUS_MOVE);
        target.draw(square);
    }

    // Draw selected square highlights (if valid peice)
    if (selected_square >= 0) {
        square.setPosition(position + board_index_to_position(selected_square));
        square.setFillColor(IS_LIGHT_SQUARE(selected_square) ? LIGHT_SELECTED_SQUARE : DARK_SELECTED_SQUARE);
        target.draw(square);

        for (const Move &move : selected_moves) {
            square.setPosition(position + board_index_to_position(move.target_square()));
            square.setFillColor(IS_LIGHT_SQUARE(move.target_square()) ? LIGHT_AVAILABLE_TARGET : DARK_AVAILABLE_TARGET);
            target.draw(square);
        }
    }

    // Draw peices
    sf::Sprite peice;
    for (uint32 i = 0; i < 64; ++i) {
        if (!peices[i]) {
            continue;
        }
        peice.setPosition(board_index_to_position(i));
        peice.setTexture(peice_textures.at(peices[i]));
        target.draw(peice);
    }
}
