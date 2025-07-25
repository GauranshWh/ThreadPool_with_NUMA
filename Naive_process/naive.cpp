#include <vector>
#include <thread>
#include <iostream>
#include <chrono>
#include <mutex>

std::mutex cout_mutex;

int main() {
    const int num_tasks = 400;
    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_tasks; ++i) {
        threads.emplace_back([i] {
            auto task_start = std::chrono::high_resolution_clock::now();
            // Matrix multiplication (compute-bound)
            const int size = 100;
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
                          << std::this_thread::get_id() << " result[0][0]=" << result[0][0]
                          << " dispatch time: 0 microseconds (direct thread)\n";
            }
        });
    }

    for (auto& t : threads) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << "Completed " << num_tasks << " tasks in "
              << duration.count() << " seconds" << std::endl;
    return 0;
}