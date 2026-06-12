// 调度器正确性测试：用手算可验证的小规模用例核对模拟结果。
// 运行：make test
//
// TODO(B): 为 sjf 补充对应用例；新增调度策略时必须同步添加用例。
#include <cmath>
#include <cstdio>
#include <vector>

#include "hpcsim/simulator.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                               \
        }                                                               \
    } while (0)

bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

hpcsim::Job make_job(int id, double submit, double runtime, int cores) {
    hpcsim::Job j;
    j.id = id;
    j.submit_time = submit;
    j.runtime = runtime;
    j.remaining = runtime;
    j.cores = cores;
    return j;
}

// 严格 FCFS：大作业 J1 占满集群时，队首 J2 阻塞，J3 不得插队
void test_fcfs_blocking() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 4));  // 占满整个集群
    jobs.push_back(make_job(1, 1.0, 5.0, 4));   // 队首，需等 J1 结束
    jobs.push_back(make_job(2, 2.0, 5.0, 1));   // 小作业，严格 FCFS 下不能越过 J2
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    hpcsim::Summary s = sim.run(jobs);

    CHECK(near(jobs[0].start_time, 0.0));
    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[2].start_time, 15.0));  // J2 占满 4 核，J3 等到 15
    CHECK(s.num_jobs == 3);
    CHECK(near(s.makespan, 20.0));
}

// FCFS 并行装填：空闲核足够时多个作业可同时运行
void test_fcfs_packing() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 4));
    jobs.push_back(make_job(1, 1.0, 5.0, 2));
    jobs.push_back(make_job(2, 2.0, 5.0, 2));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    sim.run(jobs);

    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[2].start_time, 10.0));  // 与 J2 一起装入 4 核
}

// 跨节点分配：作业核数超过单节点容量时拆到多个节点
void test_cross_node_allocation() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 4));  // 2 节点 x 2 核，必须跨节点
    jobs.push_back(make_job(1, 1.0, 5.0, 1));
    hpcsim::Simulator sim(hpcsim::Cluster(2, 2), hpcsim::make_scheduler("fcfs"));
    sim.run(jobs);

    CHECK(near(jobs[0].start_time, 0.0));
    CHECK(jobs[0].allocation.size() == 2);  // 每个节点 2 核
    CHECK(near(jobs[1].start_time, 10.0));  // 集群被 J1 占满
}

// EASY Backfilling：小作业可插队，且不推迟队首作业
void test_backfill() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));   // 运行中，留 2 核空闲
    jobs.push_back(make_job(1, 1.0, 100.0, 4));  // 队首，需整集群，影子时刻 = 10
    jobs.push_back(make_job(2, 2.0, 5.0, 2));    // 2 + 5 <= 10，可插队
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("backfill"));
    sim.run(jobs);

    CHECK(near(jobs[2].start_time, 2.0));   // 被回填
    CHECK(near(jobs[1].start_time, 10.0));  // 队首未被推迟
}

// 对照：相同负载下 FCFS 不允许 J3 插队
void test_fcfs_no_backfill() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));
    jobs.push_back(make_job(1, 1.0, 100.0, 4));
    jobs.push_back(make_job(2, 2.0, 5.0, 2));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    sim.run(jobs);

    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[2].start_time, 110.0));
}

// RR 抢占：单核集群、时间片 5，两个 10s 作业交替执行
// 时间线：J1 跑 [0,5) 后回队尾 -> J2 跑 [5,10) -> J1 跑 [10,15] 完成 -> J2 跑 [15,20] 完成
void test_rr_preemption() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 1));
    jobs.push_back(make_job(1, 0.5, 10.0, 1));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 1), hpcsim::make_scheduler("rr", 5.0));
    sim.run(jobs);

    CHECK(near(jobs[0].start_time, 0.0));
    CHECK(near(jobs[1].start_time, 5.0));
    CHECK(near(jobs[0].finish_time, 15.0));
    CHECK(near(jobs[1].finish_time, 20.0));
    CHECK(near(jobs[0].remaining, 0.0));
    CHECK(near(jobs[1].remaining, 0.0));
}

}  // namespace

int main() {
    test_fcfs_blocking();
    test_fcfs_packing();
    test_cross_node_allocation();
    test_backfill();
    test_fcfs_no_backfill();
    test_rr_preemption();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
