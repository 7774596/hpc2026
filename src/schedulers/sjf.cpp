// SJF（Shortest Job First）：基础版实现。
// 当前语义：在"当前放得下"的作业中选 runtime（用户估计时长）最短者（非严格 SJF）。
//
// TODO(B): 1) 确认语义：严格 SJF（全局最短者放不下则阻塞）还是当前的非严格版本，
//             报告中需说明选择理由；
//          2) 增加防饥饿机制（如等待时间加权 aging），并在实验中展示长作业饥饿现象。
#include "hpcsim/scheduler.h"

namespace hpcsim {

class SjfScheduler : public Scheduler {
public:
    std::string name() const override { return "sjf"; }

    Job* pick_next(const SchedContext& ctx) override {
        Job* best = nullptr;
        for (Job* j : ctx.pending) {
            if (j->cores > ctx.cluster.free_cores()) continue;
            if (!best || j->runtime < best->runtime) best = j;
        }
        return best;
    }
};

std::unique_ptr<Scheduler> make_sjf() {
    return std::unique_ptr<Scheduler>(new SjfScheduler());
}

}  // namespace hpcsim
