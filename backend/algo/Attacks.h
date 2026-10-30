// Five attack models. All of them respect isolated machines and blocked links.
//   bfs    worm: spreads in waves                      (Queue)
//   dfs    insider: lateral movement with backtracking (Stack)
//   apt    persistent attacker follows the easiest route to a target (Dijkstra)
//   hub    targeted failure: knocks out the k best-connected machines (max-heap)
//   random random failure: knocks out k machines picked uniformly
#pragma once
#include <string>
#include <vector>
#include "Traversal.h"
#include "ShortestPath.h"
#include "../ds/Heap.h"

inline std::vector<Step> aptAttack(Graph& g, int src, int target, Rng& rng, double aggr) {
    std::vector<Step> steps;
    g.nodes[src].status = Status::INFECTED;
    steps.push_back({"infect", -1, src, 0});
    PathResult p = dijkstra(g, src, target, Weight::EASE);
    if (!p.found) { steps.push_back({"stuck", src, -1, 0}); return steps; }
    for (size_t i = 1; i < p.path.size(); ++i) {
        int u = p.path[i - 1], v = p.path[i];
        Edge* e = g.findEdge(u, v);
        Machine& m = g.nodes[v];
        if (m.status == Status::INFECTED) continue;      // already compromised, keep walking
        if (m.status != Status::HEALTHY) break;          // patched or isolated: attacker is stopped
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; ++attempt) {   // persistent: three tries per hop
            if (rng.uni() < crossChance(*e, aggr)) ok = true;
            else steps.push_back({"fail", u, v, (int)i});
        }
        if (!ok) break;
        m.status = Status::INFECTED;
        steps.push_back({"infect", u, v, (int)i});
    }
    return steps;
}

// Targeted failure: take the k healthy machines with the most usable links.
inline std::vector<Step> hubAttack(Graph& g, int k) {
    struct Item { int deg; int id; };
    struct Before { bool operator()(const Item& a, const Item& b) const { return a.deg != b.deg ? a.deg > b.deg : a.id < b.id; } };
    Heap<Item, Before> pq;
    for (int v = 0; v < g.size(); ++v)
        if (g.nodes[v].status == Status::HEALTHY) pq.push({g.activeDegree(v), v});
    std::vector<Step> steps;
    Item it;
    for (int rank = 0; rank < k && pq.pop(it); ++rank) {
        g.nodes[it.id].status = Status::INFECTED;
        steps.push_back({"infect", -1, it.id, rank});
    }
    return steps;
}

// Random failure: partial Fisher-Yates shuffle, take the first k.
inline std::vector<Step> randomAttack(Graph& g, int k, Rng& rng) {
    std::vector<int> pool;
    for (int v = 0; v < g.size(); ++v) if (g.nodes[v].status == Status::HEALTHY) pool.push_back(v);
    std::vector<Step> steps;
    for (int rank = 0; rank < k && rank < (int)pool.size(); ++rank) {
        int j = rank + rng.range(0, (int)pool.size() - 1 - rank);
        std::swap(pool[rank], pool[j]);
        g.nodes[pool[rank]].status = Status::INFECTED;
        steps.push_back({"infect", -1, pool[rank], rank});
    }
    return steps;
}

inline bool needsSource(const std::string& mode) { return mode != "hub" && mode != "random"; }

inline std::vector<Step> runAttack(Graph& g, const std::string& mode, int src, int target, int count,
                                   Rng& rng, double aggr) {
    if (mode == "dfs") return dfsAttack(g, src, rng, aggr);
    if (mode == "apt") return aptAttack(g, src, target, rng, aggr);
    if (mode == "hub") return hubAttack(g, count);
    if (mode == "random") return randomAttack(g, count, rng);
    return bfsAttack(g, src, rng, aggr);
}

inline int countInfections(const std::vector<Step>& steps) {
    int n = 0;
    for (auto& s : steps) if (std::string(s.type) == "infect") ++n;
    return n;
}
