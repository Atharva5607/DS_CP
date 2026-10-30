// Resilience analytics: Monte Carlo attack comparison, defence advisor and the
// targeted-vs-random robustness curve. All of them work on a private copy of the
// network, so the live simulation is never disturbed.
#pragma once
#include <vector>
#include <algorithm>
#include "../core/Graph.h"
#include "../algo/Attacks.h"
#include "../algo/Traversal.h"
#include "../ds/Heap.h"

inline void clearInfections(Graph& g) {
    for (auto& m : g.nodes) if (m.status == Status::INFECTED) m.status = Status::HEALTHY;
}

struct MonteResult { double avg = 0; int mn = 0; int mx = 0; int trials = 0; };

// Average number of machines compromised over many seeded runs. g is a copy.
inline MonteResult monteAttack(Graph g, const std::string& mode, int src, int target, int count,
                               double aggr, int trials, uint32_t seed) {
    clearInfections(g);
    std::vector<Status> orig;
    for (auto& m : g.nodes) orig.push_back(m.status);
    MonteResult r; r.mn = 1 << 30;
    long long total = 0;
    for (int t = 0; t < trials; ++t) {
        for (int i = 0; i < g.size(); ++i) g.nodes[i].status = orig[i];
        if (needsSource(mode) && g.nodes[src].status != Status::HEALTHY) break;
        Rng rng(seed + 7919u * (t + 1));
        int hit = countInfections(runAttack(g, mode, src, target, count, rng, aggr));
        total += hit; r.mn = std::min(r.mn, hit); r.mx = std::max(r.mx, hit); ++r.trials;
    }
    if (r.trials == 0) { r.mn = 0; return r; }
    r.avg = (double)total / r.trials;
    return r;
}

struct Advice { int node; double avg; };
struct AdviceResult { double baseline = 0; std::vector<Advice> best; };

// Which single machine, if isolated now, reduces the expected BFS spread the most?
// A max-heap of size K keeps the K best candidates while we scan all machines.
inline AdviceResult adviseIsolation(Graph g, int src, double aggr, int trials, uint32_t seed, int topK) {
    AdviceResult out;
    clearInfections(g);
    out.baseline = monteAttack(g, "bfs", src, -1, 0, aggr, trials, seed).avg;
    struct Before { bool operator()(const Advice& a, const Advice& b) const { return a.avg > b.avg; } };
    Heap<Advice, Before> worstOnTop;
    for (int v = 0; v < g.size(); ++v) {
        if (v == src || g.nodes[v].status != Status::HEALTHY) continue;
        g.nodes[v].status = Status::ISOLATED;
        double avg = monteAttack(g, "bfs", src, -1, 0, aggr, trials, seed).avg;
        g.nodes[v].status = Status::HEALTHY;
        worstOnTop.push({v, avg});
        if ((int)worstOnTop.size() > topK) { Advice drop; worstOnTop.pop(drop); }
    }
    Advice a;
    while (worstOnTop.pop(a)) out.best.push_back(a);
    std::reverse(out.best.begin(), out.best.end());       // best first
    return out;
}

struct RobustnessResult {
    std::vector<double> targeted, random;                 // largest segment / machines, per removal
    int total = 0;
};

// Remove machines one by one and watch the largest segment shrink.
// Targeted: always remove the best-connected machine (adaptive). Random: average of trials.
inline RobustnessResult robustness(Graph g, int randomTrials, uint32_t seed) {
    clearInfections(g);
    std::vector<int> alive;
    for (int v = 0; v < g.size(); ++v) if (g.active(v)) alive.push_back(v);
    RobustnessResult r;
    r.total = (int)alive.size();
    if (r.total == 0) return r;
    std::vector<Status> orig;
    for (auto& m : g.nodes) orig.push_back(m.status);
    auto largestFrac = [&]() { return (double)components(g).largest / r.total; };

    r.targeted.push_back(largestFrac());
    for (int step = 0; step < r.total; ++step) {
        int best = -1, bd = -1;
        for (int v : alive) if (g.active(v) && g.activeDegree(v) > bd) { bd = g.activeDegree(v); best = v; }
        g.nodes[best].status = Status::ISOLATED;
        r.targeted.push_back(largestFrac());
    }
    r.random.assign(r.total + 1, 0.0);
    for (int t = 0; t < randomTrials; ++t) {
        for (int i = 0; i < g.size(); ++i) g.nodes[i].status = orig[i];
        Rng rng(seed + 104729u * (t + 1));
        std::vector<int> order = alive;
        for (int i = (int)order.size() - 1; i > 0; --i) std::swap(order[i], order[rng.range(0, i)]);
        r.random[0] += largestFrac();
        for (int step = 0; step < r.total; ++step) {
            g.nodes[order[step]].status = Status::ISOLATED;
            r.random[step + 1] += largestFrac();
        }
    }
    for (auto& x : r.random) x /= randomTrials;
    return r;
}
