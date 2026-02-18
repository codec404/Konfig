-- Migration 003: Service Instance Tracking
-- Description: Creates tables for tracking connected service instances
-- Author: System
-- Date: 2026-02-18

-- ═══════════════════════════════════════════════════════════════════
-- Service Instances Table
-- ═══════════════════════════════════════════════════════════════════

CREATE TABLE IF NOT EXISTS service_instances (
    id SERIAL PRIMARY KEY,
    service_name VARCHAR(255) NOT NULL,
    instance_id VARCHAR(255) NOT NULL,
    current_config_version BIGINT,
    last_heartbeat TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    status VARCHAR(50) DEFAULT 'connected',
    metadata JSONB,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(service_name, instance_id)
);

CREATE INDEX IF NOT EXISTS idx_service_instances_name ON service_instances(service_name);
CREATE INDEX IF NOT EXISTS idx_service_instances_status ON service_instances(status);

-- Migration complete
SELECT 'Migration 003: Service instances table created' as status;