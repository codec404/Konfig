-- Migration 004: Audit and Health Monitoring
-- Description: Creates audit log and health check tables
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Audit Log Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS audit_log (
    id SERIAL PRIMARY KEY,
    config_id VARCHAR(255),
    action VARCHAR(100) NOT NULL,
    performed_by VARCHAR(255),
    details JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_audit_config_id ON audit_log(config_id);
CREATE INDEX IF NOT EXISTS idx_audit_created_at ON audit_log(created_at);

-- ═══════════════════════════════════════════════════════════════════
-- Health Checks Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS health_checks (
    id SERIAL PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    instance_id VARCHAR(255) NOT NULL,
    config_version BIGINT,
    status VARCHAR(50) NOT NULL,
    error_message TEXT,
    metrics JSONB,
    checked_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_health_service ON health_checks(service_name, instance_id);
CREATE INDEX IF NOT EXISTS idx_health_checked_at ON health_checks(checked_at);

-- Migration complete
SELECT 'Migration 004: Audit and health tables created' as status;