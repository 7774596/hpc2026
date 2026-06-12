#include "hpcsim/simulator.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "hpcsim/event.h"

namespace hpcsim {

Simulator::Simulator(Cluster cluster, std::unique_ptr<Scheduler> scheduler)
    : cluster_(std::move(cluster)), scheduler_(std::move(scheduler)) {}

Summary Simulator::run(std::vector<Job>& jobs, const SimOptions& opt) {
    EventQueue eq;
    for (size_t i = 0; i < jobs.size(); ++i) {
        assert(jobs[i].id == static_cast<int>(i) && "simulator requires jobs[i].id == i");
        jobs[i].remaining = jobs[i].runtime;
        eq.push(make_event(jobs[i].submit_time, EventType::JobArrival, jobs[i].id));
    }

    std::vector<Job*> pending;
    std::vector<Job*> running;
    MetricsCollector metrics;
    const double slice = scheduler_->time_slice();
    double now = 0.0;

    while (!eq.empty()) {
        Event ev = eq.top();
        eq.pop();
        now = ev.time;
        Job* j = &jobs[ev.job_id];

        switch (ev.type) {
            case EventType::JobArrival:
                j->state = JobState::Waiting;
                pending.push_back(j);
                break;
            case EventType::JobFinish:
                // allocation 保留作为运行历史，供指标/调试使用
                cluster_.release(*j, now - j->last_start);
                j->remaining = 0.0;
                j->state = JobState::Finished;
                running.erase(std::find(running.begin(), running.end(), j));
                metrics.record_job(*j);
                break;
            case EventType::TimeSliceExpire:
                // 时间片用完：释放资源、扣减剩余时长、回到队尾等待下一轮
                cluster_.release(*j, now - j->last_start);
                j->remaining -= now - j->last_start;
                j->allocation.clear();
                j->state = JobState::Waiting;
                running.erase(std::find(running.begin(), running.end(), j));
                pending.push_back(j);
                break;
        }

        // 资源或队列状态变化后，反复询问调度器直到无作业可派发
        while (true) {
            SchedContext ctx{now, pending, cluster_, running};
            Job* pick = scheduler_->pick_next(ctx);
            if (!pick) break;

            std::vector<Job*>::iterator it = std::find(pending.begin(), pending.end(), pick);
            assert(it != pending.end() && "scheduler returned a job not in pending");

            bool ok = cluster_.try_allocate(*pick);
            assert(ok && "scheduler violated contract: cores > cluster.free_cores()");
            if (!ok) break;  // 防御：避免违反契约时死循环

            if (pick->start_time < 0) pick->start_time = now;  // 首次启动
            pick->last_start = now;
            pick->state = JobState::Running;
            pending.erase(it);
            running.push_back(pick);

            if (slice > 0.0 && pick->remaining > slice) {
                eq.push(make_event(now + slice, EventType::TimeSliceExpire, pick->id));
            } else {
                pick->finish_time = now + pick->remaining;
                eq.push(make_event(pick->finish_time, EventType::JobFinish, pick->id));
            }
        }
    }

    Summary s = metrics.finalize(cluster_, scheduler_->name());
    if (!opt.jobs_csv_path.empty()) metrics.write_jobs_csv(opt.jobs_csv_path);
    return s;
}

}  // namespace hpcsim
