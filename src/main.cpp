#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "hpcsim/config.h"
#include "hpcsim/metrics.h"
#include "hpcsim/simulator.h"
#include "hpcsim/workload.h"

#ifdef HPCSIM_USE_MPI
#include "parallel/mpi_runner.h"
#endif

namespace {

void print_usage(const char* prog) {
    std::printf(
        "Usage: %s [options]\n"
        "  --config <path>     config file (default: configs/small.ini)\n"
        "  --scheduler <name>  fcfs | sjf | rr | backfill (overrides config)\n"
        "  --mode <m>          sim  : serial discrete-event simulation (default)\n"
        "                      exec : real execution via MPI+OpenMP (needs hpcsim_mpi build)\n"
        "  --output <dir>      output directory (default: results)\n"
        "  --tag <tag>         output file tag (default: scheduler name)\n"
        "  --help              show this message\n",
        prog);
}

}  // namespace

int main(int argc, char** argv) {
    std::string config_path = "configs/small.ini";
    std::string scheduler_override;
    std::string mode = "sim";
    std::string output_dir = "results";
    std::string tag;

    for (int i = 1; i < argc; ++i) {
        auto next = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", opt);
                std::exit(1);
            }
            return argv[++i];
        };
        if (std::strcmp(argv[i], "--config") == 0) {
            config_path = next("--config");
        } else if (std::strcmp(argv[i], "--scheduler") == 0) {
            scheduler_override = next("--scheduler");
        } else if (std::strcmp(argv[i], "--mode") == 0) {
            mode = next("--mode");
        } else if (std::strcmp(argv[i], "--output") == 0) {
            output_dir = next("--output");
        } else if (std::strcmp(argv[i], "--tag") == 0) {
            tag = next("--tag");
        } else {
            print_usage(argv[0]);
            return std::strcmp(argv[i], "--help") == 0 ? 0 : 1;
        }
    }

    try {
        hpcsim::Config cfg = hpcsim::Config::from_file(config_path);
        std::string sched_name =
            scheduler_override.empty() ? cfg.get("scheduler", "fcfs") : scheduler_override;

        if (mode == "exec") {
#ifdef HPCSIM_USE_MPI
            return hpcsim::run_mpi(cfg, sched_name, output_dir, tag);
#else
            std::fprintf(stderr,
                         "exec mode is not available in this binary; build with `make mpi` "
                         "and run via mpirun build/hpcsim_mpi ...\n");
            return 1;
#endif
        }
        if (mode != "sim") {
            std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
            return 1;
        }

        // ---- sim 模式 ----
        int nodes = cfg.get_int("nodes", 4);
        int cores_per_node = cfg.get_int("cores_per_node", 8);
        hpcsim::Cluster cluster(nodes, cores_per_node);

        hpcsim::WorkloadParams wp = hpcsim::workload_params_from_config(cfg);
        // 分配允许跨节点，上限为集群总核数
        wp.cores_max = std::min(wp.cores_max, cluster.total_cores());
        wp.cores_min = std::min(wp.cores_min, wp.cores_max);

        std::string trace = cfg.get("trace", "");
        std::vector<hpcsim::Job> jobs =
            trace.empty() ? hpcsim::generate_workload(wp) : hpcsim::load_trace_csv(trace);

        std::printf("[sim] scheduler=%s nodes=%d cores/node=%d jobs=%d\n", sched_name.c_str(),
                    nodes, cores_per_node, static_cast<int>(jobs.size()));

        double rr_quantum = cfg.get_double("rr_quantum", 50.0);
        hpcsim::Simulator sim(std::move(cluster),
                              hpcsim::make_scheduler(sched_name, rr_quantum));

        hpcsim::ensure_dir(output_dir);
        if (tag.empty()) tag = sched_name;
        hpcsim::SimOptions opt;
        opt.jobs_csv_path = output_dir + "/" + tag + "_jobs.csv";

        hpcsim::Summary s = sim.run(jobs, opt);
        hpcsim::MetricsCollector::print_summary(s);
        hpcsim::MetricsCollector::append_summary_csv(output_dir + "/summary.csv", s);
        std::printf("[sim] per-job csv : %s\n", opt.jobs_csv_path.c_str());
        std::printf("[sim] summary csv : %s/summary.csv\n", output_dir.c_str());
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
