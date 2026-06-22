-- Migration v3: multi-parent attach-DI chain
ALTER TABLE logger_sensor ADD COLUMN all_parent_ids TEXT;
