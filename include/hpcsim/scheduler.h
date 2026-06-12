#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hpcsim/cluster.h"
#include "hpcsim/types.h"

namespace hpcsim {

// 调度决策上下文（只读）。模拟器在每个事件后反复调用 pick_next，
// 直到返回 nullptr 为止，因此调度器只需每次给出"下一个要启动的作业"。
struct SchedContext {
    double now;                        // 当前模拟时刻
    const std::vector<Job*>& pending;  // 等待队列（到达/回队顺序，调度器不得修改）
    const Cluster& cluster;            // 集群当前资源状态
    const std::vector<Job*>& running;  // 正在运行的作业
};

// 调度策略统一接口。
// 契约：
//   1) pick_next 只读，不得修改 Job/Cluster 状态；
//   2) 返回的作业必须属于 pending 且 cores <= cluster.free_cores()
//      （分配允许跨节点，因此"放得下"只取决于总空闲核数）；
//   3) 资源分配、状态机和事件登记由模拟器统一执行；
//   4) time_slice() > 0 表示抢占式策略：作业每次最多连续运行一个时间片，
//      到期后回到等待队列尾部（模拟器处理 TimeSliceExpire 事件）。
class Scheduler {
public:
    virtual ~Scheduler() {}
    virtual std::string name() const = 0;
    virtual Job* pick_next(const SchedContext& ctx) = 0;
    virtual double time_slice() const { return 0.0; }  // 0 = 非抢占
};

// 工厂：fcfs | sjf | rr | backfill（rr_quantum 仅 RR 使用，单位模拟秒）
std::unique_ptr<Scheduler> make_scheduler(const std::string& name, double rr_quantum = 50.0);
std::vector<std::string> scheduler_names();

}  // namespace hpcsim
