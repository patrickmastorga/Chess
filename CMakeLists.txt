cmake_minimum_required(VERSION 3.15)

# Project name and C++ standard
project(chess)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executables
add_executable(engine)
add_executable(demo)
add_executable(binpack_test)

# Source files for each target
target_sources(engine PRIVATE
    src/engine.cpp
    src/board/board.cpp
    src/movegen/movegen.cpp
    src/perft/perft.cpp
)

target_sources(demo PRIVATE
    src/demo.cpp
    src/board/board.cpp
    src/movegen/movegen.cpp
    src/game/game.cpp
    src/graphical_board/graphical_board.cpp
    src/uci_engine/process_handler.cpp
    src/uci_engine/uci_engine.cpp
)

target_sources(binpack_test PRIVATE
    src/nn_training/data_stream/test.cpp
    src/board/board.cpp
    src/movegen/movegen.cpp
    src/nn_training/data_stream/data_stream.cpp
)

# Find and link SFML libraries
set(BUILD_SHARED_LIBS OFF)
set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR "C:/SFML-2.6.2/lib/cmake/SFML")
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Link libraries for 'demo' target
target_link_libraries(demo PRIVATE sfml-graphics sfml-window sfml-system)