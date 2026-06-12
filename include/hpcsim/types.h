#pragma once

#include <utility>
#include <vector>

namespace hpcsim {

enum class JobState { Pending, Waiting, Running, Finished };

// 一个待调度的作业（HPC 集群中的一次批处理任务）
struct Job {
    int id = -1;
    double submit_time = 0.0;  // 到达/提交时刻（虚拟秒）
    double runtime = 0.0;      // 总执行时长（虚拟秒），Backfilling 中亦作为用户估计值
    int cores = 1;             // 需要的核数（可跨节点分配）

    // 以下字段由模拟引擎填充
    double remaining = 0.0;    // 剩余执行时长（RR 时间片轮转时递减）
    double start_time = -1.0;  // 首次开始执行时刻
    double last_start = -1.0;  // 最近一次开始执行时刻（时间片段起点）
    double finish_time = -1.0; // 完成时刻
    JobState state = JobState::Pending;
    std::vector<std::pair<int, int>> allocation;  // (节点 id, 占用核数) 列表

    double wait_time() const { return start_time - submit_time; }
    double turnaround() const { return finish_time - submit_time; }
    int first_node() const { return allocation.empty() ? -1 : allocation.front().first; }
};

}  // namespace hpcsim
