// Unit tests for the custom data structures, algorithms and the simulation.  Run: make test
#include <cassert>
#include <iostream>
#include "sim/Simulation.h"

static int passed = 0;
#define CHECK(c) do { if (!(c)) { std::cerr << "FAIL line " << __LINE__ << ": " #c "\n"; return 1; } ++passed; } while (0)
static bool has(const std::string& s, const char* t) { return s.find(t) != std::string::npos; }

struct MinFirst { bool operator()(int a, int b) const { return a < b; } };

int main() {
    // LinkedList / Stack / Queue
    Queue<int> q; Stack<int> s; int x;
    for (int i = 1; i <= 3; ++i) { q.push(i); s.push(i); }
    CHECK(q.pop(x) && x == 1);          // FIFO
    CHECK(s.pop(x) && x == 3);          // LIFO
    CHECK(q.size() == 2 && s.size() == 2);

    // Heap
    Heap<int, MinFirst> h;
    int vals[] = {9, 4, 7, 1, 8, 2};
    for (int v : vals) h.push(v);
    int prev = -1; bool sorted = true;
    while (h.pop(x)) { if (x < prev) sorted = false; prev = x; }
    CHECK(sorted);

    // HashTable (forces several rehashes)
    HashTable<int> ht(4);
    for (int i = 0; i < 200; ++i) ht.put("PC-" + std::to_string(i), i);
    CHECK(ht.size() == 200 && ht.get("PC-137") && *ht.get("PC-137") == 137);
    CHECK(ht.erase("PC-137") && !ht.get("PC-137") && ht.size() == 199);

    // AVL: ordered, balanced, range query, delete
    AVLTree t;
    for (int i = 0; i < 1000; ++i) t.insert(i, i);            // sorted input = worst case for a plain BST
    CHECK(t.height() <= 12);
    std::vector<int> out; t.range(10, 14, out);
    CHECK(out.size() == 5 && out[0] == 10 && out[4] == 14);
    t.erase(12); out.clear(); t.range(10, 14, out);
    CHECK(out.size() == 4 && t.size() == 999);

    // DSU
    DSU d(5);
    CHECK(d.unite(0, 1) && d.unite(1, 2) && !d.unite(0, 2) && d.find(0) == d.find(2) && d.find(3) != d.find(0));

    // Hand-built network:   0 -1- 1 -1- 2 -4- 3      0-2 direct costs 10
    Graph g;
    for (int i = 0; i < 4; ++i) g.addNode("N" + std::to_string(i), NodeType::PC, 3, 50);
    g.addEdge(0, 1, 1, 50); g.addEdge(1, 2, 1, 50); g.addEdge(0, 2, 10, 50); g.addEdge(2, 3, 4, 50);
    PathResult p = dijkstra(g, 0, 3, Weight::COST);
    CHECK(p.found && p.total == 6 && p.path.size() == 4);     // 0-1-2-3
    MstResult k = kruskal(g), pr = prim(g);
    CHECK(k.edges.size() == 3 && k.cost == 6 && k.fullCost == 16);
    CHECK(pr.edges.size() == 3 && pr.cost == 6 && pr.components == 1);
    g.nodes[1].status = Status::ISOLATED;                     // isolate the bridge machine
    p = dijkstra(g, 0, 3, Weight::COST);
    CHECK(p.found && p.total == 14);                          // forced onto 0-2-3
    ConnInfo c = components(g);
    CHECK(c.count == 1 && c.largest == 3);
    g.nodes[1].status = Status::HEALTHY;

    // Link-level isolation: cutting 0-1 and 0-2 leaves machine 0 alone
    CHECK(g.setBlocked(0, 1, true) && g.setBlocked(0, 2, true));
    c = components(g);
    CHECK(c.count == 2 && c.largest == 3);
    CHECK(!dijkstra(g, 0, 3, Weight::COST).found);
    CHECK(g.blockedCount() == 2);
    g.setBlocked(0, 1, false); g.setBlocked(0, 2, false);

    // Articulation points and bridges: 2 is a cut vertex, 2-3 is a bridge
    CriticalInfo ci = findCritical(g);
    CHECK(ci.isCut[2] && !ci.isCut[0] && !ci.isCut[3]);
    CHECK(ci.bridges.size() == 1 && ci.bridges[0].first == 2 && ci.bridges[0].second == 3);

    // Attacks with every edge crossed (risk 50 * 2.0 = 100%)
    Rng r(7);
    auto steps = bfsAttack(g, 0, r, 2.0);
    CHECK(countInfections(steps) == 4);
    for (auto& n : g.nodes) n.status = Status::HEALTHY;
    steps = dfsAttack(g, 0, r, 2.0);
    CHECK(countInfections(steps) == 4);
    for (auto& n : g.nodes) n.status = Status::HEALTHY;
    steps = aptAttack(g, 0, 3, r, 2.0);                        // easiest route 0-1-2-3
    CHECK(countInfections(steps) >= 3 && g.nodes[3].status == Status::INFECTED);
    for (auto& n : g.nodes) n.status = Status::HEALTHY;
    steps = hubAttack(g, 1);                                   // machine 2 has the most links
    CHECK(countInfections(steps) == 1 && g.nodes[2].status == Status::INFECTED);
    for (auto& n : g.nodes) n.status = Status::HEALTHY;
    steps = randomAttack(g, 3, r);
    CHECK(countInfections(steps) == 3);

    // Prim and Kruskal always agree on cost, on every topology
    const char* topos[] = {"random", "mesh", "ring", "star", "tree", "dense"};
    for (const char* tp : topos) {
        Rng rr(11); Graph gg; generateGraph(gg, tp, 40, rr);
        CHECK(components(gg).count == 1);                      // generators are connected
        MstResult a = kruskal(gg), b = prim(gg);
        CHECK(a.cost == b.cost && a.edges.size() == 39 && b.edges.size() == 39);
    }

    // Analytics: targeted removal hurts more than random removal on a star
    {
        Rng rr(3); Graph star; generateGraph(star, "star", 20, rr);
        RobustnessResult rob = robustness(star, 20, 5);
        CHECK(rob.total == 20 && rob.targeted.size() == 21);
        CHECK(rob.targeted[1] < rob.random[1]);                // losing the hub shatters the star
        MonteResult mr = monteAttack(star, "bfs", 5, -1, 0, 1.0, 50, 9);
        CHECK(mr.trials == 50 && mr.avg >= 1.0);
        AdviceResult adv = adviseIsolation(star, 5, 1.0, 30, 9, 3);
        CHECK(adv.best.size() == 3 && adv.best[0].avg <= adv.best[2].avg);
        CHECK(adv.best[0].avg <= adv.baseline + 1e-9);
    }

    // Benchmark produces timings for all rows
    BenchRow br = benchOne("random", 100, 1);
    CHECK(br.n == 100 && br.bfs > 0 && br.prim > 0 && br.kruskal > 0);

    // Full simulation: incident priority, undo, link cuts, save/load round trip
    Simulation sim;
    sim.generate("mesh", 16, 5);
    std::string res = sim.attack(0, "bfs", 2.0, -1, 3);
    CHECK(has(res, "\"infected\":"));
    CHECK(has(sim.respond(), "Responded"));
    CHECK(has(sim.undo(), "Undid recover"));
    CHECK(has(sim.cutLink(0, 1), "Cut link"));
    CHECK(has(sim.cutLink(0, 1), "already cut"));
    CHECK(has(sim.undo(), "Undid cut"));
    CHECK(has(sim.attack(-1, "hub", 1.0, -1, 2), "Targeted failure"));
    {
        Simulation fresh; fresh.generate("mesh", 16, 5);
        CHECK(has(fresh.attack(5, "apt", 1.0, -1, 1), "APT needs a target"));
        CHECK(has(fresh.attack(5, "apt", 1.0, 10, 1), "APT from"));
    }
    CHECK(has(sim.backbone("prim"), "Prim"));
    CHECK(has(sim.compareMst(), "\"sameCost\":true"));
    CHECK(has(sim.critical(), "\"cut\":["));
    CHECK(has(sim.timelineJson(), "\"entries\":["));
    std::string saved = sim.exportText();
    Simulation other;
    CHECK(has(other.importText(saved), "Network loaded"));
    CHECK(other.exportText() == saved);                        // lossless round trip
    CHECK(has(other.importText("garbage"), "Not a NETSIM"));
    CHECK(has(other.addMachine("SERVER"), "newNode"));
    CHECK(has(other.addLink(0, 16, 4, 40), "Linked"));
    CHECK(has(other.machine("PC-0"), "true") || has(other.machine("SRV-0"), "true") || has(other.machine("RTR-0"), "true"));

    std::cout << "All " << passed << " checks passed\n";
    return 0;
}
