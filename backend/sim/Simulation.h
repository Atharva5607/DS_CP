// Ties everything together: owns the network and every supporting data structure.
//   Graph (adjacency LinkedLists) ... topology, link-level blocking
//   HashTable<int>   ................. name -> machine id            O(1)
//   AVLTree          ................. machines ordered by risk      O(log n)
//   Heap (max)       ................. incident queue by severity    O(log n)
//   Stack<Action>    ................. undo history of defences
//   LinkedList<str>  ................. incident / event log
//   timeline_        ................. every action with its steps (session replay)
#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include "../core/Graph.h"
#include "../core/Topology.h"
#include "../ds/HashTable.h"
#include "../ds/AVLTree.h"
#include "../ds/Heap.h"
#include "../ds/Stack.h"
#include "../algo/Traversal.h"
#include "../algo/ShortestPath.h"
#include "../algo/SpanningTree.h"
#include "../algo/Articulation.h"
#include "../algo/Attacks.h"
#include "Analysis.h"
#include "Benchmark.h"

struct Incident { int node; int severity; int seq; };
struct IncidentBefore {   // higher severity first, older incident wins ties
    bool operator()(const Incident& a, const Incident& b) const {
        return a.severity != b.severity ? a.severity > b.severity : a.seq < b.seq;
    }
};
struct Action { std::string kind; int node; Status prev; int cost; int node2; };
struct Snap { int affected, infected, isolated, recovered, healthy, cut; long long cost; };
struct TimelineEntry { std::string label; std::vector<Step> steps; Snap snap; };

class Simulation {
public:
    Simulation() : rng_(1) {}

    /* ---------------- building the network ---------------- */
    void generate(const std::string& topo, int n, uint32_t seed) {
        if (n < 6) n = 6;
        if (n > 60) n = 60;
        topo_ = (topo == "ring" || topo == "star" || topo == "tree" || topo == "mesh" || topo == "dense") ? topo : "random";
        seed_ = seed;
        rng_ = Rng(seed);
        generateGraph(g_, topo_, n, rng_);
        rebuildIndexes();
        resetSession();
        log_.clear();
        logEvent("Generated " + topo_ + " network with " + std::to_string(n) + " machines, " +
                 std::to_string(g_.edges().size()) + " links");
    }

    std::string addMachine(const std::string& type) {
        if (g_.size() >= 80) return wrap({}, msg("The map is limited to 80 machines."));
        NodeType t = type == "SERVER" ? NodeType::SERVER : type == "ROUTER" ? NodeType::ROUTER : NodeType::PC;
        int id = g_.size(), imp, risk; std::string name;
        if (t == NodeType::ROUTER)      { name = "RTR-"; imp = rng_.range(7, 9);  risk = rng_.range(15, 50); }
        else if (t == NodeType::SERVER) { name = "SRV-"; imp = rng_.range(8, 10); risk = rng_.range(20, 60); }
        else                            { name = "PC-";  imp = rng_.range(1, 5);  risk = rng_.range(40, 90); }
        name += std::to_string(id);
        g_.addNode(name, t, imp, risk);
        byName_.put(name, id);
        riskIdx_.insert(key(risk, id), id);
        wasInfected_.push_back(0);
        std::string m = "Added " + name + " (not linked yet)";
        logEvent(m);
        return wrap({}, "{\"message\":\"" + esc(m) + "\",\"newNode\":" + std::to_string(id) + "}");
    }

    std::string addLink(int u, int v, int cost, int risk) {
        if (!valid(u) || !valid(v) || u == v) return wrap({}, msg("Select two different machines (click one, shift-click the other)."));
        if (g_.hasEdge(u, v)) return wrap({}, msg("Those machines are already linked."));
        cost = cost < 1 ? 1 : cost > 99 ? 99 : cost;
        risk = risk < 1 ? 1 : risk > 99 ? 99 : risk;
        g_.addEdge(u, v, cost, risk);
        std::string m = "Linked " + g_.nodes[u].name + " and " + g_.nodes[v].name + " (cost " +
                        std::to_string(cost) + ", risk " + std::to_string(risk) + ")";
        logEvent(m);
        return wrap({}, msg(m));
    }

