#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../standard_definitions.h"
#include "../board/board.h"
#include "../game/game.h"

struct UCIEngine;
struct UCIOption;

/*
    wrapper for uci compatible engine executable
    given the path to an UCI engine executable, creates a new process and facilitates communication with the engine
*/
struct UCIEngine
{
    // Initializes engine process
    UCIEngine(std::filesystem::path engine_path, std::filesystem::path log_file_path);

    // begins UCI communication with "uci" command
    // returns false if engine does not respond
    bool uci_init();

    // sets the debug status of the engine with the "debug" command
    bool set_debug(bool value);

    // checks if the engine is ready using the "is_ready" command
    bool is_ready();

    // set option of the engine with the "setoption" command
    bool set_option(std::string name, std::string value="");

    // Not implemented registration ucinewgame

    // sets the position in the engine with the "position" command
    bool set_position(std::string start, std::vector<std::string> &moves);
    bool set_position(std::string start);
    bool set_position(Game &game);
    bool set_position(Board &board);

    // gets the best move from the engine with the "go" command
    std::string best_move(std::chrono::milliseconds think_time);

    // Not implemented stop ponderhit

    // terminates the engine using the "quit" command
    void quit();
    
    // returns true if the child process is currenty running
    bool is_running();

    // terminate the child process
    void terminate_child();

    // writes the input to the child process
    // adds newline charecter
    bool write_line(std::string line);

    // name of the engine
    std::string name;
    // author of the engine
    std::string author;
    // list of options for engine
    std::vector<UCIOption> options;

    ~UCIEngine();
private:
    // reads input from the child process until either the match string is reached or a timeout occurs
    bool read_until(std::string &data_read, std::string match, std::chrono::milliseconds timeout);

    // Function to clean whitespace from a string
    static std::vector<std::string> parse_words(std::string &string);

    // file descriptor for write end of parent to child pipe
    int fd_write_to_engine;
    // file descriptor for read end of child to parent pipe
    int fd_read_from_engine;
    // stream for receiving output from engine
    pid_t pid;

    // flag for if the uci has been initialized
    bool uci_active;

    // log for recording communcation with engine
    std::ofstream log;
};

/*
    struct representing a option for a UCI engine
*/
struct UCIOption
{
    // constructor for option from uci option definition (broken into words)
    UCIOption(std::vector<std::string> &definition);

    // name of option
    std::string name;
    // type of option
    std::string type;
    // default value of option
    std::string default_value;
    // min/max value of option
    std::string min_value;
    std::string max_value;
    // predefined value of option
    std::string predefined_value;

    std::string as_string();
};