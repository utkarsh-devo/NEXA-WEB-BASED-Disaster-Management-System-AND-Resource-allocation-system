-- ============================================================================
-- NEXA — MySQL 8.0 schema
-- 18 specification tables + bed_reservations (needed so hospital capacity
-- stays consistent with actual reservations, spec Module 10).
-- Run:  mysql -u root -p < database/schema/schema.sql
-- Operational demo data is inserted by the backend on first start
-- ("auto_seed": true) or from database/seed/seed_demo.sql.
-- ============================================================================
CREATE DATABASE IF NOT EXISTS nexa CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE nexa;

SET FOREIGN_KEY_CHECKS = 0;
DROP TABLE IF EXISTS system_logs, simulation_events, simulation_scenarios, resource_availability, incident_events,
    deployment_resources, deployments, emergency_requests, bed_reservations, roads, locations, response_teams,
    resources, resource_types, hospitals, incidents, incident_categories, users, roles;
SET FOREIGN_KEY_CHECKS = 1;

-- ---------------------------------------------------------------- 1-2 access
CREATE TABLE roles (
    id          TINYINT UNSIGNED PRIMARY KEY,
    code        VARCHAR(32)  NOT NULL UNIQUE,
    name        VARCHAR(64)  NOT NULL
) ENGINE=InnoDB;

