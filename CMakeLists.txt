cmake_minimum_required(VERSION 3.15)

# Project name and C++ standard
project(chess)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executables
add_executable(engine)
add_executable(demo)

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

# Find and link SFML libraries
set(BUILD_SHARED_LIBS OFF)
set(SFML_DIR "C:/SFML-2.6.2/lib/cmake/SFML")
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Link libraries for 'demo' target
target_link_libraries(demo PRIVATE sfml-graphics sfml-window sfml-system)

# Compiler flags (similar to BASE_CFLAGS, DEBUG_CFLAGS, and RELEASE_CFLAGS in Makefile)
if (MSVC)
    # Compiler flags for MSVC
    set(BASE_CFLAGS "/std:c++20" "/W4")
    set(DEBUG_CFLAGS "/Zi" "/Od")
    set(RELEASE_CFLAGS "/O2")
else()
    # Compiler flags for GCC/Clang (for cross-platform)
    set(BASE_CFLAGS "-std=c++20" "-Wall")
    set(DEBUG_CFLAGS "-g" "-O0")
    set(RELEASE_CFLAGS "-O3")
endif()

# Apply flags to targets
target_compile_options(engine PRIVATE ${BASE_CFLAGS})
target_compile_options(demo PRIVATE ${BASE_CFLAGS})

# Handle build types
if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_options(engine PRIVATE ${DEBUG_CFLAGS})
    target_compile_options(demo PRIVATE ${DEBUG_CFLAGS})
else()
    target_compile_options(engine PRIVATE ${RELEASE_CFLAGS})
    target_compile_options(demo PRIVATE ${RELEASE_CFLAGS})
endif()