#include <iostream>
#include <chrono>

#include "../../types.h"
#include "data_stream.h"
#include "feature_sets.h"
#include "data_loader.h"

int main()
{
    std::cout << "Beginning Test..." << std::endl;

    DataLoader<BasicFeatureSetBatch> *data_loader = create_basic_data_loader("C:\\Users\\patri\\Documents\\GitHub\\chess2024\\src\\nn_training\\training_data\\datasets\\test80-2024-02-feb.binpack", 256, 0.35f, 4);

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0;; i++) {
        if (i % 1000 == 0) {
            std::cout << "\r            \r" << i;
            std::cout.flush();
        }

        BasicFeatureSetBatch *batch = get_basic_batch(data_loader);
        destroy_basic_batch(batch);

        if (!batch) {
            std::cout << "\nDone! " << i << " batches were processed!" << std::endl;;
            break;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Time elapsed: " << std::chrono::duration_cast<std::chrono::seconds>(end - start).count() << "s" << std::endl;
    destroy_basic_data_loader(data_loader);
    return 0;
}