    /* ---------------- attacks ---------------- */
    std::string attack(int src, const std::string& mode, double aggr, int target, int count) {
        if (aggr < 0.1) aggr = 0.1;
        if (aggr > 5.0) aggr = 5.0;
        if (needsSource(mode)) {
            if (!valid(src)) return wrap({}, msg("Pick a machine to start the attack from."));
            if (g_.nodes[src].status != Status::HEALTHY)
                return wrap({}, msg(g_.nodes[src].name + " is not healthy, so it cannot be the entry point."));
            if (mode == "apt") {
                if (!valid(target) || target == src)
                    return wrap({}, msg("APT needs a target: shift-click a second machine."));
                if (!g_.active(target)) return wrap({}, msg("The target machine is isolated."));
            }
        }
        if (count < 1) count = 1;
        if (count > g_.size()) count = g_.size();
        std::vector<Step> steps = runAttack(g_, mode, src, target, count, rng_, aggr);
        int hit = 0;
        for (auto& s : steps) if (std::string(s.type) == "infect") { onInfect(s.b); ++hit; }
        std::string m;
        if (mode == "dfs") m = "DFS lateral movement from " + g_.nodes[src].name + " compromised " + std::to_string(hit) + " machine(s)";
        else if (mode == "apt") m = "APT from " + g_.nodes[src].name + " toward " + g_.nodes[target].name + " compromised " +
                                    std::to_string(hit) + " machine(s)" +
                                    (g_.nodes[target].status == Status::INFECTED ? " and reached the target" : ", stopped before the target");
        else if (mode == "hub") m = "Targeted failure took out the " + std::to_string(hit) + " best-connected machine(s)";
        else if (mode == "random") m = "Random failure took out " + std::to_string(hit) + " machine(s)";
        else m = "BFS spread from " + g_.nodes[src].name + " compromised " + std::to_string(hit) + " machine(s)";
        logEvent(m);
        record(m, steps);
        return wrap(steps, msg(m));
    }

    /* ---------------- defence ---------------- */
    std::string isolate(int node) {
        if (!valid(node)) return wrap({}, msg("Pick a machine to isolate."));
        Machine& m = g_.nodes[node];
        if (m.status == Status::ISOLATED) return wrap({}, msg(m.name + " is already isolated."));
        undo_.push({"isolate", node, m.status, 0, -1});
        m.status = Status::ISOLATED;
        ConnInfo c = components(g_);
        std::string s = "Isolated " + m.name + ". Network now has " + std::to_string(c.count) +
                        " segment(s), largest has " + std::to_string(c.largest) + " machines";
        logEvent(s);
        std::vector<Step> st = {{"set", node, (int)Status::ISOLATED, 0}};
        record(s, st);
        return wrap(st, msg(s));
    }

    std::string cutLink(int u, int v) {
        Edge* e = valid(u) && valid(v) ? g_.findEdge(u, v) : nullptr;
        if (!e) return wrap({}, msg("There is no link between the two selected machines."));
        if (e->blocked) return wrap({}, msg("That link is already cut."));
        g_.setBlocked(u, v, true);
        undo_.push({"cut", u, Status::HEALTHY, 0, v});
        ConnInfo c = components(g_);
        std::string s = "Cut link " + g_.nodes[u].name + " - " + g_.nodes[v].name + ". Network now has " +
                        std::to_string(c.count) + " segment(s)";
        logEvent(s);
        std::vector<Step> st = {{"cut", u, v, 0}};
        record(s, st);
        return wrap(st, msg(s));
    }

    std::string restoreLink(int u, int v) {
        Edge* e = valid(u) && valid(v) ? g_.findEdge(u, v) : nullptr;
        if (!e) return wrap({}, msg("There is no link between the two selected machines."));
        if (!e->blocked) return wrap({}, msg("That link is not cut."));
        g_.setBlocked(u, v, false);
        undo_.push({"restore", u, Status::HEALTHY, 0, v});
        std::string s = "Restored link " + g_.nodes[u].name + " - " + g_.nodes[v].name;
        logEvent(s);
        std::vector<Step> st = {{"uncut", u, v, 0}};
        record(s, st);
        return wrap(st, msg(s));
    }

