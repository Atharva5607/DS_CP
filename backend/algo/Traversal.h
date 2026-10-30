// Graph traversal algorithms used by the simulator.
//  - bfsAttack : worm-style spread in waves              (Queue)
//  - dfsAttack : lateral movement that digs deep         (Stack, with backtracking)
//  - components: separate network segments after defence (BFS + Queue)
// Isolated machines and blocked links are never crossed.
#pragma once
#include <vector>
#include "../core/Graph.h"
#include "../ds/Queue.h"
#include "../ds/Stack.h"

// Probability that an attacker crosses an edge: edge risk scaled by aggressiveness.
inline double crossChance(const Edge& e, double aggr) {
    double p = (e.risk / 100.0) * aggr;
    return p > 1.0 ? 1.0 : p;
}

// BFS: every infected machine tries each neighbour once; waves spread level by level.
// O(V + E)
inline std::vector<Step> bfsAttack(Graph& g, int src, Rng& rng, double aggr) {
    std::vector<Step> steps;
    std::vector<int> depth(g.size(), 0);
    Queue<int> q;
    g.nodes[src].status = Status::INFECTED;
    steps.push_back({"infect", -1, src, 0});
    q.push(src);
    int u;
    while (q.pop(u)) {
        steps.push_back({"dequeue", u, -1, depth[u]});
        for (auto& e : g.adj[u]) {
            int v = e.to;
            if (e.blocked || g.nodes[v].status != Status::HEALTHY) continue;
            if (rng.uni() < crossChance(e, aggr)) {
                g.nodes[v].status = Status::INFECTED;
                depth[v] = depth[u] + 1;
                steps.push_back({"infect", u, v, depth[v]});
                q.push(v);
            } else {
                steps.push_back({"fail", u, v, depth[u] + 1});
            }
        }
    }
    return steps;
}

// DFS with an explicit stack: the attacker moves into the first machine it can
// compromise, keeps going deeper, and backtracks when stuck. O(V + E)
inline std::vector<Step> dfsAttack(Graph& g, int src, Rng& rng, double aggr) {
    std::vector<Step> steps;
    std::vector<int> depth(g.size(), 0);
    std::vector<LinkedList<Edge>::Node*> cursor(g.size(), nullptr);
    Stack<int> st;
    g.nodes[src].status = Status::INFECTED;
    steps.push_back({"infect", -1, src, 0});
    cursor[src] = g.adj[src].head();
    st.push(src);
    int u;
    while (st.top(u)) {
        bool moved = false;
        while (cursor[u]) {
            Edge& e = cursor[u]->val;
            cursor[u] = cursor[u]->next;
            int v = e.to;
            if (e.blocked || g.nodes[v].status != Status::HEALTHY) continue;
            if (rng.uni() < crossChance(e, aggr)) {
                g.nodes[v].status = Status::INFECTED;
                depth[v] = depth[u] + 1;
                steps.push_back({"infect", u, v, depth[v]});
                cursor[v] = g.adj[v].head();
                st.push(v);
                moved = true;
                break;
            }
            steps.push_back({"fail", u, v, depth[u] + 1});
        }
        if (!moved) { int x; st.pop(x); steps.push_back({"backtrack", x, -1, depth[x]}); }
    }
    return steps;
}

struct ConnInfo { int count = 0; int largest = 0; };

// Connected components over machines that are not isolated and links that are not
// blocked. comp[v] is the segment id (or -1 for isolated machines). O(V + E)
inline ConnInfo components(const Graph& g, std::vector<int>* comp = nullptr) {
    ConnInfo info;
    std::vector<int> id(g.size(), -1);
    for (int s = 0; s < g.size(); ++s) {
        if (id[s] != -1 || !g.active(s)) continue;
        int size = 0;
        Queue<int> q; q.push(s); id[s] = info.count;
        int u;
        while (q.pop(u)) {
            ++size;
            for (auto& e : g.adj[u])
                if (g.usable(e) && id[e.to] == -1) { id[e.to] = info.count; q.push(e.to); }
        }
        ++info.count;
        if (size > info.largest) info.largest = size;
    }
    if (comp) *comp = id;
    return info;
}
