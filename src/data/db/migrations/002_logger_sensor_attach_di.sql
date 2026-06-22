-- Migration v2: attach-DI catalog columns on logger_sensor
ALTER TABLE logger_sensor ADD COLUMN parent_edge_sensor_id INTEGER;
ALTER TABLE logger_sensor ADD COLUMN di_type TEXT;
