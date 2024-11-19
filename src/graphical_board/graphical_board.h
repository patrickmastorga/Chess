#include <SFML/Graphics.hpp>

#include <map>
#include <vector>
#include <string>

#include "../types.h"
#include "../game/game.h"

#define WITHIN_BOARD_BOUNDS(v) ((v.x >= 0) && (v.x < 960.0f) && (v.y >= 0) && (v.y < 960.0f))

/*
    sfml compatible chess game
    fixed size 960x960 pixel board
*/
class GraphicalBoard : public Game, public sf::Drawable {
public:
    GraphicalBoard(sf::Vector2f position) noexcept;

    void initialize_from_fen(std::string fen) override;

    void from_uci_string(std::string uci_string) override;

    // attempts to select the square at the given position
    // returns true is successful
    bool attempt_selection(sf::Vector2f board_position) noexcept;

    // returns the selected square
    // returns -1 if no square is selected
    int32 get_selected_square() const noexcept;

    // resets the current selection to none
    void reset_selection() noexcept;

    // attempts to execute the move from the selected square to the square at the given position
    // returns true is successful
    bool attempt_move(sf::Vector2f board_position) noexcept;

    // does nothing if no peice is selected
    // draw a hovering peice mathching the selected peice at the specified position on the sf::RenderTarget
    // simultaneously reduces the opacity of the selected peice
    void draw_hovering_peice(sf::RenderTarget &target, sf::Vector2f mouse_position);

    // flips perspective (which color is on the bottom) default white on bottom
    void flip() noexcept;

    // transforms position on 960x960 board to square index
    uint32 board_position_to_index(sf::Vector2f board_position) const;

    // transforms square index to position on 960x960 
    sf::Vector2f board_index_to_position(uint32 index) const;

    // pixel position of top left corner when drawed on render target
    sf::Vector2f position;

    // true of the white perspective is on the bottom
    bool white_on_bottom;

private:
    // the index of the selected square
    int32 selected_square;

    // all legal moves originating from the selected square
    std::vector<Move> selected_moves;

    // maps integer peice values to 120x120 peice textures
    std::map<uint32, sf::Texture> peice_textures;

    // override method for drawind the board onto the screen
    void draw(sf::RenderTarget& target, sf::RenderStates states) const override;
};