# Network Resilience and Incident Response Simulator

Data Structure II (CS2308) course project. A C++17 backend with hand-written data
structures and graph algorithms, and a browser frontend that animates every
algorithm step. Complete version: every item of the synopsis plus the future-work list.

## Build and run

Needs only a C++17 compiler (g++ 8+ or clang). The HTTP library is bundled in `third_party/`.

    make              # builds ./netsim      (Windows MinGW: mingw32-make)
    ./netsim          # starts the server, then open http://localhost:8080
    make test         # 63 unit checks for every data structure and algorithm

Shortcuts: `./run.sh` (Linux, macOS) or `run.bat` (Windows) build and start in one go.
A `CMakeLists.txt` is included for CMake users. Run from the project root so the
server can find the `frontend/` folder. Another port: `./netsim 9000`.

## What it does

Machines (PC, server, router) form a weighted graph (cost, risk, importance per link).
You attack it, defend it, recover it and analyse how resilient it is.

**Build the network:** six generators (random, mesh, ring, star, tree, dense); add machines
and links by hand; cut and restore single links; drag machines on the map; save the
network to a `.netsim` file and load it again.

**Attack models:**

| Model | Algorithm | Idea |
|---|---|---|
| Worm (BFS) | BFS + Queue | Infection crosses links in waves |
| Lateral movement (DFS) | DFS + Stack | Attacker digs deep and backtracks |
| Targeted APT | Dijkstra + Min-Heap | Follows the easiest route to a chosen target, retrying each hop |
| Hub failure | Max-Heap | Takes out the best-connected machines |
| Random failure | Fisher-Yates | Takes out random machines, for comparison with hub failure |

**Defence and recovery:** isolate machines, cut links, handle incidents in severity order
(max-heap), undo (stack), rebuild the cheapest backbone with Kruskal or Prim,
cheapest and easiest routes with Dijkstra.

**Analysis:** critical machines and bridge links (Tarjan), robustness curve (targeted vs
random removal), Monte Carlo comparison of attack models, a defence advisor that
tries isolating every machine and ranks the best, Prim vs Kruskal comparison
(cost, heap operations, union-find operations, time), benchmark of all algorithms across
network sizes (50 to 3200 machines) and topologies, status and degree charts,
a session timeline and the spread curve of the last attack.

**Frontend:** pan and zoom, drag machines, four colour modes (status, risk, segment,
critical), size by importance, link-cost labels, travelling infection packets, a live
queue/stack strip, distance badges while Dijkstra runs, play/pause/step/skip and speed
control, algorithm trace, replay of the last attack and of the whole session, five
ready-made scenarios, light and dark theme, keyboard shortcuts, PNG export.

## Where each data structure and algorithm is used

| Item | File | Used for |
|---|---|---|
| Graph, adjacency LinkedList | `core/Graph.h`, `ds/LinkedList.h` | Topology; cost, risk, importance, blocked flag on links |
| BFS + Queue | `algo/Traversal.h` | Worm spread, network segments |
| DFS + Stack | `algo/Traversal.h` | Lateral movement with backtracking |
| Dijkstra + Min-Heap | `algo/ShortestPath.h` | Cheapest and easiest routes, APT path |
| Kruskal + Heap + DSU | `algo/SpanningTree.h`, `ds/DSU.h` | Recovery backbone |
| Prim + Min-Heap | `algo/SpanningTree.h` | Recovery backbone, compared with Kruskal |
| Tarjan DFS | `algo/Articulation.h` | Critical machines and bridges |
| Max-Heap | `sim/Simulation.h`, `algo/Attacks.h` | Incident queue, hub attack, advisor top-K |
| Hash table (chaining, rehash) | `ds/HashTable.h` | Machine name to id lookup |
| AVL tree | `ds/AVLTree.h` | Risk-ordered index with range queries |
| Stack | `sim/Simulation.h` | Undo history |
| LinkedList | `sim/Simulation.h` | Event log |
| Monte Carlo, robustness | `sim/Analysis.h` | Attack comparison, advisor, robustness curve |
| Benchmark | `sim/Benchmark.h` | Timing across sizes and topologies |

Everything in `ds/` is hand-written (no `std::queue`, `std::priority_queue`, `std::map`).
`std::vector` and `std::string` are used for plain arrays and text.

| Algorithm | Time | Space |
|---|---|---|
| BFS, DFS, components, Tarjan | O(V + E) | O(V) |
| Dijkstra | O((V + E) log V) | O(V) |
| Kruskal | O(E log E) | O(V + E) |
| Prim | O(E log V) | O(V + E) |
| Heap push / pop | O(log n) | O(n) |
| Hash table | O(1) average | O(n) |
| AVL tree | O(log n) | O(n) |

## Architecture

    browser (HTML, CSS, JavaScript canvas)  <-- JSON over HTTP -->  backend/main.cpp (cpp-httplib)
                                                                          |
                                                                   sim/Simulation.h
                                                                   core/  ds/  algo/

Each algorithm run returns a list of steps (`infect`, `dequeue`, `fail`, `backtrack`,
`visit`, `relax`, `set`, `cut`, `uncut`). The browser replays them, so all logic stays in C++.
The server records every action with its steps, which is what powers the session replay.

    backend/   core/ ds/ algo/ sim/ main.cpp
    frontend/  index.html  css/style.css  js/{util,charts,view,engine,panels,app}.js
    tests/     test_all.cpp
    third_party/httplib.h

## API

`POST /api/generate | attack | isolate | cut | restore | undo | respond | path | backbone |
reset | addnode | addlink | import`,
`GET /api/state | timeline | machine | risky | critical | mstcompare | robustness |
benchmark | monte | advise | export`.

## Save file format (.netsim)

Plain text: `NETSIM 1`, then `T topology seed`, `R recoveryCost`, `G rngState`,
`N id name type importance risk baseRisk status affected` per machine and
`E u v cost risk importance blocked` per link.

## Suggested demo for the review

1. Pick the "Worm outbreak" scenario and watch the queue strip and the trace.
2. Reset, run DFS from the same machine and compare the spread (Analytics, attack comparison).
3. Run the defence advisor, isolate its best pick, attack again, handle incidents.
4. Colour by "Critical machines", cut a bridge link and show the segments change.
5. Open Algorithms: compare Prim and Kruskal, then run the benchmark.
6. Replay the session, save the network, load it back.

## Notes

Tested on Linux with g++ 13 and Chromium. The Windows and macOS builds follow the same
Makefile and CMake files but were not run here.
