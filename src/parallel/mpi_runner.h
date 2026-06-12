#pragma once

#include <string>

#include "hpcsim/config.h"

namespace hpcsim {

// exec 模式入口（仅在 make mpi 构建中可用）：
// rank 0 作为调度主控（Master），其余每个 rank 模拟一个计算节点（Worker），
// Worker 收到作业后用 OpenMP 多线程真实执行合成计算负载。
// 内部完成 MPI_Init / MPI_Finalize，返回进程退出码。
int run_mpi(const Config& cfg, const std::string& scheduler_name);

}  // namespace hpcsim
