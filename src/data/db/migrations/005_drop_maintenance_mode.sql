-- Migration v5: remove unused global maintenance_mode setting.
--
-- Audit H-G: rebuild the table instead of `ALTER TABLE ... DROP COLUMN`,
-- because DROP COLUMN requires SQLite >= 3.35 and would prevent the app
-- from starting on older distros (e.g. Ubuntu 20.04 ships SQLite 3.31).
CREATE TABLE app_settings_new (
    id                       INTEGER PRIMARY KEY CHECK (id = 1),
    theme                    TEXT    NOT NULL DEFAULT 'dark',
    system_timezone          TEXT    NOT NULL DEFAULT 'Asia/Ho_Chi_Minh',
    data_retention_days      INTEGER NOT NULL DEFAULT 30,
    history_flush_interval_s INTEGER NOT NULL DEFAULT 5
);

INSERT INTO app_settings_new (
    id, theme, system_timezone, data_retention_days, history_flush_interval_s
)
SELECT id, theme, system_timezone, data_retention_days,
       COALESCE(history_flush_interval_s, 5)
FROM app_settings;

DROP TABLE app_settings;

ALTER TABLE app_settings_new RENAME TO app_settings;

INSERT OR IGNORE INTO app_settings (id) VALUES (1);
