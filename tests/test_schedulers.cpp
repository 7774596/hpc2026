// 调度器正确性测试：用手算可验证的小规模用例核对模拟结果。
// 运行：make test
//
// 新增调度策略时必须同步添加用例。
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

// 非严格 SJF：资源释放后在"放得下"的等待作业中选 runtime 最短者
void test_sjf_shortest_first() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));  // 占满 2 核集群
    jobs.push_back(make_job(1, 1.0, 20.0, 2));
    jobs.push_back(make_job(2, 2.0, 5.0, 2));   // J0 结束后应先于 J1 启动
    hpcsim::Simulator sim(hpcsim::Cluster(1, 2), hpcsim::make_scheduler("sjf"));
    sim.run(jobs);

    CHECK(near(jobs[2].start_time, 10.0));
    CHECK(near(jobs[2].finish_time, 15.0));
    CHECK(near(jobs[1].start_time, 15.0));
    CHECK(near(jobs[1].finish_time, 35.0));
}

// 非严格 SJF：队首放不下时，仍可从后续等待作业中选最短的（区别于 FCFS 阻塞）
void test_sjf_skip_blocked_head() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));  // 占 2 核，留 2 核空闲
    jobs.push_back(make_job(1, 1.0, 5.0, 4));   // 需整集群，J0 运行期间放不下
    jobs.push_back(make_job(2, 2.0, 3.0, 2));   // 2 核可装入空闲核
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("sjf"));
    sim.run(jobs);

    CHECK(near(jobs[2].start_time, 2.0));
    CHECK(near(jobs[1].start_time, 10.0));
}

// 对照：相同负载下 FCFS 因队首 J1 放不下而阻塞 J2
void test_fcfs_blocks_while_sjf_runs() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));
    jobs.push_back(make_job(1, 1.0, 5.0, 4));
    jobs.push_back(make_job(2, 2.0, 3.0, 2));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("fcfs"));
    sim.run(jobs);

    CHECK(near(jobs[2].start_time, 15.0));  // 等 J1 跑完
}

// Backfilling 边界：插队作业若在 shadow 时刻之后完成，则不得回填
void test_backfill_reject_late_job() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 2));   // shadow = 10
    jobs.push_back(make_job(1, 1.0, 100.0, 4));
    jobs.push_back(make_job(2, 2.0, 9.0, 2));     // 2 + 9 = 11 > 10，不可插队
    hpcsim::Simulator sim(hpcsim::Cluster(1, 4), hpcsim::make_scheduler("backfill"));
    sim.run(jobs);

    CHECK(near(jobs[2].start_time, 110.0));  // 等 J1 结束后才能启动
    CHECK(near(jobs[1].start_time, 10.0));
}

// RR 边界：quantum 大于作业时长时不触发抢占，行为接近 FCFS
void test_rr_large_quantum_no_preempt() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 10.0, 1));
    jobs.push_back(make_job(1, 0.0, 10.0, 1));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 1), hpcsim::make_scheduler("rr", 100.0));
    sim.run(jobs);

    CHECK(near(jobs[0].start_time, 0.0));
    CHECK(near(jobs[0].finish_time, 10.0));
    CHECK(near(jobs[1].start_time, 10.0));
    CHECK(near(jobs[1].finish_time, 20.0));
}

// RR 边界：remaining == quantum 时一次跑完，不多登记 TimeSliceExpire
void test_rr_quantum_equals_runtime() {
    std::vector<hpcsim::Job> jobs;
    jobs.push_back(make_job(0, 0.0, 5.0, 1));
    jobs.push_back(make_job(1, 0.0, 5.0, 1));
    hpcsim::Simulator sim(hpcsim::Cluster(1, 1), hpcsim::make_scheduler("rr", 5.0));
    sim.run(jobs);

    CHECK(near(jobs[0].finish_time, 5.0));
    CHECK(near(jobs[1].finish_time, 10.0));
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
    test_sjf_shortest_first();
    test_sjf_skip_blocked_head();
    test_fcfs_blocks_while_sjf_runs();
    test_backfill();
    test_backfill_reject_late_job();
    test_fcfs_no_backfill();
    test_rr_large_quantum_no_preempt();
    test_rr_quantum_equals_runtime();
    test_rr_preemption();

    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
