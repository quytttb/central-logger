-- Central Logger — SQLite schema (user_version = 3)
-- Reference: docs/thiet_ke_db.md
-- Engine: SQLite (QSQLITE) — see docs/adr/0001-db.md

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS logger_info (
    id                      INTEGER PRIMARY KEY AUTOINCREMENT,
    station_code            TEXT    NOT NULL UNIQUE,
    name                    TEXT    NOT NULL,
    host                    TEXT    NOT NULL,
    modbus_port             INTEGER NOT NULL DEFAULT 5020,
    modbus_unit_id          INTEGER NOT NULL DEFAULT 1,
    central_poll_interval_s INTEGER NOT NULL DEFAULT 2,
    timeout_s               REAL    NOT NULL DEFAULT 2.0,
    enabled                 INTEGER NOT NULL DEFAULT 1,
    api_port                INTEGER NOT NULL DEFAULT 8080,
    api_token               TEXT,
    last_revision           INTEGER NOT NULL DEFAULT -1,
    status                  TEXT    NOT NULL DEFAULT 'offline',
    last_seen               TEXT,
    note                    TEXT,
    created_at              TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX IF NOT EXISTS idx_logger_info_status    ON logger_info(status);
CREATE INDEX IF NOT EXISTS idx_logger_info_last_seen ON logger_info(last_seen);

CREATE TABLE IF NOT EXISTS logger_sensor (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    logger_id       INTEGER NOT NULL,
    edge_sensor_id  INTEGER NOT NULL,
    sensor_type     TEXT    NOT NULL DEFAULT 'UNKNOWN',
    name            TEXT    NOT NULL DEFAULT '',
    unit            TEXT    NOT NULL DEFAULT '',
    min_threshold          REAL,
    max_threshold          REAL,
    active                 INTEGER NOT NULL DEFAULT 1,
    parent_edge_sensor_id  INTEGER,
    di_type                TEXT,
    all_parent_ids         TEXT,
    UNIQUE(logger_id, sensor_type, edge_sensor_id),
    FOREIGN KEY (logger_id) REFERENCES logger_info(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_logger_sensor_logger_id ON logger_sensor(logger_id);

CREATE TABLE IF NOT EXISTS sensor_reading (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    sensor_id        INTEGER NOT NULL,
    value            REAL    NOT NULL,
    valid            INTEGER NOT NULL DEFAULT 1,
    alarm            INTEGER NOT NULL DEFAULT 0,
    stale            INTEGER NOT NULL DEFAULT 0,
    logger_timestamp INTEGER NOT NULL DEFAULT 0,
    recorded_at      TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    FOREIGN KEY (sensor_id) REFERENCES logger_sensor(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_sensor_reading_sensor_recorded ON sensor_reading(sensor_id, recorded_at);
CREATE INDEX IF NOT EXISTS idx_sensor_reading_recorded        ON sensor_reading(recorded_at);

CREATE TABLE IF NOT EXISTS system_event (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    logger_id  INTEGER,
    event_type TEXT    NOT NULL,
    message    TEXT    NOT NULL DEFAULT '',
    level      TEXT    NOT NULL DEFAULT 'info',
    created_at TEXT    NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now')),
    FOREIGN KEY (logger_id) REFERENCES logger_info(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_system_event_logger_created ON system_event(logger_id, created_at);
CREATE INDEX IF NOT EXISTS idx_system_event_created        ON system_event(created_at);

CREATE TABLE IF NOT EXISTS app_settings (
    id                  INTEGER PRIMARY KEY CHECK (id = 1),
    theme               TEXT    NOT NULL DEFAULT 'dark',
    system_timezone     TEXT    NOT NULL DEFAULT 'Asia/Ho_Chi_Minh',
    data_retention_days INTEGER NOT NULL DEFAULT 30,
    maintenance_mode    INTEGER NOT NULL DEFAULT 0
);

INSERT OR IGNORE INTO app_settings (id) VALUES (1);
