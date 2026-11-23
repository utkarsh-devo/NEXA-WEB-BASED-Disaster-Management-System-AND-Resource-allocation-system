# NEXA — Disaster Response, Emergency Resource Allocation, Spatial Search & Network Routing

A full-stack emergency-operations platform whose computational core is a **C++17 engine of hand-written data structures and algorithms**. The React console only visualises what the engine computes.

```
 React 18 console (Vite, React Router, framer-motion, Recharts, MapLibre GL + OSM/CARTO tiles)
        │  REST /api/*  +  WebSocket /ws/live
        ▼
 Drogon HTTP layer ── controllers/ (JSON mapping, auth, error → HTTP status)
        │
 Service layer ────── services/  (incident, resource, request, priority, ranking, spatial,
        │                          network, allocation, hospital, analytics, simulation, lab, auth)
        ├── OperationalStore (in-memory indexes: AVL, Hash, Trie, Suffix Array, KD/Quad/Range,
        │                     Graph, Heap/PQ, Queue, Stack, Linked/Multi-linked lists, Sparse
        │                     matrix, Interval trees, K-ary taxonomy)
        ▼
 Repositories ─────── MySQL (Drogon ORM, transactions)   |   In-memory (tests / --memory demo)
```

> **All demo data is SIMULATED.** Place names and approximate coordinates in Delhi NCR are real so the map is meaningful; hospital capacities, incidents, units and road travel times are invented. Travel times are estimates from road-class speeds, not live traffic.

---

## 1. Quick start (no MySQL needed)

The fastest way to see everything working is the **in-memory mode**: the engine seeds the demo dataset at start-up and nothing is persisted.

```bash
# backend
cd backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release        # see §3 for installing Drogon
cmake --build build --config Release
cd build
./nexa_server --memory              # Windows: .\Release\nexa_server.exe --memory

# frontend (new terminal)
cd frontend
npm install
npm run dev                          # http://localhost:5173  (proxies /api and /ws to :8080)
```

Demo accounts (created by the seeder):

| user | password | role |
|---|---|---|
| `admin` | `Nexa@2026` | Administrator |
| `operator` | `Operator@2026` | Incident Operator |
| `coordinator` | `Coord@2026` | Resource Coordinator |
| `medic` | `Medic@2026` | Medical Coordinator |
| `viewer` | `Viewer@2026` | Read-only Viewer |

## 2. Running with MySQL

```bash
mysql -u root -p < database/schema/schema.sql          # creates database `nexa` + 19 tables
mysql -u root -p -e "CREATE USER 'nexa_app'@'localhost' IDENTIFIED BY 'change-me'; GRANT ALL ON nexa.* TO 'nexa_app'@'localhost';"
```

Edit `backend/build/config.json` (copied from `config.example.json` on first configure) → `custom_config.mysql`. Then run `./nexa_server`. With `"auto_seed": true` the engine inserts the demo dataset on the first start into an empty database (timestamps relative to that moment). Alternatively load the pre-generated `database/seed/seed_demo.sql`.

## 3. Prerequisites

| Tool | Version | Notes |
|---|---|---|
| C++ compiler | GCC ≥ 9, Clang ≥ 10, MSVC 2019+ | C++17 |
| CMake | ≥ 3.16 | |
| Drogon | ≥ 1.8 (with MySQL support) | pulls in jsoncpp, trantor |
| MySQL | 8.0.16+ | CHECK constraints are used |
| Node.js | ≥ 18 | frontend |
| GoogleTest | optional | if absent, the bundled shim runs the same tests |

**Windows (recommended: vcpkg)**
```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg; C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg install drogon[mysql]:x64-windows gtest:x64-windows
cmake -S backend -B backend\build -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
cmake --build backend\build --config Release
```
**Ubuntu / WSL**
```bash
sudo apt install build-essential cmake libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev libmysqlclient-dev libgtest-dev
git clone --recurse-submodules https://github.com/drogonframework/drogon && cd drogon && cmake -B build -DBUILD_MYSQL=ON && sudo cmake --build build --target install
```
To build only the engine + tests (no Drogon): `cmake -S backend -B build -DNEXA_BUILD_SERVER=OFF`.

## 4. Tests

```bash
cd backend/build && ctest --output-on-failure        # or ./nexa_tests
```
42 tests: every data structure (checked against brute force where applicable — KD/Quad/Range trees, suffix structures, MST Kruskal = Prim), plus integration tests of the full workflow on the in-memory repositories: registration → classification → priority → ranking → allocation → completion → resolution, **transaction rollback on a simulated DB failure**, conflict rules (unavailable units, simultaneous deployments, duplicate codes), FIFO vs priority request queues, reproducible simulation, auth/roles. They also pass under AddressSanitizer + UBSan.

## 5. Folder structure

