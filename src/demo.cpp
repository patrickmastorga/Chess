#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Font.hpp>

#include <filesystem>
#include <iostream>
#include <future>

#include "graphical_board/graphical_board.h"
#include "uci_engine/uci_engine.h"

#define BOARD_SIZE 960

/*
#define TITLE_HEIGHT 40.0f
#define DISTANCE_FROM_TOP 3.0f
#define TITLE_SIZE 26U
#define SUBTITLE_SIZE 12U
static void draw_title(sf::RenderTarget& target, std::string left, std::string right) {
    // create Title Bar
    sf::RectangleShape title_bar(sf::Vector2f(960, TITLE_HEIGHT));
    title_bar.setFillColor(sf::Color(40, 40, 40)); // Dark grey color

    // load font
    sf::Font font;
    font.loadFromFile("assets/fonts/arial.ttf");

    // middle text
    sf::Text title_middle("  vs.  ", font, TITLE_SIZE);
    title_middle.setFillColor(sf::Color::White);

    // calculate position
    sf::Vector2f title_pos((960 - title_middle.getGlobalBounds().width) / 2.0f, DISTANCE_FROM_TOP);
    title_middle.setPosition(title_pos);

    // white player
    sf::Text title_left(left, font, TITLE_SIZE);
    title_left.setOrigin(title_left.getGlobalBounds().width, 0.0f);
    title_left.setFillColor(sf::Color::White);
    title_left.setPosition(title_pos);

    // black player
    sf::Text title_right(right, font, TITLE_SIZE);
    title_right.setOrigin(-title_middle.getGlobalBounds().width, 0.0f);
    title_right.setFillColor(sf::Color::White);
    title_right.setPosition(title_pos);

    // controls
    sf::Text reset_button("reset (enter)", font, TITLE_SIZE / 2);
    sf::Text switch_button("switch (tab)", font, TITLE_SIZE / 2);
    reset_button.setPosition(2.0f, DISTANCE_FROM_TOP / 2.0f);
    switch_button.setPosition(2.0f, TITLE_HEIGHT / 2.0f + DISTANCE_FROM_TOP / 2.0f);
    reset_button.setFillColor(sf::Color(150, 150, 150));
    switch_button.setFillColor(sf::Color(150, 150, 150));

    // draw title
    target.draw(title_bar);
    target.draw(title_middle);
    target.draw(title_left);
    target.draw(title_right);
    target.draw(reset_button);
    target.draw(switch_button);
}
*/

int main()
{
    // SFML window creation
    sf::RenderWindow window(sf::VideoMode(BOARD_SIZE, BOARD_SIZE), "demo", sf::Style::Close | sf::Style::Titlebar);
    window.setVerticalSyncEnabled(true);

    // add window icon
    // get the full path of the window icon
    std::filesystem::path source_path = __FILE__;
    std::filesystem::path source_directory = source_path.parent_path();
    sf::Image window_icon;
    window_icon.loadFromFile(source_directory.string() + "/graphical_board/assets/120px/icon.png");
    window.setIcon(window_icon.getSize().x, window_icon.getSize().y, window_icon.getPixelsPtr());

    // create view for holding the game at fixed size
    sf::View game_view(sf::FloatRect(0, 0, BOARD_SIZE, BOARD_SIZE));

    // create board
    GraphicalBoard board(sf::Vector2f(0, 0));

    // initialize engine
    UCIEngine engine(source_directory.string() + "/uci_engine/engines/stockfish17.exe", source_directory.string() + "/uci_engine/engines/log.txt");
    if (!engine.start()) {
        throw new std::runtime_error("Unable to start engine process!");
    }
    engine.uci_init();

    // used to handle mouse events    
    bool mouse_pressed_last_frame = false;

    // used to handle asyncrounous move getting
    std::future<std::string> engine_move;
    bool computer_move_in_progress = false;

    while (window.isOpen())
    {
        // Handle events
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed) {
                window.close();
                return 0;
            }

            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Tab) {
                    board.flip();
                }

                if (event.key.code == sf::Keyboard::Enter) {
                    board.initialize_starting_position();
                }
            }
        }

        // Update screen
        window.setView(game_view);
        window.draw(board);

        if (board.white_on_bottom == board.white_to_move()) {
            // human turn
            // get the mouse position in world coordinates
            sf::Vector2f mouse_position = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            // mouse position relative to board
            sf::Vector2f mouse_board_position = mouse_position - board.position;

            if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && !mouse_pressed_last_frame) {
                // mouse down
                // attempt to play move then attempt to set selected square
                if (!board.attempt_move(mouse_board_position)) {
                    board.attempt_selection(mouse_board_position);
                }

                mouse_pressed_last_frame = true;
            
            } else if (sf::Mouse::isButtonPressed(sf::Mouse::Left)) {
                // mouse drag
                // draw hovering peice
                board.draw_hovering_peice(window, mouse_position);

            } else if (mouse_pressed_last_frame) {
                // mouse up
                // attempt to play move
                board.attempt_move(mouse_board_position);

                mouse_pressed_last_frame = false;
            }
   
        } else if (!board.get_legal_moves().empty()) {
            // computer turn
            if (!computer_move_in_progress) {
                // computer turn just started
                computer_move_in_progress = true;
                // launch the move calculation asynchronously
                engine.set_position(board);
                engine_move = std::async(std::launch::async, &UCIEngine::best_move, &engine, std::chrono::milliseconds(100));
            }
            if (engine_move.wait_for(std::chrono::milliseconds::zero()) == std::future_status::ready) {
                // computer has finished thinking
                computer_move_in_progress = false;
                std::string move = engine_move.get();
                if (!board.input_move(move)) {
                    throw new std::runtime_error("Engine move is not recognized as legal!");
                }
            }
        }

        window.display();
    }
    return 0;
}