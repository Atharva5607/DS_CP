// Dijkstra's algorithm on top of our own min-heap.  O((V + E) log V)
//   COST : defender's cheapest route (edge cost)
//   EASE : attacker's easiest route; risky (weak) links are cheap for the attacker
// Isolated machines are never used.
#pragma once
#include <vector>
#include <limits>
#include "../core/Graph.h"
#include "../ds/Heap.h"

enum class Weight { COST, EASE };

struct PathResult {
    bool found = false;
    long long total = 0;
    std::vector<int> path;
    std::vector<Step> steps;
};

inline int edgeWeight(const Edge& e, Weight w) {
    if (w == Weight::COST) return e.cost;
    int x = 101 - e.risk;
    return x < 1 ? 1 : x;
}

inline PathResult dijkstra(const Graph& g, int s, int t, Weight w) {
    struct QE { long long d; int v; };
    struct Before { bool operator()(const QE& a, const QE& b) const { return a.d < b.d; } };
    PathResult res;
    if (!g.active(s) || (t >= 0 && !g.active(t))) return res;
    const long long INF = std::numeric_limits<long long>::max() / 4;
    std::vector<long long> dist(g.size(), INF);
    std::vector<int> prev(g.size(), -1);
    std::vector<char> done(g.size(), 0);
    Heap<QE, Before> pq;
    dist[s] = 0; pq.push({0, s});
    QE cur;
    while (pq.pop(cur)) {
        int u = cur.v;
        if (done[u]) continue;            // stale heap entry (lazy deletion)
        done[u] = 1;
        res.steps.push_back({"visit", u, -1, (int)cur.d});
        if (t >= 0 && u == t) break;
        for (auto& e : g.adj[u]) {
            if (!g.usable(e) || done[e.to]) continue;
            long long nd = dist[u] + edgeWeight(e, w);
            if (nd < dist[e.to]) {
                dist[e.to] = nd; prev[e.to] = u;
                pq.push({nd, e.to});
                res.steps.push_back({"relax", u, e.to, (int)nd});
            }
        }
    }
    if (t < 0 || dist[t] >= INF) return res;
    res.found = true; res.total = dist[t];
    for (int v = t; v != -1; v = prev[v]) res.path.insert(res.path.begin(), v);
    return res;
}
