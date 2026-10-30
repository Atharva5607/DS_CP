// Tarjan's articulation points and bridges (one DFS with discovery / low-link
// values).  An articulation point is a machine whose loss splits the network;
// a bridge is a link whose loss does the same.  O(V + E)
#pragma once
#include <vector>
#include <utility>
#include <algorithm>
#include "../core/Graph.h"

struct CriticalInfo {
    std::vector<char> isCut;                          // per machine
    std::vector<std::pair<int, int>> bridges;         // (u, v) with u < v
};

inline CriticalInfo findCritical(const Graph& g) {
    int n = g.size();
    CriticalInfo out;
    out.isCut.assign(n, 0);
    std::vector<int> disc(n, 0), low(n, 0), parent(n, -1);
    std::vector<LinkedList<Edge>::Node*> cursor(n, nullptr);
    std::vector<int> stack;                           // explicit DFS stack (no recursion)
    std::vector<int> rootKids(n, 0);
    int timer = 0;
    for (int r = 0; r < n; ++r) {
        if (disc[r] || !g.active(r)) continue;
        disc[r] = low[r] = ++timer; cursor[r] = g.adj[r].head();
        stack.push_back(r);
        while (!stack.empty()) {
            int u = stack.back();
            if (cursor[u]) {
                Edge& e = cursor[u]->val; cursor[u] = cursor[u]->next;
                int v = e.to;
                if (!g.usable(e) || v == parent[u]) continue;
                if (disc[v]) { low[u] = std::min(low[u], disc[v]); continue; }
                parent[v] = u; disc[v] = low[v] = ++timer; cursor[v] = g.adj[v].head();
                if (u == r) ++rootKids[r];
                stack.push_back(v);
            } else {
                stack.pop_back();
                int p = parent[u];
                if (p < 0) continue;
                low[p] = std::min(low[p], low[u]);
                if (low[u] > disc[p]) out.bridges.push_back({std::min(p, u), std::max(p, u)});
                if (p != r && low[u] >= disc[p]) out.isCut[p] = 1;
            }
        }
        if (rootKids[r] > 1) out.isCut[r] = 1;
    }
    return out;
}
