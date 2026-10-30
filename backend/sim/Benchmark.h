// Performance comparison of the algorithms across network sizes and topologies.
// Times are averages in microseconds over several repetitions.
#pragma once
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include "../core/Topology.h"
#include "../algo/Traversal.h"
#include "../algo/ShortestPath.h"
#include "../algo/SpanningTree.h"
#include "../algo/Articulation.h"

struct BenchRow {
    std::string label; int n = 0; int edges = 0;
    double bfs = 0, dfs = 0, dijkstra = 0, kruskal = 0, prim = 0, critical = 0;
};

template <typename F>
double timeMicros(F f, int reps) {
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) f();
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / reps;
}

inline BenchRow benchOne(const std::string& topo, int n, uint32_t seed) {
    Rng rng(seed);
    Graph g;
    generateGraph(g, topo, n, rng);
    BenchRow r; r.label = topo; r.n = n; r.edges = (int)g.edges().size();
    int reps = std::max(3, 150000 / (n + r.edges));
    auto reset = [&]() { for (auto& m : g.nodes) m.status = Status::HEALTHY; };
    Rng a(1);
    r.bfs = timeMicros([&]() { reset(); bfsAttack(g, 0, a, 1000.0); }, reps);
    r.dfs = timeMicros([&]() { reset(); dfsAttack(g, 0, a, 1000.0); }, reps);
    reset();
    r.dijkstra = timeMicros([&]() { dijkstra(g, 0, -1, Weight::COST); }, reps);
    r.kruskal = timeMicros([&]() { kruskal(g); }, reps);
    r.prim = timeMicros([&]() { prim(g); }, reps);
    r.critical = timeMicros([&]() { findCritical(g); }, reps);
    return r;
}

inline void benchJson(std::ostringstream& o, const BenchRow& r) {
    o << "{\"label\":\"" << r.label << "\",\"n\":" << r.n << ",\"edges\":" << r.edges << std::fixed
      << std::setprecision(1) << ",\"bfs\":" << r.bfs << ",\"dfs\":" << r.dfs << ",\"dijkstra\":" << r.dijkstra
      << ",\"kruskal\":" << r.kruskal << ",\"prim\":" << r.prim << ",\"critical\":" << r.critical << "}";
}

inline std::string benchmarkJson(const std::string& topo, uint32_t seed) {
    const int sizes[] = {50, 100, 200, 400, 800, 1600, 3200};
    const char* topos[] = {"random", "mesh", "ring", "star", "tree", "dense"};
    std::ostringstream o;
    o << "{\"topology\":\"" << topo << "\",\"scaling\":[";
    bool first = true;
    for (int n : sizes) { o << (first ? "" : ","); first = false; benchJson(o, benchOne(topo, n, seed)); }
    o << "],\"compare\":[";
    first = true;
    for (const char* t : topos) { o << (first ? "" : ","); first = false; benchJson(o, benchOne(t, 800, seed)); }
    o << "]}";
    return o.str();
}
