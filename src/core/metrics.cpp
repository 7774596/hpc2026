#include "hpcsim/metrics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace hpcsim {

void ensure_dir(const std::string& path) {
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

Summary MetricsCollector::finalize(const Cluster& cluster, const std::string& scheduler_name) const {
    Summary s;
    s.scheduler = scheduler_name;
    s.num_jobs = static_cast<int>(finished_.size());
    if (finished_.empty()) return s;

    double min_submit = std::numeric_limits<double>::max();
    double max_finish = 0.0;
    double sum_wait = 0.0, sum_turn = 0.0;
    for (const Job& j : finished_) {
        min_submit = std::min(min_submit, j.submit_time);
        max_finish = std::max(max_finish, j.finish_time);
        sum_wait += j.wait_time();
        sum_turn += j.turnaround();
    }
    s.makespan = max_finish - min_submit;
    s.avg_wait = sum_wait / s.num_jobs;
    s.avg_turnaround = sum_turn / s.num_jobs;
    s.throughput = s.makespan > 0 ? s.num_jobs / s.makespan : 0.0;

    double busy_total = 0.0, busy_mean = 0.0, busy_var = 0.0;
    for (const Node& n : cluster.nodes()) busy_total += n.busy_core_seconds;
    busy_mean = busy_total / cluster.num_nodes();
    for (const Node& n : cluster.nodes()) {
        double d = n.busy_core_seconds - busy_mean;
        busy_var += d * d;
    }
    busy_var /= cluster.num_nodes();
    s.utilization =
        s.makespan > 0 ? busy_total / (cluster.total_cores() * s.makespan) : 0.0;
    s.load_balance_cv = busy_mean > 0 ? std::sqrt(busy_var) / busy_mean : 0.0;
    return s;
}

void MetricsCollector::write_jobs_csv(const std::string& path) const {
    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "[warn] cannot write %s\n", path.c_str());
        return;
    }
    // node 列为作业的首个分配节点；跨节点作业的完整 allocation 暂不落盘
    out << "job_id,submit,start,finish,wait,turnaround,cores,node\n";
    for (const Job& j : finished_) {
        int first_node = j.first_node();
        out << j.id << ',' << j.submit_time << ',' << j.start_time << ',' << j.finish_time << ','
            << j.wait_time() << ',' << j.turnaround() << ',' << j.cores << ',' << first_node
            << '\n';
    }
}

void MetricsCollector::append_summary_csv(const std::string& path, const Summary& s) {
    bool need_header = true;
    {
        std::ifstream probe(path);
        need_header = !probe.good() || probe.peek() == std::ifstream::traits_type::eof();
    }
    std::ofstream out(path, std::ios::app);
    if (!out) {
        std::fprintf(stderr, "[warn] cannot write %s\n", path.c_str());
        return;
    }
    if (need_header) {
        out << "scheduler,num_jobs,makespan,avg_wait,avg_turnaround,throughput,utilization,"
               "load_balance_cv\n";
    }
    out << s.scheduler << ',' << s.num_jobs << ',' << s.makespan << ',' << s.avg_wait << ','
        << s.avg_turnaround << ',' << s.throughput << ',' << s.utilization << ','
        << s.load_balance_cv << '\n';
}

void MetricsCollector::print_summary(const Summary& s) {
    std::printf("==== Simulation Summary ====\n");
    std::printf("scheduler        : %s\n", s.scheduler.c_str());
    std::printf("jobs finished    : %d\n", s.num_jobs);
    std::printf("makespan         : %.2f s\n", s.makespan);
    std::printf("avg wait         : %.2f s\n", s.avg_wait);
    std::printf("avg turnaround   : %.2f s\n", s.avg_turnaround);
    std::printf("throughput       : %.4f jobs/s\n", s.throughput);
    std::printf("utilization      : %.2f %%\n", s.utilization * 100.0);
    std::printf("load balance CV  : %.4f (lower is better)\n", s.load_balance_cv);
}

}  // namespace hpcsim