    std::string undo() {
        Action a;
        if (!undo_.pop(a)) return wrap({}, msg("Nothing to undo."));
        std::vector<Step> st;
        std::string s;
        if (a.kind == "cut" || a.kind == "restore") {
            bool block = a.kind == "restore";
            g_.setBlocked(a.node, a.node2, block);
            st.push_back({block ? "cut" : "uncut", a.node, a.node2, 0});
            s = "Undid " + a.kind + " on " + g_.nodes[a.node].name + " - " + g_.nodes[a.node2].name;
        } else {
            Machine& m = g_.nodes[a.node];
            m.status = a.prev;
            if (a.prev == Status::INFECTED) pushIncident(a.node);
            if (a.kind == "recover") recoveryCost_ -= a.cost;
            st.push_back({"set", a.node, (int)a.prev, 0});
            s = "Undid " + a.kind + " on " + m.name;
        }
        logEvent(s);
        record(s, st);
        return wrap(st, msg(s));
    }

    std::string respond() {  // take the most severe open incident and clean that machine
        Incident inc;
        while (incidents_.pop(inc)) {
            Machine& m = g_.nodes[inc.node];
            if (m.status != Status::INFECTED) continue;   // stale entry (already isolated/recovered)
            int cost = m.importance * 5 + 10;
            undo_.push({"recover", inc.node, Status::INFECTED, cost, -1});
            m.status = Status::RECOVERED;
            recoveryCost_ += cost;
            std::string s = "Responded to " + m.name + " (severity " + std::to_string(inc.severity) +
                            "), recovery cost " + std::to_string(cost);
            logEvent(s);
            std::vector<Step> st = {{"set", inc.node, (int)Status::RECOVERED, 0}};
            record(s, st);
            return wrap(st, msg(s));
        }
        return wrap({}, msg("No open incidents."));
    }

    std::string reset() {
        riskIdx_.clear();
        resetSession();
        for (auto& m : g_.nodes) {
            m.status = Status::HEALTHY; m.risk = m.baseRisk;
            riskIdx_.insert(key(m.risk, m.id), m.id);
            for (auto& e : g_.adj[m.id]) e.blocked = false;      // every cut link is restored too
        }
        logEvent("Incident reset: all machines healthy, all links restored");
        return wrap({}, msg("Incident reset. The network is healthy again."));
    }

    /* ---------------- routes and recovery ---------------- */
    std::string path(int s, int t, const std::string& mode) {
        if (!valid(s) || !valid(t)) return wrap({}, msg("Choose both a start and a target machine."));
        Weight w = (mode == "ease") ? Weight::EASE : Weight::COST;
        PathResult r = dijkstra(g_, s, t, w);
        std::ostringstream o;
        if (!r.found) {
            std::string m = "No route from " + g_.nodes[s].name + " to " + g_.nodes[t].name +
                            " (isolated machines and cut links block it).";
            o << "{\"message\":\"" << esc(m) << "\",\"path\":[]}";
            return wrap(r.steps, o.str());
        }
        std::string m = std::string(w == Weight::COST ? "Cheapest route" : "Easiest attack route") + " " +
                        g_.nodes[s].name + " -> " + g_.nodes[t].name + ": total " + std::to_string(r.total) +
                        ", " + std::to_string(r.path.size() - 1) + " hop(s)";
        logEvent(m);
        o << "{\"message\":\"" << esc(m) << "\",\"path\":[";
        for (size_t i = 0; i < r.path.size(); ++i) o << (i ? "," : "") << r.path[i];
        o << "]}";
        return wrap(r.steps, o.str());
    }

    std::string backbone(const std::string& algo) {
        bool usePrim = algo == "prim";
        MstResult r = usePrim ? prim(g_) : kruskal(g_);
        std::string m = std::string("Recovery backbone (") + (usePrim ? "Prim" : "Kruskal") + "): " +
                        std::to_string(r.edges.size()) + " links, cost " + std::to_string(r.cost) + " vs " +
                        std::to_string(r.fullCost) + " for all links";
        if (r.components > 1) m += ". " + std::to_string(r.components) + " segments cannot be joined";
        logEvent(m);
        std::ostringstream o;
        o << "{\"message\":\"" << esc(m) << "\",\"algo\":\"" << (usePrim ? "prim" : "kruskal") << "\",\"mst\":";
        edgeListJson(o, r.edges);
        o << "}";
        return wrap({}, o.str());
    }

