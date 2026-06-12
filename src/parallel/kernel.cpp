#include "kernel.h"

#include <chrono>

namespace hpcsim {

namespace {

double now_seconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

// 每线程的忙等计算循环；sink 防止编译器把计算优化掉
volatile double g_sink = 0.0;

}  // namespace

void busy_kernel(double wall_seconds, int num_threads) {
    if (num_threads < 1) num_threads = 1;
    if (wall_seconds <= 0) return;

    const double t_end = now_seconds() + wall_seconds;

#pragma omp parallel num_threads(num_threads)
    {
        double local = 1.000001;
        while (now_seconds() < t_end) {
            for (int i = 0; i < 200000; ++i) {
                local = local * 1.0000001 + 1e-12;
            }
        }
#pragma omp critical
        g_sink += local;
    }
}

}  // namespace hpcsim
