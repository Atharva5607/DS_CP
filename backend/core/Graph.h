// Network model: machines are nodes, connections are weighted undirected edges
// (cost, risk, importance). Adjacency lists are custom LinkedLists.
#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "../ds/LinkedList.h"

enum class NodeType { PC, SERVER, ROUTER };
enum class Status { HEALTHY, INFECTED, ISOLATED, RECOVERED };

inline const char* typeName(NodeType t) {
    return t == NodeType::PC ? "PC" : t == NodeType::SERVER ? "SERVER" : "ROUTER";
}
inline const char* statusName(Status s) {
    switch (s) {
        case Status::HEALTHY: return "healthy";
        case Status::INFECTED: return "infected";
        case Status::ISOLATED: return "isolated";
        default: return "recovered";
    }
}

struct Edge { int to; int cost; int risk; int importance; bool blocked = false; };
struct UEdge { int u, v, cost, risk, importance; bool blocked; };

struct Machine {
    int id;
    std::string name;
    NodeType type;
    int importance;   // 1..10  business value of the machine
    int risk;         // 0..100 current exposure score (rises next to infections)
    int baseRisk;     // risk at creation, used by reset
    Status status;
};

// One animation/log event produced by an algorithm run. The frontend replays them.
struct Step { const char* type; int a; int b; int t; };

// Small deterministic RNG so every scenario is reproducible from a seed.
struct Rng {
    uint32_t s;
    explicit Rng(uint32_t seed = 1) : s(seed ? seed : 1) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
    double uni() { return (next() & 0xFFFFFF) / 16777216.0; }
};

class Graph {
public:
    std::vector<Machine> nodes;
    std::vector<LinkedList<Edge>> adj;

    int addNode(const std::string& name, NodeType t, int importance, int risk) {
        int id = (int)nodes.size();
        nodes.push_back(Machine{id, name, t, importance, risk, risk, Status::HEALTHY});
        adj.emplace_back();
        return id;
    }
    Edge* findEdge(int u, int v) const {
        for (auto& e : adj[u]) if (e.to == v) return &e;
        return nullptr;
    }
    bool hasEdge(int u, int v) const { return findEdge(u, v) != nullptr; }
    bool addEdgeFull(int u, int v, int cost, int risk, int imp, bool blocked) {
        if (u < 0 || v < 0 || u >= size() || v >= size() || u == v || hasEdge(u, v)) return false;
        adj[u].push_back(Edge{v, cost, risk, imp, blocked});
        adj[v].push_back(Edge{u, cost, risk, imp, blocked});
        return true;
    }
    bool addEdge(int u, int v, int cost, int risk) {
        if (u < 0 || v < 0 || u >= size() || v >= size()) return false;
        int imp = (nodes[u].importance + nodes[v].importance + 1) / 2;
        return addEdgeFull(u, v, cost, risk, imp, false);
    }
    // link-level isolation: a blocked link is ignored by every algorithm
    bool setBlocked(int u, int v, bool b) {
        Edge* a = findEdge(u, v); Edge* c = findEdge(v, u);
        if (!a || !c) return false;
        a->blocked = c->blocked = b;
        return true;
    }
    bool usable(const Edge& e) const { return !e.blocked && active(e.to); }
    int blockedCount() const {
        int c = 0;
        for (int u = 0; u < size(); ++u) for (auto& e : adj[u]) if (u < e.to && e.blocked) ++c;
        return c;
    }
    int degree(int v) const { return (int)adj[v].size(); }
    int size() const { return (int)nodes.size(); }
    bool active(int v) const { return nodes[v].status != Status::ISOLATED; }
    int activeDegree(int v) const {
        int d = 0;
        for (auto& e : adj[v]) if (usable(e)) ++d;
        return d;
    }
    std::vector<UEdge> edges() const {
        std::vector<UEdge> out;
        for (int u = 0; u < size(); ++u)
            for (auto& e : adj[u]) if (u < e.to) out.push_back({u, e.to, e.cost, e.risk, e.importance, e.blocked});
        return out;
    }
    void clear() { nodes.clear(); adj.clear(); }
};