    std::string compareMst() {
        int reps = std::max(20, 20000 / (g_.size() + (int)g_.edges().size() + 1));
        MstResult k, p;
        double kt = timeMicros([&]() { k = kruskal(g_); }, reps);
        double pt = timeMicros([&]() { p = prim(g_); }, reps);
        std::ostringstream o;
        o << std::fixed << std::setprecision(1) << "{\"full\":" << k.fullCost << ",\"sameCost\":"
          << (k.cost == p.cost ? "true" : "false") << ",\"kruskal\":{\"cost\":" << k.cost << ",\"edges\":"
          << k.edges.size() << ",\"heapOps\":" << k.heapOps << ",\"dsuOps\":" << k.dsuOps << ",\"micros\":" << kt
          << ",\"mst\":";
        edgeListJson(o, k.edges);
        o << "},\"prim\":{\"cost\":" << p.cost << ",\"edges\":" << p.edges.size() << ",\"heapOps\":" << p.heapOps
          << ",\"dsuOps\":0,\"micros\":" << pt << ",\"mst\":";
        edgeListJson(o, p.edges);
        o << "}}";
        return o.str();
    }

    /* ---------------- analysis ---------------- */
    std::string critical() {
        CriticalInfo ci = findCritical(g_);
        std::ostringstream o;
        o << "{\"cut\":[";
        bool first = true;
        for (int v = 0; v < g_.size(); ++v) {
            if (!ci.isCut[v]) continue;
            Status old = g_.nodes[v].status;
            g_.nodes[v].status = Status::ISOLATED;
            ConnInfo c = components(g_);
            g_.nodes[v].status = old;
            o << (first ? "" : ",") << "{\"node\":" << v << ",\"segments\":" << c.count << ",\"largest\":" << c.largest << "}";
            first = false;
        }
        o << "],\"bridges\":[";
        for (size_t i = 0; i < ci.bridges.size(); ++i)
            o << (i ? "," : "") << "{\"u\":" << ci.bridges[i].first << ",\"v\":" << ci.bridges[i].second << "}";
        o << "]}";
        return o.str();
    }

    std::string monte(int src, int target, double aggr, int trials, int count) {
        if (trials < 10) trials = 10;
        if (trials > 500) trials = 500;
        if (count < 1) count = 3;
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << "{\"trials\":" << trials << ",\"rows\":[";
        bool first = true;
        auto row = [&](const char* mode, const char* label, bool ok) {
            if (!ok) return;
            MonteResult r = monteAttack(g_, mode, src, target, count, aggr, trials, seed_ + 17);
            o << (first ? "" : ",") << "{\"mode\":\"" << mode << "\",\"label\":\"" << label << "\",\"avg\":" << r.avg
              << ",\"min\":" << r.mn << ",\"max\":" << r.mx << "}";
            first = false;
        };
        bool haveSrc = valid(src) && g_.active(src);
        row("bfs", "BFS worm", haveSrc);
        row("dfs", "DFS lateral", haveSrc);
        row("apt", "APT to target", haveSrc && valid(target) && target != src && g_.active(target));
        row("hub", ("Hub failure x" + std::to_string(count)).c_str(), true);
        row("random", ("Random failure x" + std::to_string(count)).c_str(), true);
        o << "]}";
        return o.str();
    }

    std::string advise(int src, double aggr) {
        if (!valid(src) || !g_.active(src)) return "{\"ok\":false,\"message\":\"Select the machine the attack will start from.\"}";
        AdviceResult r = adviseIsolation(g_, src, aggr, 60, seed_ + 29, 5);
        std::ostringstream o;
        o << std::fixed << std::setprecision(2) << "{\"ok\":true,\"source\":" << src << ",\"baseline\":" << r.baseline << ",\"best\":[";
        for (size_t i = 0; i < r.best.size(); ++i)
            o << (i ? "," : "") << "{\"node\":" << r.best[i].node << ",\"avg\":" << r.best[i].avg << "}";
        o << "]}";
        return o.str();
    }

