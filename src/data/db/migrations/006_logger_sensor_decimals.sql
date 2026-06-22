-- Migration v6: per-sensor display precision (decimals) on logger_sensor
ALTER TABLE logger_sensor ADD COLUMN decimals INTEGER NOT NULL DEFAULT 4;
