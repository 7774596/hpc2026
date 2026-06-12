#pragma once

#include <functional>
#include <queue>
#include <vector>

namespace hpcsim {

enum class EventType {
    JobArrival,       // 作业到达，进入等待队列
    JobFinish,        // 作业执行完成，释放资源
    TimeSliceExpire,  // 时间片到期（RR 等抢占式策略），作业回到队尾
};

struct Event {
    double time = 0.0;
    EventType type = EventType::JobArrival;
    int job_id = -1;

    // 同一时刻的处理顺序：先 Finish/Expire（释放资源）再 Arrival（接新作业），
    // 避免人为推迟本可立即启动的作业
    static int rank(EventType t) {
        switch (t) {
            case EventType::JobFinish: return 0;
            case EventType::TimeSliceExpire: return 1;
            default: return 2;  // JobArrival
        }
    }

    // 最小堆比较：时间优先，其次事件类型，最后按 job_id 保证确定性
    bool operator>(const Event& o) const {
        if (time != o.time) return time > o.time;
        if (type != o.type) return rank(type) > rank(o.type);
        return job_id > o.job_id;
    }
};

// C++11 下 Event 含默认成员初始化器、不是聚合类型，统一用本助手构造
inline Event make_event(double time, EventType type, int job_id) {
    Event e;
    e.time = time;
    e.type = type;
    e.job_id = job_id;
    return e;
}

using EventQueue = std::priority_queue<Event, std::vector<Event>, std::greater<Event>>;

}  // namespace hpcsim