```
NEXA/
├── backend/
│   ├── include/
│   │   ├── data_structures/   27 hand-written structures (header-only templates)
│   │   ├── algorithms/        Sorting, Searching (incl. median-of-medians), Graph, Backtracking
│   │   ├── models/            domain types + enums
│   │   ├── repositories/      interfaces, MySQL + in-memory implementations
│   │   ├── services/          business logic, OperationalStore, scoring config
│   │   ├── controllers/       Drogon controllers, JSON mappers, API helpers
│   │   └── utilities/         Geo (Haversine), validation, errors, SHA-256, CSV, logger
│   ├── src/{services,controllers,repositories}/ + main.cpp
│   ├── tests/                 unit + integration tests (GoogleTest API)
│   ├── tools/export_seed_sql.cpp
│   ├── CMakeLists.txt, config.example.json
├── frontend/src/{pages,components,components/lab,layouts,hooks,services,maps,charts,styles}
├── database/{schema,seed,migrations,backups}
├── map_data/{locations,road_networks}   CSV export of the seeded road graph
├── documentation/{architecture,api,diagrams}
├── reports/  logs/
```

## 6. Where each data structure is used

| Structure | File | Operational use |
|---|---|---|
| Dynamic array | `DynamicArray.hpp` | registries of incidents/resources/hospitals; every collection |
| Stack | `Stack.hpp` | operator action history → *Undo last status change* |
| Queue (circular) | `Queue.hpp` | FIFO emergency requests; BFS |
| Doubly linked list | `LinkedList.hpp` | incident timelines, resource & request history, WebSocket connections |
| Multi-linked list | `MultiLinkedList.hpp` | incident ↔ resource deployment relationships |
| Sparse matrix | `SparseMatrix.hpp` | incident × resource units matrix (Allocation page) |
| BST | `BST.hpp` | hospital index by name (alphabetical listing) |
| AVL tree | `AVLTree.hpp` | ordered id index of every registry; incident time index |
| Red-black, B-tree, B+ tree, threaded BT | lab files | DSA Lab comparisons (B-tree explains MySQL indexes) |
| K-ary tree | `KaryTree.hpp` | disaster-category taxonomy, classification path & analytics roll-up |
| Binary heap / indexed PQ | `BinaryHeap.hpp`, `PriorityQueue.hpp` | incident priority engine, escalated requests, Dijkstra, Prim |
| Hash table (4 strategies) | `HashTable.hpp` | id & code lookups, PQ position map, sessions |
| Graph (list + matrix) | `Graph.hpp`, `GraphAlgorithms.hpp` | road network: BFS, DFS, Dijkstra, Kruskal, Prim, components |
| Disjoint set | `DisjointSet.hpp` | Kruskal |
| KD tree (3-D on unit sphere) | `KDTree.hpp` | nearest resources/hospitals, road-vertex snapping, DBSCAN clusters |
| Quad tree | `QuadTree.hpp` | map region queries, density grid |
| Range tree | `RangeTree.hpp` | guaranteed O(log² n + m) incident box queries |
| Interval tree | `IntervalTree.hpp` | per-resource commitments (conflict check), per-hospital bed windows |
| Segment tree | `SegmentTree.hpp` | analytics timeline range sum / max with lazy updates |
| Trie | `Trie.hpp` | global search autocomplete (Ctrl + K) |
| Suffix array + LCP | `SuffixArray.hpp` | substring search in incident descriptions |
| Suffix tree | `SuffixTree.hpp` | lab comparison with the suffix array |
| Searching / sorting | `Searching.hpp`, `Sorting.hpp` | ranking, listings, median distance, P90 response time |
| Backtracking | `Backtracking.hpp` | Rat in a Maze, N-Queens (lab) |

## 7. Documented formulas (configurable in Settings / `config.json`)

* **Priority** `P = 100·(wS·S + wA·A + wU·U + wT·T + wSt·St + wTs·Ts)/Σw` — severity, people affected (log-scaled), unmet units, age (anti-starvation), status, category time-sensitivity. Ties → FIFO.
* **Suitability** `Q = 100·(wC·C + wP·P + wAv·Av + wK·K)/Σw` with `P = 1/(1 + roadKm/scale)`. Hard filters exclude unavailable units and units with no required capability. The nearest unit is not automatically chosen.
* **Request urgency** `R = 50·u/5 + 20·min(1, log₁₀(1+people)/3) + 30·incidentPriority/100`; `R ≥ threshold` or urgency 5 → priority queue.

## 8. Honest limitations

* The Drogon/MySQL layer (controllers, repositories, `main.cpp`) was compile-checked against signature-equivalent stand-in headers, not against a real Drogon install, in the environment where it was written. The engine, services and tests were fully compiled and run. If your Drogon version differs, small API adjustments may be needed (most likely `setContentTypeCodeAndCustomString` → `setContentTypeString`).
* Passwords use salted, iterated SHA-256 (self-contained); production systems should use Argon2id/bcrypt.
* Suffix-tree construction is the naive O(n²) insertion (Ukkonen is O(n)); the production search uses the suffix array.
* B+ tree deletion is lazy (no merge/redistribution); documented in the header.
* "Median Search" is implemented as median-of-medians selection; if your syllabus defines it differently, replace `algo::medianSearch` only.
* Road travel times are seeded estimates; routes are shortest *for the chosen weights*.
* Commitment windows are open-ended until 2038-01-19 so they fit a 32-bit `long` (MSVC).
