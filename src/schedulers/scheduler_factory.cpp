#include <stdexcept>

#include "hpcsim/scheduler.h"

namespace hpcsim {

std::unique_ptr<Scheduler> make_fcfs();
std::unique_ptr<Scheduler> make_sjf();
std::unique_ptr<Scheduler> make_round_robin(double quantum);
std::unique_ptr<Scheduler> make_backfill();

std::unique_ptr<Scheduler> make_scheduler(const std::string& name, double rr_quantum) {
    if (name == "fcfs") return make_fcfs();
    if (name == "sjf") return make_sjf();
    if (name == "rr" || name == "round_robin") {
        if (rr_quantum <= 0) throw std::invalid_argument("rr_quantum must be positive");
        return make_round_robin(rr_quantum);
    }
    if (name == "backfill" || name == "easy") return make_backfill();
    throw std::invalid_argument("unknown scheduler: " + name +
                                " (expected fcfs|sjf|rr|backfill)");
}

std::vector<std::string> scheduler_names() {
    std::vector<std::string> names;
    names.push_back("fcfs");
    names.push_back("sjf");
    names.push_back("rr");
    names.push_back("backfill");
    return names;
}

}  // namespace hpcsim
