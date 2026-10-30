// Topology generators shared by the simulator and the benchmark.
// Every generator returns a connected graph (random, ring, star, tree, mesh, dense).
#pragma once
#include <cmath>
#include <string>
#include "Graph.h"

inline void generateGraph(Graph& g, const std::string& topo, int n, Rng& rng) {
    g.clear();
    for (int i = 0; i < n; ++i) {
        int roll = rng.range(0, 99);
        NodeType t = roll < 15 ? NodeType::ROUTER : roll < 35 ? NodeType::SERVER : NodeType::PC;
        if (topo == "star" && i == 0) t = NodeType::ROUTER;
        std::string name; int imp, risk;
        if (t == NodeType::ROUTER)      { name = "RTR-"; imp = rng.range(7, 9);  risk = rng.range(15, 50); }
        else if (t == NodeType::SERVER) { name = "SRV-"; imp = rng.range(8, 10); risk = rng.range(20, 60); }
        else                            { name = "PC-";  imp = rng.range(1, 5);  risk = rng.range(40, 90); }
        g.addNode(name + std::to_string(i), t, imp, risk);
    }
    auto link = [&](int u, int v) { g.addEdge(u, v, rng.range(1, 20), rng.range(15, 95)); };
    if (topo == "ring") {
        for (int i = 0; i < n; ++i) link(i, (i + 1) % n);
        for (int k = 0; k < n / 6; ++k) link(rng.range(0, n - 1), rng.range(0, n - 1));
    } else if (topo == "star") {
        for (int i = 1; i < n; ++i) link(0, i);
        for (int k = 0; k < n / 8; ++k) link(rng.range(1, n - 1), rng.range(1, n - 1));
    } else if (topo == "tree") {
        for (int i = 1; i < n; ++i) link(i, (i - 1) / 2);
    } else if (topo == "mesh") {
        int cols = (int)std::ceil(std::sqrt((double)n));
        for (int i = 0; i < n; ++i) {
            if (i % cols != cols - 1 && i + 1 < n) link(i, i + 1);
            if (i + cols < n) link(i, i + cols);
        }
    } else if (topo == "dense") {
        for (int i = 1; i < n; ++i) link(i, rng.range(0, i - 1));
        for (int k = 0; k < n * 4; ++k) link(rng.range(0, n - 1), rng.range(0, n - 1));
    } else {  // random, always connected
        for (int i = 1; i < n; ++i) link(i, rng.range(0, i - 1));
        for (int k = 0; k < (n * 6) / 10; ++k) link(rng.range(0, n - 1), rng.range(0, n - 1));
    }
}
