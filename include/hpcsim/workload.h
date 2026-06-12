#pragma once

#include <string>
#include <vector>

#include "hpcsim/config.h"
#include "hpcsim/types.h"

namespace hpcsim {

// 合成负载参数（泊松到达流）
struct WorkloadParams {
    int num_jobs = 1000;
    double arrival_rate = 1.0;            // 到达率（作业/模拟秒）
    std::string runtime_dist = "uniform"; // uniform | exponential
    double runtime_min = 10.0;
    double runtime_max = 600.0;
    double runtime_mean = 120.0;          // exponential 分布的均值
    int cores_min = 1;
    int cores_max = 8;
    unsigned seed = 42;
};

// 从配置文件读取负载参数（main 和 MPI 执行器共用）
WorkloadParams workload_params_from_config(const Config& cfg);

// 按参数生成合成作业流（按 submit_time 升序，id 即下标）
// TODO(B): 增加更真实的负载模型（如重尾分布、日周期），以及与真实 trace 的对齐
std::vector<Job> generate_workload(const WorkloadParams& p);

// 从 CSV 加载作业 trace，格式：id,submit_time,runtime,cores（支持 # 注释行）。
// 注意：加载后会按到达时间重新连续编号 id（模拟器要求 id == 下标）。
std::vector<Job> load_trace_csv(const std::string& path);

}  // namespace hpcsim
