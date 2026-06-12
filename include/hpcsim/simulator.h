#pragma once

#include <memory>
#include <string>
#include <vector>

#include "hpcsim/cluster.h"
#include "hpcsim/metrics.h"
#include "hpcsim/scheduler.h"

namespace hpcsim {

struct SimOptions {
    std::string jobs_csv_path;  // 为空则不输出逐作业明细
};

// 串行离散事件模拟器。事件：JobArrival / JobFinish / TimeSliceExpire（抢占式策略）。
// 要求 jobs[i].id == i（generate_workload / load_trace_csv 已保证）。
class Simulator {
public:
    Simulator(Cluster cluster, std::unique_ptr<Scheduler> scheduler);

    // 运行模拟。jobs 中的 start/finish/allocation 等字段会被原地填写。
    Summary run(std::vector<Job>& jobs, const SimOptions& opt = SimOptions());

private:
    Cluster cluster_;
    std::unique_ptr<Scheduler> scheduler_;
};

}  // namespace hpcsim
