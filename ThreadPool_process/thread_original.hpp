#pragma once
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <stdexcept>
#include <iostream>
#include <chrono>

class ThreadPool {
public:
    ThreadPool(size_t num_threads, size_t queue_capacity = 1000);
    ~ThreadPool();

    void enqueue(std::function<void()> task);
    void shutdown();
    void shutdown_now();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
    size_t queue_capacity;

    void worker_thread();
};

ThreadPool::ThreadPool(size_t num_threads, size_t queue_capacity)
    : stop(false), queue_capacity(queue_capacity) {
    if (num_threads == 0) {
        throw std::invalid_argument("Number of threads must be greater than 0");
    }

    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(&ThreadPool::worker_thread, this);
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue(std::function<void()> task) {
    if (stop) {
        throw std::runtime_error("Cannot enqueue tasks after shutdown");
    }
    if (!task) {
        throw std::invalid_argument("Task cannot be null");
    }
    auto enqueue_time = std::chrono::high_resolution_clock::now();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (tasks.size() >= queue_capacity) {
            throw std::runtime_error("Task queue is full");
        }
        tasks.push([task, enqueue_time] {
            auto start = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::micro> dispatch_time = start - enqueue_time;
            {
                static std::mutex cout_mutex;
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Task dispatch time: " << dispatch_time.count() << " microseconds\n";
            }
            task();
        });
    }
    condition.notify_one();
}

void ThreadPool::worker_thread() {
    while (!stop) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); });
            if (stop && tasks.empty()) {
                return;
            }
            task = std::move(tasks.front());
            tasks.pop();
        }

        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "Exception in thread: " << e.what() << std::endl;
        }
    }
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::shutdown_now() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
        std::queue<std::function<void()>> empty;
        tasks.swap(empty);
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}