    std::string robust() {
        RobustnessResult r = robustness(g_, 40, seed_ + 43);
        auto half = [&](const std::vector<double>& v) {
            for (size_t i = 0; i < v.size(); ++i) if (v[i] < 0.5) return (int)i;
            return -1;
        };
        std::ostringstream o;
        o << std::fixed << std::setprecision(3) << "{\"total\":" << r.total << ",\"targeted\":[";
        for (size_t i = 0; i < r.targeted.size(); ++i) o << (i ? "," : "") << r.targeted[i];
        o << "],\"random\":[";
        for (size_t i = 0; i < r.random.size(); ++i) o << (i ? "," : "") << r.random[i];
        o << "],\"halfTargeted\":" << half(r.targeted) << ",\"halfRandom\":" << half(r.random) << "}";
        return o.str();
    }

    std::string benchmark(const std::string& topo) { return benchmarkJson(topo, seed_); }

    /* ---------------- lookups ---------------- */
    std::string machine(const std::string& name) {       // hash table lookup
        int* id = byName_.get(name);
        if (!id) return "{\"found\":false}";
        return "{\"found\":true,\"id\":" + std::to_string(*id) + "}";
    }

    std::string risky(int lo, int hi) {                  // AVL range query
        std::vector<int> ids;
        riskIdx_.range(key(lo, 0), key(hi, 999999), ids);
        std::ostringstream o;
        o << "{\"height\":" << riskIdx_.height() << ",\"ids\":[";
        for (size_t i = 0; i < ids.size(); ++i) o << (i ? "," : "") << ids[i];
        o << "]}";
        return o.str();
    }

    /* ---------------- save / load / replay ---------------- */
    std::string exportText() const {
        std::ostringstream o;
        o << "NETSIM 1\nT " << topo_ << " " << seed_ << "\nR " << recoveryCost_ << "\nG " << rng_.s << "\n";
        for (auto& m : g_.nodes)
            o << "N " << m.id << " " << m.name << " " << typeName(m.type) << " " << m.importance << " " << m.risk
              << " " << m.baseRisk << " " << statusName(m.status) << " " << (int)wasInfected_[m.id] << "\n";
        for (auto& e : g_.edges())
            o << "E " << e.u << " " << e.v << " " << e.cost << " " << e.risk << " " << e.importance << " " << (e.blocked ? 1 : 0) << "\n";
        return o.str();
    }

