# NEXA frontend

React 18 + Vite command console. `npm install && npm run dev` → http://localhost:5173.
`/api` and `/ws` are proxied to the C++ engine on `http://localhost:8080` (override with `NEXA_API=http://host:port npm run dev`).

* `src/pages/` — one file per screen (18 pages)
* `src/components/` — HUD panels, tables, forms, modal; `components/lab/` — DSA Lab visualisers
* `src/maps/` — MapLibre wrapper (OSM data via CARTO dark tiles; rendering only)
* `src/charts/` — Recharts wrappers; `src/services/` — REST + WebSocket clients
* `src/styles/` — design tokens, layout, animations (respects `prefers-reduced-motion`)
