#include <iostream>

#include "types.h"
#include "board/board.h"
#include "perft/perft.h"

using namespace std;

int main() {
    
    perft::perft_speed_test();
    //perft::perft_accuracy_test();
    //perft::perft("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 5);

    return 0;
}