    std::string importText(const std::string& text) {
        std::istringstream in(text);
        std::string line;
        if (!std::getline(in, line) || line.compare(0, 8, "NETSIM 1") != 0)
            return wrap({}, errMsg("Not a NETSIM save file."));
        Graph ng; std::vector<char> aff; long long rc = 0; uint32_t rs = 1, sd = 1; std::string topo = "random";
        auto parseStatus = [](const std::string& s) {
            return s == "infected" ? Status::INFECTED : s == "isolated" ? Status::ISOLATED
                 : s == "recovered" ? Status::RECOVERED : Status::HEALTHY;
        };
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            std::string tag; ls >> tag;
            if (tag == "T") ls >> topo >> sd;
            else if (tag == "R") ls >> rc;
            else if (tag == "G") ls >> rs;
            else if (tag == "N") {
                int id, imp, risk, base, a; std::string name, type, st;
                if (!(ls >> id >> name >> type >> imp >> risk >> base >> st >> a) || id != ng.size() || ng.size() >= 200)
                    return wrap({}, errMsg("Corrupt machine line in the save file."));
                NodeType t = type == "SERVER" ? NodeType::SERVER : type == "ROUTER" ? NodeType::ROUTER : NodeType::PC;
                ng.addNode(name, t, imp, risk);
                ng.nodes[id].baseRisk = base; ng.nodes[id].status = parseStatus(st);
                aff.push_back((char)a);
            } else if (tag == "E") {
                int u, v, c, r, im, b;
                if (!(ls >> u >> v >> c >> r >> im >> b) || !ng.addEdgeFull(u, v, c, r, im, b != 0))
                    return wrap({}, errMsg("Corrupt link line in the save file."));
            }
        }
        if (ng.size() < 2) return wrap({}, errMsg("The save file has no network in it."));
        g_ = ng; seed_ = sd; topo_ = topo; rng_.s = rs ? rs : 1;
        rebuildIndexes();
        resetSession();
        wasInfected_ = aff;
        recoveryCost_ = rc;
        for (auto& m : g_.nodes) if (m.status == Status::INFECTED) pushIncident(m.id);
        log_.clear();
        logEvent("Loaded network with " + std::to_string(g_.size()) + " machines, " + std::to_string(g_.edges().size()) + " links");
        return wrap({}, "{\"message\":\"Network loaded.\",\"loaded\":true}");
    }

    std::string timelineJson() const {
        std::ostringstream o;
        o << "{\"entries\":[";
        for (size_t i = 0; i < timeline_.size(); ++i) {
            const TimelineEntry& t = timeline_[i];
            o << (i ? "," : "") << "{\"label\":\"" << esc(t.label) << "\",\"snap\":{\"affected\":" << t.snap.affected
              << ",\"infected\":" << t.snap.infected << ",\"isolated\":" << t.snap.isolated << ",\"recovered\":"
              << t.snap.recovered << ",\"healthy\":" << t.snap.healthy << ",\"cut\":" << t.snap.cut << ",\"cost\":"
              << t.snap.cost << "},\"steps\":";
            stepsJson(o, t.steps);
            o << "}";
        }
        o << "]}";
        return o.str();
    }

    /* ---------------- state ---------------- */
    std::string state() const {
        CriticalInfo ci = findCritical(g_);
        std::vector<int> seg;
        components(g_, &seg);
        std::ostringstream o;
        o << "{\"topology\":\"" << topo_ << "\",\"seed\":" << seed_ << ",\"nodes\":[";
        for (auto& m : g_.nodes)
            o << (m.id ? "," : "") << "{\"id\":" << m.id << ",\"name\":\"" << m.name << "\",\"type\":\""
              << typeName(m.type) << "\",\"imp\":" << m.importance << ",\"risk\":" << m.risk
              << ",\"deg\":" << g_.degree(m.id) << ",\"seg\":" << seg[m.id] << ",\"cut\":"
              << (ci.isCut[m.id] ? "true" : "false") << ",\"status\":\"" << statusName(m.status) << "\"}";
        o << "],\"edges\":[";
        auto es = g_.edges();
        for (size_t i = 0; i < es.size(); ++i) {
            bool bridge = false;
            for (auto& b : ci.bridges) if (b.first == es[i].u && b.second == es[i].v) { bridge = true; break; }
            o << (i ? "," : "") << "{\"u\":" << es[i].u << ",\"v\":" << es[i].v << ",\"cost\":" << es[i].cost
              << ",\"risk\":" << es[i].risk << ",\"imp\":" << es[i].importance << ",\"blocked\":"
              << (es[i].blocked ? "true" : "false") << ",\"bridge\":" << (bridge ? "true" : "false") << "}";
        }
        o << "],\"metrics\":" << metrics() << ",\"incidents\":[";
        auto copy = incidents_;                           // drain a copy: heap order = priority order
        Incident inc; int shown = 0;
        while (copy.pop(inc) && shown < 12) {
            if (g_.nodes[inc.node].status != Status::INFECTED) continue;
            o << (shown++ ? "," : "") << "{\"node\":" << inc.node << ",\"sev\":" << inc.severity << "}";
        }
        o << "],\"log\":[";
        bool first = true;
        for (auto& line : log_) { o << (first ? "" : ",") << "\"" << esc(line) << "\""; first = false; }
        o << "]}";
        return o.str();
    }

