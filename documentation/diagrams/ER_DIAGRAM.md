# NEXA ER diagram

```mermaid
erDiagram
  roles ||--o{ users : "role_id"
  incident_categories ||--o{ incident_categories : "parent_id"
  incident_categories ||--o{ incidents : "category_id"
  simulation_scenarios ||--o{ incidents : "scenario_id"
  simulation_scenarios ||--o{ simulation_events : "scenario_id"
  incidents ||--o{ incident_events : "incident_id"
  incidents ||--o{ emergency_requests : "incident_id"
  incidents ||--o{ deployments : "incident_id"
  incidents ||--o{ bed_reservations : "incident_id"
  emergency_requests |o--o| deployments : "request_id / deployment_id"
  deployments ||--|{ deployment_resources : "deployment_id"
  resources ||--o{ deployment_resources : "resource_id"
  resource_types ||--o{ resources : "type_id"
  response_teams ||--o{ resources : "team_id"
  locations ||--o{ response_teams : "base_location_id"
  resources ||--o{ resource_availability : "resource_id"
  hospitals ||--o{ bed_reservations : "hospital_id"
  locations ||--o{ roads : "from_location_id"
  locations ||--o{ roads : "to_location_id"

  incidents { bigint id PK; varchar code UK; varchar title; int category_id FK; tinyint severity; decimal latitude; decimal longitude; enum status; int affected_people; varchar required_capabilities; int required_units; datetime created_at; tinyint simulated }
  resources { bigint id PK; varchar code UK; int type_id FK; int capacity; int available; enum status; decimal latitude; decimal longitude; varchar capabilities }
  deployments { bigint id PK; bigint incident_id FK; bigint request_id FK; enum status; datetime created_at; datetime completed_at }
  deployment_resources { bigint deployment_id PK; bigint resource_id PK; int units; decimal suitability; decimal route_km; decimal eta_minutes; text reason; tinyint released }
  hospitals { bigint id PK; varchar name UK; enum category; int total_beds; int available_beds; int icu_beds; int icu_available }
  roads { bigint id PK; bigint from_location_id FK; bigint to_location_id FK; decimal distance_km; decimal travel_minutes; decimal penalty; tinyint blocked }
```

Constraints worth mentioning in the viva: `CHECK (available BETWEEN 0 AND capacity)`, `CHECK (severity BETWEEN 1 AND 5)`, `CHECK (distance_km >= 0 …)` (Dijkstra requires non-negative weights), composite PK on `deployment_resources`, `UNIQUE (scenario_id, seq)` for replay ordering, cascading deletes only for dependent history rows.
