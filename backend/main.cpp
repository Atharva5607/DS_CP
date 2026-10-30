// Network Resilience and Incident Response Simulator - HTTP server.
// All algorithms live in backend/; this file only exposes them as a small JSON API
// and serves the web UI from ./frontend.
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include "httplib.h"
#include "sim/Simulation.h"

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    Simulation sim;
    sim.generate("random", 22, 42);
    std::mutex mtx;
    httplib::Server svr;
    svr.set_payload_max_length(4 * 1024 * 1024);

    const char* roots[] = {"frontend", "../frontend", "../../frontend"};
    bool mounted = false;
    for (const char* r : roots) if (svr.set_mount_point("/", r)) { mounted = true; break; }
    if (!mounted) std::cerr << "Warning: frontend folder not found. Run from the project root.\n";

    using Req = httplib::Request;
    using Res = httplib::Response;
    auto str = [](const Req& r, const char* k, const char* d) {
        return r.has_param(k) ? r.get_param_value(k) : std::string(d);
    };
    auto num = [&](const Req& r, const char* k, int d) {
        return std::atoi(str(r, k, std::to_string(d).c_str()).c_str());
    };
    auto dbl = [&](const Req& r, const char* k, double d) {
        return std::atof(str(r, k, std::to_string(d).c_str()).c_str());
    };
    auto json = [](Res& res, const std::string& body) {
        res.set_header("Cache-Control", "no-store");
        res.set_content(body, "application/json");
    };
    // every API call runs under one lock: the simulation is a single shared object
    auto get = [&](const char* path, std::function<std::string(const Req&)> fn) {
        svr.Get(path, [&, fn](const Req& r, Res& res) { std::lock_guard<std::mutex> lk(mtx); json(res, fn(r)); });
    };
    auto post = [&](const char* path, std::function<std::string(const Req&)> fn) {
        svr.Post(path, [&, fn](const Req& r, Res& res) { std::lock_guard<std::mutex> lk(mtx); json(res, fn(r)); });
    };

    get("/api/state", [&](const Req&) { return sim.state(); });
    get("/api/timeline", [&](const Req&) { return sim.timelineJson(); });
    get("/api/machine", [&](const Req& r) { return sim.machine(str(r, "name", "")); });
    get("/api/risky", [&](const Req& r) { return sim.risky(num(r, "min", 0), num(r, "max", 100)); });
    get("/api/critical", [&](const Req&) { return sim.critical(); });
    get("/api/mstcompare", [&](const Req&) { return sim.compareMst(); });
    get("/api/robustness", [&](const Req&) { return sim.robust(); });
    get("/api/benchmark", [&](const Req& r) { return sim.benchmark(str(r, "topology", "random")); });
    get("/api/monte", [&](const Req& r) {
        return sim.monte(num(r, "source", -1), num(r, "target", -1), dbl(r, "aggr", 0.9), num(r, "trials", 200), num(r, "count", 3));
    });
    get("/api/advise", [&](const Req& r) { return sim.advise(num(r, "source", -1), dbl(r, "aggr", 0.9)); });

    post("/api/generate", [&](const Req& r) {
        sim.generate(str(r, "topology", "random"), num(r, "n", 22), (uint32_t)num(r, "seed", 42));
        return sim.state();
    });
    post("/api/attack", [&](const Req& r) {
        return sim.attack(num(r, "source", -1), str(r, "mode", "bfs"), dbl(r, "aggr", 0.9), num(r, "target", -1), num(r, "count", 3));
    });
    post("/api/isolate", [&](const Req& r) { return sim.isolate(num(r, "node", -1)); });
    post("/api/cut", [&](const Req& r) { return sim.cutLink(num(r, "u", -1), num(r, "v", -1)); });
    post("/api/restore", [&](const Req& r) { return sim.restoreLink(num(r, "u", -1), num(r, "v", -1)); });
    post("/api/undo", [&](const Req&) { return sim.undo(); });
    post("/api/respond", [&](const Req&) { return sim.respond(); });
    post("/api/path", [&](const Req& r) { return sim.path(num(r, "from", -1), num(r, "to", -1), str(r, "mode", "cost")); });
    post("/api/backbone", [&](const Req& r) { return sim.backbone(str(r, "algo", "kruskal")); });
    post("/api/reset", [&](const Req&) { return sim.reset(); });
    post("/api/addnode", [&](const Req& r) { return sim.addMachine(str(r, "type", "PC")); });
    post("/api/addlink", [&](const Req& r) {
        return sim.addLink(num(r, "u", -1), num(r, "v", -1), num(r, "cost", 5), num(r, "risk", 50));
    });
    post("/api/import", [&](const Req& r) { return sim.importText(r.body); });

    svr.Get("/api/export", [&](const Req&, Res& res) {
        std::lock_guard<std::mutex> lk(mtx);
        res.set_header("Content-Disposition", "attachment; filename=\"network.netsim\"");
        res.set_content(sim.exportText(), "text/plain");
    });

    std::cout << "Network Resilience Simulator running at http://localhost:" << port << "\n";
    if (!svr.listen("127.0.0.1", port)) { std::cerr << "Could not bind port " << port << "\n"; return 1; }
    return 0;
}
