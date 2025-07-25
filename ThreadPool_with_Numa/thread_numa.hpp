#pragma once
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <random>
#include <memory>
#include <chrono>
#include <numa.h>

class WorkStealingThreadPool {
public:
    struct Task {
        std::function<void()> func;
        int numa_node; // -1 if no affinity
        Task(std::function<void()> f, int node = -1) : func(std::move(f)), numa_node(node) {}
    };

    WorkStealingThreadPool(size_t num_threads, size_t queue_capacity = 1000);
    ~WorkStealingThreadPool();

    void enqueue(std::function<void()> task, int numa_node = -1);
    void shutdown();
    void shutdown_now();

private:
    struct Worker {
        std::deque<Task> queue;
        std::unique_ptr<std::mutex> queue_mutex; // Movable mutex
        std::thread thread;
        int numa_node;
        Worker() : queue_mutex(std::make_unique<std::mutex>()) {}
    };

    std::vector<Worker> workers;
    std::atomic<bool> stop{false};
    size_t queue_capacity;
    std::random_device rd;
    std::mt19937 rng;

    void worker_thread(size_t worker_id);
    bool try_pop_local(size_t worker_id, Task& task);
    bool try_steal(size_t victim_id, Task& task);
};

WorkStealingThreadPool::WorkStealingThreadPool(size_t num_threads, size_t queue_capacity)
    : queue_capacity(queue_capacity), rng(rd()) {
    if (num_threads == 0) {
        throw std::invalid_argument("Number of threads must be greater than 0");
    }
    if (numa_available() < 0) {
        throw std::runtime_error("NUMA not available");
    }

    workers.reserve(num_threads);
    int max_nodes = numa_max_node() + 1;
    for (size_t i = 0; i < num_threads; ++i) {
        int node = max_nodes > 1 ? (i % max_nodes) : 0;
        workers.emplace_back();
        workers.back().numa_node = node;
        workers.back().thread = std::thread(&WorkStealingThreadPool::worker_thread, this, i);
    }
}

WorkStealingThreadPool::~WorkStealingThreadPool() {
    shutdown();
}

void WorkStealingThreadPool::enqueue(std::function<void()> task, int numa_node) {
    if (stop) {
        throw std::runtime_error("Cannot enqueue tasks after shutdown");
    }
    if (!task) {
        throw std::invalid_argument("Task cannot be null");
    }

    auto enqueue_time = std::chrono::high_resolution_clock::now();
    size_t target = 0;
    if (numa_node >= 0 && numa_node <= numa_max_node()) {
        for (size_t i = 0; i < workers.size(); ++i) {
            if (workers[i].numa_node == numa_node) {
                target = i;
                break;
            }
        }
    } else {
        std::uniform_int_distribution<size_t> dist(0, workers.size() - 1);
        target = dist(rng);
    }

    {
        std::lock_guard<std::mutex> lock(*workers[target].queue_mutex);
        if (workers[target].queue.size() >= queue_capacity) {
            throw std::runtime_error("Task queue is full");
        }
        workers[target].queue.push_front(Task([task, enqueue_time] {
            auto start = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> dispatch_time = start - enqueue_time;
            {
                static std::mutex cout_mutex;
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Task dispatch time: " << dispatch_time.count() << " microseconds\n";
            }
            task();
        }, numa_node));
    }
}

void WorkStealingThreadPool::worker_thread(size_t worker_id) {
    if (numa_run_on_node(workers[worker_id].numa_node) < 0) {
        std::cerr << "Failed to pin thread to NUMA node " << workers[worker_id].numa_node << std::endl;
    }
    while (!stop) {
        Task task(nullptr, -1);
        if (try_pop_local(worker_id, task)) {
            try {
                task.func();
            } catch (const std::exception& e) {
                std::cerr << "Exception in thread: " << e.what() << std::endl;
            }
            continue;
        }

        std::uniform_int_distribution<size_t> dist(0, workers.size() - 1);
        for (size_t i = 0; i < workers.size(); ++i) {
            size_t victim = dist(rng);
            if (victim != worker_id && try_steal(victim, task)) {
                try {
                    static std::mutex cout_mutex;
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Thread " << std::this_thread::get_id() << " stole task from worker " << victim << std::endl;
                    task.func();
                } catch (const std::exception& e) {
                    std::cerr << "Exception in thread: " << e.what() << std::endl;
                }
                break;
            }
        }
    }
}

bool WorkStealingThreadPool::try_pop_local(size_t worker_id, Task& task) {
    std::lock_guard<std::mutex> lock(*workers[worker_id].queue_mutex);
    if (workers[worker_id].queue.empty()) {
        return false;
    }
    task = std::move(workers[worker_id].queue.front());
    workers[worker_id].queue.pop_front();
    return true;
}

bool WorkStealingThreadPool::try_steal(size_t victim_id, Task& task) {
    std::lock_guard<std::mutex> lock(*workers[victim_id].queue_mutex);
    if (workers[victim_id].queue.empty()) {
        return false;
    }
    task = std::move(workers[victim_id].queue.back());
    workers[victim_id].queue.pop_back();
    return true;
}

void WorkStealingThreadPool::shutdown() {
    stop = true;
    for (auto& worker : workers) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }
}

void WorkStealingThreadPool::shutdown_now() {
    stop = true;
    for (auto& worker : workers) {
        std::lock_guard<std::mutex> lock(*worker.queue_mutex);
        worker.queue.clear();
    }
    for (auto& worker : workers) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }
}