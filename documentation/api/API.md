# NEXA REST & WebSocket API

Base URL: `http://localhost:8080`. All endpoints except `/api/health` and `/api/auth/login` need `Authorization: Bearer <token>` (or `?token=` for downloads / WebSocket). Errors: `{ "error": { "code": "VALIDATION_ERROR|NOT_FOUND|CONFLICT|UNAUTHORIZED|FORBIDDEN|PERSISTENCE_ERROR", "message": "…" } }` with status 400/404/409/401/403/500.

| Area | Method & path | Permission | Notes |
|---|---|---|---|
| Auth | `POST /api/auth/login` `{username,password}` | public | returns token, role, permissions |
| | `POST /api/auth/logout`, `GET /api/auth/me` | view | |
| Users | `GET/POST /api/users`, `PUT /api/users/{id}`, `POST /api/users/{id}/password` | manage_users | |
| System | `GET /api/health` | public | |
| | `GET /api/dashboard`, `GET /api/activity?limit=` | view | |
| | `GET /api/system/logs`, `POST /api/system/reload` | manage_settings | reload = rebuild indexes from DB |
| | `GET/PUT /api/settings/weights` | view / manage_settings | priority + suitability weights |
| | `POST /api/actions/undo` | manage_incidents | pops the operator-action Stack |
| Incidents | `GET /api/incidents?status=&categoryId=&minSeverity=&prefix=&text=&sortBy=&algorithm=` | view | `stats` shows index + sort used |
| | `POST /api/incidents`, `GET/PUT/DELETE /api/incidents/{id}` | manage_incidents | delete archives if history exists |
| | `PATCH /api/incidents/{id}/status` `{status,note}`, `POST /api/incidents/{id}/notes` | manage_incidents | |
| | `GET /api/categories` | view | K-ary taxonomy with open counts |
| Search | `GET /api/search/incidents/suggest?q=` · `/text?q=` | view | Trie · Suffix array |
| | `GET /api/search/resources/suggest?q=` · `/lookup?field=&value=&method=linear|binary|hash` · `/code?code=` | view | |
| Resources | `GET /api/resources?category=&status=&capability=&prefix=&availableOnly=&sortBy=&algorithm=` | view | |
| | `POST /api/resources`, `GET/PUT/DELETE /api/resources/{id}`, `PATCH …/status` | manage_resources | delete = deactivate |
| | `GET /api/resource-types` | view | |
| Requests | `GET/POST /api/requests`, `GET /api/requests/{id}`, `POST …/escalate`, `POST …/cancel` | view / manage_requests | |
| | `GET /api/request-queue`, `POST /api/request-queue/process` `{mode: auto|fifo|priority}` | view / allocate | |
| Priority | `GET /api/priority/queue`, `GET /api/priority/heap`, `POST /api/priority/refresh`, `GET /api/priority/next` | view | |
| Ranking | `GET /api/ranking/{incidentId}?category=&algorithm=&limit=&road=&units=` | view | |
| Allocation | `GET /api/deployments?status=`, `POST /api/deployments` `{incidentId, items?:[{resourceId,units}], units?, category?, requestId?}` | view / allocate | items empty → automatic |
| | `GET /api/deployments/{id}`, `POST …/complete`, `POST …/cancel` `{reason}` | allocate | |
| | `POST /api/allocation/reassign` `{resourceId, fromDeploymentId, toIncidentId}`, `GET /api/allocation/matrix` | allocate / view | |
| Spatial | `GET /api/spatial/nearest?lat&lon&k&kind&category&capability&availableOnly` | view | KD-tree |
| | `/radius?…&km=`, `/region?minLat&minLon&maxLat&maxLon&kind&method=quadtree|rangetree|linear`, `/clusters?eps&minPoints`, `/density`, `/compare`, `/layers` | view | |
| Network | `GET /api/network/graph`, `/summary`, `/route?from&to&mode` or `?fromLat&fromLon&toLat&toLon&mode`, `/compare`, `/explore?source&algorithm=bfs|dfs`, `/mst?algorithm=kruskal|prim&mode`, `/components` | view | mode = distance|time|cost |
| | `PATCH /api/network/roads/{id}` `{blocked}` | manage_network | |
| Hospitals | `GET/POST /api/hospitals`, `GET/PUT/DELETE /api/hospitals/{id}`, `POST …/reserve` `{incidentId,beds,hours}`, `GET …/load?from&until` | view / manage_hospitals | |
| | `GET /api/medical/suitable?incidentId|lat&lon&capability&beds&limit`, `GET /api/reservations`, `POST /api/reservations/{id}/release` | | |
| Analytics | `GET /api/analytics/timeline?days&bucketHours&from&to`, `/response`, `/categories`, `/hospitals`, `/status` | view | segment-tree range query |
| CSV | `GET /api/export/{incidents|resources|deployments|hospitals}`, `POST /api/import/{resources|incidents}` (text/csv body) | view / manage_* | |
| Simulation | `GET/POST /api/simulations`, `GET /api/simulations/{id}/replay`, `POST …/step`, `POST …/resolve` | view / run_simulation | |
| DSA Lab | `GET /api/lab/catalog`, `GET /api/lab/benchmark?suite&n&repeats`, `POST /api/lab/run/{linear|sort|search|tree|heap|hash|graph|spatial|text|interval|segment|backtracking|sparse|multilist}` | view | |
| Live | `WS /ws/live?token=` | view | `{type, entityId, title, detail, lat, lon, severity, at}` |

Live event types: `incident.created|updated|resolved|deleted`, `deployment.created|closed`, `resource.updated`, `request.created`, `hospital.updated`, `network.updated`, `simulation.step`.
