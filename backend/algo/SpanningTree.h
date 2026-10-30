// Minimum spanning trees: the cheapest set of links that reconnects every
// surviving machine (isolated machines and blocked links are ignored).
//   Kruskal: edges leave a heap in cost order, a DSU rejects cycles.   O(E log E)
//   Prim   : grow one tree outward, always taking the cheapest frontier link
//            from a heap.                                              O(E log V)
// Both count their heap / union-find operations so the two can be compared.
#pragma once
#include <vector>
#include "../core/Graph.h"
#include "../ds/Heap.h"
#include "../ds/DSU.h"

struct MstResult {
    std::vector<UEdge> edges;
    long long cost = 0;          // cost of the recovery backbone
    long long fullCost = 0;      // cost of all usable links between surviving machines
    int components = 0;          // >1 means the survivors cannot be fully reconnected
    long long heapOps = 0;       // pushes + pops
    long long dsuOps = 0;        // union / find calls (Kruskal only)
};

inline int aliveCount(const Graph& g) {
    int alive = 0;
    for (int i = 0; i < g.size(); ++i) if (g.active(i)) ++alive;
    return alive;
}

inline MstResult kruskal(const Graph& g) {
    struct Before { bool operator()(const UEdge& a, const UEdge& b) const { return a.cost < b.cost; } };
    MstResult r;
    Heap<UEdge, Before> pq;
    for (auto& e : g.edges())
        if (!e.blocked && g.active(e.u) && g.active(e.v)) { pq.push(e); ++r.heapOps; r.fullCost += e.cost; }
    DSU dsu(g.size());
    UEdge e;
    while (pq.pop(e)) {
        ++r.heapOps; ++r.dsuOps;
        if (dsu.unite(e.u, e.v)) { r.edges.push_back(e); r.cost += e.cost; }
    }
    r.components = aliveCount(g) - (int)r.edges.size();
    return r;
}

inline MstResult prim(const Graph& g) {
    struct Cand { int cost; int from; int to; int risk; int imp; };
    struct Before { bool operator()(const Cand& a, const Cand& b) const { return a.cost < b.cost; } };
    MstResult r;
    for (auto& e : g.edges())
        if (!e.blocked && g.active(e.u) && g.active(e.v)) r.fullCost += e.cost;
    std::vector<char> in(g.size(), 0);
    int comps = 0;
    for (int s = 0; s < g.size(); ++s) {
        if (in[s] || !g.active(s)) continue;
        ++comps;                                   // new tree in the forest
        Heap<Cand, Before> pq;
        auto grow = [&](int v) {
            in[v] = 1;
            for (auto& e : g.adj[v])
                if (g.usable(e) && !in[e.to]) { pq.push({e.cost, v, e.to, e.risk, e.importance}); ++r.heapOps; }
        };
        grow(s);
        Cand c;
        while (pq.pop(c)) {
            ++r.heapOps;
            if (in[c.to]) continue;                // stale candidate (lazy deletion)
            r.edges.push_back({c.from < c.to ? c.from : c.to, c.from < c.to ? c.to : c.from, c.cost, c.risk, c.imp, false});
            r.cost += c.cost;
            grow(c.to);
        }
    }
    r.components = comps;
    return r;
}
