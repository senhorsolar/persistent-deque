#include <algorithm>
#include <chrono>
#include <deque>

#include "PersistentDeque.h"

template <typename F>
double measure_time_us(F&& f)
{
    const auto start = std::chrono::steady_clock::now();
    f();
    const auto end = std::chrono::steady_clock::now();
    return static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
}

void measure_clones(std::size_t n, std::size_t branches, std::size_t edits_per_branch, std::size_t reps)
{
    std::size_t std_sum = 0;
    std::size_t persistent_sum = 0;

    auto run_persistent = [&]() {
        PersistentDeque<std::size_t> base;
        for (std::size_t i = 0; i < n; ++i) {
            base.PushBack(i);
        }

        std::vector<PersistentDeque<std::size_t>> copies;
        copies.reserve(branches);
        for (std::size_t i = 0; i < branches; ++i) {
            copies.push_back(base);
        }

        for (std::size_t i = 0; i < branches; ++i) {
            auto& q = copies[i];
            for (std::size_t e = 0; e < edits_per_branch; ++e) {
                q.PushFront(e);
                persistent_sum += q.PopBack();
            }
        }
    };

    auto run_std = [&]() {
        std::deque<std::size_t> base;
        for (std::size_t i = 0; i < n; ++i) {
            base.push_back(i);
        }

        std::vector<std::deque<std::size_t>> copies;
        copies.reserve(branches);
        for (std::size_t i = 0; i < branches; ++i) {
            copies.push_back(base);
        }

        for (std::size_t i = 0; i < branches; ++i) {
            auto& q = copies[i];
            for (std::size_t e = 0; e < edits_per_branch; ++e) {
                q.push_front(e);
                std_sum += q.back();
                q.pop_back();
            }
        }
    };

    double persistent_us = 0;
    double std_us = 0;

    for (std::size_t i = 0; i < reps; ++i) {
        persistent_us += measure_time_us(run_persistent);
        std_us += measure_time_us(run_std);
    }

    std::cout << "measure_clones n=" << n
              << " branches=" << branches
              << " edits=" << edits_per_branch
              << " reps=" << reps
              << "\n-- persistent avg =" << (persistent_us / reps / 1000.0) << "ms"
              << "\n-- std avg = " << (std_us / reps / 1000.0) << "ms"
              << "\n-- speedup with persistence=" << (std_us / persistent_us)
              << "\n-- std value equivalence? " << ((persistent_sum == std_sum) ? "true" : "false")
              << "\n";
}

void measure_single_copy(std::size_t n, std::size_t reps)
{
    std::size_t std_sum = 0;
    std::size_t persistent_sum = 0;

    auto run_persistent = [&]() {
        PersistentDeque<std::size_t, 1024*8> base; // This capacity should be tuned

        for (std::size_t i = 0; i < n; ++i) {
            base.PushBack(i);
            base.PushFront(i);
        }

        for (std::size_t i = 0; i < n; ++i) {
            persistent_sum += base.PopBack();
            persistent_sum += base.PopFront();
        }
    };

    auto run_std = [&]() {
        std::deque<std::size_t> base;
        for (std::size_t i = 0; i < n; ++i) {
            base.push_back(i);
            base.push_front(i);
        }

        for (std::size_t i = 0; i < n; ++i) {
            std_sum += base.back();
            base.pop_back();
            std_sum += base.front();
            base.pop_front();
        }
    };

    double persistent_us = 0;
    double std_us = 0;

    for (std::size_t i = 0; i < reps; ++i) {
        persistent_us += measure_time_us(run_persistent);
        std_us += measure_time_us(run_std);
    }

    std::cout << "measure_single_copy n=" << n*2
              << " reps=" << reps
              << "\n-- persistent avg =" << (persistent_us / reps / 1000.0) << "ms"
              << "\n-- std avg = " << (std_us / reps / 1000.0) << "ms"
              << "\n-- speedup with persistence=" << (std_us / persistent_us)
              << "\n-- std value equivalence? " << ((persistent_sum == std_sum) ? "true" : "false")
              << "\n";
}

void measure_find(std::size_t n)
{
    // Binary search on sorted queues

    PersistentDeque<std::size_t> persistent_dq;
    std::deque<std::size_t> std_dq;

    bool all_found_persistent = true;
    bool all_found_std = true;

    for (std::size_t i = 0; i < n; ++i) {
        persistent_dq.PushBack(i);
        std_dq.push_back(i);
    }

    auto run_persistent = [&]() {
        for (std::size_t i = 0; i < n; ++i) {
            // Careful not to short-circuit
            all_found_persistent =
                std::binary_search(persistent_dq.begin(), persistent_dq.end(), i) && all_found_persistent;
        }
    };

    auto run_std = [&]() {
        for (std::size_t i = 0; i < n; ++i) {
            // Careful not to short-circuit
            all_found_std =
                std::binary_search(std_dq.begin(), std_dq.end(), i) && all_found_std;
        }
    };

    double persistent_us = measure_time_us(run_persistent);
    double std_us = measure_time_us(run_std);

    std::cout << "measure_find n=" << n
              << "\n-- persistent avg =" << (persistent_us / n / 1000.0) << "ms"
              << "\n-- std avg = " << (std_us / n / 1000.0) << "ms"
              << "\n-- speedup with persistence=" << (std_us / persistent_us)
              << "\n-- std value equivalence? " << ((all_found_persistent == all_found_std) ? "true" : "false")
              << "\n";
}

int main()
{
    measure_clones(200000, 64, 64, 5);
    measure_single_copy(3000000, 5);
    measure_find(100000);
}
