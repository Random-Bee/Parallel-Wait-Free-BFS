// Stalled threads, for the experiments of stalls.sh. Built with -DSTALLS, each
// of the last BFS_STALLED of a program's workers busy-waits BFS_STALL_US
// microseconds at the first stall point it reaches in every level or round, as
// a thread delayed while it holds part of that level's work would. Without
// -DSTALLS a stall point compiles to nothing, so the benchmarked programs are
// unchanged
#pragma once

#ifdef STALLS
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>

namespace stalls {

// a whole number from 0 to `most` from the environment, 0 if unset; anything
// else ends the program, since a run with a misread setting would be wrong
inline long long setting(const char* name, long long most) {
    const char* text = std::getenv(name);
    if(text == nullptr || *text == '\0') return 0;
    char* end = nullptr;
    errno = 0;
    long long value = std::strtoll(text, &end, 10);
    if(errno != 0 || *end != '\0' || value < 0 || value > most) {
        std::fprintf(stderr, "Error: %s must be a whole number from 0 to %lld\n", name, most);
        std::exit(2);
    }
    return value;
}

inline long long stalled_workers() {
    static const long long count = setting("BFS_STALLED", INT_MAX);
    return count;
}

inline long long stall_us() {
    static const long long us = setting("BFS_STALL_US", 60'000'000);
    return us;
}

// true at the first stall point this thread reaches in a level or round, so
// `round` must change from one level or round to the next
inline bool first_in(long long round) {
    thread_local long long last_round = -1;
    if(round == last_round) return false;
    last_round = round;
    return true;
}

inline void wait_if_stalled(long long worker, long long workers) {
    if(worker < workers - stalled_workers() || stall_us() == 0) return;
    auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(stall_us());
    while(std::chrono::steady_clock::now() < until) {}
}

}  // namespace stalls

// worker and workers are evaluated only at a round's first stall point, since
// some stall points are passed once per vertex or edge
#define STALL_POINT(worker, workers, round) \
    do { \
        if(stalls::first_in(round)) stalls::wait_if_stalled((worker), (workers)); \
    } while(0)
#else
#define STALL_POINT(worker, workers, round) ((void)0)
#endif
