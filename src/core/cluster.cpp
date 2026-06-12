#include "hpcsim/cluster.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace hpcsim {

Cluster::Cluster(int num_nodes, int cores_per_node) {
    if (num_nodes <= 0 || cores_per_node <= 0) {
        throw std::invalid_argument("cluster: nodes and cores_per_node must be positive");
    }
    nodes_.reserve(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
        Node n;
        n.id = i;
        n.total_cores = cores_per_node;
        n.free_cores = cores_per_node;
        nodes_.push_back(n);
    }
    total_cores_ = num_nodes * cores_per_node;
    free_cores_ = total_cores_;
}

bool Cluster::try_allocate(Job& job) {
    if (job.cores > free_cores_) return false;

    job.allocation.clear();
    int need = job.cores;
    for (Node& n : nodes_) {
        if (need == 0) break;
        if (n.free_cores == 0) continue;
        int take = std::min(n.free_cores, need);
        n.free_cores -= take;
        need -= take;
        job.allocation.push_back(std::make_pair(n.id, take));
    }
    assert(need == 0 && "free_cores_ bookkeeping out of sync");
    free_cores_ -= job.cores;
    return true;
}

void Cluster::release(const Job& job, double elapsed) {
    for (const std::pair<int, int>& a : job.allocation) {
        Node& n = nodes_.at(a.first);
        n.free_cores += a.second;
        assert(n.free_cores <= n.total_cores && "release: core count overflow");
        n.busy_core_seconds += a.second * elapsed;
    }
    free_cores_ += job.cores;
}

}  // namespace hpcsim
