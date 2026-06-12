#pragma once

#include <vector>

#include "hpcsim/types.h"

namespace hpcsim {

struct Node {
    int id = 0;
    int total_cores = 0;
    int free_cores = 0;
    double busy_core_seconds = 0.0;  // 累计忙时（核·秒），用于利用率/负载均衡统计
};

// 集群资源模型：N 个节点 × 每节点 M 核，提供核的分配与释放。
class Cluster {
public:
    Cluster(int num_nodes, int cores_per_node);

    int num_nodes() const { return static_cast<int>(nodes_.size()); }
    int total_cores() const { return total_cores_; }
    int free_cores() const { return free_cores_; }
    const std::vector<Node>& nodes() const { return nodes_; }

    // 尝试为作业分配 job.cores 个核（first-fit，允许跨节点）。
    // 成功则填写 job.allocation 并返回 true；失败不改变任何状态。
    bool try_allocate(Job& job);

    // 释放作业占用的核，并按本次运行时长 elapsed 累计各节点忙时。
    void release(const Job& job, double elapsed);

private:
    std::vector<Node> nodes_;
    int total_cores_ = 0;
    int free_cores_ = 0;
};

}  // namespace hpcsim
