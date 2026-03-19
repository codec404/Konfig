-- Migration 009: Named Configs
-- Adds config_name to config_metadata so each service can have multiple
-- independent named configs (e.g. "database-config", "feature-flags"),
-- each with their own version sequence.
--
-- Before: version is unique per service_name
-- After:  version is unique per (service_name, config_name)

-- Add config_name column; existing rows become "default"
ALTER TABLE config_metadata
    ADD COLUMN IF NOT EXISTS config_name VARCHAR(255) NOT NULL DEFAULT 'default';

-- Drop the old per-service version uniqueness constraint
ALTER TABLE config_metadata
    DROP CONSTRAINT IF EXISTS config_metadata_service_name_version_key;

-- New uniqueness: version is unique within (service, named-config)
ALTER TABLE config_metadata
    ADD CONSTRAINT config_metadata_service_config_version_key
    UNIQUE (service_name, config_name, version);

-- Index for efficient lookups by named config
CREATE INDEX IF NOT EXISTS idx_service_config_name
    ON config_metadata (service_name, config_name);

-- Patch up sample data inserted by 001_core_tables.sql
UPDATE config_metadata SET config_name = 'default' WHERE config_name = '';

SELECT '009: Named configs migration complete' AS status;
