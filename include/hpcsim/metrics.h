#pragma once

#include <string>
#include <vector>

#include "hpcsim/cluster.h"
#include "hpcsim/types.h"

namespace hpcsim {

// 一次模拟运行的汇总指标（对应报告第五章的 4 个评估指标）
struct Summary {
    std::string scheduler;
    int num_jobs = 0;
    double makespan = 0.0;         // max(finish) - min(submit)
    double avg_wait = 0.0;         // 平均等待时间 = start - submit
    double avg_turnaround = 0.0;   // 平均周转时间 = finish - submit
    double throughput = 0.0;       // num_jobs / makespan
    double utilization = 0.0;      // sum(busy core-seconds) / (total_cores * makespan)
    double load_balance_cv = 0.0;  // 各节点忙时的变异系数（越小越均衡）
};

class MetricsCollector {
public:
    void record_job(const Job& job) { finished_.push_back(job); }
    Summary finalize(const Cluster& cluster, const std::string& scheduler_name) const;
    void write_jobs_csv(const std::string& path) const;

    static void append_summary_csv(const std::string& path, const Summary& s);
    static void print_summary(const Summary& s);

private:
    std::vector<Job> finished_;
};

// 跨平台创建目录（单层，已存在则忽略）
void ensure_dir(const std::string& path);

}  // namespace hpcsim
