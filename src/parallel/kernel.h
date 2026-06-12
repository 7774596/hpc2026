#pragma once

namespace hpcsim {

// 合成计算负载：用 num_threads 个 OpenMP 线程做浮点运算，持续 wall_seconds 真实秒，
// 用于在 exec 模式下"真实地"占用节点的多核资源。
void busy_kernel(double wall_seconds, int num_threads);

}  // namespace hpcsim
