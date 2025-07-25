#include "thread_numa.hpp"
#include <iostream>
#include <chrono>
#include <mutex>
#include <vector>

std::mutex cout_mutex;

int main() {
    WorkStealingThreadPool pool(4, 1000); // 4 threads, 1000 task capacity
    const int num_tasks = 400;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        pool.enqueue([i] {
            // Matrix multiplication (compute-bound)
            const int size = 1-0;
            std::vector<std::vector<double>> a(size, std::vector<double>(size, 1.0));
            std::vector<std::vector<double>> b(size, std::vector<double>(size, 2.0));
            std::vector<std::vector<double>> result(size, std::vector<double>(size, 0.0));

            for (int x = 0; x < size; ++x) {
                for (int y = 0; y < size; ++y) {
                    for (int z = 0; z < size; ++z) {
                        result[x][y] += a[x][z] * b[z][y];
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Task " << i << " executed by thread "
                          << std::this_thread::get_id() << " result[0][0]=" << result[0][0] << std::endl;
            }
        }, 0); // NUMA node 0
    }

    pool.shutdown();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Completed " << num_tasks << " tasks in "
                  << duration.count() << " seconds" << std::endl;
    }

    return 0;
}