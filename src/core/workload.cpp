#include "hpcsim/workload.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <stdexcept>

namespace hpcsim {

WorkloadParams workload_params_from_config(const Config& cfg) {
    WorkloadParams p;
    p.num_jobs = cfg.get_int("num_jobs", p.num_jobs);
    p.arrival_rate = cfg.get_double("arrival_rate", p.arrival_rate);
    p.runtime_dist = cfg.get("runtime_dist", p.runtime_dist);
    p.runtime_min = cfg.get_double("runtime_min", p.runtime_min);
    p.runtime_max = cfg.get_double("runtime_max", p.runtime_max);
    p.runtime_mean = cfg.get_double("runtime_mean", p.runtime_mean);
    p.cores_min = cfg.get_int("cores_min", p.cores_min);
    p.cores_max = cfg.get_int("cores_max", p.cores_max);
    p.seed = static_cast<unsigned>(cfg.get_int("seed", static_cast<int>(p.seed)));
    return p;
}

std::vector<Job> generate_workload(const WorkloadParams& p) {
    if (p.num_jobs <= 0) throw std::invalid_argument("workload: num_jobs must be positive");
    if (p.cores_min < 1 || p.cores_max < p.cores_min) {
        throw std::invalid_argument("workload: invalid cores range");
    }

    std::mt19937 rng(p.seed);
    std::exponential_distribution<double> interarrival(p.arrival_rate);
    std::uniform_real_distribution<double> runtime_uniform(p.runtime_min, p.runtime_max);
    std::exponential_distribution<double> runtime_exp(1.0 / p.runtime_mean);
    std::uniform_int_distribution<int> cores_dist(p.cores_min, p.cores_max);

    std::vector<Job> jobs;
    jobs.reserve(p.num_jobs);
    double t = 0.0;
    for (int i = 0; i < p.num_jobs; ++i) {
        Job j;
        j.id = i;
        t += interarrival(rng);
        j.submit_time = t;
        if (p.runtime_dist == "exponential") {
            j.runtime = std::min(std::max(runtime_exp(rng), p.runtime_min), p.runtime_max);
        } else {
            j.runtime = runtime_uniform(rng);
        }
        j.remaining = j.runtime;
        j.cores = cores_dist(rng);
        jobs.push_back(j);
    }
    return jobs;
}

std::vector<Job> load_trace_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("trace: cannot open " + path);

    std::vector<Job> jobs;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line.front() == '#') continue;
        Job j;
        if (std::sscanf(line.c_str(), "%d,%lf,%lf,%d", &j.id, &j.submit_time, &j.runtime,
                        &j.cores) == 4) {
            j.remaining = j.runtime;
            jobs.push_back(j);
        }
    }
    std::sort(jobs.begin(), jobs.end(),
              [](const Job& a, const Job& b) { return a.submit_time < b.submit_time; });
    // 模拟器要求 id == 下标，按到达顺序重新编号（原始 id 仅用于 trace 文件内引用）
    for (size_t i = 0; i < jobs.size(); ++i) jobs[i].id = static_cast<int>(i);
    return jobs;
}

}  // namespace hpcsim
