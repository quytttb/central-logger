-- Migration v5: remove unused global maintenance_mode setting
ALTER TABLE app_settings DROP COLUMN maintenance_mode;
