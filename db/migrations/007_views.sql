-- Migration 007: Useful Views
-- Description: Creates views for common queries and monitoring
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Latest Configs View
-- ═══════════════════════════════════════════════════════════════════

CREATE OR REPLACE VIEW latest_configs AS
SELECT DISTINCT ON (service_name)
    config_id,
    service_name,
    version,
    format,
    created_at,
    created_by,
    description,
    is_active
FROM config_metadata
WHERE is_active = true
ORDER BY service_name, version DESC;

-- ═══════════════════════════════════════════════════════════════════
-- Active Rollouts View
-- ═══════════════════════════════════════════════════════════════════

CREATE OR REPLACE VIEW active_rollouts AS
SELECT
    r.id as rollout_id,
    r.config_id,
    cm.service_name,
    cm.version,
    r.strategy,
    r.target_percentage,
    r.current_percentage,
    r.status,
    r.started_at,
    r.completed_at
FROM rollout_state r
JOIN config_metadata cm ON r.config_id = cm.config_id
WHERE r.status IN ('IN_PROGRESS', 'PENDING');

-- ═══════════════════════════════════════════════════════════════════
-- Service Health Summary View
-- ═══════════════════════════════════════════════════════════════════

CREATE OR REPLACE VIEW service_health_summary AS
SELECT
    si.service_name,
    COUNT(DISTINCT si.instance_id) as total_instances,
    COUNT(DISTINCT CASE WHEN si.status = 'connected' THEN si.instance_id END) as connected_instances,
    MAX(si.current_config_version) as latest_version,
    MAX(si.last_heartbeat) as last_heartbeat
FROM service_instances si
GROUP BY si.service_name;

-- ═══════════════════════════════════════════════════════════════════
-- Validation Statistics View
-- ═══════════════════════════════════════════════════════════════════

CREATE OR REPLACE VIEW validation_stats AS
SELECT 
    service_name,
    COUNT(*) as total_validations,
    SUM(CASE WHEN validation_result = true THEN 1 ELSE 0 END) as successful,
    SUM(CASE WHEN validation_result = false THEN 1 ELSE 0 END) as failed,
    ROUND(100.0 * SUM(CASE WHEN validation_result = true THEN 1 ELSE 0 END) / NULLIF(COUNT(*), 0), 2) as success_rate
FROM validation_history
GROUP BY service_name;

-- Migration complete
SELECT 'Migration 007: Views created' as status;