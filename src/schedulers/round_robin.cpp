// Round-Robin：抢占式时间片轮转。
// 选取规则与 FCFS 相同（取队首），轮转性来自时间片机制：time_slice() > 0 时，
// 模拟器让作业最多连续运行一个时间片，到期触发 TimeSliceExpire、释放资源并
// 回到队尾，从而队列中的作业轮流获得执行机会。
//
// TODO(B): 1) 队首放不下时当前与 FCFS 一样阻塞，可讨论"跳过队首找第一个放得下"
//             的变体（注意会破坏轮转公平性，需在报告中说明取舍）；
//          2) 实验中扫描 rr_quantum（过小→上下文切换开销大，过大→退化为 FCFS）。
#include "hpcsim/scheduler.h"

namespace hpcsim {

class RoundRobinScheduler : public Scheduler {
public:
    explicit RoundRobinScheduler(double quantum) : quantum_(quantum) {}

    std::string name() const override { return "rr"; }
    double time_slice() const override { return quantum_; }

    Job* pick_next(const SchedContext& ctx) override {
        if (ctx.pending.empty()) return nullptr;
        Job* head = ctx.pending.front();
        if (head->cores > ctx.cluster.free_cores()) return nullptr;
        return head;
    }

private:
    double quantum_;
};

std::unique_ptr<Scheduler> make_round_robin(double quantum) {
    return std::unique_ptr<Scheduler>(new RoundRobinScheduler(quantum));
}

}  // namespace hpcsim
