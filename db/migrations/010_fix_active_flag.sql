-- Migration 010: Fix is_active deduplication
-- Ensures at most one version per (service_name, config_name) is marked active.
-- The highest version in each named config becomes the sole active version.

UPDATE config_metadata cm
SET is_active = false
WHERE is_active = true
  AND version < (
      SELECT MAX(version)
      FROM config_metadata cm2
      WHERE cm2.service_name = cm.service_name
        AND cm2.config_name  = cm.config_name
        AND cm2.is_active    = true
  );

SELECT '010: is_active deduplication complete' AS status;