private:
    Graph g_;
    HashTable<int> byName_;
    AVLTree riskIdx_;
    Heap<Incident, IncidentBefore> incidents_;
    Stack<Action> undo_;
    LinkedList<std::string> log_;
    std::vector<TimelineEntry> timeline_;
    std::vector<char> wasInfected_;
    Rng rng_;
    std::string topo_ = "random";
    uint32_t seed_ = 1;
    int seq_ = 0;
    long long recoveryCost_ = 0;

    static long long key(int risk, int id) { return (long long)risk * 1000000LL + id; }
    bool valid(int v) const { return v >= 0 && v < g_.size(); }

    void rebuildIndexes() {
        byName_.clear(); riskIdx_.clear();
        for (auto& m : g_.nodes) { byName_.put(m.name, m.id); riskIdx_.insert(key(m.risk, m.id), m.id); }
    }
    void resetSession() {
        incidents_.clear(); undo_.clear(); timeline_.clear();
        recoveryCost_ = 0; seq_ = 0;
        wasInfected_.assign(g_.size(), 0);
    }
    void logEvent(const std::string& s) {
        log_.push_back(s);
        std::string drop;
        while (log_.size() > 80) log_.pop_front(drop);
    }
    void pushIncident(int node) {
        const Machine& m = g_.nodes[node];
        incidents_.push({node, m.importance * 10 + g_.activeDegree(node) * 2 + m.risk / 10, ++seq_});
    }
    void onInfect(int node) {
        wasInfected_[node] = 1;
        pushIncident(node);
        for (auto& e : g_.adj[node]) {                    // neighbours become more exposed
            Machine& w = g_.nodes[e.to];
            if (w.status != Status::HEALTHY) continue;
            riskIdx_.erase(key(w.risk, w.id));
            w.risk = w.risk + 8 > 100 ? 100 : w.risk + 8;
            riskIdx_.insert(key(w.risk, w.id), w.id);
        }
    }
    Snap snap() const {
        Snap s{0, 0, 0, 0, 0, g_.blockedCount(), recoveryCost_};
        for (auto& m : g_.nodes) {
            switch (m.status) {
                case Status::INFECTED: ++s.infected; break;
                case Status::ISOLATED: ++s.isolated; break;
                case Status::RECOVERED: ++s.recovered; break;
                default: ++s.healthy;
            }
            if (wasInfected_[m.id]) ++s.affected;
        }
        return s;
    }
    void record(const std::string& label, const std::vector<Step>& steps) {
        if (timeline_.size() < 500) timeline_.push_back({label, steps, snap()});
    }
    std::string metrics() const {
        Snap s = snap();
        int n = g_.size(), atRisk = 0;
        for (auto& m : g_.nodes) if (m.status == Status::INFECTED) atRisk += m.importance;
        ConnInfo c = components(g_);
        std::ostringstream o;
        o << "{\"total\":" << n << ",\"healthy\":" << s.healthy << ",\"infected\":" << s.infected << ",\"isolated\":"
          << s.isolated << ",\"recovered\":" << s.recovered << ",\"affected\":" << s.affected << ",\"saved\":"
          << std::fixed << std::setprecision(1) << (n ? 100.0 * (n - s.affected) / n : 0.0) << ",\"atRisk\":" << atRisk
          << ",\"segments\":" << c.count << ",\"largest\":" << c.largest << ",\"recoveryCost\":" << recoveryCost_
          << ",\"cutLinks\":" << s.cut << ",\"undoDepth\":" << undo_.size() << ",\"avlHeight\":" << riskIdx_.height()
          << ",\"hashBuckets\":" << byName_.bucketCount() << ",\"links\":" << g_.edges().size() << "}";
        return o.str();
    }
    static std::string esc(const std::string& s) {
        std::string r;
        for (char c : s) { if (c == '"' || c == '\\') r += '\\'; r += c; }
        return r;
    }
    static std::string msg(const std::string& s) { return "{\"message\":\"" + esc(s) + "\"}"; }
    static std::string errMsg(const std::string& s) { return "{\"message\":\"" + esc(s) + "\",\"error\":true}"; }
    static void stepsJson(std::ostringstream& o, const std::vector<Step>& steps) {
        o << "[";
        for (size_t i = 0; i < steps.size(); ++i)
            o << (i ? "," : "") << "{\"type\":\"" << steps[i].type << "\",\"a\":" << steps[i].a
              << ",\"b\":" << steps[i].b << ",\"t\":" << steps[i].t << "}";
        o << "]";
    }
    static void edgeListJson(std::ostringstream& o, const std::vector<UEdge>& es) {
        o << "[";
        for (size_t i = 0; i < es.size(); ++i)
            o << (i ? "," : "") << "{\"u\":" << es[i].u << ",\"v\":" << es[i].v << ",\"cost\":" << es[i].cost << "}";
        o << "]";
    }
    std::string wrap(const std::vector<Step>& steps, const std::string& extra) const {
        std::ostringstream o;
        o << "{\"steps\":";
        stepsJson(o, steps);
        o << ",\"extra\":" << extra << ",\"state\":" << state() << "}";
        return o.str();
    }
};
