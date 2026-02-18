-- Migration 008: Database Permissions
-- Description: Grants necessary permissions to configuser
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Grant Permissions
-- ═══════════════════════════════════════════════════════════════════

GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO configuser;
GRANT ALL PRIVILEGES ON ALL SEQUENCES IN SCHEMA public TO configuser;
GRANT EXECUTE ON ALL FUNCTIONS IN SCHEMA public TO configuser;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO configuser;

-- Grant view permissions explicitly
GRANT SELECT ON latest_configs TO configuser;
GRANT SELECT ON active_rollouts TO configuser;
GRANT SELECT ON service_health_summary TO configuser;
GRANT SELECT ON validation_stats TO configuser;

-- Migration complete
SELECT 'Migration 008: Permissions granted' as status;