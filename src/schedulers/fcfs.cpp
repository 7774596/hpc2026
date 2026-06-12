// FCFS（First-Come-First-Served）：参考实现，作为其他策略的对照基线。
// 严格 FCFS：队首作业放不下时整个队列阻塞等待（不允许后面的作业插队）。
#include "hpcsim/scheduler.h"

namespace hpcsim {

class FcfsScheduler : public Scheduler {
public:
    std::string name() const override { return "fcfs"; }

    Job* pick_next(const SchedContext& ctx) override {
        if (ctx.pending.empty()) return nullptr;
        Job* head = ctx.pending.front();
        if (head->cores > ctx.cluster.free_cores()) return nullptr;
        return head;
    }
};

std::unique_ptr<Scheduler> make_fcfs() {
    return std::unique_ptr<Scheduler>(new FcfsScheduler());
}

}  // namespace hpcsim
