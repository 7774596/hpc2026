// EASY Backfilling。
// 思路：队首作业放不下时，先为它计算"影子时刻"（shadow time，按运行中作业的
// 预计完成时间推算队首最早可启动的时刻），随后允许后面的作业插队，
// 条件是插队作业预计在影子时刻之前完成，从而不推迟队首作业。
//
// 由于分配允许跨节点，"放得下"只取决于总空闲核数，影子时刻按全局核数
// 累计即可，插队作业满足时间条件就不会推迟队首（无需节点级预留校验）。
//
// TODO(B): 1) runtime 同时是真实时长与用户估计（完美估计假设），如需研究估计
//             误差对 Backfilling 的影响，与 A 讨论在 Job 中增加独立的估计字段；
//          2) 与 fcfs 在相同负载下对比利用率提升，作为报告中的重点实验。
#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "hpcsim/scheduler.h"

namespace hpcsim {

class BackfillScheduler : public Scheduler {
public:
    std::string name() const override { return "backfill"; }

    Job* pick_next(const SchedContext& ctx) override {
        if (ctx.pending.empty()) return nullptr;

        Job* head = ctx.pending.front();
        if (head->cores <= ctx.cluster.free_cores()) return head;

        const double shadow = shadow_time(ctx, *head);
        for (size_t i = 1; i < ctx.pending.size(); ++i) {
            Job* j = ctx.pending[i];
            if (j->cores > ctx.cluster.free_cores()) continue;
            if (ctx.now + j->remaining <= shadow) return j;
        }
        return nullptr;
    }

private:
    // 估算队首作业最早可启动时刻：按预计完成时间（last_start + remaining，
    // 完美估计假设）排序运行中作业，逐个累计释放核数直到总空闲核数足够。
    static double shadow_time(const SchedContext& ctx, const Job& head) {
        std::vector<std::pair<double, int>> releases;  // (预计完成时刻, 释放核数)
        releases.reserve(ctx.running.size());
        for (const Job* r : ctx.running) {
            releases.push_back(std::make_pair(r->last_start + r->remaining, r->cores));
        }
        std::sort(releases.begin(), releases.end());

        int free_cores = ctx.cluster.free_cores();
        for (const std::pair<double, int>& rel : releases) {
            free_cores += rel.second;
            if (free_cores >= head.cores) return rel.first;
        }
        return std::numeric_limits<double>::max();
    }
};

std::unique_ptr<Scheduler> make_backfill() {
    return std::unique_ptr<Scheduler>(new BackfillScheduler());
}

}  // namespace hpcsim
