Thread Pool with NUMA Awareness
A C++17 project comparing three multithreading approaches: naive multithreading, a standard thread pool, and a NUMA-aware work-stealing thread pool. The goal is 3x faster task dispatch than naive multithreading for compute-bound tasks (100x100 matrix multiplication).
Files

naive.cpp: Naive multithreading (one thread per task).
main_original.cpp: Main for standard thread pool.
main_numa.cpp: Main for NUMA-aware work-stealing thread pool.
thread_original.hpp: Standard ThreadPool class.
thread_numa.hpp: NUMA-aware WorkStealingThreadPool class.

Prerequisites

OS: Linux (e.g., Ubuntu).
Compiler: g++ with C++17 support.
Dependencies: libnuma-dev, POSIX threads (pthread).

Install dependencies:
sudo apt-get update
sudo apt-get install g++ libnuma-dev

Build and Run

Clone the repository:
git clone <repository-url>
cd threadpool_with_NUMA


Build:
g++ -std=c++17 main_original.cpp -pthread -o original
g++ -std=c++17 main_numa.cpp -pthread -lnuma -o numa
g++ -std=c++17 naive.cpp -pthread -o naive


Run and save output:
./original > original_output.txt
./numa > numa_output.txt
./naive > naive_output.txt


Analyze results:

Total runtime:tail -n 1 original_output.txt
tail -n 1 numa_output.txt
tail -n 1 naive_output.txt


Average dispatch times:grep "Task dispatch time" original_output.txt | awk '{sum+=$4; count++} END {print "Original: ", sum/count, " µs"}'
grep "Task dispatch time" numa_output.txt | awk '{sum+=$4; count++} END {print "NUMA: ", sum/count, " µs"}'
grep "Completed" naive_output.txt | awk '{print "Naive (est.): ", $5/400*1000000, " µs"}'


Work-stealing events:grep "stole task" numa_output.txt



Performance

Naive: ~250-500µs/task dispatch, slowest due to thread creation.
Original ThreadPool: ~50-125µs/task, faster with thread reuse.
NUMA ThreadPool: Targets ~80-100µs/task (3x faster than naive) via work stealing and NUMA pinning. Best on multi-socket systems.

Check NUMA nodes:
numactl --hardware

Profiling
Profile cache misses and memory loads:
perf stat -e cache-misses,mem-loads ./numa > numa_perf.txt

If perf is restricted, enable access:
sudo sysctl -w kernel.perf_event_paranoid=1

