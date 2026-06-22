-- Migration v4: history flush interval in app_settings
ALTER TABLE app_settings ADD COLUMN history_flush_interval_s INTEGER DEFAULT 5;
UPDATE app_settings SET history_flush_interval_s = 5
WHERE id = 1 AND history_flush_interval_s IS NULL;
