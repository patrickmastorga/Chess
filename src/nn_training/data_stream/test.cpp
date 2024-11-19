#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cstring>

#include "data_stream.h"
#include "../../types.h"

void test_binpack(std::filesystem::path path);

int main() {
    std::filesystem::path source_path = __FILE__;
    std::filesystem::path binpack_path = source_path.parent_path().parent_path() / "datasets\\test80-2024-02-feb.binpack";

    BinpackTrainingDataStream stream(binpack_path, 1, 0, 1);

    while (stream.get_next_entry()) {}

    return 0;
}

void test_binpack(std::filesystem::path path) {
    std::ifstream file(path, std::ios::binary);
    
    if (!file.is_open()) {
        std::cerr << "Invalid path";
        return;
    }

    size_t max_block_size = 0;

    uint8 header[8];

    while (file.peek() != std::ifstream::traits_type::eof()) {

        if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) {
            std::cerr << "Invalid ending";
            return;
        }
        
        char mystery = header[4];

        if (header[0] != 'B' || header[1] != 'I' || header[2] != 'N' || header[3] != 'P')
        {
            std::cerr << "Invalid binpack file or chunk.";
            return;
        }

        uint32 block_size = 
               header[4]
            | (header[5] << 8)
            | (header[6] << 16)
            | (header[7] << 24);
        
        if (block_size > max_block_size) {
            max_block_size = block_size;
        }

        file.seekg(block_size, std::ios::cur);
    }
    
    std::cout << "max block size: " << max_block_size;
}