CREATE TABLE users (
    id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    username       VARCHAR(40)  NOT NULL UNIQUE,
    display_name   VARCHAR(80)  NOT NULL,
    password_hash  CHAR(64)     NOT NULL,      -- hex SHA-256^20000(salt:password)
    salt           CHAR(32)     NOT NULL,
    role_id        TINYINT UNSIGNED NOT NULL,
    active         TINYINT(1)   NOT NULL DEFAULT 1,
    created_at     DATETIME     NOT NULL,
    last_login_at  DATETIME     NULL,
    CONSTRAINT fk_users_role FOREIGN KEY (role_id) REFERENCES roles(id)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 3-4 incidents
CREATE TABLE incident_categories (
    id                   INT PRIMARY KEY,                  -- explicit ids, mirrored by the K-ary taxonomy
    parent_id            INT NULL,
    name                 VARCHAR(80)  NOT NULL,
    code                 VARCHAR(32)  NOT NULL UNIQUE,
    time_sensitivity     DECIMAL(4,3) NOT NULL DEFAULT 0.5,
    default_capabilities VARCHAR(255) NOT NULL DEFAULT '',
    CONSTRAINT fk_cat_parent FOREIGN KEY (parent_id) REFERENCES incident_categories(id),
    CONSTRAINT chk_cat_ts CHECK (time_sensitivity BETWEEN 0 AND 1)
) ENGINE=InnoDB;

CREATE TABLE simulation_scenarios (
    id             BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name           VARCHAR(120) NOT NULL,
    scenario_type  VARCHAR(32)  NOT NULL,
    description    VARCHAR(255) NOT NULL,
    center_lat     DECIMAL(9,6) NOT NULL,
    center_lon     DECIMAL(9,6) NOT NULL,
    incident_count INT NOT NULL,
    seed           BIGINT UNSIGNED NOT NULL,
    created_at     DATETIME NOT NULL
) ENGINE=InnoDB;

CREATE TABLE incidents (
    id                    BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    code                  VARCHAR(32)  NULL UNIQUE,         -- INC-YYYY-NNNN, assigned right after insert
    title                 VARCHAR(160) NOT NULL,
    description           TEXT         NOT NULL,
    category_id           INT          NOT NULL,
    severity              TINYINT      NOT NULL,
    latitude              DECIMAL(9,6) NOT NULL,
    longitude             DECIMAL(9,6) NOT NULL,
    location_name         VARCHAR(120) NOT NULL DEFAULT '',
    status                ENUM('PENDING','ACTIVE','CONTAINED','RESOLVED','ARCHIVED') NOT NULL DEFAULT 'PENDING',
    affected_people       INT UNSIGNED NOT NULL DEFAULT 0,
    required_capabilities VARCHAR(255) NOT NULL DEFAULT '',
    required_units        INT NOT NULL DEFAULT 1,
    reported_by           VARCHAR(80)  NOT NULL DEFAULT '',
    reporter_contact      VARCHAR(80)  NOT NULL DEFAULT '',
    created_at            DATETIME NOT NULL,
    updated_at            DATETIME NOT NULL,
    resolved_at           DATETIME NULL,
    simulated             TINYINT(1) NOT NULL DEFAULT 0,
    scenario_id           BIGINT UNSIGNED NULL,
    CONSTRAINT fk_inc_category FOREIGN KEY (category_id) REFERENCES incident_categories(id),
    CONSTRAINT fk_inc_scenario FOREIGN KEY (scenario_id) REFERENCES simulation_scenarios(id) ON DELETE SET NULL,
    CONSTRAINT chk_inc_severity CHECK (severity BETWEEN 1 AND 5),
    CONSTRAINT chk_inc_lat CHECK (latitude BETWEEN -90 AND 90),
    CONSTRAINT chk_inc_lon CHECK (longitude BETWEEN -180 AND 180),
    CONSTRAINT chk_inc_units CHECK (required_units >= 1),
    INDEX idx_inc_status (status),
    INDEX idx_inc_created (created_at),
    INDEX idx_inc_category (category_id),
    INDEX idx_inc_geo (latitude, longitude)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 5-6 resources
CREATE TABLE resource_types (
    id                   INT PRIMARY KEY,
    name                 VARCHAR(80) NOT NULL UNIQUE,
    category             ENUM('ambulance','fire_engine','rescue_vehicle','medical_supplies','rescue_equipment',
                              'food_water','shelter','personnel','special_team') NOT NULL,
    default_capabilities VARCHAR(255) NOT NULL DEFAULT ''
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 10 locations
CREATE TABLE locations (
    id         BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name       VARCHAR(120) NOT NULL,
    kind       ENUM('junction','station','hospital','shelter','depot') NOT NULL,
    latitude   DECIMAL(9,6) NOT NULL,
    longitude  DECIMAL(9,6) NOT NULL,
    INDEX idx_loc_geo (latitude, longitude)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 8 response teams
CREATE TABLE response_teams (
    id                BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name              VARCHAR(120) NOT NULL UNIQUE,
    contact           VARCHAR(80)  NULL,
    base_location_id  BIGINT UNSIGNED NULL,
    CONSTRAINT fk_team_location FOREIGN KEY (base_location_id) REFERENCES locations(id) ON DELETE SET NULL
) ENGINE=InnoDB;

CREATE TABLE resources (
    id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    code          VARCHAR(32)  NOT NULL UNIQUE,
    name          VARCHAR(120) NOT NULL,
    type_id       INT NOT NULL,
    team_id       BIGINT UNSIGNED NULL,
    capacity      INT NOT NULL,
    available     INT NOT NULL,
    status        ENUM('AVAILABLE','DEPLOYED','MAINTENANCE','INACTIVE') NOT NULL DEFAULT 'AVAILABLE',
    latitude      DECIMAL(9,6) NOT NULL,
    longitude     DECIMAL(9,6) NOT NULL,
    capabilities  VARCHAR(255) NOT NULL DEFAULT '',
    updated_at    DATETIME NOT NULL,
    CONSTRAINT fk_res_type FOREIGN KEY (type_id) REFERENCES resource_types(id),
    CONSTRAINT fk_res_team FOREIGN KEY (team_id) REFERENCES response_teams(id) ON DELETE SET NULL,
    CONSTRAINT chk_res_capacity CHECK (capacity >= 1),
    CONSTRAINT chk_res_available CHECK (available BETWEEN 0 AND capacity),
    INDEX idx_res_status (status),
    INDEX idx_res_type (type_id)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 7 hospitals
CREATE TABLE hospitals (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(160) NOT NULL UNIQUE,
    category        ENUM('general','trauma','burn','pediatric','multispecialty') NOT NULL,
    latitude        DECIMAL(9,6) NOT NULL,
    longitude       DECIMAL(9,6) NOT NULL,
    total_beds      INT NOT NULL,
    available_beds  INT NOT NULL,
    icu_beds        INT NOT NULL DEFAULT 0,
    icu_available   INT NOT NULL DEFAULT 0,
    capabilities    VARCHAR(255) NOT NULL DEFAULT '',
    contact         VARCHAR(80)  NOT NULL DEFAULT '',
    address         VARCHAR(255) NOT NULL DEFAULT '',
    updated_at      DATETIME NOT NULL,
    CONSTRAINT chk_hosp_beds CHECK (available_beds BETWEEN 0 AND total_beds),
    CONSTRAINT chk_hosp_icu CHECK (icu_available BETWEEN 0 AND icu_beds AND icu_beds <= total_beds)
) ENGINE=InnoDB;

CREATE TABLE bed_reservations (
    id              BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    hospital_id     BIGINT UNSIGNED NOT NULL,
    incident_id     BIGINT UNSIGNED NOT NULL,
    beds            INT NOT NULL,
    reserved_from   DATETIME NOT NULL,
    reserved_until  DATETIME NOT NULL,
    released        TINYINT(1) NOT NULL DEFAULT 0,
    CONSTRAINT fk_bed_hospital FOREIGN KEY (hospital_id) REFERENCES hospitals(id),
    CONSTRAINT fk_bed_incident FOREIGN KEY (incident_id) REFERENCES incidents(id),
    CONSTRAINT chk_bed_count CHECK (beds >= 1),
    CONSTRAINT chk_bed_window CHECK (reserved_until >= reserved_from),
    INDEX idx_bed_hospital (hospital_id, released)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 11 roads
CREATE TABLE roads (
    id                BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    from_location_id  BIGINT UNSIGNED NOT NULL,
    to_location_id    BIGINT UNSIGNED NOT NULL,
    name              VARCHAR(200) NOT NULL,
    distance_km       DECIMAL(8,3) NOT NULL,
    travel_minutes    DECIMAL(8,2) NOT NULL,     -- ESTIMATED from road class speeds, not live traffic
    penalty           DECIMAL(8,2) NOT NULL DEFAULT 0,
    blocked           TINYINT(1)   NOT NULL DEFAULT 0,
    CONSTRAINT fk_road_from FOREIGN KEY (from_location_id) REFERENCES locations(id) ON DELETE CASCADE,
    CONSTRAINT fk_road_to   FOREIGN KEY (to_location_id)   REFERENCES locations(id) ON DELETE CASCADE,
    CONSTRAINT chk_road_nonneg CHECK (distance_km >= 0 AND travel_minutes >= 0 AND penalty >= 0),   -- Dijkstra needs non-negative weights
    INDEX idx_road_from (from_location_id),
    INDEX idx_road_to (to_location_id)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 9, 12, 13 requests & deployments
CREATE TABLE emergency_requests (
    id               BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    incident_id      BIGINT UNSIGNED NOT NULL,
    assistance_type  ENUM('medical','rescue','fire','evacuation','supplies','shelter') NOT NULL,
    description      TEXT NOT NULL,
    urgency          TINYINT NOT NULL,
    people_affected  INT UNSIGNED NOT NULL DEFAULT 0,
    status           ENUM('QUEUED','ESCALATED','PROCESSING','FULFILLED','CANCELLED') NOT NULL DEFAULT 'QUEUED',
    urgency_score    DECIMAL(6,2) NOT NULL DEFAULT 0,
    requested_by     VARCHAR(80) NOT NULL DEFAULT '',
    created_at       DATETIME NOT NULL,
    processed_at     DATETIME NULL,
    deployment_id    BIGINT UNSIGNED NULL,
    CONSTRAINT fk_req_incident FOREIGN KEY (incident_id) REFERENCES incidents(id) ON DELETE CASCADE,
    CONSTRAINT chk_req_urgency CHECK (urgency BETWEEN 1 AND 5),
    INDEX idx_req_status (status)
) ENGINE=InnoDB;

CREATE TABLE deployments (
    id            BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    incident_id   BIGINT UNSIGNED NOT NULL,
    request_id    BIGINT UNSIGNED NULL,
    status        ENUM('ACTIVE','COMPLETED','CANCELLED') NOT NULL DEFAULT 'ACTIVE',
    created_by    VARCHAR(80) NOT NULL,
    notes         VARCHAR(500) NOT NULL DEFAULT '',
    created_at    DATETIME NOT NULL,
    completed_at  DATETIME NULL,
    CONSTRAINT fk_dep_incident FOREIGN KEY (incident_id) REFERENCES incidents(id),
    CONSTRAINT fk_dep_request  FOREIGN KEY (request_id) REFERENCES emergency_requests(id) ON DELETE SET NULL,
    INDEX idx_dep_status (status),
    INDEX idx_dep_incident (incident_id)
) ENGINE=InnoDB;

ALTER TABLE emergency_requests
    ADD CONSTRAINT fk_req_deployment FOREIGN KEY (deployment_id) REFERENCES deployments(id) ON DELETE SET NULL;

CREATE TABLE deployment_resources (
    deployment_id  BIGINT UNSIGNED NOT NULL,
    resource_id    BIGINT UNSIGNED NOT NULL,
    units          INT NOT NULL,
    suitability    DECIMAL(6,2) NOT NULL DEFAULT 0,
    route_km       DECIMAL(8,2) NOT NULL DEFAULT 0,
    eta_minutes    DECIMAL(8,2) NOT NULL DEFAULT 0,
    reason         TEXT NOT NULL,                  -- WHY the resource was selected (spec Module 9)
    released       TINYINT(1) NOT NULL DEFAULT 0,
    released_at    DATETIME NULL,
    PRIMARY KEY (deployment_id, resource_id),
    CONSTRAINT fk_dr_deployment FOREIGN KEY (deployment_id) REFERENCES deployments(id) ON DELETE CASCADE,
    CONSTRAINT fk_dr_resource   FOREIGN KEY (resource_id) REFERENCES resources(id),
    CONSTRAINT chk_dr_units CHECK (units >= 1),
    INDEX idx_dr_resource (resource_id, released)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 14-15 histories
CREATE TABLE incident_events (
    id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    incident_id  BIGINT UNSIGNED NOT NULL,
    event_type   VARCHAR(32)  NOT NULL,
    message      TEXT NOT NULL,
    actor        VARCHAR(80)  NOT NULL,
    created_at   DATETIME NOT NULL,
    CONSTRAINT fk_evt_incident FOREIGN KEY (incident_id) REFERENCES incidents(id) ON DELETE CASCADE,
    INDEX idx_evt_incident (incident_id, created_at)
) ENGINE=InnoDB;

CREATE TABLE resource_availability (
    id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    resource_id  BIGINT UNSIGNED NOT NULL,
    action       VARCHAR(32)  NOT NULL,          -- REGISTERED | UPDATED | DEPLOYED | RELEASED | STATUS
    detail       VARCHAR(255) NOT NULL,
    created_at   DATETIME NOT NULL,
    CONSTRAINT fk_ra_resource FOREIGN KEY (resource_id) REFERENCES resources(id) ON DELETE CASCADE,
    INDEX idx_ra_resource (resource_id, created_at)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- 17-18 simulation & logs
CREATE TABLE simulation_events (
    id           BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    scenario_id  BIGINT UNSIGNED NOT NULL,
    seq          INT NOT NULL,
    event_type   VARCHAR(32)  NOT NULL,
    summary      VARCHAR(600) NOT NULL,
    entity_id    BIGINT NOT NULL DEFAULT 0,
    latitude     DECIMAL(9,6) NOT NULL DEFAULT 0,
    longitude    DECIMAL(9,6) NOT NULL DEFAULT 0,
    created_at   DATETIME NOT NULL,
    CONSTRAINT fk_se_scenario FOREIGN KEY (scenario_id) REFERENCES simulation_scenarios(id) ON DELETE CASCADE,
    UNIQUE KEY uq_se_seq (scenario_id, seq)
) ENGINE=InnoDB;

CREATE TABLE system_logs (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    level       ENUM('INFO','WARN','ERROR','AUDIT') NOT NULL,
    source      VARCHAR(40)  NOT NULL,
    message     VARCHAR(600) NOT NULL,
    actor       VARCHAR(80)  NOT NULL,
    created_at  DATETIME NOT NULL,
    INDEX idx_log_created (created_at)
) ENGINE=InnoDB;

-- ---------------------------------------------------------------- reference data
INSERT INTO roles(id, code, name) VALUES
    (1, 'ADMIN', 'Administrator'),
    (2, 'INCIDENT_OPERATOR', 'Incident Operator'),
    (3, 'RESOURCE_COORDINATOR', 'Resource Coordinator'),
    (4, 'MEDICAL_COORDINATOR', 'Medical Coordinator'),
    (5, 'VIEWER', 'Read-only Viewer');
