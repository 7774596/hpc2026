// MPI 主从执行器（exec 模式）。
//
// 协议：Master(rank 0) --TAG_JOB(JobMsg)--> Worker(rank 1..N)
//      Worker --TAG_DONE(DoneMsg)--> Master
//      Master --TAG_STOP--> Worker（全部作业完成后）
//
// 时间映射：模拟时间 * exec_time_scale = 真实墙钟时间。
// 例如 exec_time_scale=0.01 时，runtime=300s 的作业实际执行 3s。
//
// v1 限制（报告中需说明，后续迭代）：
//   TODO(A): 1) 每个 Worker 同时只执行一个作业（节点独占式），尚未支持节点内
//               多作业并发共享核，因此 Master 端按"节点忙/闲"调度而非按核调度；
//            2) Master 端当前用 FCFS 派发，待接入 Scheduler 接口以支持四种策略；
//            3) 增加更完整的节点级利用率统计。
#include "mpi_runner.h"

#ifdef HPCSIM_USE_MPI

#include <mpi.h>

#include <chrono>
#include <cstdio>
#include <deque>
#include <fstream>
#include <limits>
#include <thread>
#include <vector>

#include "hpcsim/metrics.h"
#include "hpcsim/workload.h"
#include "kernel.h"

namespace hpcsim {

namespace {

constexpr int TAG_JOB = 1;
constexpr int TAG_DONE = 2;
constexpr int TAG_STOP = 3;

struct JobMsg {
    int id;
    int cores;
    double wall_seconds;  // 已按 exec_time_scale 折算的真实执行时长
};

struct DoneMsg {
    int id;
    double wall_elapsed;
};

void write_exec_jobs_csv(const std::string& path, const std::vector<Job>& jobs) {
    std::ofstream out(path);
    if (!out) {
        std::fprintf(stderr, "[warn] cannot write %s\n", path.c_str());
        return;
    }
    out << "job_id,submit,start,finish,wait,turnaround,cores,worker\n";
    for (const Job& j : jobs) {
        out << j.id << ',' << j.submit_time << ',' << j.start_time << ',' << j.finish_time << ','
            << (j.start_time - j.submit_time) << ',' << (j.finish_time - j.submit_time) << ','
            << j.cores << ',' << j.first_node() << '\n';
    }
}

void append_exec_summary_csv(const std::string& path, const std::string& tag,
                             const std::string& scheduler_name, int num_workers,
                             int num_jobs, double time_scale, double makespan,
                             double avg_wait, double avg_turnaround, double throughput,
                             double wall_time) {
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
        out << "tag,scheduler,num_workers,num_jobs,time_scale,makespan,avg_wait,"
               "avg_turnaround,throughput,wall_time\n";
    }
    out << tag << ',' << scheduler_name << ',' << num_workers << ',' << num_jobs << ','
        << time_scale << ',' << makespan << ',' << avg_wait << ',' << avg_turnaround << ','
        << throughput << ',' << wall_time << '\n';
}

void run_worker() {
    while (true) {
        MPI_Status st;
        MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
        if (st.MPI_TAG == TAG_STOP) {
            MPI_Recv(nullptr, 0, MPI_BYTE, 0, TAG_STOP, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            break;
        }
        JobMsg m;
        MPI_Recv(&m, sizeof(m), MPI_BYTE, 0, TAG_JOB, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        double t0 = MPI_Wtime();
        busy_kernel(m.wall_seconds, m.cores);
        DoneMsg d{m.id, MPI_Wtime() - t0};
        MPI_Send(&d, sizeof(d), MPI_BYTE, 0, TAG_DONE, MPI_COMM_WORLD);
    }
}

int run_master(const Config& cfg, const std::string& scheduler_name, int num_workers,
               const std::string& output_dir, const std::string& tag) {
    WorkloadParams wp = workload_params_from_config(cfg);
    const double scale = cfg.get_double("exec_time_scale", 0.01);
    std::vector<Job> jobs = generate_workload(wp);

    std::printf("[master] workers=%d jobs=%d time_scale=%g scheduler=%s(FCFS dispatch for now)\n",
                num_workers, static_cast<int>(jobs.size()), scale, scheduler_name.c_str());

    std::vector<char> busy(num_workers + 1, 0);  // 按 rank 下标，rank 0 不用
    std::deque<Job*> queue;
    size_t next_arrival = 0;
    size_t completed = 0;

    const double t0 = MPI_Wtime();
    auto now_sim = [&]() { return (MPI_Wtime() - t0) / scale; };

    while (completed < jobs.size()) {
        // 1) 按模拟时钟推进作业到达
        double now = now_sim();
        while (next_arrival < jobs.size() && jobs[next_arrival].submit_time <= now) {
            queue.push_back(&jobs[next_arrival++]);
        }

        // 2) 回收已完成作业
        int flag = 1;
        while (flag) {
            MPI_Status st;
            MPI_Iprobe(MPI_ANY_SOURCE, TAG_DONE, MPI_COMM_WORLD, &flag, &st);
            if (!flag) break;
            DoneMsg d;
            MPI_Recv(&d, sizeof(d), MPI_BYTE, st.MPI_SOURCE, TAG_DONE, MPI_COMM_WORLD,
                     MPI_STATUS_IGNORE);
            busy[st.MPI_SOURCE] = 0;
            jobs[d.id].finish_time = now_sim();
            jobs[d.id].state = JobState::Finished;
            ++completed;
        }

        // 3) 向空闲 Worker 派发（FCFS）
        while (!queue.empty()) {
            int free_rank = -1;
            for (int r = 1; r <= num_workers; ++r) {
                if (!busy[r]) {
                    free_rank = r;
                    break;
                }
            }
            if (free_rank < 0) break;

            Job* j = queue.front();
            queue.pop_front();
            JobMsg m{j->id, j->cores, j->runtime * scale};
            MPI_Send(&m, sizeof(m), MPI_BYTE, free_rank, TAG_JOB, MPI_COMM_WORLD);
            j->start_time = now_sim();
            j->last_start = j->start_time;
            j->allocation.assign(1, {free_rank, j->cores});
            j->state = JobState::Running;
            busy[free_rank] = 1;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int r = 1; r <= num_workers; ++r) {
        MPI_Send(nullptr, 0, MPI_BYTE, r, TAG_STOP, MPI_COMM_WORLD);
    }

    // 汇总（模拟时间口径）
    double min_submit = std::numeric_limits<double>::max();
    double sum_wait = 0.0, sum_turn = 0.0, max_finish = 0.0;
    for (const Job& j : jobs) {
        if (j.submit_time < min_submit) min_submit = j.submit_time;
        sum_wait += j.start_time - j.submit_time;
        sum_turn += j.finish_time - j.submit_time;
        if (j.finish_time > max_finish) max_finish = j.finish_time;
    }
    const double makespan = max_finish - min_submit;
    const double avg_wait = sum_wait / jobs.size();
    const double avg_turnaround = sum_turn / jobs.size();
    const double throughput = makespan > 0 ? jobs.size() / makespan : 0.0;
    const double wall_time = MPI_Wtime() - t0;
    std::printf("==== Exec Mode Summary (simulated-time metrics) ====\n");
    std::printf("jobs finished    : %zu\n", jobs.size());
    std::printf("makespan         : %.2f s\n", makespan);
    std::printf("avg wait         : %.2f s\n", avg_wait);
    std::printf("avg turnaround   : %.2f s\n", avg_turnaround);
    std::printf("throughput       : %.4f jobs/s\n", throughput);
    std::printf("wall time        : %.2f s\n", wall_time);

    if (!output_dir.empty()) {
        ensure_dir(output_dir);
        const std::string out_tag = tag.empty() ? "exec" : tag;
        append_exec_summary_csv(output_dir + "/exec_summary.csv", out_tag, scheduler_name,
                                num_workers, static_cast<int>(jobs.size()), scale, makespan,
                                avg_wait, avg_turnaround, throughput, wall_time);
        write_exec_jobs_csv(output_dir + "/" + out_tag + "_exec_jobs.csv", jobs);
        std::printf("[exec] per-job csv : %s/%s_exec_jobs.csv\n", output_dir.c_str(),
                    out_tag.c_str());
        std::printf("[exec] summary csv : %s/exec_summary.csv\n", output_dir.c_str());
    }
    return 0;
}

}  // namespace

int run_mpi(const Config& cfg, const std::string& scheduler_name,
            const std::string& output_dir, const std::string& tag) {
    MPI_Init(nullptr, nullptr);
    int rank = 0, world = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world);

    int ret = 0;
    if (world < 2) {
        if (rank == 0) std::fprintf(stderr, "exec mode needs at least 2 MPI ranks (1 master + 1 worker)\n");
        ret = 1;
    } else if (rank == 0) {
        ret = run_master(cfg, scheduler_name, world - 1, output_dir, tag);
    } else {
        run_worker();
    }

    MPI_Finalize();
    return ret;
}

}  // namespace hpcsim

#endif  // HPCSIM_USE_MPI
