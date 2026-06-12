// 模拟器 + FCFS 正确性测试：用手算可验证的微型场景断言调度结果。
// TODO(B): 为 sjf / rr / backfill 各加一组同样风格的手算用例。
#include <cmath>
#include <cstdio>
#include <vector>

#include "hpcsim/simulator.hpp"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        if (!(cond)) {                                               \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                            \
        }                                                            \
    } while (0)

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

hpcsim::Job make_job(int id, double submit, double runtime, int cores) {
    hpcsim::Job j;
    j.id = id;
    j.submit_time = submit;
    j.runtime = runtime;
    j.runtime_est = runtime;
    j.cores = cores;
    return j;
}

// 场景 1：1 节点 4 核。J0 占满 4 核跑 10s；J1、J2 各要 2 核，
// 必须等 J0 结束后在 t=10 同时启动。
void test_fcfs_basic() {
    std::vector<hpcsim::Job> jobs = {
        make_job(0, 0.0, 10.0, 4),
        make_job(1, 1.0, 5.0, 2),
        make_job(2, 2.0, 5.0, 2),
    };
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    hpcsim::Summary s = sim.run(jobs);

    CHECK(near(jobs[0].start_time, 0.0));
    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[2].start_time, 10.0));
    CHECK(near(s.makespan, 15.0));
    CHECK(s.num_jobs == 3);
}

// 场景 2：严格 FCFS 不允许插队。J1 要 4 核被阻塞时，
// 后到的 1 核小作业 J2 也必须排在 J1 之后（t=15 才能跑）。
void test_fcfs_no_jump() {
    std::vector<hpcsim::Job> jobs = {
        make_job(0, 0.0, 10.0, 4),
        make_job(1, 1.0, 5.0, 4),
        make_job(2, 2.0, 5.0, 1),
    };
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    sim.run(jobs);

    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[2].start_time, 15.0));
}

// 场景 3：同样的负载换成 backfill，小作业 J2 应当插队
// （预计 7s 完成，早于 J1 的影子时刻 t=10，不推迟 J1）。
void test_backfill_jumps() {
    std::vector<hpcsim::Job> jobs = {
        make_job(0, 0.0, 10.0, 4),
        make_job(1, 1.0, 5.0, 4),
        make_job(2, 2.0, 5.0, 1),
    };
    hpcsim::Simulator sim(hpcsim::Cluster(2, 4), hpcsim::make_scheduler("backfill"));
    sim.run(jobs);

    CHECK(near(jobs[1].start_time, 1.0));  // 双节点下 J1 直接上第二个节点
    CHECK(near(jobs[2].start_time, 2.0));
}

}  // namespace

int main() {
    test_fcfs_basic();
    test_fcfs_no_jump();
    test_backfill_jumps();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
}
